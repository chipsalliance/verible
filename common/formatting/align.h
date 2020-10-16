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
#include "common/formatting/format_token.h"
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
  // Mark the start of a new column for alignment.
  // 'symbol' is a reference to the original source syntax subtree.
  // 'properties' contains alignment configuration for the column.
  // 'path' represents relative position within the enclosing syntax subtree,
  // and is used as a key for ordering and matching columns.
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

// This enum signals to the GetPartitionAlignmentSubranges() function
// how a token partition should be included or excluded in partition groups.
enum class AlignmentGroupAction {
  kIgnore,   // This does not influence the current matching range.
  kMatch,    // Include this partition in the current matching range.
  kNoMatch,  // Close the current matching range (if any).
};

// This struct drives partition sub-range selection in the
// GetPartitionAlignmentSubranges() function.
struct AlignedPartitionClassification {
  AlignmentGroupAction action;

  // Matches that differ in subtype will also mark alignment group boundaries.
  // These values are up to the user's interpretation.  They are only checked
  // for equality in decision-making.
  // For example, when scanning a token partition that contains different
  // syntax groups, like assignments and function calls, the caller could
  // enumerate these subtypes 1 and 2, respectively, to distinguish the
  // alignable groups that are returned.
  // In situations where there can be only one alignment group subtype,
  // this value does not matter.
  int match_subtype;
};

struct TaggedTokenPartitionRange {
  TokenPartitionRange range;

  // Same as that in AlignedPartitionClassification::match_subtype.
  int match_subtype;

  TaggedTokenPartitionRange(TokenPartitionRange r, int subtype)
      : range(r), match_subtype(subtype) {}

  TaggedTokenPartitionRange(TokenPartitionIterator begin,
                            TokenPartitionIterator end, int subtype)
      : range(begin, end), match_subtype(subtype) {}
};

// Adapter function for extracting ranges of tokens that represent the same type
// of group to align (same syntax).
std::vector<TaggedTokenPartitionRange>
GetSubpartitionsBetweenBlankLinesSingleTag(
    const TokenPartitionRange& full_range, int subtype);

// From a range of token 'partitions', this selects sub-ranges to align.
// 'partition_selector' decides which partitions qualify for alignment.
//   When there is a match, the AlignedPartitionClassification::match_subtype
//   is also compared: if it matches, continue to grow the current
//   TaggedTokenPartitionRange, and if it doesn't match, start a new one.
// 'min_match_count' sets the minimum sub-range size to return.
//
// Visualization from 'partition_selector's perspective:
//
// case 1:
//   nomatch
//   match    // not enough matches to yield a group for min_match_count=2
//   nomatch
//
// case 2:
//   nomatch
//   match    // an alignment group starts here
//   match    // continues as long as subtype is the same
//   match    // ends here, inclusively
//   nomatch
//
// case 3:
//   nomatch
//   match    // an alignment group starts here
//   ignore   // ... continues ...
//   match    // ends here, inclusively
//   nomatch
//
// case 4:
//   nomatch
//   match (A)  // start group subtype A
//   match (A)  // continue group
//   match (B)  // finish previous group, start group subtype B
//   match (B)  // continue group
//
std::vector<TaggedTokenPartitionRange> GetPartitionAlignmentSubranges(
    const TokenPartitionRange& partitions,
    const std::function<AlignedPartitionClassification(
        const TokenPartitionTree&)>& partition_selector,
    int min_match_count = 2);

// This is the interface used to extract alignment cells from ranges of tokens.
// Note that it is not required to use a ColumnSchemaScanner.
using AlignmentCellScannerFunction =
    std::function<std::vector<ColumnPositionEntry>(const TokenPartitionTree&)>;

// For sections of code that are deemed alignable, this enum controls
// the formatter behavior.
enum class AlignmentPolicy {
  // Preserve text as-is.
  kPreserve,

  // No-align: flush text to left while obeying spacing constraints
  kFlushLeft,

  // Attempt tabular alignment.
  kAlign,

  // Infer whether user wanted flush-left or alignment, based on original
  // spacing.
  kInferUserIntent,
};

std::ostream& operator<<(std::ostream&, AlignmentPolicy);

bool AbslParseFlag(absl::string_view text, AlignmentPolicy* policy,
                   std::string* error);

std::string AbslUnparseFlag(const AlignmentPolicy& policy);

