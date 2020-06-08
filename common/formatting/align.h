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

#ifndef VERIBLE_COMMON_FORMATTING_ALIGN_H_
#define VERIBLE_COMMON_FORMATTING_ALIGN_H_

#include <functional>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/formatting/token_partition_tree.h"
#include "common/strings/position.h"  // for ByteOffsetSet
#include "common/text/token_info.h"
#include "common/text/tree_context_visitor.h"
#include "common/util/logging.h"

namespace verible {

// Attributes of columns of text alignment (controlled by developer).
struct AlignmentColumnProperties {
  // If true format cell with padding to the right: |text   |
  // else format cell with padding to the left:     |   text|
  bool flush_left = true;

  AlignmentColumnProperties() = default;
  explicit AlignmentColumnProperties(bool flush_left)
      : flush_left(flush_left) {}
};

// This object represents a bid for a new column as a row of tokens is scanned.
struct ColumnPositionEntry {
  // Establishes total ordering among columns.
  // This is used as a key for determining column uniqueness.
  SyntaxTreePath path;

  // Identifies the token that starts each sparse cell.
  TokenInfo starting_token;

  // Properties of alignment columns (controlled by developer).
  AlignmentColumnProperties properties;
};

// TODO(fangism): support column groups (VectorTree)

// ColumnSchemaScanner traverses syntax subtrees of similar types and
// collects the positions that wish to register columns for alignment
// consideration.
// This serves as a base class for scanners that mark new columns
// for alignment.
// Subclasses are expected to implement the Visit({node, leaf}) virtual methods
// and call ReserveNewColumn() in locations that want a new columns.
class ColumnSchemaScanner : public TreeContextPathVisitor {
 public:
  ColumnSchemaScanner() = default;

  // Returns the collection of column position entries.
  const std::vector<ColumnPositionEntry>& SparseColumns() const {
    return sparse_columns_;
  }

 protected:
  // TODO(fangism): support specifying desired column characteristics, like
  // flush_left.
  void ReserveNewColumn(const Symbol& symbol,
                        const AlignmentColumnProperties& properties,
                        const SyntaxTreePath& path);

  // Reserve a column using the current path as the key.
  void ReserveNewColumn(const Symbol& symbol,
                        const AlignmentColumnProperties& properties) {
    ReserveNewColumn(symbol, properties, Path());
  }

 private:
  // Keeps track of unique positions where new columns are desired.
  std::vector<ColumnPositionEntry> sparse_columns_;
};

// This is the interface used to extract alignment cells from ranges of tokens.
// Note that it is not required to use a ColumnSchemaScanner.
using AlignmentCellScannerFunction =
    std::function<std::vector<ColumnPositionEntry>(const TokenPartitionTree&)>;

// This is the interface used to sub-divide a range of token partitions into
// a sequence of sub-ranges for the purposes of formatting aligned groups.
using ExtractAlignmentGroupsFunction =
    std::function<std::vector<TokenPartitionRange>(const TokenPartitionRange&)>;

// This predicate function is used to select partitions to be ignored within
// an alignment group.  For example, one may wish to ignore comment-only lines.
using IgnoreAlignmentRowPredicate =
    std::function<bool(const TokenPartitionTree&)>;

// Instantiates a ScannerType (implements ColumnSchemaScanner) and extracts
// column alignment information, suitable as an AlignmentCellScannerFunction.
// A 'row' corresponds to a range of format tokens over which spacing is to be
// adjusted to achieve alignment.
// Returns a sequence of column entries that will be uniquified and ordered
// for alignment purposes.
template <class ScannerType>
std::vector<ColumnPositionEntry> ScanPartitionForAlignmentCells(
    const TokenPartitionTree& row) {
  const UnwrappedLine& unwrapped_line = row.Value();
  // Walk the original syntax tree that spans a subset of the tokens spanned by
  // this 'row', and detect the sparse set of columns found by the scanner.
  const Symbol* origin = ABSL_DIE_IF_NULL(unwrapped_line.Origin());
  ScannerType scanner;
  origin->Accept(&scanner);
  return scanner.SparseColumns();
}

// Convenience function for generating alignment cell scanners.
// This can be useful for constructing maps of scanners based on type.
//
// Example:
//   static const auto* kAlignHandlers =
//      new std::map<NodeEnum, verible::AlignmentCellScannerFunction>{
//         {NodeEnum::kTypeA,
//          AlignmentCellScannerGenerator<TypeA_ColumnSchemaScanner>()},
//         {NodeEnum::kTypeB,
//          AlignmentCellScannerGenerator<TypeB_ColumnSchemaScanner>()},
//         ...
//   };
template <class ScannerType>
AlignmentCellScannerFunction AlignmentCellScannerGenerator() {
  return [](const TokenPartitionTree& row) {
    return ScanPartitionForAlignmentCells<ScannerType>(row);
  };
}

// This struct bundles together the various functions needed for aligned
// formatting.
struct AlignedFormattingHandler {
  // This function subdivides a range of token partitions (e.g. all of the
  // children of a parent partition of interest) into groups of lines that will
  // align with each other.
  ExtractAlignmentGroupsFunction extract_alignment_groups;

  // This returns true for lines (in each alignment group) that should be
  // ignored for alignment purposes, such as comment-only lines.
  IgnoreAlignmentRowPredicate ignore_partition_predicate;

  // This function scans lines (token ranges)
  // for token positions that mark the start of a new column.
  AlignmentCellScannerFunction alignment_cell_scanner;
};

// This aligns sections of text by modifying the spacing between tokens.
// 'partition_ptr' is a partition that can span one or more sections of
// code to align.  The partitions themselves are not reshaped, however,
// the inter-token spacing of tokens spanned by these partitions can be
// modified.
// Currently, alignment groups are separated at partition boundaries
// that span one or more blank lines (hard-coded for now).
//
// Let a 'line' be a unit of text to be aligned.
// Groups of lines are aligned together, as if their contents were table cells.
// Vertical alignment is achieved by sizing each column in the table to
// the max cell width in each column, and padding spaces as necessary.
//
// See description of AlignedFormattingHandler for a description of each
// function needed for aligned formatting.
//
// Other parameters:
// 'full_text' is the string_view buffer of whole text being formatted, not just
// the text spanned by 'partition_ptr'.
// 'ftoken_base' should be very first mutable iterator into the whole
// array of PreFormatTokens that spans 'full_text'.
// 'column_limit' is the column width beyond which the aligner should fallback
// to a safer action, e.g. refusing to align and leaving spacing untouched.
//
// Illustrated example:
// The following text:
//
//    aaa bb[11][22]
//    ccc[33] dd[444]
//
// could be arranged into a table (| for column delimiters):
//
//    aaa     | bb | [11]  |[22]
//    ccc[33] | dd | [444] |
//
// and assuming one space of padding between columns,
// and with every column flushed-left, result in:
//
//    aaa     bb [11]  [22]
//    ccc[33] dd [444]
//
void TabularAlignTokens(TokenPartitionTree* partition_ptr,
                        const AlignedFormattingHandler& alignment_handler,
                        MutableFormatTokenRange::iterator ftoken_base,
                        absl::string_view full_text,
                        const ByteOffsetSet& disabled_byte_ranges,
                        int column_limit);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_ALIGN_H_
