// Copyright 2017-2020 The Verible Authors.
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

#include "verible/common/formatting/align.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-context-visitor.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/enum-flags.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"
#include "verible/common/util/tree-operations.h"
#include "verible/common/util/vector-tree-iterators.h"
#include "verible/common/util/vector-tree.h"

namespace verible {

static const verible::EnumNameMap<AlignmentPolicy> &AlignmentPolicyNameMap() {
  static const verible::EnumNameMap<AlignmentPolicy> kAlignmentPolicyNameMap({
      {"align", AlignmentPolicy::kAlign},
      {"flush-left", AlignmentPolicy::kFlushLeft},
      {"preserve", AlignmentPolicy::kPreserve},
      {"infer", AlignmentPolicy::kInferUserIntent},
      // etc.
  });
  return kAlignmentPolicyNameMap;
}

std::ostream &operator<<(std::ostream &stream, AlignmentPolicy policy) {
  return AlignmentPolicyNameMap().Unparse(policy, stream);
}

bool AbslParseFlag(std::string_view text, AlignmentPolicy *policy,
                   std::string *error) {
  return AlignmentPolicyNameMap().Parse(text, policy, error, "AlignmentPolicy");
}

std::string AbslUnparseFlag(const AlignmentPolicy &policy) {
  std::ostringstream stream;
  stream << policy;
  return stream.str();
}

static int EffectiveCellWidth(const FormatTokenRange &tokens) {
  if (tokens.empty()) return 0;
  VLOG(2) << __FUNCTION__;
  // Sum token text lengths plus required pre-spacings (except first token).
  // Note: LeadingSpacesLength() honors where original spacing when preserved.
  return std::accumulate(
      tokens.begin(), tokens.end(), -tokens.front().LeadingSpacesLength(),
      [](int total_width, const PreFormatToken &ftoken) {
        const int pre_width = ftoken.LeadingSpacesLength();
        const int text_length = ftoken.token->text().length();
        VLOG(2) << " +" << pre_width << " +" << text_length;
        // TODO(fangism): account for multi-line tokens like
        // block comments.
        return total_width + ftoken.LeadingSpacesLength() + text_length;
      });
}

static int EffectiveLeftBorderWidth(const FormatTokenRange &tokens) {
  if (tokens.empty()) return 0;
  return tokens.front().before.spaces_required;
}

using ColumnsTreePath = SyntaxTreePath;

struct AlignmentCell {
  // Slice of format tokens in this cell (may be empty range).
  FormatTokenRange tokens;
  // The width of this token excerpt that complies with minimum spacing.
  int compact_width = 0;
  // Width of the left-side spacing before this cell, which can be considered
  // as a space-only column, usually no more than 1 space wide.
  int left_border_width = 0;

  // Returns true when neither the cell nor its subcells contain any tokens.
  bool IsUnused() const { return (tokens.empty() && compact_width == 0); }
  // Returns true when the cell contains subcells with tokens.
  bool IsComposite() const { return (tokens.empty() && compact_width > 0); }

  int TotalWidth() const { return left_border_width + compact_width; }

  FormatTokenRange ConstTokensRange() const {
    return {tokens.begin(), tokens.end()};
  }

