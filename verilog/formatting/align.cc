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

#include "verilog/formatting/align.h"

#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_join.h"
#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/range.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/token_info.h"
#include "common/text/tree_context_visitor.h"
#include "common/text/tree_utils.h"
#include "common/util/algorithm.h"
#include "common/util/casts.h"
#include "common/util/container_iterator_range.h"
#include "common/util/iterator_adaptors.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"
#include "common/util/value_saver.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

using verible::down_cast;
using verible::PreFormatToken;
using verible::Symbol;
using verible::SymbolCastToNode;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::SyntaxTreePath;
using verible::TokenInfo;
using verible::TokenPartitionTree;
using verible::TreeContextPathVisitor;
using verible::UnwrappedLine;
using verible::ValueSaver;

typedef UnwrappedLine::range_type TokenRange;
typedef std::vector<PreFormatToken> ftoken_array_type;
typedef ftoken_array_type::iterator mutable_ftoken_iterator;
typedef verible::container_iterator_range<mutable_ftoken_iterator>
    MutableTokenRange;

template <class T>
static bool TokensAreAllComments(const T& tokens) {
  return std::find_if(
             tokens.begin(), tokens.end(),
             [](const typename T::value_type& token) {
               return !IsComment(verilog_tokentype(token.token->token_enum));
             }) == tokens.end();
}

static bool IgnorePartition(const TokenPartitionTree& partition) {
  const auto& uwline = partition.Value();
  const auto token_range = uwline.TokensRange();
  CHECK(!token_range.empty());
  // ignore lines containing only comments
  if (TokensAreAllComments(token_range)) return true;
  // ignore partitions belonging to preprocessing directives
  if (IsPreprocessorKeyword(verilog_tokentype(token_range.front().TokenEnum())))
    return true;
  return false;
}

template <class T>
struct SequenceStreamPrinter {
  const T& sequence;
  absl::string_view separator;
  absl::string_view begin;
  absl::string_view end;
};

template <class T>
std::ostream& operator<<(std::ostream& stream,
                         const SequenceStreamPrinter<T>& t) {
  return stream << t.begin
                << absl::StrJoin(t.sequence.begin(), t.sequence.end(),
                                 t.separator, absl::StreamFormatter())
                << t.end;
}

template <class T>
SequenceStreamPrinter<T> SequencePrinter(const T& t,
                                         absl::string_view sep = ", ",
                                         absl::string_view begin = "",
                                         absl::string_view end = "") {
  return SequenceStreamPrinter<T>{t, sep, begin, end};
}

static SequenceStreamPrinter<SyntaxTreePath> PathPrinter(
    const SyntaxTreePath& path) {
  return SequencePrinter(path, ",", "[", "]");
}

using TokenPartitionIterator = std::vector<TokenPartitionTree>::iterator;
using TokenPartitionRange =
    verible::container_iterator_range<TokenPartitionIterator>;

// Detects when there is a vertical separation of more than one line between
// two token partitions.
class BlankLineSeparatorDetector {
 public:
  // 'bounds' range must not be empty.
  explicit BlankLineSeparatorDetector(const TokenPartitionRange& bounds)
      : previous_end_(
            bounds.front().Value().TokensRange().front().token->text.begin()) {}
  bool operator()(const TokenPartitionTree& node) {
    const auto range = node.Value().TokensRange();
    if (range.empty()) return false;
    const auto begin = range.front().token->text.begin();
    const auto end = range.back().token->text.end();
    const auto gap = verible::make_string_view_range(previous_end_, begin);
    // A blank line between partitions contains 2+ newlines.
    const bool new_bound = std::count(gap.begin(), gap.end(), '\n') >= 2;
    previous_end_ = end;
    return new_bound;
  }

 private:
  // Keeps track of the end of the previous partition, which is the start
  // of each inter-partition gap (string_view).
  absl::string_view::const_iterator previous_end_;
};

// Subdivides the 'bounds' range into sub-ranges broken up by blank lines.
static void FindPartitionGroupBoundaries(
    const TokenPartitionRange& bounds,
    std::vector<TokenPartitionIterator>* subpartitions) {
  VLOG(2) << __FUNCTION__;
  subpartitions->clear();
  if (bounds.empty()) return;
  subpartitions->push_back(bounds.begin());
  // Bookkeeping for the end of the previous token range, used to evaluate
  // the inter-token-range text, looking for blank line.
  verible::find_all(bounds.begin(), bounds.end(),
                    std::back_inserter(*subpartitions),
                    BlankLineSeparatorDetector(bounds));
  subpartitions->push_back(bounds.end());
  VLOG(2) << "end of " << __FUNCTION__
          << ", boundaries: " << subpartitions->size();
}

