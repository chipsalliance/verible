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

#include <algorithm>
#include <iomanip>
#include <ostream>

#include "absl/container/fixed_array.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/container_iterator_range.h"
#include "common/util/value_saver.h"

namespace verible {

namespace layout_optimizer_internal {

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

// Largest possible column value, used as infinity.
constexpr int kInfinity = std::numeric_limits<int>::max();

}  // namespace

std::ostream& operator<<(std::ostream& stream, LayoutType type) {
  switch (type) {
    case LayoutType::kLine:
      return stream << "line";
    case LayoutType::kStack:
      return stream << "stack";
  }
  LOG(FATAL) << "Unknown layout type: " << int(type);
  return stream;
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

  auto left_it = begin();
  auto it = left_it + 1;
  while (it != end() && it->column <= column) {
    left_it = it;
    ++it;
  }
  return left_it;
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

LayoutFunction LayoutFunctionFactory::Stack(
    absl::FixedArray<LayoutFunction::const_iterator>& segments) const {
  CHECK(!segments.empty());

  LayoutFunction result;

  // Use fist line's spacing for new layouts.
  const auto& first_layout_item = segments.front()->layout.Value();
  const auto spaces_before = first_layout_item.SpacesBefore();
  const auto break_decision = first_layout_item.MustWrap();
  // Use last line's span for new layouts. Other lines won't be modified by
  // any further layout combinations.
  const int span = segments.back()->span;

  const float line_breaks_penalty =
      (segments.size() - 1) * style_.line_break_penalty;

  // Iterate over columns from left to right and process a segment of each
  // LayoutFunction that is under currently iterated column.
  int current_column = 0;
  do {
    // Point iterators to segments under current column.
    for (auto& segment_it : segments) {
      segment_it.MoveToKnotAtOrToTheLeftOf(current_column);
    }

    auto new_segment = LayoutFunctionSegment{
        current_column,
        LayoutTree(
            LayoutItem(LayoutType::kStack, spaces_before, break_decision)),
        span, line_breaks_penalty, 0};

    for (const auto& segment_it : segments) {
      new_segment.intercept += segment_it->CostAt(current_column);
      new_segment.gradient += segment_it->gradient;
      AdoptLayoutAndFlattenIfSameType(segment_it->layout, &new_segment.layout);
    }
    result.push_back(std::move(new_segment));

    {
      // Find next column.
      int next_column = kInfinity;
      for (auto segment_it : segments) {
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
    absl::FixedArray<LayoutFunction::const_iterator>& segments) {
  CHECK(!segments.empty());

  LayoutFunction result;

  // Initial value set to an iterator that doesn't point to any existing
  // segment.
  LayoutFunction::const_iterator last_min_cost_segment =
      segments.front().Container().end();

  int current_column = 0;
  // Iterate (in increasing order) over starting columns (knots) of all
  // segments of every LayoutFunction.
  do {
    // Starting column of the next closest segment.
    int next_knot = kInfinity;

    for (auto& segment_it : segments) {
      segment_it.MoveToKnotAtOrToTheLeftOf(current_column);

      const int column =
          (segment_it + 1).IsEnd() ? kInfinity : segment_it[1].column;
      if (column < next_knot) next_knot = column;
    }

    do {
      const LayoutFunction::const_iterator min_cost_segment = *std::min_element(
          segments.begin(), segments.end(),
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
      for (const auto& segment : segments) {
        if (segment->gradient >= min_cost_segment->gradient) continue;
        float gamma = (segment->CostAt(current_column) -
                       min_cost_segment->CostAt(current_column)) /
                      (min_cost_segment->gradient - segment->gradient);
        int column = current_column + std::ceil(gamma);
        if (column > current_column && column < next_knot &&
            column < next_column) {
          next_column = column;
        }
      }

      current_column = next_column;
    } while (current_column < next_knot);
  } while (current_column < kInfinity);

  return result;
}

}  // namespace layout_optimizer_internal

}  // namespace verible