  void UpdateWidths() {
    compact_width = EffectiveCellWidth(ConstTokensRange());
    left_border_width = EffectiveLeftBorderWidth(tokens);
  }
};

using AlignmentRow = VectorTree<AlignmentCell>;
using AlignmentMatrix = std::vector<AlignmentRow>;

static std::ostream &operator<<(std::ostream &stream,
                                const AlignmentCell &cell) {
  if (!cell.tokens.empty()) {
    // See UnwrappedLine::AsCode for similar printing.
    stream << absl::StrJoin(cell.tokens, " ",
                            [](std::string *out, const PreFormatToken &token) {
                              absl::StrAppend(out, token.Text());
                            });
  }
  return stream;
}

// Type of functions used to generate textual node representations that are
// suitable for use in rectangular cell.
// The function is called with a tree node as its only argument. It should
// return a string containing the cell's text and a single character used as
// a filler for cell's empty space.
template <typename ValueType>
using CellLabelGetterFunc =
    std::function<std::pair<std::string, char>(const VectorTree<ValueType> &)>;

// Recursively creates a tree with cells textual data. Its main purpose is to
// split multi-line cell labels and calculate how many lines have to be printed.
// This is a helper function used in ColumsTreeFormatter.
template <typename ValueType, typename Cell>
static std::size_t CreateTextNodes(
    const VectorTree<ValueType> &src_node, VectorTree<Cell> *dst_node,
    const CellLabelGetterFunc<ValueType> &get_cell_label) {
  static constexpr std::size_t kMinCellWidth = 2;

  std::size_t depth = 0;
  std::size_t subtree_depth = 0;

  for (const auto &src_child : src_node.Children()) {
    const auto [text, filler] = get_cell_label(src_child);
    const std::vector<std::string> lines = absl::StrSplit(text, '\n');
    auto *dst_child = dst_node;
    for (const auto &line : lines) {
      dst_child->Children().emplace_back(
          Cell{line, filler, std::max(line.size(), kMinCellWidth)});
      dst_child = &dst_child->Children().back();
    }
    depth = std::max(depth, lines.size());
    subtree_depth = std::max(
        subtree_depth, CreateTextNodes(src_child, dst_child, get_cell_label));
  }
  return depth + subtree_depth;
}

// Prints visualization of columns tree 'root' to a 'stream'. The 'root' node
// itself is not visualized. The 'get_cell_label' callback is used to get the
// cell label printed for each node.
//
// The label's text can contain multiple lines. Each line can contain up to 3
// fields separated by tab character ('\t'). The first field is aligned to the
// left. The second field is either aligned to the right (when there are
// 2 fields) or centered (when there are 3 fields). The third field is aligned
// to the right. Empty space is filled with label's filler character.
template <typename ValueType>
static void ColumnsTreeFormatter(
    std::ostream &stream, const VectorTree<ValueType> &root,
    const CellLabelGetterFunc<ValueType> &get_cell_label) {
  if (root.Children().empty()) return;

  static constexpr std::string_view kCellSeparator = "|";

  struct Cell {
    std::string text;
    char filler = ' ';
    std::size_t width = 0;
  };
  VectorTree<Cell> text_tree;

  const std::size_t depth =
      CreateTextNodes<ValueType, Cell>(root, &text_tree, get_cell_label);

  // Adjust cells width to fit all their children
  for (auto &node : VectorTreePostOrderTraversal(text_tree)) {
    // Include separator width in cell width
    node.Value().width += kCellSeparator.size();
    if (is_leaf(node)) continue;
    const std::size_t children_width =
        std::accumulate(node.Children().begin(), node.Children().end(), 0,
                        [](std::size_t width, const VectorTree<Cell> &child) {
                          return width + child.Value().width;
                        });
    if (node.Value().width < children_width) {
      node.Value().width = children_width;
    }
  }
  // Adjust cells width to fill their parents
  for (auto &node : VectorTreePreOrderTraversal(text_tree)) {
    if (is_leaf(node)) continue;
    std::size_t children_width =
        std::accumulate(node.Children().begin(), node.Children().end(), 0,
                        [](std::size_t width, const VectorTree<Cell> &child) {
                          return width + child.Value().width;
                        });
    // There is at least one child; each cell minimum width is equal to:
    // CreateTextNodes::kMinCellWidth + kCellSeparator.size()
    CHECK_GT(children_width, 0);
    if (node.Value().width > children_width) {
      auto extra_width = node.Value().width - children_width;
      for (auto &child : node.Children()) {
        CHECK_GT(children_width, 0);
        const auto added_child_width =
            extra_width * child.Value().width / children_width;  // NOLINT
        extra_width -= added_child_width;
        children_width -= child.Value().width;
        child.Value().width += added_child_width;
      }
    }
  }

  std::vector<std::string> lines(depth);
  auto range = VectorTreePreOrderTraversal(text_tree);
  auto level_offset = NumAncestors(text_tree) + 1;
  for (auto &node : make_range(range.begin() + 1, range.end())) {
    auto &cell = node.Value();
    const std::size_t level = NumAncestors(node) - level_offset;
    if (level > 0 && verible::IsFirstChild(node)) {
      const int padding_len = lines[level - 1].size() - lines[level].size() -
                              node.Parent()->Value().width;
      if (padding_len > 0) {
        if (lines[level].empty()) {
          lines[level].append(std::string(padding_len, ' '));
        } else if (padding_len > int(kCellSeparator.size())) {
          lines[level].append(absl::StrCat(
              kCellSeparator,
              std::string(padding_len - kCellSeparator.size(), ' ')));
        }
      }
    }

    const std::vector<std::string_view> parts = absl::StrSplit(cell.text, '\t');

    const auto width = cell.width - kCellSeparator.size();

    switch (parts.size()) {
      case 1: {
        const std::string pad(width - parts[0].size(), cell.filler);
        absl::StrAppend(&lines[level], kCellSeparator, parts[0], pad);
        break;
      }
      case 2: {
        const std::string pad(width - parts[0].size() - parts[1].size(),
                              cell.filler);
        absl::StrAppend(&lines[level], kCellSeparator, parts[0], pad,
                        parts.back());
        break;
      }
      case 3: {
        std::size_t pos =
            std::clamp((width - parts[1].size()) / 2, parts[0].size() + 1,
                       width - parts[2].size() - parts[1].size() - 1);
        const std::string left_pad(pos - parts[0].size(), cell.filler);
        const std::string right_pad(
            width - parts[2].size() - (pos + parts[1].size()), cell.filler);
        absl::StrAppend(&lines[level], kCellSeparator, parts[0], left_pad,
                        parts[1], right_pad, parts[2]);
        break;
      }
      default:
        CHECK_LE(parts.size(), 3);
    }
  }
  for (const auto &line : lines) {
    if (!line.empty()) stream << line << kCellSeparator << "\n";
  }
}

// These properties are calculated/aggregated from alignment cells.
struct AlignedColumnConfiguration {
  int width = 0;
  int left_border = 0;

  int TotalWidth() const { return left_border + width; }

  void UpdateFromCell(const AlignmentCell &cell) {
    width = std::max(width, cell.compact_width);
    left_border = std::max(left_border, cell.left_border_width);
  }
};

/* static */ ColumnPositionTree *ColumnSchemaScanner::ReserveNewColumn(
    ColumnPositionTree *parent_column, const Symbol &symbol,
    const AlignmentColumnProperties &properties, const SyntaxTreePath &path) {
  CHECK_NOTNULL(parent_column);
  // The path helps establish a total ordering among all desired alignment
  // points, given that they may come from optional or repeated language
  // constructs.
  const SyntaxTreeLeaf *leaf = GetLeftmostLeaf(symbol);
  // It is possible for a node to be empty, in which case, ignore.
  if (leaf == nullptr) return nullptr;
  if (parent_column->Parent() != nullptr && parent_column->Children().empty()) {
    // Starting token of a column and its first subcolumn must be the same.
    // (subcolumns overlap their parent column).
    CHECK_EQ(parent_column->Value().starting_token, leaf->get());
  }
  // It's possible the previous cell's path was intentionally altered
  // to effectively fuse it with the cell that is about to be added.
  // When this occurs, take the (previous) leftmost token, and suppress
  // adding a new column.
  if (parent_column->Children().empty() ||
      parent_column->Children().back().Value().path != path) {
    parent_column->Children().emplace_back(
        ColumnPositionEntry{path, leaf->get(), properties});
    const auto &column = parent_column->Children().back();
    ColumnsTreePath column_path;
    verible::Path(column, column_path);
    VLOG(2) << "reserving new column for " << TreePathFormatter(path) << " at "
            << TreePathFormatter(column_path);
  }
  return &parent_column->Children().back();
}

struct AggregateColumnData {
  AggregateColumnData() = default;

  // This is taken as the first seen set of properties in any given column.
  AlignmentColumnProperties properties;
  // These tokens's positions will be used to identify alignment cell
  // boundaries.
  std::vector<TokenInfo> starting_tokens;

  SyntaxTreePath path;

  void Import(const ColumnPositionEntry &cell) {
    if (starting_tokens.empty()) {
      path = cell.path;
      // Take the first set of properties, and ignore the rest.
      // They should be consistent, coming from alignment cell scanners,
      // but this is not verified.
      properties = cell.properties;
    }
    starting_tokens.push_back(cell.starting_token);
  }
};

class ColumnSchemaAggregator {
 public:
  void Collect(const ColumnPositionTree &columns) {
    CollectColumnsTree(columns, &columns_);
  }