static NodeEnum GetPartitionNodeEnum(const TokenPartitionTree& partition) {
  const auto* origin = partition.Value().Origin();
  return NodeEnum(SymbolCastToNode(*origin).Tag().tag);
}

static bool VerifyRowsOriginalNodeTypes(
    const std::vector<TokenPartitionIterator>& rows) {
  const auto first_node_type = GetPartitionNodeEnum(*rows.front());
  for (const auto& row : verible::make_range(rows.begin() + 1, rows.end())) {
    const auto node_type = GetPartitionNodeEnum(*row);
    if (node_type != first_node_type) {
      VLOG(2) << "Cannot format-align rows of different syntax tree node "
                 "types.  First: "
              << first_node_type << ", Other: " << node_type;
      return false;
    }
  }
  return true;
}

// Computes the path of the next sibling.
static SyntaxTreePath NextSiblingPath(const SyntaxTreePath& path) {
  CHECK(!path.empty());
  auto next = path;
  ++next.back();
  return next;
}

struct ColumnPositionEntry {
  // Establishes total ordering among columns.
  SyntaxTreePath path;
  // Identifies the token that starts each sparse cell.
  TokenInfo starting_token;
};

// TODO(fangism): support column groups (VectorTree)

static int EffectiveCellWidth(const TokenRange& tokens) {
  if (tokens.empty()) return 0;
  VLOG(2) << __FUNCTION__;
  // Sum token text lengths plus required pre-spacings (except first token).
  // Note: LeadingSpacesLength() honors where original spacing when preserved.
  return std::accumulate(tokens.begin(), tokens.end(),
                         -tokens.front().LeadingSpacesLength(),
                         [](int total_width, const PreFormatToken& ftoken) {
                           VLOG(2) << " +" << ftoken.before.spaces_required
                                   << " +" << ftoken.token->text.length();
                           // TODO(fangism): account for multi-line tokens like
                           // block comments.
                           return total_width + ftoken.LeadingSpacesLength() +
                                  ftoken.token->text.length();
                         });
}

static int EffectiveLeftBorderWidth(const MutableTokenRange& tokens) {
  if (tokens.empty()) return 0;
  return tokens.front().before.spaces_required;
}

struct AlignmentCell {
  // Slice of format tokens in this cell (may be empty range).
  MutableTokenRange tokens;
  // The width of this token excerpt that complies with minimum spacing.
  int compact_width = 0;
  // Width of the left-side spacing before this cell, which can be considered
  // as a space-only column, usually no more than 1 space wide.
  int left_border_width = 0;

