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

// Improve code formatting by optimizing token partitions layout with
// algorithm documented in https://research.google/pubs/pub44667/
// (similar tool for R language: https://github.com/google/rfmt)

#include "common/formatting/layout_optimizer.h"

#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/state_node.h"
#include "common/util/value_saver.h"

namespace verible {

std::ostream& operator<<(std::ostream& stream, const LayoutType& type) {
  switch (type) {
    case LayoutType::kLayoutLine:
      return stream << "[<line>]";
    case LayoutType::kLayoutHorizontalMerge:
      return stream << "[<horizontal>]";
    case LayoutType::kLayoutVerticalMerge:
      return stream << "[<vertical>]";
    case LayoutType::kLayoutIndent:
      return stream << "[<indent>]";
  }
  LOG(FATAL) << "Unknown layout type " << int(type);
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Layout& layout) {
  const auto type = layout.GetType();
  if (type == LayoutType::kLayoutLine) {
    return stream << "[" << layout.Text() << "]"
                  << ", spacing: " << layout.SpacesBeforeLayout()
                  << ", length: " << layout.Length()
                  << (layout.MustWrapLayout()
                          ? ", must-wrap"
                          : (layout.MustAppendLayout() ? ", must-append" : ""));
  }
  return stream << layout.GetType()
                << ", indent: " << layout.GetIndentationSpaces()
                << ", spacing: " << layout.SpacesBeforeLayout();
}

std::ostream& operator<<(std::ostream& stream, const Knot& knot) {
  return stream << "(column: " << knot.column_ << ", span: " << knot.span_
                << ", intercept: " << knot.intercept_
                << ", gradient: " << knot.gradient_ << ", layout_tree:\n"
                << knot.layout_ << ", spaces: " << knot.before_spaces_
                << ", action: " << knot.break_decision_ << ")\n";
}

KnotSet KnotSet::FromUnwrappedLine(const UnwrappedLine& uwline,
                                   const BasicFormatStyle& style) {
  auto layout = Layout(uwline);
  const auto span = layout.Length();
  auto layout_tree = LayoutTree(layout);
  KnotSet knot_set;

  if (span < style.column_limit) {
    knot_set.AppendKnot(Knot(0,     // column (starting)
                             span,  // layout span (columns)
                             0,     // intercept
                             0,  // zero gradient because of under column limit
                             layout_tree,  // layout
                             layout.SpacesBeforeLayout(),
                             layout.SpacingOptionsLayout()));
    knot_set.AppendKnot(Knot(style.column_limit - span, span,
                             0,                                // intercept
                             style.over_column_limit_penalty,  // gradient
                             layout_tree, layout.SpacesBeforeLayout(),
                             layout.SpacingOptionsLayout()));
  } else {
    knot_set.AppendKnot(
        Knot(0, span,
             // cost of choosing this solution
             // columns over limit x over column limit penalty
             (span - style.column_limit) * style.over_column_limit_penalty,
             style.over_column_limit_penalty,  // over column limit penalty
             layout_tree, layout.SpacesBeforeLayout(),
             layout.SpacingOptionsLayout()));
  }

  return knot_set;
}

KnotSet KnotSet::IndentBlock(const KnotSet right, int indent,
                             const BasicFormatStyle& style) {
  KnotSet ret;

  auto s2 = KnotSetIterator(right);

  auto s1_margin = 0;
  auto s2_margin = indent;
  s2.MoveToMargin(s2_margin);

  const auto infinity = std::numeric_limits<int>::max();

  while (true) {
    auto g2 = s2.CurrentKnot().GetGradient();

    auto overhang = s2_margin - style.column_limit;
    auto g_cur = g2 - style.over_column_limit_penalty * (overhang >= 0);
    float i_cur = s2.CurrentKnotValueAt(s2_margin) -
                  style.over_column_limit_penalty * std::max(overhang, 0);

    auto current_layout = Layout(indent);
    auto current_layout_tree = LayoutTree(current_layout);
    current_layout_tree.AdoptSubtree(s2.CurrentKnot().GetLayout());
    ret.AppendKnot(Knot(s1_margin,                            // column
                        indent + s2.CurrentKnot().GetSpan(),  // span
                        i_cur,                                // intercept
                        g_cur,                                // gradient
                        current_layout_tree,                  // layout
                        0,                                    // spaces before
                        SpacingOptions::Undecided));  // spacing decision

    const auto kn2 = s2.NextKnotColumn();
    if (kn2 == infinity) {
      break;
    }
    s2.Advance();

    s2_margin = kn2;
    s1_margin = s2_margin - indent;
  }

  VLOG(4) << "Indent:\n" << ret;
  return ret;
}

KnotSet KnotSet::InterceptPlusConst(float const_val) const {
  KnotSet ret;
  for (const auto& itr : knots_) {
    ret.AppendKnot(Knot{itr.GetColumn(), itr.GetSpan(),
                        itr.GetIntercept() + const_val, itr.GetGradient(),
                        itr.GetLayout(), itr.GetSpacesBefore(),
                        itr.GetSpacingOptions()});
  }
  return ret;
}

std::ostream& operator<<(std::ostream& stream, const KnotSet& knot_set) {
  stream << "{\n";
  for (const auto& itr : knot_set.knots_) {
    stream << "  " << itr;
  }
  stream << "}\n";
  return stream;
}

// FIXME(ldk): I think that none of those kind of functions should be const.
//    All operations like join/merge/wrap and so on should result in empty
//    solultion set.
KnotSet SolutionSet::VerticalJoin(const BasicFormatStyle& style) {
  KnotSet ret;

  // Iterator set
  ResetIteratorSet();

  const auto first_knot_spaces_before = front()[0].GetSpacesBefore();
  const auto first_knot_spacing_action = front()[0].GetSpacingOptions();
  const int last_knot_set_span = back()[0].GetSpan();
  const auto infinity = std::numeric_limits<int>::max();
  const auto plus_const =
      (size() <= 1) ? 0 : (size() - 1) * style.line_break_penalty;

  int margin = 0;
  while (true) {
    float current_intercept = 0;
    int current_gradient = 0;
    auto current_layout =
        Layout(LayoutType::kLayoutVerticalMerge, iterator_set_.front()
                                                     .CurrentKnot()
                                                     .GetLayout()
                                                     .Value()
                                                     .SpacesBeforeLayout());
    auto current_layout_tree = LayoutTree(current_layout);

    for (auto& itr : iterator_set_) {
      const auto& knot = itr.CurrentKnot();
      current_intercept += knot.ValueAt(margin);
      current_gradient += knot.GetGradient();
      current_layout_tree.AdoptSubtree(knot.GetLayout());
    }

    ret.AppendKnot(Knot(margin, last_knot_set_span,
                        current_intercept + plus_const, current_gradient,
                        current_layout_tree, first_knot_spaces_before,
                        first_knot_spacing_action));

    int d_star = infinity;

    for (auto& itr : iterator_set_) {
      int knot = itr.NextKnotColumn();

      if (knot == infinity || knot <= margin) {
        continue;
      }

      if ((knot - margin) < d_star) {
        d_star = knot - margin;
      }
    }

    if (d_star == infinity) {
      break;
    }

    margin += d_star;
    MoveIteratorSet(margin);
  }

  clear();
  return ret;
}

KnotSet SolutionSet::HorizontalJoin(const BasicFormatStyle& style) {
  // Consecutively merge subpartitions
  while (size() > 1) {
    const auto left = front();
    pop_front();
    const auto right = front();
    pop_front();

    // Join horizontally
    const auto& joined = HorizontalJoin(left, right, style);

    push_front(joined);
  }

  KnotSet ret = front();
  clear();
  return ret;
}

KnotSet SolutionSet::MinimalSet(const BasicFormatStyle& style) {
  if (size() == 0) {
    return KnotSet();
  } else if (size() == 1) {
    const auto knot_set = front();
    clear();
    return knot_set;
  }

  KnotSet ret;

  // iterator set
  ResetIteratorSet();

  int k_l = 0;
  int last_min_value_idx = -1;
  int last_min_soln_idx = -1;

  const auto infinity = std::numeric_limits<int>::max();

  while (k_l < infinity) {
    const auto k_h = IteratorSetMinimalNextColumn() - 1;
    const auto gradients = IteratorSetCurrentGradients();

    while (true) {
      // Cost _values_ at knot 'k_l'
      const auto values = IteratorSetValuesAt(k_l);
      const auto min_value_itr = std::min_element(values.begin(), values.end());
      const auto min_value_idx = std::distance(values.begin(), min_value_itr);
      const auto min_value = *min_value_itr;

      const auto min_gradient = gradients[min_value_idx];

      const auto& min_soln = iterator_set_[min_value_idx];
      const auto min_soln_idx = min_soln.GetIndex();

      if ((min_value_idx != last_min_value_idx) ||
          (min_soln_idx != last_min_soln_idx)) {
        ret.AppendKnot(Knot(k_l, min_soln.CurrentKnot().GetSpan(), min_value,
                            min_gradient, min_soln.CurrentKnot().GetLayout(),
                            min_soln.CurrentKnot().GetSpacesBefore(),
                            min_soln.CurrentKnot().GetSpacingOptions()));
        last_min_value_idx = min_value_idx;
        last_min_soln_idx = min_soln_idx;
      }

      std::vector<int> distances_to_cross;
      for (unsigned int i = 0; i < iterator_set_.size(); i += 1) {
        if (gradients[i] >= min_gradient) {
          continue;
        }

        float gamma = (values[i] - min_value) / (min_gradient - gradients[i]);
        distances_to_cross.push_back(ceil(gamma));
      }

      std::vector<int> crossovers;
      for (const auto& d : distances_to_cross) {
        if (d > 0 && k_l + d <= k_h) {
          crossovers.push_back(k_l + d);
        }
      }

      if (crossovers.size() > 0) {
        k_l = *std::min_element(crossovers.begin(), crossovers.end());
      } else {
        k_l = k_h + 1;
        if (k_l < infinity) {
          MoveIteratorSet(k_l);
        }
        break;
      }
    }
  }

  return ret;
}

KnotSet SolutionSet::WrapSet(const BasicFormatStyle& style) {
  if (size() == 0) {
    return KnotSet();
  } else if (size() == 1) {
    const auto knot_set = front();
    clear();
    return knot_set;
  }

  SolutionSet elt_layouts;
  std::transform(begin(), end(), std::back_inserter(elt_layouts),
                 [](const KnotSet& knot_set) { return knot_set; });

  SolutionSet wrap_solutions;
  std::transform(begin(), end(), std::back_inserter(wrap_solutions),
                 [](const KnotSet& knot_set) { return KnotSet(); });

  assert(elt_layouts.size() == size());
  assert(elt_layouts.size() == wrap_solutions.size());

  const int n = size();
  for (int i = n - 1; i >= 0; --i) {
    SolutionSet solution_i;
    auto line_layout = elt_layouts[i];

    for (int j = i; j < n - 1; ++j) {
      const auto full_soln =
          SolutionSet{line_layout, wrap_solutions[j + 1]}.VerticalJoin(style);

      const float cpack = 1e-3;
      solution_i.push_back(full_soln.InterceptPlusConst(
          style.line_break_penalty + cpack * (n - j)));
      const auto& elt_layout = elt_layouts[j + 1];
      if (elt_layout.MustWrap()) {
        line_layout = SolutionSet{line_layout, elt_layout}.VerticalJoin(style);
      } else {
        line_layout =
            SolutionSet{line_layout, elt_layout}.HorizontalJoin(style);
      }
    }

    solution_i.push_back(line_layout);
    wrap_solutions[i] = solution_i.MinimalSet(style);
  }

  clear();
  auto ks = wrap_solutions.size() > 0 ? wrap_solutions[0] : KnotSet();
  VLOG(4) << "WrapSet:\n" << ks;
  return ks;
}

KnotSet SolutionSet::HorizontalJoin(const KnotSet& left, const KnotSet& right,
                                    const BasicFormatStyle& style) {
  KnotSet ret;

  auto s1 = KnotSetIterator(left);
  auto s2 = KnotSetIterator(right);

  auto s1_margin = 0;
  auto s2_margin =
      s1.CurrentKnot().GetSpan() + s2.CurrentKnot().GetSpacesBefore();
  s2.MoveToMargin(s2_margin);

  const auto infinity = std::numeric_limits<int>::max();

  while (true) {
    auto g1 = s1.CurrentKnot().GetGradient();
    auto g2 = s2.CurrentKnot().GetGradient();

    auto overhang = s2_margin - style.column_limit;
    auto g_cur = g1 + g2 - style.over_column_limit_penalty * (overhang >= 0);
    float i_cur = s1.CurrentKnotValueAt(s1_margin) +
                  s2.CurrentKnotValueAt(s2_margin) -
                  style.over_column_limit_penalty * std::max(overhang, 0);

    const auto s1_layout = s1.CurrentKnot().GetLayout();
    const auto s2_layout = s2.CurrentKnot().GetLayout();
    auto current_layout = Layout(LayoutType::kLayoutHorizontalMerge,
                                 s1_layout.Value().SpacesBeforeLayout());
    auto current_layout_tree = LayoutTree(current_layout);
    current_layout_tree.AdoptSubtree(s1_layout);
    current_layout_tree.AdoptSubtree(s2_layout);
    ret.AppendKnot(Knot(
        s1_margin,
        s1.CurrentKnot().GetSpan() + s2.CurrentKnot().GetSpacesBefore() +
            s2.CurrentKnot().GetSpan(),
        i_cur, g_cur, current_layout_tree, s1.CurrentKnot().GetSpacesBefore(),
        s1.CurrentKnot().GetSpacingOptions()));

    const auto kn1 = s1.NextKnotColumn();
    const auto kn2 = s2.NextKnotColumn();

    if (kn1 == infinity && kn2 == infinity) {
      break;
    }

    if (kn1 - s1_margin <= kn2 - s2_margin) {
      s1.Advance();
      s1_margin = kn1;
      s2_margin = s1_margin + s1.CurrentKnot().GetSpan() +
                  s2.CurrentKnot().GetSpacesBefore();
      s2.MoveToMargin(s2_margin);
    } else {
      s2.Advance();
      s2_margin = kn2;
      s1_margin = s2_margin - s1.CurrentKnot().GetSpan() -
                  s2.CurrentKnot().GetSpacesBefore();
    }
  }

  VLOG(4) << "ret:\n" << ret;
  return ret;
}

std::ostream& operator<<(std::ostream& ostream,
                         const SolutionSet& solution_set) {
  for (const auto& itr : solution_set) {
    ostream << itr;
  }
  return ostream;
}

void TreeReconstructor::TraverseTree(const LayoutTree& layout_tree) {
  const auto type = layout_tree.Value().GetType();

  switch (type) {
    case LayoutType::kLayoutLine: {
      const auto& uwline = layout_tree.Value().AsUnwrappedLine();
      assert(layout_tree.Children().size() == 0);

      if (active_unwrapped_line_ == nullptr) {
        unwrapped_lines_.push_back(uwline);
        active_unwrapped_line_ = &unwrapped_lines_.back();
        active_unwrapped_line_->SetIndentationSpaces(
            current_indentation_spaces_);
      }

      active_unwrapped_line_->SpanUpToToken(
          layout_tree.Value().AsUnwrappedLine().TokensRange().end());
      return;
    }

    case LayoutType::kLayoutHorizontalMerge: {
      // Organize children horizontally (by appending to current unwrapped
      // line)
      for (const auto& child : layout_tree.Children()) {
        TraverseTree(child);
      }
      return;
    }

    case LayoutType::kLayoutVerticalMerge: {
      // Nothing to do
      if (layout_tree.Children().size() == 0) {
        return;
      }

      if (layout_tree.Children().size() == 1) {
        TraverseTree(layout_tree.Children().front());
        return;
      }

      int indentation = current_indentation_spaces_;

      // Appending. Need to calculate new indentation for
      // second and later layouts
      if (active_unwrapped_line_ != nullptr) {
        indentation = FitsOnLine(*active_unwrapped_line_, style_).final_column +
                      layout_tree.Value().SpacesBeforeLayout();
      }

      // Append that child
      TraverseTree(layout_tree.Children().front());

      // Wrap other ones
      const ValueSaver<int> indent_saver(&current_indentation_spaces_,
                                         indentation);
      for (auto itr = std::next(layout_tree.Children().begin());
           itr != layout_tree.Children().end(); ++itr) {
        // Organize childrens vertically
        active_unwrapped_line_ = nullptr;
        TraverseTree(*itr);
      }
      return;
    }

    case LayoutType::kLayoutIndent: {
      assert(layout_tree.Children().size() == 1);
      const auto relative_indentation =
          layout_tree.Value().GetIndentationSpaces();

      const ValueSaver<int> indent_saver(
          &current_indentation_spaces_,
          current_indentation_spaces_ + relative_indentation);

      // Apply indentation for children by enforcing
      // start of new line
      active_unwrapped_line_ = nullptr;
      TraverseTree(layout_tree.Children().front());
      return;
    }
  }
}

void OptimizeTokenPartitionTree(TokenPartitionTree* node,
                                const BasicFormatStyle& style) {
  // VLOG(4) << "Optimize token partition tree:\n" << *node;
  const auto indentation = node->Value().IndentationSpaces();

  std::function<KnotSet(const TokenPartitionTree&)> TraverseTree =
      [&TraverseTree, &style](const TokenPartitionTree& n) {
        const auto policy = n.Value().PartitionPolicy();

        // leaf
        if (n.Children().size() == 0) {
          return KnotSet::FromUnwrappedLine(n.Value(), style);
        }

        switch (policy) {
          case PartitionPolicyEnum::kOptimalLayout: {
            // Support only function/macro/system calls
            assert(n.Children().size() == 2);

            const auto& function_header = n.Children()[0];
            const auto& function_args = n.Children()[1];

            const auto header_knot_set = TraverseTree(function_header);
            const auto args_knot_set = TraverseTree(function_args);

            SolutionSet choice_set;
            // Prefer HorizontalJoin over VerticalJoin
            // FIXME(ldk): Order of subsolutions shouldn't matter
            if (!args_knot_set.MustWrap()) {
              choice_set.push_back(
                  SolutionSet{header_knot_set, args_knot_set}.HorizontalJoin(
                      style));
            }
            choice_set.push_back(SolutionSet{
                header_knot_set,
                KnotSet::IndentBlock(args_knot_set, style.wrap_spaces, style)}
                                     .VerticalJoin(style));
            return choice_set.MinimalSet(style);
          }

          // FIXME(ldk): How to handle kFitOnLineElseExpand?
          //     Try to append (currently) or all-or-nothing (originally)?
          case PartitionPolicyEnum::kFitOnLineElseExpand: {
            // VLOG(4) << "wrap_subpartitions:\n" << n;
            SolutionSet wrap_set;
            std::transform(n.Children().begin(), n.Children().end(),
                           std::back_inserter(wrap_set),
                           [&TraverseTree](const TokenPartitionTree& subnode) {
                             return TraverseTree(subnode);
                           });
            return wrap_set.WrapSet(style);
          }

          default: {
            LOG(ERROR) << "Unsupported policy: " << policy;
            LOG(ERROR) << "Node:\n" << n;
            assert(false);
            return KnotSet();
          }
        }
      };

  auto solution = TraverseTree(*ABSL_DIE_IF_NULL(node));
  assert(solution.Size() > 0);
  VLOG(4) << "solution:\n" << solution;

  KnotSetIterator itr(solution);
  itr.MoveToMargin(indentation);
  assert(itr.Done() == false);
  VLOG(4) << "layout:\n" << itr.CurrentKnot().GetLayout();

  TreeReconstructor tree_reconstructor(indentation, style);
  tree_reconstructor.TraverseTree(itr.CurrentKnot().GetLayout());
  tree_reconstructor.ReplaceTokenPartitionTreeNode(node);
}

}  // namespace verible