  // Sort columns by syntax tree path assigned to them and create an index that
  // maps syntax tree path to a column. Call this after collecting all columns.
  void Finalize() {
    syntax_to_columns_map_.clear();

    for (auto &node : VectorTreePreOrderTraversal(columns_)) {
      if (node.Parent()) {
        // Index the column
        auto it = syntax_to_columns_map_.emplace_hint(
            syntax_to_columns_map_.end(), node.Value().path, ColumnsTreePath{});
        verible::Path(node, it->second);
      }
      if (!is_leaf(node)) {
        // Sort subcolumns. This puts negative paths (leading non-tree token
        // columns) before empty, zero, and positive ones.
        std::sort(node.Children().begin(), node.Children().end(),
                  [](const auto &a, const auto &b) {
                    return a.Value().path < b.Value().path;
                  });
        // Propagate left_border_override property to the left subcolumn
        auto &left_child_data = node.Children().front().Value();
        left_child_data.properties.left_border_override =
            std::max(left_child_data.properties.left_border_override,
                     node.Value().properties.left_border_override);
      }
    }
  }

  const std::map<SyntaxTreePath, ColumnsTreePath> &SyntaxToColumnsMap() const {
    return syntax_to_columns_map_;
  }

  const VectorTree<AggregateColumnData> &Columns() const { return columns_; }

  VectorTree<AlignmentColumnProperties> ColumnProperties() const {
    return Transform<VectorTree<AlignmentColumnProperties>>(
        columns_, [](const VectorTree<AggregateColumnData> &data_node) {
          return data_node.Value().properties;
        });
  }

 private:
  void CollectColumnsTree(const ColumnPositionTree &column,
                          VectorTree<AggregateColumnData> *aggregate_column) {
    CHECK_NOTNULL(aggregate_column);
    for (const auto &subcolumn : column.Children()) {
      const auto [index_entry, insert] =
          syntax_to_columns_map_.try_emplace(subcolumn.Value().path);
      VectorTree<verible::AggregateColumnData> *aggregate_subcolumn;
      if (insert) {
        aggregate_column->Children().emplace_back();
        aggregate_subcolumn = &aggregate_column->Children().back();
        // Put aggregate column node's path in created index entry
        verible::Path(*aggregate_subcolumn, index_entry->second);
      } else {
        // Fact: existing aggregate_subcolumn is a direct child of
        // aggregate_column
        CHECK_GT(static_cast<int>(aggregate_column->Children().size()),
                 index_entry->second.back());
        aggregate_subcolumn =
            &aggregate_column->Children()[index_entry->second.back()];
      }
      aggregate_subcolumn->Value().Import(subcolumn.Value());
      CollectColumnsTree(subcolumn, aggregate_subcolumn);
    }
  }

  // Keeps track of unique positions where new columns are desired.
  // The nodes are sets of starting tokens, from which token ranges will be
  // computed per cell.
  VectorTree<AggregateColumnData> columns_;
  // 1:1 map between syntax tree's path and columns tree's path
  std::map<SyntaxTreePath, ColumnsTreePath> syntax_to_columns_map_;
};

// CellLabelGetterFunc which creates a label with column's path relative to
// its parent column and either '<' or '>' filler characters indicating whether
// the column flushes to the left or the right.
// `T` should be either AggregateColumnData or ColumnPositionEntry.
template <typename T>
static std::pair<std::string, char> GetColumnDataCellLabel(
    const VectorTree<T> &node) {
  std::ostringstream label;
  const SyntaxTreePath &path = node.Value().path;
  auto begin = path.begin();
  if (node.Parent()) {
    // Find and skip common prefix
    const auto &parent_path = node.Parent()->Value().path;
    auto parent_begin = parent_path.begin();
    while (begin != path.end() && parent_begin != parent_path.end() &&
           *begin == *parent_begin) {
      ++begin;
      ++parent_begin;
    }
  }
  label << " \t ";
  if (begin != path.begin() && begin != path.end()) label << ".";
  label << SequenceFormatter(
      iterator_range<SyntaxTreePath::const_iterator>(begin, path.end()), ".");
  label << " \t ";

  return {label.str(), node.Value().properties.flush_left ? '<' : '>'};
}

static std::ostream &operator<<(std::ostream &stream,
                                const VectorTree<AggregateColumnData> &tree) {
  ColumnsTreeFormatter<AggregateColumnData>(
      stream, tree, GetColumnDataCellLabel<AggregateColumnData>);
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const ColumnPositionTree &tree) {
  ColumnsTreeFormatter<ColumnPositionEntry>(
      stream, tree, GetColumnDataCellLabel<ColumnPositionEntry>);
  return stream;
}

static std::ostream &operator<<(std::ostream &stream,
                                const VectorTree<AlignmentCell> &tree) {
  ColumnsTreeFormatter<AlignmentCell>(
      stream, tree,
      [](const VectorTree<AlignmentCell> &node)
          -> std::pair<std::string, char> {
        const auto &cell = node.Value();
        if (cell.IsUnused()) {
          return {"", '.'};
        }
        const auto width_info = absl::StrCat("\t(", cell.left_border_width, "+",
                                             cell.compact_width, ")\t");
        if (cell.IsComposite()) {
          return {absl::StrCat("/", width_info, "\\"), '`'};
        }
        std::ostringstream label;
        label << "\t" << cell << "\t\n" << width_info;
        return {label.str(), ' '};
      });
  return stream;
}

static std::ostream &operator<<(
    std::ostream &stream, const VectorTree<AlignedColumnConfiguration> &tree) {
  ColumnsTreeFormatter<AlignedColumnConfiguration>(
      stream, tree, [](const VectorTree<AlignedColumnConfiguration> &node) {
        const auto &cell = node.Value();
        const auto label =
            absl::StrCat("\t", cell.left_border, "+", cell.width, "\t");
        return std::pair<std::string, char>{label, ' '};
      });
  return stream;
}

struct AlignmentRowData {
  // Range of format tokens whose space is to be adjusted for alignment.
  FormatTokenRange ftoken_range;