// This represents one unit of alignable work, which is usually a filtered
// subset of partitions within a contiguous range of partitions.
class AlignablePartitionGroup {
 public:
  AlignablePartitionGroup(const std::vector<TokenPartitionIterator>& rows,
                          const AlignmentCellScannerFunction& scanner,
                          AlignmentPolicy policy)
      : alignable_rows_(rows),
        alignment_cell_scanner_(scanner),
        alignment_policy_(policy) {}

  bool IsEmpty() const { return alignable_rows_.empty(); }

  TokenPartitionRange Range() const {
    return TokenPartitionRange(alignable_rows_.front(),
                               alignable_rows_.back() + 1);
  }

  // This executes alignment, depending on the alignment_policy.
  // 'full_text' is the original text buffer that spans all string_views
  // referenced by format tokens and token partition trees.
  // 'column_limit' is the maximum text width allowed post-alignment.
  // 'ftokens' is the original mutable array of formatting tokens from which
  // token partition trees were created.
  void Align(absl::string_view full_text, int column_limit,
             std::vector<PreFormatToken>* ftokens) const;

 private:
  struct GroupAlignmentData;
  static GroupAlignmentData CalculateAlignmentSpacings(
      const std::vector<TokenPartitionIterator>& rows,
      const AlignmentCellScannerFunction& cell_scanner_gen,
      MutableFormatTokenRange::iterator ftoken_base, int column_limit);

  void ApplyAlignment(const GroupAlignmentData& align_data,
                      MutableFormatTokenRange::iterator ftoken_base) const;

 private:
  // The set of partitions to treat as rows for tabular alignment.
  const std::vector<TokenPartitionIterator> alignable_rows_;

  // This function scans each row to identify column positions and properties of
  // alignable cells (containing token ranges).
  const AlignmentCellScannerFunction alignment_cell_scanner_;

  // Controls how this group should be aligned or flushed or preserved.
  const AlignmentPolicy alignment_policy_;
};

// This is the interface used to sub-divide a range of token partitions into
// a sequence of sub-ranges for the purposes of formatting aligned groups.
using ExtractAlignmentGroupsFunction =
    std::function<std::vector<AlignablePartitionGroup>(
        const TokenPartitionRange&)>;

// This predicate function is used to select partitions to be ignored within
// an alignment group.  For example, one may wish to ignore comment-only lines.
using IgnoreAlignmentRowPredicate =
    std::function<bool(const TokenPartitionTree&)>;

// Select subset of iterators inside a partition range that are not ignored
// by the predicate.
std::vector<TokenPartitionIterator> FilterAlignablePartitions(
    const TokenPartitionRange& range,
    const IgnoreAlignmentRowPredicate& ignore_partition_predicate);

// This adapter composes several functions for alignment (legacy interface) into
// one used in the current interface.  This exists to help migrate existing code
// to the new interface.
// This is only useful when all of the AlignablePartitionGroups want to be
// handled the same way using the same AlignmentCellScannerFunction and
// AlignmentPolicy.
// TODO(fangism): phase this out this interface by rewriting
// TabularAlignTokens().
ExtractAlignmentGroupsFunction ExtractAlignmentGroupsAdapter(
    const std::function<std::vector<TaggedTokenPartitionRange>(
        const TokenPartitionRange&)>& legacy_extractor,
    const IgnoreAlignmentRowPredicate& legacy_ignore_predicate,
    const AlignmentCellScannerFunction& alignment_cell_scanner,
    AlignmentPolicy alignment_policy);

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

// This aligns sections of text by modifying the spacing between tokens.
// 'partition_ptr' is a partition that can span one or more sections of
// code to align.  The partitions themselves are not reshaped, however,
// the inter-token spacing of tokens spanned by these partitions can be
// modified.
// 'extract_alignment_groups' is a function that returns groups of token
// partitions to align along with their column extraction functions.
// (See AlignablePartitionGroup.)
//
// How it works:
// Let a 'line' be a unit of text to be aligned.
// Groups of lines are aligned together, as if their contents were table cells.
// Vertical alignment is achieved by sizing each column in the table to
// the max cell width in each column, and padding spaces as necessary.
//
// Other parameters:
// 'full_text' is the string_view buffer of whole text being formatted, not just
// the text spanned by 'partition_ptr'.
// 'ftokens' points to the array of PreFormatTokens that spans 'full_text'.
// 'disabled_byte_ranges' contains information about which ranges of text
// are to preserve their original spacing (no-formatting).
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
    TokenPartitionTree* partition_ptr,
    const ExtractAlignmentGroupsFunction& extract_alignment_groups,
    std::vector<PreFormatToken>* ftokens, absl::string_view full_text,
    const ByteOffsetSet& disabled_byte_ranges, int column_limit);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_ALIGN_H_
