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

#include "common/formatting/align.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <vector>

#include "absl/strings/str_join.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/display_utils.h"
#include "common/strings/range.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/text/tree_utils.h"
#include "common/util/algorithm.h"
#include "common/util/container_iterator_range.h"
#include "common/util/enum_flags.h"
#include "common/util/logging.h"

namespace verible {

static const verible::EnumNameMap<AlignmentPolicy> kAlignmentPolicyNameMap = {
    {"align", AlignmentPolicy::kAlign},
    {"flush-left", AlignmentPolicy::kFlushLeft},
    {"preserve", AlignmentPolicy::kPreserve},
    {"infer", AlignmentPolicy::kInferUserIntent},
    // etc.
};

std::ostream& operator<<(std::ostream& stream, AlignmentPolicy policy) {
  return kAlignmentPolicyNameMap.Unparse(policy, stream);
}

bool AbslParseFlag(absl::string_view text, AlignmentPolicy* policy,
                   std::string* error) {
  return kAlignmentPolicyNameMap.Parse(text, policy, error, "AlignmentPolicy");
}

std::string AbslUnparseFlag(const AlignmentPolicy& policy) {
  std::ostringstream stream;
  stream << policy;
  return stream.str();
}

static int EffectiveCellWidth(const FormatTokenRange& tokens) {
  if (tokens.empty()) return 0;
  VLOG(2) << __FUNCTION__;
  // Sum token text lengths plus required pre-spacings (except first token).
  // Note: LeadingSpacesLength() honors where original spacing when preserved.
  return std::accumulate(
      tokens.begin(), tokens.end(), -tokens.front().LeadingSpacesLength(),
      [](int total_width, const PreFormatToken& ftoken) {
        const int pre_width = ftoken.LeadingSpacesLength();
        const int text_length = ftoken.token->text().length();
        VLOG(2) << " +" << pre_width << " +" << text_length;
        // TODO(fangism): account for multi-line tokens like
        // block comments.
        return total_width + ftoken.LeadingSpacesLength() + text_length;
      });
}

static int EffectiveLeftBorderWidth(const MutableFormatTokenRange& tokens) {
  if (tokens.empty()) return 0;
  return tokens.front().before.spaces_required;
}

struct AlignmentCell {
  // Slice of format tokens in this cell (may be empty range).
  MutableFormatTokenRange tokens;
  // The width of this token excerpt that complies with minimum spacing.
  int compact_width = 0;
  // Width of the left-side spacing before this cell, which can be considered
  // as a space-only column, usually no more than 1 space wide.
  int left_border_width = 0;

  FormatTokenRange ConstTokensRange() const {
    return FormatTokenRange(tokens.begin(), tokens.end());
  }

  void UpdateWidths() {
    compact_width = EffectiveCellWidth(ConstTokensRange());
    left_border_width = EffectiveLeftBorderWidth(tokens);
  }
};

std::ostream& operator<<(std::ostream& stream, const AlignmentCell& cell) {
  if (!cell.tokens.empty()) {
    // See UnwrappedLine::AsCode for similar printing.
    stream << absl::StrJoin(cell.tokens, " ",
                            [](std::string* out, const PreFormatToken& token) {
                              absl::StrAppend(out, token.Text());
                            });
  }
  return stream;
}

// These properties are calculated/aggregated from alignment cells.
struct AlignedColumnConfiguration {
  int width = 0;
  int left_border = 0;

  int TotalWidth() const { return left_border + width; }