  // Set of cells found that correspond to an ordered, sparse set of columns
  // to be aligned with other rows.
  ColumnPositionTree sparse_columns;
};

static void FillAlignmentRow(
    const AlignmentRowData &row_data,
    const std::map<SyntaxTreePath, ColumnsTreePath> &columns_map,
    AlignmentRow *row) {
  const auto &sparse_columns(row_data.sparse_columns);
  FormatTokenRange remaining_tokens_range(row_data.ftoken_range);

  FormatTokenRange *prev_cell_tokens = nullptr;
  if (!is_leaf(sparse_columns)) {
    for (const auto &col : VectorTreeLeavesTraversal(sparse_columns)) {
      const auto column_loc_iter = columns_map.find(col.Value().path);
      CHECK(column_loc_iter != columns_map.end());

      const auto token_iter = std::find_if(
          remaining_tokens_range.begin(), remaining_tokens_range.end(),
          [=](const PreFormatToken &ftoken) {
            return BoundsEqual(ftoken.Text(),
                               col.Value().starting_token.text());
          });
      CHECK(token_iter != remaining_tokens_range.end());
      remaining_tokens_range.set_begin(token_iter);

      if (prev_cell_tokens != nullptr) prev_cell_tokens->set_end(token_iter);

      AlignmentRow &row_cell = verible::DescendPath(
          *row, column_loc_iter->second.begin(), column_loc_iter->second.end());
      row_cell.Value().tokens = remaining_tokens_range;
      prev_cell_tokens = &row_cell.Value().tokens;
    }
  }
}

// Recursively calculates widths of each cell's subcells and, if needed, updates
// cell's width to fit all subcells.
static void UpdateAndPropagateRowCellWidths(AlignmentRow *node) {
  node->Value().UpdateWidths();

  if (is_leaf(*node)) return;

  int total_width = 0;
  for (auto &child : node->Children()) {
    UpdateAndPropagateRowCellWidths(&child);
    total_width += child.Value().TotalWidth();
  }

  if (node->Value().tokens.empty()) {
    node->Value().left_border_width =
        node->Children().front().Value().left_border_width;
    node->Value().compact_width = total_width - node->Value().left_border_width;
  }
}

static void ComputeRowCellWidths(AlignmentRow *row) {
  VLOG(2) << __FUNCTION__;
  UpdateAndPropagateRowCellWidths(row);

  // Force leftmost table border to be 0 because these cells start new lines
  // and thus should not factor into alignment calculation.
  // Note: this is different from how StateNode calculates column positions.
  auto *front = row;
  while (!front->Children().empty()) {
    front = &front->Children().front();
    front->Value().left_border_width = 0;
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

using AlignedFormattingColumnSchema = VectorTree<AlignedColumnConfiguration>;

static AlignedFormattingColumnSchema ComputeColumnWidths(
    const AlignmentMatrix &matrix,
    const VectorTree<AlignmentColumnProperties> &column_properties) {
  VLOG(2) << __FUNCTION__;

  AlignedFormattingColumnSchema column_configs =
      Transform<AlignedFormattingColumnSchema>(
          matrix.front(),
          [](const AlignmentRow &) { return AlignedColumnConfiguration{}; });

  // Check which cell before delimiter is the longest
  // If this cell is in the last row, the sizes of column with delimiter
  // must be set to 0
  int longest_cell_before_delimiter = 0;
  bool align_to_last_row = false;
  for (const AlignmentRow &row : matrix) {
    auto column_prop_iter =
        VectorTreePreOrderTraversal(column_properties).begin();
    const auto column_prop_end =
        VectorTreePreOrderTraversal(column_properties).end();
    for (const auto &node : VectorTreePreOrderTraversal(row)) {
      const auto next_prop = std::next(column_prop_iter, 1);
      if (next_prop != column_prop_end &&
          next_prop->Value().contains_delimiter) {
        if (longest_cell_before_delimiter < node.Value().TotalWidth()) {
          longest_cell_before_delimiter = node.Value().TotalWidth();
          if (&row == &matrix.back()) align_to_last_row = true;
        }
        break;
      }
      ++column_prop_iter;
    }
  }

  for (const AlignmentRow &row : matrix) {
    auto column_iter = VectorTreePreOrderTraversal(column_configs).begin();
    auto column_prop_iter =
        VectorTreePreOrderTraversal(column_properties).begin();

    for (const auto &node : VectorTreePreOrderTraversal(row)) {
      if (column_prop_iter->Value().contains_delimiter && align_to_last_row) {
        column_iter->Value().width = 0;
        column_iter->Value().left_border = 0;
      } else {
        column_iter->Value().UpdateFromCell(node.Value());
        if (column_prop_iter->Value().left_border_override !=
            verible::AlignmentColumnProperties::kNoBorderOverride) {
          column_iter->Value().left_border =
              column_prop_iter->Value().left_border_override;
        }
      }
      ++column_iter;
      ++column_prop_iter;
    }
  }

  // Make sure columns are wide enough to fit all their subcolumns
  for (auto &column_iter : VectorTreePostOrderTraversal(column_configs)) {
    if (!is_leaf(column_iter)) {
      int children_width = std::accumulate(
          column_iter.Children().begin(), column_iter.Children().end(), 0,
          [](int width, const AlignedFormattingColumnSchema &node) {
            return width + node.Value().TotalWidth();
          });
      column_iter.Value().left_border =
          std::max(column_iter.Value().left_border,
                   column_iter.Children().front().Value().left_border);
      column_iter.Value().width =
          std::max(column_iter.Value().width,
                   children_width - column_iter.Value().left_border);
    }
  }

  VLOG(2) << "end of " << __FUNCTION__;
  return column_configs;
}

// Saved spacing mutation so that it can be examined before applying.
// There is one of these for every format token that immediately follows an
// alignment column boundary.
struct DeferredTokenAlignment {
  // Points to the token to be modified.
  FormatTokenRange::iterator ftoken;
  // This is the spacing that would produce aligned formatting.
  int new_before_spacing;

  DeferredTokenAlignment(FormatTokenRange::iterator t, int spaces)
      : ftoken(t), new_before_spacing(spaces) {}

  // This value reflects an edit-distance (number of spaces) between aligned and
  // flushed-left formatting.
  int AlignVsFlushLeftSpacingDifference() const {
    return new_before_spacing - ftoken->before.spaces_required;
  }
};

static void ComputeAlignedRowCellSpacings(
    const VectorTree<verible::AlignedColumnConfiguration> &column_configs,
    const VectorTree<verible::AlignmentColumnProperties> &properties,
    const AlignmentRow &row, std::vector<DeferredTokenAlignment> *align_actions,
    int *accrued_spaces) {
  ColumnsTreePath node_path;
  verible::Path(row, node_path);
  VLOG(2) << TreePathFormatter(node_path) << " " << __FUNCTION__ << std::endl;

  if (row.Children().empty()) return;

  auto column_config_it = column_configs.Children().begin();
  auto column_properties_it = properties.Children().begin();
  for (const auto &cell : row.Children()) {
    node_path.clear();
    verible::Path(cell, node_path);
    if (cell.Value().IsUnused()) {
      const int total_width = column_config_it->Value().left_border +
                              column_config_it->Value().width;

      VLOG(2) << TreePathFormatter(node_path)
              << " unused cell; width: " << total_width;

      *accrued_spaces += total_width;
    } else if (cell.Value().IsComposite()) {
      // Cummulative subcolumns width might be smaller than their parent
      // column's width.
      const int subcolumns_width = std::accumulate(
          column_config_it->Children().begin(),
          column_config_it->Children().end(), 0,
          [](int width, const VectorTree<AlignedColumnConfiguration> &node) {
            return width + node.Value().TotalWidth();
          });
      const int padding =
          column_config_it->Value().TotalWidth() - subcolumns_width;

      VLOG(2) << TreePathFormatter(node_path) << " composite cell"
              << "; padding: " << padding << "; flush: "
              << (column_properties_it->Value().flush_left ? "left" : "right");

      if (!column_properties_it->Value().flush_left) *accrued_spaces += padding;
      ComputeAlignedRowCellSpacings(*column_config_it, *column_properties_it,
                                    cell, align_actions, accrued_spaces);
      if (column_properties_it->Value().flush_left) *accrued_spaces += padding;
    } else {
      *accrued_spaces += column_config_it->Value().left_border;

      VLOG(2) << TreePathFormatter(node_path) << " token cell"
              << "; starting token: " << cell.Value().tokens.front().Text();

      // Align by setting the left-spacing based on sum of cell widths
      // before this one.
      const int padding =
          column_config_it->Value().width - cell.Value().compact_width;
      FormatTokenRange::iterator ftoken = cell.Value().tokens.begin();
      int left_spacing;
      if (column_properties_it->Value().flush_left) {
        if (column_properties_it->Value().contains_delimiter) {
          left_spacing = 0;
          *accrued_spaces += padding;
        } else {
          left_spacing = *accrued_spaces;
          *accrued_spaces = padding;
        }
      } else {  // flush right
        left_spacing = *accrued_spaces + padding;
        *accrued_spaces = 0;
      }
      align_actions->emplace_back(ftoken, left_spacing);

      VLOG(2) << TreePathFormatter(node_path)
              << " ... left_spacing: " << left_spacing;
    }

    ++column_config_it;
    ++column_properties_it;
  }
}

// Align cells by adjusting pre-token spacing for a single row.
static std::vector<DeferredTokenAlignment> ComputeAlignedRowSpacings(
    const VectorTree<verible::AlignedColumnConfiguration> &column_configs,
    const VectorTree<verible::AlignmentColumnProperties> &properties,
    const AlignmentRow &row) {
  VLOG(2) << __FUNCTION__ << "; row:\n" << row;
  std::vector<DeferredTokenAlignment> align_actions;
  int accrued_spaces = 0;

  ComputeAlignedRowCellSpacings(column_configs, properties, row, &align_actions,
                                &accrued_spaces);

  VLOG(2) << "end of " << __FUNCTION__;
  return align_actions;
}

static const AlignmentRow *RightmostSubcolumnWithTokens(
    const AlignmentRow &node) {
  if (!node.Value().tokens.empty()) return &node;
  for (const auto &child : reversed_view(node.Children())) {
    if (child.Value().TotalWidth() > 0) {
      return RightmostSubcolumnWithTokens(child);
    }
  }
  return nullptr;
}

static FormatTokenRange EpilogRange(const TokenPartitionTree &partition,
                                    const AlignmentRow &last_subcol) {
  // Identify the unaligned epilog tokens of this 'partition', i.e. those not
  // spanned by 'row'.
  auto partition_end = partition.Value().TokensRange().end();
  auto row_end = last_subcol.Value().tokens.end();
  return {row_end, partition_end};
}

// This width calculation accounts for the unaligned tokens in the tail position
// of each aligned row (e.g. unaligned trailing comments).
static bool AlignedRowsFitUnderColumnLimit(
    const std::vector<TokenPartitionIterator> &rows,
    const AlignmentMatrix &matrix, int total_column_width, int column_limit) {
  auto partition_iter = rows.begin();
  for (const auto &row : matrix) {
    const auto *rightmost_subcolumn = RightmostSubcolumnWithTokens(row);
    if (rightmost_subcolumn) {
      // Identify the unaligned epilog text on each partition.
      const FormatTokenRange epilog_range(
          EpilogRange(**partition_iter, *rightmost_subcolumn));
      const int aligned_partition_width =
          total_column_width + EffectiveCellWidth(epilog_range);
      if (aligned_partition_width > column_limit) {
        VLOG(2) << "Total aligned partition width " << aligned_partition_width
                << " exceeds limit " << column_limit
                << ", so not aligning this group.";
        return false;
      }
    }
    ++partition_iter;
  }
  return true;
}

// Holds alignment calculations for an alignable group of token partitions.
struct AlignablePartitionGroup::GroupAlignmentData {
  // Contains alignment calculations.
  AlignmentMatrix matrix;

  // If this is empty, don't do any alignment.
  std::vector<std::vector<DeferredTokenAlignment>> align_actions_2D;

  int MaxAbsoluteAlignVsFlushLeftSpacingDifference() const {
    int result = std::numeric_limits<int>::min();
    for (const auto &align_actions : align_actions_2D) {
      for (const auto &action : align_actions) {
        int abs_diff = std::abs(action.AlignVsFlushLeftSpacingDifference());
        result = std::max(abs_diff, result);
      }
    }
    return result;
  }

  AlignmentPolicy InferUserIntendedAlignmentPolicy(
      const TokenPartitionRange &partitions) const;
};

AlignablePartitionGroup::GroupAlignmentData
AlignablePartitionGroup::CalculateAlignmentSpacings(
    const std::vector<TokenPartitionIterator> &rows,
    const AlignmentCellScannerFunction &cell_scanner_gen, int column_limit) {
  VLOG(1) << __FUNCTION__;
  GroupAlignmentData result;
  // Alignment requires 2+ rows.
  if (rows.size() <= 1) return result;

  // Rows validation:
  // In many (but not all) cases, all rows' nodes have the same type.
  // TODO(fangism): plumb through an optional verification function.

  VLOG(2) << "Walking syntax subtrees for each row";
  ColumnSchemaAggregator column_schema;
  std::vector<AlignmentRowData> alignment_row_data;
  alignment_row_data.reserve(rows.size());
  // Simultaneously step through each node's tree, adding a column to the
  // schema if *any* row wants it.  This captures optional and repeated
  // constructs.
  for (const auto &row : rows) {
    // Each row should correspond to an individual list element
    const UnwrappedLine &unwrapped_line = row->Value();

    // Scan each token-range for cell boundaries based on syntax,
    // and establish partial ordering based on syntax tree paths.
    auto sparse_columns = cell_scanner_gen(*row);
    // Make sure columns are properly ordered.
    std::sort(
        sparse_columns.Children().begin(), sparse_columns.Children().end(),
        [](const auto &a, const auto &b) {
          return CompareSyntaxTreePath(a.Value().path, b.Value().path) < 0;
        });
    const AlignmentRowData row_data{
        // Extract the range of format tokens whose spacings should be adjusted.
        unwrapped_line.TokensRange(), std::move(sparse_columns)};

    alignment_row_data.emplace_back(row_data);
    VLOG(2) << "Row sparse columns:\n" << row_data.sparse_columns;

    // Aggregate union of all column keys (syntax tree paths).
    column_schema.Collect(row_data.sparse_columns);
  }
  VLOG(2) << "Generating column schema from collected row data";
  column_schema.Finalize();
  VLOG(2) << "Column schema:\n" << column_schema.Columns();

  // Populate a matrix of cells, where cells span token ranges.
  // Null cells (due to optional constructs) are represented by empty ranges,
  // effectively width 0.
  VLOG(2) << "Filling dense matrix from sparse representation";
  result.matrix.resize(rows.size());
  {
    auto row_data_iter = alignment_row_data.cbegin();
    for (auto &row : result.matrix) {
      VLOG(3) << "Row tokens: "
              << StringSpanOfTokenRange(
                     FormatTokenRange(row_data_iter->ftoken_range.begin(),
                                      row_data_iter->ftoken_range.end()));

      row = Transform<AlignmentRow>(
          column_schema.Columns(), [](const VectorTree<AggregateColumnData> &) {
            return AlignmentCell{};
          });

      FillAlignmentRow(*row_data_iter, column_schema.SyntaxToColumnsMap(),
                       &row);
      ComputeRowCellWidths(&row);
      VLOG(2) << "Filled row:\n" << row;

      ++row_data_iter;
    }
  }

  // Extract other non-computed column properties.
  const auto column_properties = column_schema.ColumnProperties();

  // Compute max widths per column.
  VectorTree<AlignedColumnConfiguration> column_configs(
      ComputeColumnWidths(result.matrix, column_properties));

  VLOG(2) << "Column widths:\n" << column_configs;

  {
    // Total width does not include initial left-indentation.
    // Assume indentation is the same for all partitions in each group.
    const int indentation = rows.front()->Value().IndentationSpaces();
    const int total_column_width =
        indentation + column_configs.Value().TotalWidth();
    VLOG(2) << "Total (aligned) column width = " << total_column_width;
    // if the aligned columns would exceed the column limit, then refuse to
    // align for now.  However, this check alone does not include text that
    // follows the last aligned column, like trailing comments and EOL comments.
    if (total_column_width > column_limit) {
      VLOG(2) << "Total aligned column width " << total_column_width
              << " exceeds limit " << column_limit
              << ", so not aligning this group.";
      return result;
    }
    // Also check for length of unaligned trailing tokens.
    if (!AlignedRowsFitUnderColumnLimit(rows, result.matrix, total_column_width,
                                        column_limit)) {
      return result;
    }
  }

  // TODO(fangism): implement overflow mitigation fallback strategies.

  // At this point, the proposed alignment/padding 'fits'.

  // Compute pre-token spacings of each row to align to the column configs.
  // Store the mutation set in a 2D structure that reflects the original token
  // partitions and alignment matrix representation.
  result.align_actions_2D.reserve(result.matrix.size());

  for (const auto &row : result.matrix) {
    result.align_actions_2D.push_back(
        ComputeAlignedRowSpacings(column_configs, column_properties, row));
  }
  return result;
}

// This applies pre-calculated alignment spacings to aligned groups of format
// tokens.
void AlignablePartitionGroup::ApplyAlignment(
    const GroupAlignmentData &align_data) const {
  auto row = alignable_rows_.begin();
  for (const auto &align_actions : align_data.align_actions_2D) {
    (*row)->Children().clear();
    VLOG(3) << __FUNCTION__ << " processing row: " << **row;
    if (!align_actions.empty()) {
      auto &node = **row;
      auto &line = node.Value();
      auto ftokens = line.TokensRange();

      line.SetPartitionPolicy(PartitionPolicyEnum::kAlreadyFormatted);

      verible::TokenPartitionTree *current_cell = nullptr;
      if (align_actions.front().ftoken != ftokens.begin()) {
        node.Children().emplace_back(
            UnwrappedLine(0, ftokens.begin(), PartitionPolicyEnum::kInline));
        current_cell = &node.Children().back();
      }

      for (const auto &action : align_actions) {
        if (current_cell) {
          current_cell->Value().SpanUpToToken(action.ftoken);
          VLOG(3) << "new cell: margin="
                  << current_cell->Value().IndentationSpaces() << ", tokens=[ "
                  << StringSpanOfTokenRange(current_cell->Value().TokensRange())
                  << " ]";
        }
        node.Children().emplace_back(
            UnwrappedLine(action.new_before_spacing, action.ftoken,
                          PartitionPolicyEnum::kInline));
        current_cell = &node.Children().back();
      }
      if (current_cell) {
        current_cell->Value().SpanUpToToken(ftokens.end());
        VLOG(3) << "new cell: margin="
                << current_cell->Value().IndentationSpaces() << ", tokens=[ "
                << StringSpanOfTokenRange(current_cell->Value().TokensRange())
                << " ]";
      }
    }
    ++row;
  }
}

std::vector<TokenPartitionIterator> FilterAlignablePartitions(
    const TokenPartitionRange &range,
    const IgnoreAlignmentRowPredicate &ignore_partition_predicate) {
  // This partition range may contain partitions that should not be
  // considered for column alignment purposes, so filter those out.
  std::vector<TokenPartitionIterator> qualified_partitions;
  qualified_partitions.reserve(range.size());
  // like std::copy_if, but we want the iterators, not their pointees.
  for (auto iter = range.begin(); iter != range.end(); ++iter) {
    if (!ignore_partition_predicate(*iter)) {
      VLOG(2) << "including partition: " << *iter;
      qualified_partitions.push_back(iter);
    } else {
      VLOG(2) << "excluding partition: " << *iter;
    }
  }
  return qualified_partitions;
}

ExtractAlignmentGroupsFunction ExtractAlignmentGroupsAdapter(
    const std::function<std::vector<TaggedTokenPartitionRange>(
        const TokenPartitionRange &)> &legacy_extractor,
    const IgnoreAlignmentRowPredicate &legacy_ignore_predicate,
    const AlignmentCellScannerFunction &alignment_cell_scanner,
    AlignmentPolicy alignment_policy) {
  return [legacy_extractor, legacy_ignore_predicate, alignment_cell_scanner,
          alignment_policy](const TokenPartitionRange &full_range) {
    // must copy the closures, not just reference, to ensure valid lifetime
    const std::vector<TaggedTokenPartitionRange> ranges(
        legacy_extractor(full_range));
    std::vector<AlignablePartitionGroup> groups;
    groups.reserve(ranges.size());
    for (const auto &range : ranges) {
      // Apply the same alignment scanner and policy to all alignment groups.
      // This ignores range.match_subtype.
      groups.emplace_back(
          FilterAlignablePartitions(range.range, legacy_ignore_predicate),
          alignment_cell_scanner, alignment_policy);
      if (groups.back().IsEmpty()) groups.pop_back();
    }
    return groups;
  };
}

static int MaxOfPositives2D(const std::vector<std::vector<int>> &values) {
  int result = 0;
  for (const auto &row : values) {
    for (const int delta : row) {
      // Only accumulate positive values.
      result = std::max(delta, result);
    }
  }
  return result;
}

// Educated guess whether user wants alignment.
AlignmentPolicy
AlignablePartitionGroup::GroupAlignmentData::InferUserIntendedAlignmentPolicy(
    const TokenPartitionRange &partitions) const {
  // Heuristics are implemented as a sequence of priority-ordered rules.

  {
    // If the visual distance between aligned and flushed left is sufficiently
    // small (and thus less likely to compromise readability), just align the
    // code region.  The lower this threshold value, the more conservative the
    // aligner will be about forcing alignment over blocks of code.
    constexpr int kForceAlignMaxThreshold = 2;  // TODO(fangism): configurable
    const int align_flush_diff = MaxAbsoluteAlignVsFlushLeftSpacingDifference();
    VLOG(2) << "align vs. flush diff = " << align_flush_diff;
    VLOG(2) << "  vs. " << kForceAlignMaxThreshold << " (max threshold)";
    if (align_flush_diff <= kForceAlignMaxThreshold) {
      VLOG(2) << "  <= threshold, so force-align.";
      return AlignmentPolicy::kAlign;
    }
  }

  // Compute spacing distances between the original and flush-left spacing.
  // This can be interpreted as "errors relative to flush-left spacing".
  const std::vector<std::vector<int>> flush_left_spacing_deltas(
      FlushLeftSpacingDifferences(partitions));
  const int max_excess_spaces = MaxOfPositives2D(flush_left_spacing_deltas);
  VLOG(2) << "max excess spaces = " << max_excess_spaces;

  {
    // If the worst spacing error relative to the original code is <= than
    // this threshold, then infer that the user intended code to be flush-left.
    constexpr int kFlushLeftMaxThreshold = 2;  // TODO(fangism): configurable
    VLOG(2) << "  vs. " << kFlushLeftMaxThreshold << " (max threshold)";
    if (max_excess_spaces <= kFlushLeftMaxThreshold) {
      VLOG(2) << "  <= threshold, so flush-left.";
      return AlignmentPolicy::kFlushLeft;
    }
  }

  {
    // If the user injects more than this number of spaces in excess anywhere in
    // this block of code, then trigger alignment.
    constexpr int kForceAlignMinThreshold = 4;  // TODO(fangism): configurable
    // This must be greater than kFlushLeftMaxThreshold.
    VLOG(2) << "  vs. " << kForceAlignMinThreshold << " (min threshold)";
    if (max_excess_spaces >= kForceAlignMinThreshold) {
      VLOG(2) << "  >= threshold, so align.";
      return AlignmentPolicy::kAlign;
    }
  }

  // When in doubt, preserve.
  return AlignmentPolicy::kPreserve;
}

// TODO(mglb): this eventually could be moved to token_partition_tree.cc.
void FormatUsingOriginalSpacing(TokenPartitionRange partition_range) {
  for (auto &partition : partition_range) {
    VLOG(4) << "partition before:\n"
            << TokenPartitionTreePrinter(partition, true);

    partition.Children().clear();
    auto tokens = partition.Value().TokensRange();
    if (tokens.empty()) {
      partition.Value().SetPartitionPolicy(
          PartitionPolicyEnum::kAlreadyFormatted);
      VLOG(4) << "partition after:\n"
              << TokenPartitionTreePrinter(partition, true);
      continue;
    }

    // Emulate spacing preservation using kAlreadyFormatted and kInline
    // partitions

    const int indentation = partition.Value().IndentationSpaces();

    auto line = UnwrappedLine(indentation, tokens.begin(),
                              PartitionPolicyEnum::kAlreadyFormatted);
    partition.Children().emplace_back(line);

    if (tokens.size() > 1) {
      // First token
      VLOG(5) << "token: \""
              << EscapeString(tokens.front().OriginalLeadingSpaces())
              << EscapeString(tokens.front().Text()) << '\"';
      auto slice =
          UnwrappedLine(0, tokens.begin(), PartitionPolicyEnum::kInline);
      slice.SpanNextToken();
      partition.Children().back().Children().emplace_back(slice);

      // Remaining tokens
      for (auto it = tokens.begin() + 1; it != tokens.end(); ++it) {
        const auto &token = *it;
        const auto whitespace = token.OriginalLeadingSpaces();
        VLOG(5) << "token: \"" << EscapeString(whitespace)
                << EscapeString(token.Text()) << '\"';

        int spacing = whitespace.size();
        std::size_t last_newline_pos = whitespace.find_last_of('\n');
        if (last_newline_pos != std::string_view::npos) {
          // Update end of current line.
          partition.Children().back().Value().SpanUpToToken(it);
          // Start a new line.
          // Newlines count does not matter here. All newlines in leading
          // whitespace of the first token in a line are always preserved.
          // For details, see FormatWhitespaceWithDisabledByteRanges() called
          // from Formatter::Emit().
          //
          // TODO(mglb): consider using correctly adjusted indentation to make
          // all lines indented correctly. Something like:
          // indentation + (this line orig. indent) - (1st line orig. indent)
          const auto line =
              UnwrappedLine(0, it, PartitionPolicyEnum::kAlreadyFormatted);
          partition.Children().emplace_back(line);
          // Count only spaces after the last '\n'.
          spacing -= last_newline_pos + 1;
        }

        auto slice = UnwrappedLine(spacing, it, PartitionPolicyEnum::kInline);
        slice.SpanNextToken();
        partition.Children().back().Children().emplace_back(slice);
      }
    }
    partition.Children().back().Value().SpanUpToToken(tokens.end());

    if (partition.Children().size() == 1) {
      HoistOnlyChild(partition);
    } else {
      partition.Value().SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);
    }

    VLOG(4) << "partition after:\n"
            << TokenPartitionTreePrinter(partition, true);
  }
}

void AlignablePartitionGroup::Align(int column_limit) const {
  // Compute dry-run of alignment spacings if it is needed.
  AlignmentPolicy policy = alignment_policy_;
  VLOG(2) << "AlignmentPolicy: " << policy;
  GroupAlignmentData align_data;
  switch (policy) {
    case AlignmentPolicy::kAlign:
    case AlignmentPolicy::kInferUserIntent:
      align_data = CalculateAlignmentSpacings(
          alignable_rows_, alignment_cell_scanner_, column_limit);
      break;
    default:
      break;
  }

  const TokenPartitionRange partition_range(Range());
  // If enabled, try to decide automatically based on heurstics.
  if (policy == AlignmentPolicy::kInferUserIntent) {
    policy = align_data.InferUserIntendedAlignmentPolicy(partition_range);
    VLOG(2) << "AlignmentPolicy (automatic): " << policy;
  }

  // Align or not, depending on user-elected or inferred policy.
  switch (policy) {
    case AlignmentPolicy::kAlign: {
      if (!align_data.align_actions_2D.empty()) {
        // This modifies format tokens' spacing values.
        ApplyAlignment(align_data);
      }
      break;
    }
    case AlignmentPolicy::kFlushLeft:
      // This is already the default behavior elsewhere.  Nothing else to do.
      break;
    case AlignmentPolicy::kInferUserIntent:
      // InferUserIntendedAlignmentPolicy() above should have set the policy to
      // anything other.
      LOG(ERROR) << "Alignment policy should have been decided at this point. "
                    "Defaulting to kPreserve.";
      [[fallthrough]];
    case AlignmentPolicy::kPreserve:
      FormatUsingOriginalSpacing(partition_range);
      break;
  }
}

void TabularAlignTokens(
    int column_limit, std::string_view full_text,
    const ByteOffsetSet &disabled_byte_ranges,
    const ExtractAlignmentGroupsFunction &extract_alignment_groups,
    TokenPartitionTree *partition_ptr) {
  VLOG(1) << __FUNCTION__;
  // Each subpartition is presumed to correspond to a list element or
  // possibly some other ignored element like comments.

  auto &partition = *partition_ptr;
  auto &subpartitions = partition.Children();
  // Identify groups of partitions to align, separated by blank lines.
  const TokenPartitionRange subpartitions_range(subpartitions.begin(),
                                                subpartitions.end());
  if (subpartitions_range.empty()) return;
  VLOG(2) << "extracting alignment partition groups...";
  const std::vector<AlignablePartitionGroup> alignment_groups(
      extract_alignment_groups(subpartitions_range));
  for (const auto &alignment_group : alignment_groups) {
    const TokenPartitionRange partition_range(alignment_group.Range());
    if (partition_range.empty()) continue;
    if (AnyPartitionSubRangeIsDisabled(partition_range, full_text,
                                       disabled_byte_ranges)) {
      // Within an aligned group, if the group is partially disabled
      // due to incremental formatting, then leave the new lines
      // unformatted rather than falling back to compact-left formatting.
      // However, allow the first token to be correctly indented.
      FormatUsingOriginalSpacing(partition_range);
      continue;

      // TODO(fangism): instead of disabling the whole range, sub-partition
      // it one more level, and operate on those ranges, essentially treating
      // no-format ranges like alignment group boundaries.
      // Requires IntervalSet::Intersect operation.

      // TODO(b/159824483): attempt to detect and re-use pre-existing alignment
    }

    // Calculate alignment and possibly apply it depending on alignment policy.
    alignment_group.Align(column_limit);
  }
  VLOG(1) << "end of " << __FUNCTION__;
}

std::vector<TaggedTokenPartitionRange>
GetSubpartitionsBetweenBlankLinesSingleTag(
    const TokenPartitionRange &full_range, int subtype) {
  std::vector<TokenPartitionRange> ranges(
      GetSubpartitionsBetweenBlankLines(full_range));
  std::vector<TaggedTokenPartitionRange> result;
  result.reserve(ranges.size());
  for (const auto &range : ranges) {
    result.emplace_back(range, subtype);
  }
  return result;
}

std::vector<TaggedTokenPartitionRange> GetPartitionAlignmentSubranges(
    const TokenPartitionRange &partitions,
    const std::function<AlignedPartitionClassification(
        const TokenPartitionTree &)> &partition_selector,
    int min_match_count) {
  std::vector<TaggedTokenPartitionRange> result;

  // Grab ranges of consecutive data declarations with >= 2 elements.
  int last_match_subtype = 0;
  int match_count = 0;
  auto last_range_start = partitions.begin();
  for (auto iter = last_range_start; iter != partitions.end(); ++iter) {
    const AlignedPartitionClassification align_class =
        partition_selector(*iter);
    switch (align_class.action) {
      case AlignmentGroupAction::kIgnore:
        continue;
      case AlignmentGroupAction::kMatch: {
        if (match_count == 0) {
          // This is the start of a new range of interest.
          last_range_start = iter;
          last_match_subtype = align_class.match_subtype;
        }
        if (align_class.match_subtype != last_match_subtype) {
          // Mismatch in substype, so close the last range,
          // and open a new one.
          if (match_count >= min_match_count) {
            result.emplace_back(last_range_start, iter, last_match_subtype);
          }
          match_count = 0;
          last_range_start = iter;
          last_match_subtype = align_class.match_subtype;
        }
        ++match_count;
        break;
      }
      case AlignmentGroupAction::kNoMatch: {
        if (match_count >= min_match_count) {
          result.emplace_back(last_range_start, iter, last_match_subtype);
        }
        match_count = 0;  // reset
        break;
      }
    }  // switch
  }    // for
  // Flush out the last range.
  if (match_count >= min_match_count) {
    result.emplace_back(last_range_start, partitions.end(), last_match_subtype);
  }
  return result;
}

}  // namespace verible
