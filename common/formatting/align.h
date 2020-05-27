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
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/strings/position.h"  // for ByteOffsetSet
#include "common/text/token_info.h"
#include "common/text/tree_context_visitor.h"

namespace verible {

// This object represents a bid for a new column as a row of tokens is scanned.
struct ColumnPositionEntry {
  // Establishes total ordering among columns.
  // This is used as a key for determining column uniqueness.
  SyntaxTreePath path;

  // Identifies the token that starts each sparse cell.
  TokenInfo starting_token;
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
  explicit ColumnSchemaScanner(MutableFormatTokenRange range)
      : format_token_range_(range) {}

  // Returns the collection of column position entries.
  const std::vector<ColumnPositionEntry>& SparseColumns() const {
    return sparse_columns_;
  }

  const MutableFormatTokenRange& FormatTokenRange() const {
    return format_token_range_;
  }

 protected:
  // TODO(fangism): support specifying desired column characteristics, like
  // flush_left.
  void ReserveNewColumn(const Symbol& symbol, const SyntaxTreePath& path);

  // Reserve a column using the current path as the key.
  void ReserveNewColumn(const Symbol& symbol) {
    ReserveNewColumn(symbol, Path());
  }

 private:
  // Range of format tokens whose left-spacing may be padded by alignment.
  const MutableFormatTokenRange format_token_range_;

  // Keeps track of unique positions where new columns are desired.
  std::vector<ColumnPositionEntry> sparse_columns_;
};

// TODO(fangism): rename starting with 'Alignment'
// TODO(fangism): make this interface more abstract as a plain std::function<>
// without requiring use of ColumnSchemaScanner.
using CellScannerFactory = std::function<std::unique_ptr<ColumnSchemaScanner>(
    MutableFormatTokenRange)>;

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
// Other parameters:
// 'scanner_gen' generator returns objects that scan lines
// for token positions that mark the start of a new column.
// 'ignore_pred' returns true for lines that should be ignored
// for alignment purposes, such as comment-only lines.
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
void TabularAlignTokens(
    TokenPartitionTree* partition_ptr, const CellScannerFactory& scanner_gen,
    const std::function<bool(const TokenPartitionTree&)> ignore_pred,
    MutableFormatTokenRange::iterator ftoken_base, absl::string_view full_text,
    const ByteOffsetSet& disabled_byte_ranges, int column_limit);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_ALIGN_H_