  void UpdateFromCell(const AlignmentCell& cell) {
    width = std::max(width, cell.compact_width);
    left_border = std::max(left_border, cell.left_border_width);
  }
};

typedef std::vector<AlignmentCell> AlignmentRow;
typedef std::vector<AlignmentRow> AlignmentMatrix;

void ColumnSchemaScanner::ReserveNewColumn(
    const Symbol& symbol, const AlignmentColumnProperties& properties,
    const SyntaxTreePath& path) {
  // The path helps establish a total ordering among all desired alignment
  // points, given that they may come from optional or repeated language
  // constructs.
  const SyntaxTreeLeaf* leaf = GetLeftmostLeaf(symbol);
  // It is possible for a node to be empty, in which case, ignore.
  if (leaf == nullptr) return;
  if (sparse_columns_.empty() || sparse_columns_.back().path != path) {
    // It's possible the previous cell's path was intentionally altered
    // to effectively fuse it with the cell that is about to be added.
    // When this occurs, take the (previous) leftmost token, and suppress
    // adding a new column.
    sparse_columns_.push_back(
        ColumnPositionEntry{path, leaf->get(), properties});
    VLOG(2) << "reserving new column at " << TreePathFormatter(path);
  }
}

struct AggregateColumnData {
  // This is taken as the first seen set of properties in any given column.
  AlignmentColumnProperties properties;
  // These tokens's positions will be used to identify alignment cell
  // boundaries.
  std::vector<TokenInfo> starting_tokens;

