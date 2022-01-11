// Copyright 2017-2021 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Implementation of a code layout optimizer described by
// Phillip Yelland in "A New Approach to Optimal Code Formatting"
// (https://research.google/pubs/pub44667/) and originally implemented
// in rfmt (https://github.com/google/rfmt).

#include "common/formatting/layout_optimizer.h"

#include <algorithm>
#include <iomanip>
#include <ostream>

#include "absl/container/fixed_array.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/layout_optimizer_internal.h"
#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/container_iterator_range.h"
#include "common/util/value_saver.h"

namespace verible {

void OptimizeTokenPartitionTree(const BasicFormatStyle& style,
                                TokenPartitionTree* node) {
  CHECK_NOTNULL(node);
  VLOG(4) << __FUNCTION__ << ", before:\n" << *node;

  const auto optimizer = TokenPartitionsLayoutOptimizer(style);
  const auto indentation = node->Value().IndentationSpaces();
  optimizer.Optimize(indentation, node);

  VLOG(4) << __FUNCTION__ << ", after:\n" << *node;
}

namespace {

// Adopts sublayouts of 'source' into 'destination' if 'source' and
// 'destination' types are equal and 'source' doesn't have extra indentation.
// Otherwise adopts whole 'source'.
void AdoptLayoutAndFlattenIfSameType(const LayoutTree& source,
                                     LayoutTree* destination) {
  CHECK_NOTNULL(destination);
  const auto& src_item = source.Value();
  const auto& dst_item = destination->Value();
  if (!source.is_leaf() && src_item.Type() == dst_item.Type() &&
      src_item.IndentationSpaces() == 0) {
    const auto& first_subitem = source.Children().front().Value();
    CHECK(src_item.MustWrap() == first_subitem.MustWrap());
    CHECK(src_item.SpacesBefore() == first_subitem.SpacesBefore());
    for (const auto& sublayout : source.Children())
      destination->AdoptSubtree(sublayout);
  } else {
    destination->AdoptSubtree(source);
  }
}

int AlreadyFormattedPartitionLength(const TokenPartitionTree& partition) {
  auto tokens = partition.Value().TokensRange();
  if (tokens.empty()) return 0;
  int width = 0;

  width += partition.Value().IndentationSpaces();
  width += tokens.front().Length();

  for (const auto& token : make_range(tokens.begin() + 1, tokens.end())) {
    // TODO(mglb): either handle tokens with kPreserve break_decision, or
    // explicitly check for their absence. Preserved space is currently expected
    // to be emulated with kAlreadyFormatted/kInline partitions. Only tabular
    // aligner creates such partitions.
    width += token.before.spaces_required + token.Length();
  }

  for (const auto& child : partition.Children()) {
    CHECK_EQ(child.Value().PartitionPolicy(), PartitionPolicyEnum::kInline);
    if (child.Value().TokensRange().begin() != tokens.begin()) {
      auto& first_token = child.Value().TokensRange().front();
      // Substract spacing added in the loop above
      width -= first_token.before.spaces_required;
    }
    width += child.Value().IndentationSpaces();
  }

  return width;
}

// Largest possible column value, used as infinity.
constexpr int kInfinity = std::numeric_limits<int>::max();

}  // namespace

std::ostream& operator<<(std::ostream& stream, LayoutType type) {
  switch (type) {
    case LayoutType::kLine:
      return stream << "line";
    case LayoutType::kJuxtaposition:
      return stream << "juxtaposition";
    case LayoutType::kStack:
      return stream << "stack";
  }
  LOG(WARNING) << "Unknown layout type: " << int(type);
  return stream << "???";
}

std::ostream& operator<<(std::ostream& stream, const LayoutItem& layout) {
  if (layout.Type() == LayoutType::kLine) {
    stream << "[ " << layout.Text() << " ]"
           << ", length: " << layout.Length();
  } else {
    stream << "[<" << layout.Type() << ">]";
  }
  stream << ", indentation: " << layout.IndentationSpaces()
         << ", spacing: " << layout.SpacesBefore()
         << ", must wrap: " << (layout.MustWrap() ? "YES" : "no");
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const LayoutFunctionSegment& segment) {
  stream << "[" << std::setw(3) << std::setfill(' ') << segment.column << "] ("
         << std::fixed << std::setprecision(3) << segment.intercept << " + "
         << segment.gradient << "*x), span: " << segment.span << ", layout:\n";
  segment.layout.PrintTree(&stream, 6);
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const LayoutFunction& lf) {
  if (lf.empty()) return stream << "{}";

  stream << "{\n";
  for (const auto& segment : lf) {
    stream << "  [" << std::setw(3) << std::setfill(' ') << segment.column
           << "] (" << std::fixed << std::setprecision(3) << std::setw(8)
           << std::setfill(' ') << segment.intercept << " + " << std::setw(4)
           << std::setfill(' ') << segment.gradient
           << "*x), span: " << std::setw(3) << std::setfill(' ') << segment.span
           << ", layout:\n";
    segment.layout.PrintTree(&stream, 8);
    stream << "\n";
  }
  stream << "}";
  return stream;
}

LayoutFunction::iterator LayoutFunction::begin() {
  return LayoutFunction::iterator(*this, 0);
}

LayoutFunction::iterator LayoutFunction::end() {
  return LayoutFunction::iterator(*this, size());
}

LayoutFunction::const_iterator LayoutFunction::begin() const {
  return LayoutFunction::const_iterator(*this, 0);
}

LayoutFunction::const_iterator LayoutFunction::end() const {
  return LayoutFunction::const_iterator(*this, size());
}

LayoutFunction::const_iterator LayoutFunction::AtOrToTheLeftOf(
    int column) const {
  if (empty()) return end();

  const auto it = std::lower_bound(
      begin(), end(), column, [](const LayoutFunctionSegment& s, int column) {
        return s.column <= column;
      });
  CHECK_NE(it, begin());
  return it - 1;
}

LayoutFunction LayoutFunctionFactory::Line(const UnwrappedLine& uwline) const {
  auto layout = LayoutTree(LayoutItem(uwline));
  const auto span = layout.Value().Length();

  if (span < style_.column_limit) {
    return LayoutFunction{
        // 0 <= X < column_limit-span
        {0, layout, span, 0, 0},
        // column_limit-span <= X
        {style_.column_limit - span, std::move(layout), span, 0,
         style_.over_column_limit_penalty},
    };
  } else {
    return LayoutFunction{
        {0, std::move(layout), span,
         float((span - style_.column_limit) * style_.over_column_limit_penalty),
         style_.over_column_limit_penalty},
    };
  }
}

LayoutFunction LayoutFunctionFactory::Indent(const LayoutFunction& lf,
                                             int indent) const {
  CHECK(!lf.empty());
  CHECK_GE(indent, 0);

  LayoutFunction result;

  auto indent_column = 0;
  auto column = indent;
  auto segment = lf.AtOrToTheLeftOf(column);

  while (true) {
    auto columns_over_limit = column - style_.column_limit;

    const float new_intercept =
        segment->CostAt(column) -
        style_.over_column_limit_penalty * std::max(columns_over_limit, 0);
    const int new_gradient =
        segment->gradient -
        style_.over_column_limit_penalty * (columns_over_limit >= 0);

    auto new_layout = segment->layout;
    new_layout.Value().SetIndentationSpaces(
        new_layout.Value().IndentationSpaces() + indent);

    const int new_span = indent + segment->span;

    result.push_back(LayoutFunctionSegment{indent_column, std::move(new_layout),
                                           new_span, new_intercept,
                                           new_gradient});

    ++segment;
    if (segment == lf.end()) break;
    column = segment->column;
    indent_column = column - indent;
  }

  return result;
}

LayoutFunction LayoutFunctionFactory::Juxtaposition(
    const LayoutFunction& left, const LayoutFunction& right) const {
  CHECK(!left.empty());
  CHECK(!right.empty());

  LayoutFunction result;

  auto segment_l = left.begin();
  auto segment_r = right.begin();

  auto column_l = 0;
  auto column_r = segment_l->span + segment_r->layout.Value().SpacesBefore();
  segment_r = right.AtOrToTheLeftOf(column_r);

  while (true) {
    const int columns_over_limit = column_r - style_.column_limit;

    const float new_intercept =
        segment_l->CostAt(column_l) + segment_r->CostAt(column_r) -
        style_.over_column_limit_penalty * std::max(columns_over_limit, 0);
    const int new_gradient =
        segment_l->gradient + segment_r->gradient -
        (columns_over_limit >= 0 ? style_.over_column_limit_penalty : 0);

    const auto& layout_l = segment_l->layout;
    const auto& layout_r = segment_r->layout;
    auto new_layout = LayoutTree(LayoutItem(LayoutType::kJuxtaposition,
                                            layout_l.Value().SpacesBefore(),
                                            layout_l.Value().MustWrap()));

    AdoptLayoutAndFlattenIfSameType(layout_l, &new_layout);
    AdoptLayoutAndFlattenIfSameType(layout_r, &new_layout);

    const int new_span =
        segment_l->span + segment_r->span + layout_r.Value().SpacesBefore();

    result.push_back(LayoutFunctionSegment{column_l, std::move(new_layout),
                                           new_span, new_intercept,
                                           new_gradient});

    auto next_segment_l = segment_l + 1;
    auto next_column_l = kInfinity;
    if (next_segment_l != left.end()) next_column_l = next_segment_l->column;

    auto next_segment_r = segment_r + 1;
    auto next_column_r = kInfinity;
    if (next_segment_r != right.end()) next_column_r = next_segment_r->column;

    if (next_segment_l == left.end() && next_segment_r == right.end()) break;

    if (next_segment_r == right.end() ||
        (next_column_l - column_l) <= (next_column_r - column_r)) {
      column_l = next_column_l;
      column_r = next_column_l + next_segment_l->span +
                 layout_r.Value().SpacesBefore();

      segment_l = next_segment_l;
      segment_r = right.AtOrToTheLeftOf(column_r);
    } else {
      column_r = next_column_r;
      column_l =
          next_column_r - segment_l->span - layout_r.Value().SpacesBefore();

      segment_r = next_segment_r;
    }
  }

  return result;
}

LayoutFunction LayoutFunctionFactory::Stack(
    absl::FixedArray<LayoutFunction::const_iterator>* segments) const {
  CHECK(!segments->empty());

  LayoutFunction result;

  // Use first line's spacing for new layouts.
  const auto& first_layout_item = segments->front()->layout.Value();
  const auto spaces_before = first_layout_item.SpacesBefore();
  const auto break_decision = first_layout_item.MustWrap();
  // Use last line's span for new layouts. Other lines won't be modified by
  // any further layout combinations.
  const int span = segments->back()->span;

  const float line_breaks_penalty =
      (segments->size() - 1) * style_.line_break_penalty;

  // Iterate over columns from left to right and process a segment of each
  // LayoutFunction that is under currently iterated column.
  int current_column = 0;
  do {
    // Point iterators to segments under current column.
    for (auto& segment_it : *segments) {
      segment_it.MoveToKnotAtOrToTheLeftOf(current_column);
    }

    auto new_segment = LayoutFunctionSegment{
        current_column,
        LayoutTree(
            LayoutItem(LayoutType::kStack, spaces_before, break_decision)),
        span, line_breaks_penalty, 0};

    for (const auto& segment_it : *segments) {
      new_segment.intercept += segment_it->CostAt(current_column);
      new_segment.gradient += segment_it->gradient;
      AdoptLayoutAndFlattenIfSameType(segment_it->layout, &new_segment.layout);
    }
    result.push_back(std::move(new_segment));

    {
      // Find next column.
      int next_column = kInfinity;
      for (auto segment_it : *segments) {
        if ((segment_it + 1).IsEnd()) continue;
        const int column = (segment_it + 1)->column;
        CHECK_GE(column, current_column);
        if (column < next_column) next_column = column;
      }
      current_column = next_column;
    }
  } while (current_column < kInfinity);

  return result;
}

LayoutFunction LayoutFunctionFactory::Choice(
    absl::FixedArray<LayoutFunction::const_iterator>* segments) {
  CHECK(!segments->empty());

  LayoutFunction result;

  // Initial value set to an iterator that doesn't point to any existing
  // segment.
  LayoutFunction::const_iterator last_min_cost_segment =
      segments->front().Container().end();

  int current_column = 0;
  // Iterate (in increasing order) over starting columns (knots) of all
  // segments of every LayoutFunction.
  do {
    // Starting column of the next closest segment.
    int next_knot = kInfinity;

    for (auto& segment_it : *segments) {
      segment_it.MoveToKnotAtOrToTheLeftOf(current_column);

      const int column =
          (segment_it + 1).IsEnd() ? kInfinity : segment_it[1].column;
      if (column < next_knot) next_knot = column;
    }

    do {
      const LayoutFunction::const_iterator min_cost_segment = *std::min_element(
          segments->begin(), segments->end(),
          [current_column](const auto& a, const auto& b) {
            if (a->CostAt(current_column) != b->CostAt(current_column))
              return (a->CostAt(current_column) < b->CostAt(current_column));
            // Sort by gradient when cost is the same. Favor earlier
            // element when both gradients are equal.
            return (a->gradient <= b->gradient);
          });

      if (min_cost_segment != last_min_cost_segment) {
        result.push_back(LayoutFunctionSegment{
            current_column, min_cost_segment->layout, min_cost_segment->span,
            min_cost_segment->CostAt(current_column),
            min_cost_segment->gradient});
        last_min_cost_segment = min_cost_segment;
      }

      // Find closest crossover point located before next knot.
      int next_column = next_knot;
      for (const auto& segment : *segments) {
        if (segment->gradient >= min_cost_segment->gradient) continue;
        float gamma = (segment->CostAt(current_column) -
                       min_cost_segment->CostAt(current_column)) /
                      (min_cost_segment->gradient - segment->gradient);
        int column = current_column + std::ceil(gamma);
        if (column > current_column && column < next_column) {
          next_column = column;
        }
      }

      current_column = next_column;
    } while (current_column < next_knot);
  } while (current_column < kInfinity);

  return result;
}

void TokenPartitionsLayoutOptimizer::Optimize(int indentation,
                                              TokenPartitionTree* node) const {
  CHECK_NOTNULL(node);
  CHECK_GE(indentation, 0);

  const LayoutFunction layout_function = CalculateOptimalLayout(*node);

  CHECK(!layout_function.empty());
  VLOG(4) << __FUNCTION__ << ", layout function:\n" << layout_function;

  auto iter = layout_function.AtOrToTheLeftOf(indentation);
  CHECK(iter != layout_function.end());
  VLOG(4) << __FUNCTION__ << ", layout:\n" << iter->layout;

  TreeReconstructor tree_reconstructor(indentation);
  tree_reconstructor.TraverseTree(iter->layout);
  tree_reconstructor.ReplaceTokenPartitionTreeNode(node);
}

LayoutFunction TokenPartitionsLayoutOptimizer::CalculateOptimalLayout(
    const TokenPartitionTree& node) const {
  if (node.is_leaf()) return factory_.Line(node.Value());

  const auto calculate_optimal_layout_func =
      std::bind(&TokenPartitionsLayoutOptimizer::CalculateOptimalLayout, this,
                std::placeholders::_1);

  switch (node.Value().PartitionPolicy()) {
    case PartitionPolicyEnum::kInline: {
      // Shouldn't happen - the partition with this policy should always
      // be a leaf. Anyway, try to handle it without aborting.
      LOG(ERROR) << "Partition node with kInline policy should be "
                    "a leaf. Dropping its children. Partition node:\n"
                 << node << "\n\n*** Please file a bug. ***";
      return factory_.Line(node.Value());
    }

    case PartitionPolicyEnum::kAlreadyFormatted: {
      // When not a leaf, it contains partitions with kInline
      // policy. Pack them horizontally.
      const bool all_children_are_inlines =
          std::all_of(node.Children().begin(), node.Children().end(),
                      [](const TokenPartitionTree& child) {
                        return child.Value().PartitionPolicy() ==
                               PartitionPolicyEnum::kInline;
                      });
      LOG_IF(ERROR, !all_children_are_inlines)
          << "Partition node with kAlreadyFormatted policy should not "
             "contain children with policies other than kInline. "
             "Partition node:\n"
          << node << "\n\n*** Please file a bug. ***";

      absl::FixedArray<LayoutFunction> slice_lfs(node.Children().size());
      std::transform(node.Children().begin(), node.Children().end(),
                     slice_lfs.begin(), calculate_optimal_layout_func);

      slice_lfs.front().SetMustWrap(true);

      // Preserve spacing of the first sublayout. This has to be done because
      // the first layout in a line uses IndentationSpaces instead of
      // SpacesBefore.
      const auto indent = node.Children().front().Value().IndentationSpaces();
      slice_lfs.front() = factory_.Indent(slice_lfs.front(), indent);

      return factory_.Juxtaposition(slice_lfs.begin(), slice_lfs.end());
    }

    case PartitionPolicyEnum::kOptimalFunctionCallLayout: {
      // Support only function/macro/system calls for now
      if (node.Children().size() == 2) {
        const auto& function_header = node.Children()[0];
        const auto& function_args = node.Children()[1];

        auto header = CalculateOptimalLayout(function_header);
        auto args = CalculateOptimalLayout(function_args);

        auto stack_layout = factory_.Stack({
            header,
            factory_.Indent(args, style_.wrap_spaces),
        });
        if (args.MustWrap()) {
          return stack_layout;
        }
        auto juxtaposed_layout = factory_.Juxtaposition({
            header,
            args,
        });
        return factory_.Choice({
            std::move(juxtaposed_layout),
            std::move(stack_layout),
        });
      }

      // Apply suboptimal but reasonable layout and report the problem instead
      // of aborting.
      LOG(ERROR) << "Partition with the " << node.Value().PartitionPolicy()
                 << " policy contains unexpected subpartitions. "
                 << "Applying fallback formatting. Partition node:\n"
                 << node << "\n\n*** Please file a bug. ***";
      absl::FixedArray<LayoutFunction> layouts(node.Children().size());
      std::transform(node.Children().begin(), node.Children().end(),
                     layouts.begin(), calculate_optimal_layout_func);
      const auto& first_token = node.Value().TokensRange().front();
      // Preserve line break before the fragment if the original source has it.
      if (absl::StrContains(first_token.OriginalLeadingSpaces(), "\n"))
        layouts.front().SetMustWrap(true);
      return factory_.Wrap(layouts.begin(), layouts.end());
    }

    case PartitionPolicyEnum::kAppendFittingSubPartitions:
    case PartitionPolicyEnum::kFitOnLineElseExpand: {
      absl::FixedArray<LayoutFunction> layouts(node.Children().size());
      std::transform(node.Children().begin(), node.Children().end(),
                     layouts.begin(), calculate_optimal_layout_func);
      return factory_.Wrap(layouts.begin(), layouts.end());
    }

    default:
      // Stack layout is probably syntax-safe in all situations. Try it without
      // aborting.
      LOG(ERROR) << "Unsupported partition policy: "
                 << node.Value().PartitionPolicy()
                 << ". Defaulting to stack layout. Partition node:\n"
                 << node << "\n\n*** Please file a bug. ***";
      [[fallthrough]];
    case PartitionPolicyEnum::kAlwaysExpand:
    case PartitionPolicyEnum::kTabularAlignment: {
      absl::FixedArray<LayoutFunction> layouts(node.Children().size());
      std::transform(node.Children().begin(), node.Children().end(),
                     layouts.begin(), calculate_optimal_layout_func);
      return factory_.Stack(layouts.begin(), layouts.end());
    }

      // TODO(mglb): Think about introducing PartitionPolicies that
      // correspond directly to combinators in LayoutFunctionFactory.
      // kOptimalFunctionCallLayout strategy could then be implemented
      // directly in TreeUnwrapper. It would also allow for proper
      // handling of other policies (e.g. kTabularAlignment) in subtrees.
  }
}

void TreeReconstructor::TraverseTree(const LayoutTree& layout_tree) {
  const auto& layout = layout_tree.Value();
  const auto relative_indentation = layout.IndentationSpaces();
  const ValueSaver<int> indent_saver(
      &current_indentation_spaces_,
      current_indentation_spaces_ + relative_indentation);
  // Setting indentation for a line that is going to be appended is invalid and
  // probably has been done for some reason that is not going to work as
  // intended.
  LOG_IF(WARNING, ((relative_indentation > 0) && (current_node_ != nullptr)))
      << "Discarding indentation of a line that's going to be appended.";

  switch (layout.Type()) {
    case LayoutType::kLine: {
      CHECK(layout_tree.Children().empty());

      if (current_node_ == nullptr) {
        auto uwline = UnwrappedLine(current_indentation_spaces_,
                                    layout.TokensRange().begin(),
                                    PartitionPolicyEnum::kAlreadyFormatted);
        uwline.SpanUpToToken(layout.TokensRange().end());
        current_node_ = tree_.NewChild(uwline);
      } else {
        const auto tokens = layout.TokensRange();
        CHECK(current_node_->Value().TokensRange().end() == tokens.begin());

        current_node_->Value().SpanUpToToken(tokens.end());

        auto& slices = current_node_->Children();
        // TODO(mglb): add support for break_decision == Preserve
        if (layout.SpacesBefore() == tokens.front().before.spaces_required) {
          // No need for separate inline partition
          if (!slices.empty())
            slices.back().Value().SpanUpToToken(tokens.end());
          return;
        }

        // Wrap previous tokens in the line
        if (slices.empty()) {
          current_node_->NewChild(
              UnwrappedLine(0, current_node_->Value().TokensRange().begin(),
                            PartitionPolicyEnum::kInline));
        }
        slices.back().Value().SpanUpToToken(tokens.begin());

        // Wrap tokens from current layout
        auto slice = UnwrappedLine(layout.SpacesBefore(), tokens.begin(),
                                   PartitionPolicyEnum::kInline);
        slice.SpanUpToToken(tokens.end());
        current_node_->NewChild(slice);
      }
      return;
    }

    case LayoutType::kJuxtaposition: {
      // Append all children
      for (const auto& child : layout_tree.Children()) {
        TraverseTree(child);
      }
      return;
    }

    case LayoutType::kStack: {
      if (layout_tree.Children().empty()) {
        return;
      }
      if (layout_tree.Children().size() == 1) {
        TraverseTree(layout_tree.Children().front());
        return;
      }

      // Calculate indent for 2nd and further lines.
      int indentation = current_indentation_spaces_;
      if (current_node_ != nullptr) {
        indentation = AlreadyFormattedPartitionLength(*current_node_) +
                      layout.SpacesBefore();
      }

      // Append first child
      TraverseTree(layout_tree.Children().front());

      // Put remaining children in their own (indented) lines
      const ValueSaver<int> indent_saver(&current_indentation_spaces_,
                                         indentation);
      for (const auto& child : make_range(layout_tree.Children().begin() + 1,
                                          layout_tree.Children().end())) {
        current_node_ = nullptr;
        TraverseTree(child);
      }
      return;
    }
  }
}

void TreeReconstructor::ReplaceTokenPartitionTreeNode(
    TokenPartitionTree* node) {
  CHECK_NOTNULL(node);
  CHECK(!tree_.Children().empty());

  if (tree_.Children().size() == 1) {
    *node = std::move(tree_.Children().front());
  } else {
    const auto& first_line = tree_.Children().front().Value();
    const auto& last_line = tree_.Children().back().Value();

    node->Value() = UnwrappedLine(current_indentation_spaces_,
                                  first_line.TokensRange().begin(),
                                  PartitionPolicyEnum::kAlwaysExpand);
    node->Value().SpanUpToToken(last_line.TokensRange().end());
    node->Children().clear();
    node->AdoptSubtreesFrom(&tree_);
  }
}

}  // namespace verible