  TokenRange ConstTokensRange() const {
    return TokenRange(tokens.begin(), tokens.end());
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

struct AlignedColumnConfiguration {
  int width = 0;
  int left_border = 0;
  bool flush_left = true;  // else flush_right

  int TotalWidth() const { return left_border + width; }

  void UpdateFromCell(const AlignmentCell& cell) {
    width = std::max(width, cell.compact_width);
    left_border = std::max(left_border, cell.left_border_width);
  }
};

typedef std::vector<AlignmentCell> AlignmentRow;
typedef std::vector<AlignmentRow> AlignmentMatrix;

// ColumnSchemaScanner traverses syntax subtrees of similar types and
// collects the positions that wish to register columns for alignment
// consideration.
class ColumnSchemaScanner : public TreeContextPathVisitor {
 public:
  explicit ColumnSchemaScanner(MutableTokenRange range)
      : format_token_range_(range) {}

  // Returns the collection of column position entries.
  const std::vector<ColumnPositionEntry>& SparseColumns() const {
    return sparse_columns_;
  }

  const MutableTokenRange& FormatTokenRange() const {
    return format_token_range_;
  }

 protected:
  // TODO(fangism): support specifying desired column characteristics, like
  // flush_left.
  void ReserveNewColumn(const Symbol& symbol, const SyntaxTreePath& path) {
    // The path helps establish a total ordering among all desired alignment
    // points, given that they may come from optional or repeated language
    // constructs.
    const auto* leaf = verible::GetLeftmostLeaf(symbol);
    // It is possible for a node to be empty, in which case, ignore.
    if (leaf == nullptr) return;
    if (sparse_columns_.empty() || sparse_columns_.back().path != path) {
      // It's possible the previous cell's path was intentionally altered
      // to effectively fuse it with the cell that is about to be added.
      // When this occurs, take the (previous) leftmost token, and suppress
      // adding a new column.
      sparse_columns_.push_back(ColumnPositionEntry{path, leaf->get()});
      VLOG(2) << "reserving new column at " << PathPrinter(path);
    }
  }

  // Reserve a column using the current path as the key.
  void ReserveNewColumn(const Symbol& symbol) {
    ReserveNewColumn(symbol, Path());
  }

  const SyntaxTreePath& MostRecentPath() const {
    CHECK(!sparse_columns_.empty());
    return sparse_columns_.back().path;
  }

 private:
  const MutableTokenRange format_token_range_;

  // Keeps track of unique positions where new columns are desired.
  std::vector<ColumnPositionEntry> sparse_columns_;
};

class ColumnSchemaAggregator {
 public:
  void Collect(const std::vector<ColumnPositionEntry>& row) {
    for (const auto& cell : row) {
      cell_map_[cell.path].push_back(cell.starting_token);
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

 private:
  // Keeps track of unique positions where new columns are desired.
  // The keys form the set of columns wanted across all rows.
  // The values are sets of starting tokens, from which token ranges
  // will be computed per cell.
  std::map<SyntaxTreePath, std::vector<TokenInfo>> cell_map_;

  // 1:1 map between SyntaxTreePath and column index.
  // Values are monotonically increasing, so this is binary_search-able.
  std::vector<SyntaxTreePath> column_positions_;
};

class PortDeclarationColumnSchemaScanner : public ColumnSchemaScanner {
 public:
  explicit PortDeclarationColumnSchemaScanner(MutableTokenRange range)
      : ColumnSchemaScanner(range) {}

  // Factory function, fits CellScannerFactory.
  static std::unique_ptr<ColumnSchemaScanner> Create(MutableTokenRange range) {
    return absl::make_unique<PortDeclarationColumnSchemaScanner>(range);
  }

  void Visit(const SyntaxTreeNode& node) override {
    auto tag = NodeEnum(node.Tag().tag);
    VLOG(2) << __FUNCTION__ << ", node: " << tag << " at "
            << PathPrinter(Path());
    switch (tag) {
      case NodeEnum::kPackedDimensions: {
        // Kludge: kPackedDimensions can appear in paths [2,1] and [2,0,2],
        // but we want them to line up in the same column.  Make it so.
        if (current_path_ == SyntaxTreePath{2, 1}) {
          SyntaxTreePath new_path{2, 0, 2};
          const ValueSaver<SyntaxTreePath> path_saver(&current_path_, new_path);
          // TODO(fangism): a swap-based saver would be more efficient
          // for vectors.
          TreeContextPathVisitor::Visit(node);
          return;
        }
        break;
      }
      case NodeEnum::kDataType:
        // appears in path [2,0]
      case NodeEnum::kDimensionRange:
      case NodeEnum::kDimensionScalar:
      case NodeEnum::kDimensionSlice:
      case NodeEnum::kDimensionAssociativeType:
        // all of these cases cover packed and unpacked
        ReserveNewColumn(node);
        break;
      case NodeEnum::kUnqualifiedId:
        if (Context().DirectParentIs(NodeEnum::kPortDeclaration)) {
          ReserveNewColumn(node);
        }
        break;
      case NodeEnum::kExpression:
        // optional: Early termination of tree traversal.
        // This also helps reduce noise during debugging of this visitor.
        return;
        // case NodeEnum::kConstRef: possible in CST, but should be
        // syntactically illegal in module ports context.
      default:
        break;
    }
    // recursive visitation
    TreeContextPathVisitor::Visit(node);
    VLOG(2) << __FUNCTION__ << ", leaving node: " << tag;
  }

  void Visit(const SyntaxTreeLeaf& leaf) override {
    VLOG(2) << __FUNCTION__ << ", leaf: " << leaf.get() << " at "
            << PathPrinter(Path());
    // TODO(b/70310743): subdivide and align '[' and ']' by giving them their
    // own single-character (sub) column.
    switch (leaf.get().token_enum) {
      // port directions
      case verilog_tokentype::TK_inout:
      case verilog_tokentype::TK_input:
      case verilog_tokentype::TK_output:
      case verilog_tokentype::TK_ref: {
        ReserveNewColumn(leaf);
        break;
      }

        // net types
      case verilog_tokentype::TK_wire:
      case verilog_tokentype::TK_tri:
      case verilog_tokentype::TK_tri1:
      case verilog_tokentype::TK_supply0:
      case verilog_tokentype::TK_wand:
      case verilog_tokentype::TK_triand:
      case verilog_tokentype::TK_tri0:
      case verilog_tokentype::TK_supply1:
      case verilog_tokentype::TK_wor:
      case verilog_tokentype::TK_trior:
      case verilog_tokentype::TK_wone:
      case verilog_tokentype::TK_uwire: {
        // Effectively merge/re-map this into the next node slot,
        // which is kDataType of kPortDeclaration.
        // This works-around a quirk in the CST construction where net_types
        // like 'wire' appear positionally before kDataType variable types
        // like 'reg'.
        ReserveNewColumn(leaf, NextSiblingPath(Path()));
        break;
      }
      // For now, treat [...] as a single column per dimension.
      // TODO(b/70310743): subdivide and align '[' and ']'.
      // TODO(b/70310743): Treat "[...:...]" as 5 columns.
      // Treat "[...]" (scalar) as 3 columns.
      // TODO(b/70310743): Treat the ... as a multi-column cell w.r.t.
      // the 5-column range format.
      default:
        break;
    }
    VLOG(2) << __FUNCTION__ << ", leaving leaf: " << leaf.get();
  }
};

static SequenceStreamPrinter<AlignmentRow> MatrixRowPrinter(
    const AlignmentRow& row) {
  return SequencePrinter(row, " | ", "<", ">");
}

// Translate a sparse set of columns into a fully-populated matrix row.
static void FillAlignmentRow(
    const std::vector<ColumnPositionEntry>& sparse_columns,
    const std::vector<SyntaxTreePath>& column_positions,
    MutableTokenRange partition_token_range, AlignmentRow* row) {
  VLOG(2) << __FUNCTION__;
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
    const auto column_index = std::distance(cbegin, pos_iter);
    VLOG(3) << "cell at column " << column_index;

    // Find the format token iterator that corresponds to the column start.
    // Linear time total over all outer loop iterations.
    token_iter =
        std::find_if(token_iter, token_end, [=](const PreFormatToken& ftoken) {
          return verible::BoundsEqual(ftoken.Text(), col.starting_token.text);
        });
    CHECK(token_iter != token_end);

    // Fill null-range cells between [last_column_index, column_index).
    const MutableTokenRange empty_filler(token_iter, token_iter);
    for (; last_column_index <= column_index; ++last_column_index) {
      VLOG(3) << "empty at column " << last_column_index;
      (*row)[last_column_index].tokens = empty_filler;
    }
    // At this point, the current cell has only seen its lower bound.
    // The upper bound will be set in a separate pass.
  }
  // Fill any sparse cells up to the last column.
  VLOG(3) << "fill up to last column";
  const MutableTokenRange empty_filler(token_end, token_end);
  for (; last_column_index < column_positions.size(); ++last_column_index) {
    VLOG(3) << "empty at column " << last_column_index;
    (*row)[last_column_index].tokens = empty_filler;
  }

  // In this pass, set the upper bounds of cells' token ranges.
  auto upper_bound = token_end;
  for (auto& cell : verible::reversed_view(*row)) {
    cell.tokens.set_end(upper_bound);
    upper_bound = cell.tokens.begin();
  }
  VLOG(2) << "end of " << __FUNCTION__ << ", row: " << MatrixRowPrinter(*row);
}

struct MatrixCellSizePrinter {
  const AlignmentMatrix& matrix;
};

std::ostream& operator<<(std::ostream& stream, const MatrixCellSizePrinter& p) {
  const AlignmentMatrix& matrix = p.matrix;
  for (auto& row : matrix) {
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
  }
  VLOG(2) << "end of " << __FUNCTION__ << ", cell sizes:\n"
          << MatrixCellSizePrinter{*matrix};
}

typedef std::vector<AlignedColumnConfiguration> AlignedFormattingColumnSchema;

static AlignedFormattingColumnSchema ComputeColumnWidths(
    const AlignmentMatrix& matrix) {
  VLOG(2) << __FUNCTION__;
  AlignedFormattingColumnSchema column_configs(matrix.front().size());
  for (auto& row : matrix) {
    auto column_iter = column_configs.begin();
    for (auto& cell : row) {
      column_iter->UpdateFromCell(cell);
      ++column_iter;
    }
  }
  VLOG(2) << "end of " << __FUNCTION__;
  return column_configs;
}

// Align cells by adjusting pre-token spacing for a single row.
static void AlignRowSpacings(
    const AlignedFormattingColumnSchema& column_configs, AlignmentRow* row) {
  VLOG(2) << __FUNCTION__;
  int accrued_spaces = 0;
  auto column_iter = column_configs.begin();
  for (const auto& cell : *row) {
    accrued_spaces += column_iter->left_border;
    if (cell.tokens.empty()) {
      // Accumulate spacing for the next sparse cell in this row.
      accrued_spaces += column_iter->width;
    } else {
      VLOG(2) << "at: " << cell.tokens.front().Text();
      // Align by setting the left-spacing based on sum of cell widths
      // before this one.
      const int padding = column_iter->width - cell.compact_width;
      auto& left_spacing = cell.tokens.front().before.spaces_required;
      if (column_iter->flush_left) {
        left_spacing = accrued_spaces;
        accrued_spaces = padding;
      } else {  // flush right
        left_spacing = accrued_spaces + padding;
        accrued_spaces = 0;
      }
      VLOG(2) << "left_spacing = " << left_spacing;
    }
    VLOG(2) << "accrued_spaces = " << accrued_spaces;
    ++column_iter;
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

// Given a const_iterator and the original mutable container, return
// the corresponding mutable iterator (without resorting to const_cast).
template <class Container>
typename Container::iterator ConvertToMutableIterator(
    typename Container::const_iterator const_iter,
    typename Container::iterator base) {
  const typename Container::const_iterator cbase(base);
  return base + std::distance(cbase, const_iter);
}

static MutableTokenRange ConvertToMutableTokenRange(
    const TokenRange& const_range, mutable_ftoken_iterator base) {
  return MutableTokenRange(
      ConvertToMutableIterator<ftoken_array_type>(const_range.begin(), base),
      ConvertToMutableIterator<ftoken_array_type>(const_range.end(), base));
}

using CellScannerFactory =
    std::function<std::unique_ptr<ColumnSchemaScanner>(MutableTokenRange)>;

static void AlignFilteredRows(const std::vector<TokenPartitionIterator>& rows,
                              const CellScannerFactory& cell_scanner_gen,
                              mutable_ftoken_iterator ftoken_base,
                              int column_limit) {
  VLOG(1) << __FUNCTION__;
  // Alignment requires 2+ rows.
  if (rows.size() <= 1) return;
  // Make sure all rows' nodes have the same type.
  if (!VerifyRowsOriginalNodeTypes(rows)) return;

  VLOG(2) << "Walking syntax subtrees for each row";
  ColumnSchemaAggregator column_schema;
  std::vector<std::unique_ptr<ColumnSchemaScanner>> cell_scanners;
  cell_scanners.reserve(rows.size());
  // Simultaneously step through each node's tree, adding a column to the
  // schema if *any* row wants it.  This captures optional and repeated
  // constructs.
  {
    for (const auto& row : rows) {
      // Each row should correspond to an individual list element
      const auto& unwrapped_line = row->Value();
      const auto* origin = ABSL_DIE_IF_NULL(unwrapped_line.Origin());
      VLOG(2) << "row: " << verible::StringSpanOfSymbol(*origin);

      // Partition may contain text that is outside of the span of the syntax
      // tree node that was visited, e.g. a trailing comma delimiter.
      // Exclude those tokens from alignment consideration.
      const auto& last_token = verible::GetRightmostLeaf(*origin);
      auto range_begin = unwrapped_line.TokensRange().begin();
      auto range_end = unwrapped_line.TokensRange().end();
      // Backwards search is expected to check at most a few tokens.
      while (!verible::BoundsEqual(std::prev(range_end)->Text(),
                                   last_token->get().text))
        --range_end;
      CHECK(range_begin <= range_end);

      // Scan each token-range for cell boundaries based on syntax,
      // and establish partial ordering based on syntax tree paths.
      cell_scanners.emplace_back(cell_scanner_gen(ConvertToMutableTokenRange(
          TokenRange(range_begin, range_end), ftoken_base)));
      auto& scanner = *cell_scanners.back();
      origin->Accept(&scanner);

      // Aggregate union of all column keys (syntax tree paths).
      column_schema.Collect(scanner.SparseColumns());
    }
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
  AlignmentMatrix matrix(rows.size());
  {
    auto scanner_iter = cell_scanners.begin();
    for (auto& row : matrix) {
      row.resize(num_columns);
      FillAlignmentRow((*scanner_iter)->SparseColumns(), column_positions,
                       (*scanner_iter)->FormatTokenRange(), &row);
      ++scanner_iter;
    }
  }

  // Compute compact sizes per cell.
  ComputeCellWidths(&matrix);

  // Compute max widths per column.
  AlignedFormattingColumnSchema column_configs(ComputeColumnWidths(matrix));

  // Total width does not include initial left-indentation.
  // Assume indentation is the same for all partitions in each group.
  const int indentation = rows.front().base()->Value().IndentationSpaces();
  const int total_column_width =
      std::accumulate(column_configs.begin(), column_configs.end(), indentation,
                      [](int total_width, const AlignedColumnConfiguration& c) {
                        return total_width + c.TotalWidth();
                      });
  VLOG(2) << "Total (aligned) column width = " << total_column_width;
  // if the aligned columns would exceed the column limit, then refuse to align
  // for now.  However, this check alone does not include text that follows
  // the last aligned column, like trailing commans and EOL comments.
  if (total_column_width > column_limit) {
    VLOG(1) << "Total aligned column width " << total_column_width
            << " exceeds limit " << column_limit
            << ", so not aligning this group.";
    return;
  }
  {
    auto partition_iter = rows.begin();
    for (const auto& row : matrix) {
      if (!row.empty()) {
        // Identify the unaligned epilog text on each partition.
        auto partition_end =
            partition_iter->base()->Value().TokensRange().end();
        auto row_end = row.back().tokens.end();
        const TokenRange epilog_range(row_end, partition_end);
        const int aligned_partition_width =
            total_column_width + EffectiveCellWidth(epilog_range);
        if (aligned_partition_width > column_limit) {
          VLOG(1) << "Total aligned partition width " << aligned_partition_width
                  << " exceeds limit " << column_limit
                  << ", so not aligning this group.";
          return;
        }
      }
      ++partition_iter;
    }
  }

  // TODO(fangism): check for trailing text like comments, and if aligning would
  // exceed the column limit, then for now, refuse to align.
  // TODO(fangism): implement overflow mitigation fallback strategies.

  // Adjust pre-token spacings of each row to align to the column configs.
  for (auto& row : matrix) {
    AlignRowSpacings(column_configs, &row);
  }
  VLOG(1) << "end of " << __FUNCTION__;
}

static void AlignPartitionGroup(const TokenPartitionRange& group,
                                const CellScannerFactory& scanner_gen,
                                mutable_ftoken_iterator ftoken_base,
                                int column_limit) {
  VLOG(1) << __FUNCTION__ << ", group size: " << group.size();
  // This partition group may contain partitions that should not be
  // considered for column alignment purposes, so filter those out.
  std::vector<TokenPartitionIterator> qualified_partitions;
  qualified_partitions.reserve(group.size());
  // like std::copy_if, but we want the iterators, not their pointees.
  for (auto iter = group.begin(); iter != group.end(); ++iter) {
    // TODO(fangism): pass in filter predicate as a function
    if (!IgnorePartition(*iter)) {
      VLOG(2) << "including partition: " << *iter;
      qualified_partitions.push_back(iter);
    } else {
      VLOG(2) << "excluding partition: " << *iter;
    }
  }
  // Align the qualified partitions (rows).
  AlignFilteredRows(qualified_partitions, scanner_gen, ftoken_base,
                    column_limit);
  VLOG(1) << "end of " << __FUNCTION__;
}

static absl::string_view StringSpanOfPartitionRange(
    const TokenPartitionRange& range) {
  const auto front_range = range.front().Value().TokensRange();
  const auto back_range = range.back().Value().TokensRange();
  CHECK(!front_range.empty());
  CHECK(!back_range.empty());
  return verible::make_string_view_range(front_range.front().Text().begin(),
                                         back_range.back().Text().end());
}

static bool AnyPartitionSubRangeIsDisabled(
    TokenPartitionRange range, absl::string_view full_text,
    const ByteOffsetSet& disabled_byte_ranges) {
  const auto span = StringSpanOfPartitionRange(range);
  const auto span_offsets = verible::SubstringOffsets(span, full_text);
  ByteOffsetSet diff(disabled_byte_ranges);  // copy
  diff.Complement(span_offsets);             // enabled range(s)
  ByteOffsetSet span_set;
  span_set.Add(span_offsets);
  return diff != span_set;
}

static void AlignTokenPartition(TokenPartitionTree* partition_ptr,
                                const CellScannerFactory& scanner_gen,
                                mutable_ftoken_iterator ftoken_base,
                                absl::string_view full_text,
                                const ByteOffsetSet& disabled_byte_ranges,
                                int column_limit) {
  VLOG(1) << __FUNCTION__;
  // Each subpartition is presumed to correspond to a single port declaration,
  // preprocessor directive (`ifdef), or comment.

  auto& partition = *partition_ptr;
  auto& subpartitions = partition.Children();
  // Identify groups of ports to align, separated by blank lines.
  const TokenPartitionRange subpartitions_range(subpartitions.begin(),
                                                subpartitions.end());
  if (subpartitions_range.empty()) return;
  std::vector<TokenPartitionIterator> subpartitions_bounds;
  // TODO(fangism): pass in custom alignment group partitioning function.
  FindPartitionGroupBoundaries(subpartitions_range, &subpartitions_bounds);
  CHECK_GE(subpartitions_bounds.size(), 2);
  auto prev = subpartitions_bounds.begin();
  // similar pattern to std::adjacent_difference.
  for (auto next = std::next(prev); next != subpartitions_bounds.end();
       prev = next, ++next) {
    const TokenPartitionRange group_partition_range(*prev, *next);

    // If any sub-interval in this range is disabled, skip it.
    // TODO(fangism): instead of disabling the whole range, sub-partition
    // it one more level, and operate on those ranges, essentially treating
    // no-format ranges like alignment group boundaries.
    // Requires IntervalSet::Intersect operation.
    if (group_partition_range.empty() ||
        AnyPartitionSubRangeIsDisabled(group_partition_range, full_text,
                                       disabled_byte_ranges))
      continue;

    AlignPartitionGroup(group_partition_range, scanner_gen, ftoken_base,
                        column_limit);
    // TODO(fangism): rewrite using functional composition.
  }
  VLOG(1) << "end of " << __FUNCTION__;
}

void TabularAlignTokenPartitions(TokenPartitionTree* partition_ptr,
                                 ftoken_array_type* ftokens,
                                 absl::string_view full_text,
                                 const ByteOffsetSet& disabled_byte_ranges,
                                 int column_limit) {
  VLOG(1) << __FUNCTION__;
  auto& partition = *partition_ptr;
  auto& uwline = partition.Value();
  const auto* origin = uwline.Origin();
  VLOG(1) << "origin is nullptr? " << (origin == nullptr);
  if (origin == nullptr) return;
  const auto* node = down_cast<const SyntaxTreeNode*>(origin);
  VLOG(1) << "origin is node? " << (node != nullptr);
  if (node == nullptr) return;
  // Dispatch aligning function based on syntax tree node type.
  auto ftoken_base = ftokens->begin();

  static const auto* kAlignHandlers =
      new std::map<NodeEnum, CellScannerFactory>{
          {NodeEnum::kPortDeclarationList,
           &PortDeclarationColumnSchemaScanner::Create},
      };
  const auto handler_iter = kAlignHandlers->find(NodeEnum(node->Tag().tag));
  if (handler_iter == kAlignHandlers->end()) return;
  AlignTokenPartition(partition_ptr, handler_iter->second, ftoken_base,
                      full_text, disabled_byte_ranges, column_limit);
  VLOG(1) << "end of " << __FUNCTION__;
}

}  // namespace formatter
}  // namespace verilog