  void Import(const ColumnPositionEntry& cell) {
    if (starting_tokens.empty()) {
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
  void Collect(const std::vector<ColumnPositionEntry>& row) {
    for (const auto& cell : row) {
      cell_map_[cell.path].Import(cell);
    }
  }

  size_t NumUniqueColumns() const { return cell_map_.size(); }

  // Establishes 1:1 between SyntaxTreePath and column index.
  // Call this after collecting all columns.
  void FinalizeColumnIndices() {
    column_positions_.reserve(cell_map_.size());
    for (const auto& kv : cell_map_) {
      column_positions_.push_back(kv.first);
    }
  }

  const std::vector<SyntaxTreePath>& ColumnPositions() const {
    return column_positions_;
  }

  std::vector<AlignmentColumnProperties> ColumnProperties() const {
    std::vector<AlignmentColumnProperties> properties;
    properties.reserve(cell_map_.size());
    for (const auto& entry : cell_map_) {
      properties.push_back(entry.second.properties);
    }
    return properties;
  }

 private:
  // Keeps track of unique positions where new columns are desired.
  // The keys form the set of columns wanted across all rows.
  // The values are sets of starting tokens, from which token ranges
  // will be computed per cell.
  std::map<SyntaxTreePath, AggregateColumnData> cell_map_;

  // 1:1 map between SyntaxTreePath and column index.
  // Values are monotonically increasing, so this is binary_search-able.
  std::vector<SyntaxTreePath> column_positions_;
};

static SequenceStreamFormatter<AlignmentRow> MatrixRowFormatter(
    const AlignmentRow& row) {
  return SequenceFormatter(row, " | ", "< ", " >");
}

struct AlignmentRowData {
  // Range of format tokens whose space is to be adjusted for alignment.
  MutableFormatTokenRange ftoken_range;

  // Set of cells found that correspond to an ordered, sparse set of columns
  // to be aligned with other rows.
  std::vector<ColumnPositionEntry> sparse_columns;
};

// Translate a sparse set of columns into a fully-populated matrix row.
static void FillAlignmentRow(
    const AlignmentRowData& row_data,
    const std::vector<SyntaxTreePath>& column_positions, AlignmentRow* row) {
  VLOG(2) << __FUNCTION__;
  const auto& sparse_columns(row_data.sparse_columns);
  MutableFormatTokenRange partition_token_range(row_data.ftoken_range);
  // Translate token into preformat_token iterator,
  // full token range.
  const auto cbegin = column_positions.begin();
  const auto cend = column_positions.end();
  auto pos_iter = cbegin;
  auto token_iter = partition_token_range.begin();
  const auto token_end = partition_token_range.end();
  int last_column_index = 0;
  // Find each non-empty cell, and fill in other cells with empty ranges.
  for (const auto& col : sparse_columns) {
    pos_iter = std::find(pos_iter, cend, col.path);
    // By construction, sparse_columns' paths should be a subset of those
    // in the aggregated column_positions set.
    CHECK(pos_iter != cend);
    const int column_index = std::distance(cbegin, pos_iter);
    VLOG(3) << "cell at column " << column_index;

    // Find the format token iterator that corresponds to the column start.
    // Linear time total over all outer loop iterations.
    token_iter =
        std::find_if(token_iter, token_end, [=](const PreFormatToken& ftoken) {
          return BoundsEqual(ftoken.Text(), col.starting_token.text());
        });
    CHECK(token_iter != token_end);

    // Fill null-range cells between [last_column_index, column_index).
    const MutableFormatTokenRange empty_filler(token_iter, token_iter);
    for (; last_column_index <= column_index; ++last_column_index) {
      VLOG(3) << "empty at column " << last_column_index;
      (*row)[last_column_index].tokens = empty_filler;
    }
    // At this point, the current cell has only seen its lower bound.
    // The upper bound will be set in a separate pass.
  }
  // Fill any sparse cells up to the last column.
  VLOG(3) << "fill up to last column";
  const MutableFormatTokenRange empty_filler(token_end, token_end);
  for (const int n = column_positions.size(); last_column_index < n;
       ++last_column_index) {
    VLOG(3) << "empty at column " << last_column_index;
    (*row)[last_column_index].tokens = empty_filler;
  }

  // In this pass, set the upper bounds of cells' token ranges.
  auto upper_bound = token_end;
  for (auto& cell : verible::reversed_view(*row)) {
    cell.tokens.set_end(upper_bound);
    upper_bound = cell.tokens.begin();
  }
  VLOG(2) << "end of " << __FUNCTION__ << ", row: " << MatrixRowFormatter(*row);
}

struct MatrixCellSizeFormatter {
  const AlignmentMatrix& matrix;
};

std::ostream& operator<<(std::ostream& stream,
                         const MatrixCellSizeFormatter& p) {
  const AlignmentMatrix& matrix = p.matrix;
  for (const auto& row : matrix) {
    stream << '['
           << absl::StrJoin(row, ", ",
                            [](std::string* out, const AlignmentCell& cell) {
                              absl::StrAppend(out, cell.left_border_width, "+",
                                              cell.compact_width);
                            })
           << ']' << std::endl;
  }
  return stream;
}

static void ComputeCellWidths(AlignmentMatrix* matrix) {
  VLOG(2) << __FUNCTION__;
  for (auto& row : *matrix) {
    for (auto& cell : row) {
      cell.UpdateWidths();
    }
    // Force leftmost table border to be 0 because these cells start new lines
    // and thus should not factor into alignment calculation.
    // Note: this is different from how StateNode calculates column positions.
    row.front().left_border_width = 0;
  }
  VLOG(2) << "end of " << __FUNCTION__ << ", cell sizes:\n"
          << MatrixCellSizeFormatter{*matrix};
}

typedef std::vector<AlignedColumnConfiguration> AlignedFormattingColumnSchema;

static AlignedFormattingColumnSchema ComputeColumnWidths(
    const AlignmentMatrix& matrix) {
  VLOG(2) << __FUNCTION__;
  AlignedFormattingColumnSchema column_configs(matrix.front().size());
  for (const auto& row : matrix) {
    auto column_iter = column_configs.begin();
    for (const auto& cell : row) {
      column_iter->UpdateFromCell(cell);
      ++column_iter;
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
  PreFormatToken* ftoken;
  // This is the spacing that would produce aligned formatting.
  int new_before_spacing;

  DeferredTokenAlignment(PreFormatToken* t, int spaces)
      : ftoken(t), new_before_spacing(spaces) {}

  // This value reflects an edit-distance (number of spaces) between aligned and
  // flushed-left formatting.
  int AlignVsFlushLeftSpacingDifference() const {
    return new_before_spacing - ftoken->before.spaces_required;
  }

  void Apply() const {
    // force printing of spaces
    ftoken->before.break_decision = SpacingOptions::AppendAligned;
    ftoken->before.spaces_required = new_before_spacing;
  }
};

// Align cells by adjusting pre-token spacing for a single row.
static std::vector<DeferredTokenAlignment> ComputeAlignedRowSpacings(
    const AlignedFormattingColumnSchema& column_configs,
    const std::vector<AlignmentColumnProperties>& properties,
    const AlignmentRow& row) {
  VLOG(2) << __FUNCTION__;
  std::vector<DeferredTokenAlignment> align_actions;
  int accrued_spaces = 0;
  auto column_iter = column_configs.begin();
  auto properties_iter = properties.begin();
  for (const auto& cell : row) {
    accrued_spaces += column_iter->left_border;
    if (cell.tokens.empty()) {
      // Accumulate spacing for the next sparse cell in this row.
      accrued_spaces += column_iter->width;
    } else {
      VLOG(2) << "at: " << cell.tokens.front().Text();
      // Align by setting the left-spacing based on sum of cell widths
      // before this one.
      const int padding = column_iter->width - cell.compact_width;
      PreFormatToken& ftoken = cell.tokens.front();
      int left_spacing;
      if (properties_iter->flush_left) {
        left_spacing = accrued_spaces;
        accrued_spaces = padding;
      } else {  // flush right
        left_spacing = accrued_spaces + padding;
        accrued_spaces = 0;
      }
      align_actions.emplace_back(&ftoken, left_spacing);
      VLOG(2) << "left_spacing = " << left_spacing;
    }
    VLOG(2) << "accrued_spaces = " << accrued_spaces;
    ++column_iter;
    ++properties_iter;
  }
  VLOG(2) << "end of " << __FUNCTION__;
  return align_actions;
}

// Given a const_iterator and the original mutable container, return
// the corresponding mutable iterator (without resorting to const_cast).
// The 'Container' type is not deducible from function arguments alone.
// TODO(fangism): provide this from common/util/iterator_adaptors.
template <class Container>
typename Container::iterator ConvertToMutableIterator(
    typename Container::const_iterator const_iter,
    typename Container::iterator base) {
  const typename Container::const_iterator cbase(base);
  return base + std::distance(cbase, const_iter);
}

static MutableFormatTokenRange ConvertToMutableFormatTokenRange(
    const FormatTokenRange& const_range,
    MutableFormatTokenRange::iterator base) {
  using array_type = std::vector<PreFormatToken>;
  return MutableFormatTokenRange(
      ConvertToMutableIterator<array_type>(const_range.begin(), base),
      ConvertToMutableIterator<array_type>(const_range.end(), base));
}

static FormatTokenRange EpilogRange(const TokenPartitionTree& partition,
                                    const AlignmentRow& row) {
  // Identify the unaligned epilog tokens of this 'partition', i.e. those not
  // spanned by 'row'.
  auto partition_end = partition.Value().TokensRange().end();
  auto row_end = row.back().tokens.end();
  return FormatTokenRange(row_end, partition_end);
}

// Mark format tokens as must-append to remove future decision-making.
static void CommitAlignmentDecisionToRow(
    TokenPartitionTree& partition, const AlignmentRow& row,
    MutableFormatTokenRange::iterator ftoken_base) {
  if (!row.empty()) {
    const auto ftoken_range = ConvertToMutableFormatTokenRange(
        partition.Value().TokensRange(), ftoken_base);
    for (auto& ftoken : ftoken_range) {
      SpacingOptions& decision = ftoken.before.break_decision;
      if (decision == SpacingOptions::Undecided) {
        decision = SpacingOptions::MustAppend;
      }
    }
    // Tag every subtree as having already been committed to alignment.
    partition.ApplyPostOrder([](TokenPartitionTree& node) {
      node.Value().SetPartitionPolicy(
          PartitionPolicyEnum::kSuccessfullyAligned);
    });
  }
}

static MutableFormatTokenRange GetMutableFormatTokenRange(
    const UnwrappedLine& unwrapped_line,
    MutableFormatTokenRange::iterator ftoken_base) {
  // Each row should correspond to an individual list element
  const Symbol* origin = ABSL_DIE_IF_NULL(unwrapped_line.Origin());
  VLOG(2) << "row: " << StringSpanOfSymbol(*origin);

  // Partition may contain text that is outside of the span of the syntax
  // tree node that was visited, e.g. a trailing comma delimiter.
  // Exclude those tokens from alignment consideration (for now).
  const SyntaxTreeLeaf* last_token = GetRightmostLeaf(*origin);
  const auto range_begin = unwrapped_line.TokensRange().begin();
  auto range_end = unwrapped_line.TokensRange().end();
  // Backwards search is expected to check at most a few tokens.
  while (!BoundsEqual(std::prev(range_end)->Text(), last_token->get().text()))
    --range_end;
  CHECK(range_begin <= range_end);

  // Scan each token-range for cell boundaries based on syntax,
  // and establish partial ordering based on syntax tree paths.
  return ConvertToMutableFormatTokenRange(
      FormatTokenRange(range_begin, range_end), ftoken_base);
}

// This width calculation accounts for the unaligned tokens in the tail position
// of each aligned row (e.g. unaligned trailing comments).
static bool AlignedRowsFitUnderColumnLimit(
    const std::vector<TokenPartitionIterator>& rows,
    const AlignmentMatrix& matrix, int total_column_width, int column_limit) {
  auto partition_iter = rows.begin();
  for (const auto& row : matrix) {
    if (!row.empty()) {
      // Identify the unaligned epilog text on each partition.
      const FormatTokenRange epilog_range(EpilogRange(**partition_iter, row));
      const int aligned_partition_width =
          total_column_width + EffectiveCellWidth(epilog_range);
      if (aligned_partition_width > column_limit) {
        VLOG(1) << "Total aligned partition width " << aligned_partition_width
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
    for (const auto& align_actions : align_actions_2D) {
      for (const auto& action : align_actions) {
        int abs_diff = std::abs(action.AlignVsFlushLeftSpacingDifference());
        result = std::max(abs_diff, result);
      }
    }
    return result;
  }

  AlignmentPolicy InferUserIntendedAlignmentPolicy(
      const TokenPartitionRange& partitions) const;
};

AlignablePartitionGroup::GroupAlignmentData
AlignablePartitionGroup::CalculateAlignmentSpacings(
    const std::vector<TokenPartitionIterator>& rows,
    const AlignmentCellScannerFunction& cell_scanner_gen,
    MutableFormatTokenRange::iterator ftoken_base, int column_limit) {
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
  for (const auto& row : rows) {
    // Each row should correspond to an individual list element
    const UnwrappedLine& unwrapped_line = row->Value();

    const AlignmentRowData row_data{
        // Extract the range of format tokens whose spacings should be adjusted.
        GetMutableFormatTokenRange(unwrapped_line, ftoken_base),
        // Scan each token-range for cell boundaries based on syntax,
        // and establish partial ordering based on syntax tree paths.
        cell_scanner_gen(*row)};

    alignment_row_data.emplace_back(row_data);
    // Aggregate union of all column keys (syntax tree paths).
    column_schema.Collect(row_data.sparse_columns);
  }

  // Map SyntaxTreePaths to column indices.
  VLOG(2) << "Mapping column indices";
  column_schema.FinalizeColumnIndices();
  const auto& column_positions = column_schema.ColumnPositions();
  const size_t num_columns = column_schema.NumUniqueColumns();
  VLOG(2) << "unique columns: " << num_columns;

  // Populate a matrix of cells, where cells span token ranges.
  // Null cells (due to optional constructs) are represented by empty ranges,
  // effectively width 0.
  VLOG(2) << "Filling dense matrix from sparse representation";
  result.matrix.resize(rows.size());
  {
    auto row_data_iter = alignment_row_data.cbegin();
    for (auto& row : result.matrix) {
      row.resize(num_columns);
      FillAlignmentRow(*row_data_iter, column_positions, &row);
      ++row_data_iter;
    }
  }

  // Compute compact sizes per cell.
  ComputeCellWidths(&result.matrix);

  // Compute max widths per column.
  AlignedFormattingColumnSchema column_configs(
      ComputeColumnWidths(result.matrix));

  // Extract other non-computed column properties.
  const auto column_properties = column_schema.ColumnProperties();

  {
    // Total width does not include initial left-indentation.
    // Assume indentation is the same for all partitions in each group.
    const int indentation = rows.front().base()->Value().IndentationSpaces();
    const int total_column_width = std::accumulate(
        column_configs.begin(), column_configs.end(), indentation,
        [](int total_width, const AlignedColumnConfiguration& c) {
          return total_width + c.TotalWidth();
        });
    VLOG(2) << "Total (aligned) column width = " << total_column_width;
    // if the aligned columns would exceed the column limit, then refuse to
    // align for now.  However, this check alone does not include text that
    // follows the last aligned column, like trailing comments and EOL comments.
    if (total_column_width > column_limit) {
      VLOG(1) << "Total aligned column width " << total_column_width
              << " exceeds limit " << column_limit
              << ", so not aligning this group.";
      return result;
    }
    // Also check for length of unaligned trailing tokens.
    if (!AlignedRowsFitUnderColumnLimit(rows, result.matrix, total_column_width,
                                        column_limit))
      return result;
  }

  // TODO(fangism): implement overflow mitigation fallback strategies.

  // At this point, the proposed alignment/padding 'fits'.

  // Compute pre-token spacings of each row to align to the column configs.
  // Store the mutation set in a 2D structure that reflects the original token
  // partitions and alignment matrix representation.
  result.align_actions_2D.reserve(result.matrix.size());
  for (const auto& row : result.matrix) {
    result.align_actions_2D.push_back(
        ComputeAlignedRowSpacings(column_configs, column_properties, row));
  }
  return result;
}

// This applies pre-calculated alignment spacings to aligned groups of format
// tokens.
void AlignablePartitionGroup::ApplyAlignment(
    const GroupAlignmentData& align_data,
    MutableFormatTokenRange::iterator ftoken_base) const {
  // Apply spacing adjustments (mutates format tokens)
  for (const auto& align_actions : align_data.align_actions_2D) {
    for (const auto& action : align_actions) action.Apply();
  }

  // Signal that these partitions spacing/wrapping decisions have already been
  // solved (append everything because they fit on one line).
  {
    auto partition_iter = alignable_rows_.begin();
    for (auto& row : align_data.matrix) {
      // Commits to appending all tokens in this row (mutates format tokens)
      CommitAlignmentDecisionToRow(**partition_iter, row, ftoken_base);
      ++partition_iter;
    }
  }
  VLOG(1) << "end of " << __FUNCTION__;
}

std::vector<TokenPartitionIterator> FilterAlignablePartitions(
    const TokenPartitionRange& range,
    const IgnoreAlignmentRowPredicate& ignore_partition_predicate) {
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
        const TokenPartitionRange&)>& legacy_extractor,
    const IgnoreAlignmentRowPredicate& legacy_ignore_predicate,
    const AlignmentCellScannerFunction& alignment_cell_scanner,
    AlignmentPolicy alignment_policy) {
  return [legacy_extractor, legacy_ignore_predicate, alignment_cell_scanner,
          alignment_policy](const TokenPartitionRange& full_range) {
    // must copy the closures, not just reference, to ensure valid lifetime
    const std::vector<TaggedTokenPartitionRange> ranges(
        legacy_extractor(full_range));
    std::vector<AlignablePartitionGroup> groups;
    groups.reserve(ranges.size());
    for (const auto& range : ranges) {
      // Apply the same alignment scanner and policy to all alignment groups.
      // This ignores range.match_subtype.
      groups.emplace_back(AlignablePartitionGroup{
          FilterAlignablePartitions(range.range, legacy_ignore_predicate),
          alignment_cell_scanner, alignment_policy});
      if (groups.back().IsEmpty()) groups.pop_back();
    }
    return groups;
  };
}

static int MaxOfPositives2D(const std::vector<std::vector<int>>& values) {
  int result = 0;
  for (const auto& row : values) {
    for (const int delta : row) {
      // Only accumulate positive values.
      if (delta > result) result = delta;
    }
  }
  return result;
}

// Educated guess whether user wants alignment.
AlignmentPolicy
AlignablePartitionGroup::GroupAlignmentData::InferUserIntendedAlignmentPolicy(
    const TokenPartitionRange& partitions) const {
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

void AlignablePartitionGroup::Align(
    absl::string_view full_text, int column_limit,
    std::vector<PreFormatToken>* ftokens) const {
  const TokenPartitionRange partition_range(Range());
  // Compute dry-run of alignment spacings if it is needed.
  AlignmentPolicy policy = alignment_policy_;
  VLOG(2) << "AlignmentPolicy: " << policy;
  GroupAlignmentData align_data;
  switch (policy) {
    case AlignmentPolicy::kAlign:
    case AlignmentPolicy::kInferUserIntent:
      align_data =
          CalculateAlignmentSpacings(alignable_rows_, alignment_cell_scanner_,
                                     ftokens->begin(), column_limit);
      break;
    default:
      break;
  }

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
        ApplyAlignment(align_data, ftokens->begin());
      }
      break;
    }
    case AlignmentPolicy::kFlushLeft:
      // This is already the default behavior elsewhere.  Nothing else to do.
      break;
    default:
      IndentButPreserveOtherSpacing(partition_range, full_text, ftokens);
      break;
  }
}

void TabularAlignTokens(
    TokenPartitionTree* partition_ptr,
    const ExtractAlignmentGroupsFunction& extract_alignment_groups,
    std::vector<PreFormatToken>* ftokens, absl::string_view full_text,
    const ByteOffsetSet& disabled_byte_ranges, int column_limit) {
  VLOG(1) << __FUNCTION__;
  // Each subpartition is presumed to correspond to a list element or
  // possibly some other ignored element like comments.

  auto& partition = *partition_ptr;
  auto& subpartitions = partition.Children();
  // Identify groups of partitions to align, separated by blank lines.
  const TokenPartitionRange subpartitions_range(subpartitions.begin(),
                                                subpartitions.end());
  if (subpartitions_range.empty()) return;
  VLOG(1) << "extracting alignment partition groups...";
  const std::vector<AlignablePartitionGroup> alignment_groups(
      extract_alignment_groups(subpartitions_range));
  for (const auto& alignment_group : alignment_groups) {
    const TokenPartitionRange partition_range(alignment_group.Range());
    if (partition_range.empty()) continue;
    if (AnyPartitionSubRangeIsDisabled(partition_range, full_text,
                                       disabled_byte_ranges)) {
      // Within an aligned group, if the group is partially disabled
      // due to incremental formatting, then leave the new lines
      // unformatted rather than falling back to compact-left formatting.
      // However, allow the first token to be correctly indented.
      IndentButPreserveOtherSpacing(partition_range, full_text, ftokens);
      continue;

      // TODO(fangism): instead of disabling the whole range, sub-partition
      // it one more level, and operate on those ranges, essentially treating
      // no-format ranges like alignment group boundaries.
      // Requires IntervalSet::Intersect operation.

      // TODO(b/159824483): attempt to detect and re-use pre-existing alignment
    }

    // Calculate alignment and possibly apply it depending on alignment policy.
    alignment_group.Align(full_text, column_limit, ftokens);
  }
  VLOG(1) << "end of " << __FUNCTION__;
}

std::vector<TaggedTokenPartitionRange>
GetSubpartitionsBetweenBlankLinesSingleTag(
    const TokenPartitionRange& full_range, int subtype) {
  std::vector<TokenPartitionRange> ranges(
      GetSubpartitionsBetweenBlankLines(full_range));
  std::vector<TaggedTokenPartitionRange> result;
  result.reserve(ranges.size());
  for (const auto& range : ranges) {
    result.emplace_back(range, subtype);
  }
  return result;
}

std::vector<TaggedTokenPartitionRange> GetPartitionAlignmentSubranges(
    const TokenPartitionRange& partitions,
    const std::function<AlignedPartitionClassification(
        const TokenPartitionTree&)>& partition_selector,
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
