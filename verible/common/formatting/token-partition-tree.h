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

#ifndef VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_H_
#define VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_H_

#include <cstddef>
#include <iosfwd>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/strings/position.h"  // for ByteOffsetSet
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/vector-tree.h"

namespace verible {

// Opaque type alias for a hierarchically partitioned format token stream.
// Objects of this type maintain the following invariants:
//   1) The format token range spanned by any tree node (UnwrappedLine) is
//      equal to that of its children.
//   2) Adjacent siblings begin/end iterators are equal (continuity).
//
// TODO(fangism): Promote this to a class that privately inherits the base.
// Methods on this class will preserve invariants.
using TokenPartitionTree = VectorTree<UnwrappedLine>;
using TokenPartitionIterator = TokenPartitionTree::subnodes_type::iterator;
using TokenPartitionRange = container_iterator_range<TokenPartitionIterator>;

// Analyses (non-modifying):

// Verifies the invariant properties of TokenPartitionTree at a single node.
// The 'base' argument is used to calculate iterator distances relative to the
// start of the format token array that is the basis for UnwrappedLine token
// ranges.  This function fails with a fatal-error like CHECK() if the
// invariants do not hold true.
void VerifyTreeNodeFormatTokenRanges(
    const TokenPartitionTree &node,
    std::vector<PreFormatToken>::const_iterator base);

// Verifies TokenPartitionTree invariants at every node in the tree,
// which covers and entire hierarchical partition of format tokens.
void VerifyFullTreeFormatTokenRanges(
    const TokenPartitionTree &tree,
    std::vector<PreFormatToken>::const_iterator base);

// Returns the largest leaf partitions of tokens, ordered by number of tokens
// spanned.
std::vector<const UnwrappedLine *> FindLargestPartitions(
    const TokenPartitionTree &token_partitions, size_t num_partitions);

// Compute (original_spacing - spaces_required) for every format token,
// except the first token in each partition.
// A perfectly formatted (flushed-left) original spacing will return all zeros.
std::vector<std::vector<int>> FlushLeftSpacingDifferences(
    const TokenPartitionRange &partitions);

// Custom printer, alternative to the default stream operator<<.
// Modeled after VectorTree<>::PrintTree, but suppresses printing of the
// tokens for non-leaf nodes because a node's token range always spans
// that of all of its children.
// Usage: stream << TokenPartitionTreePrinter(tree) << std::endl;
struct TokenPartitionTreePrinter {
  explicit TokenPartitionTreePrinter(
      const TokenPartitionTree &n, bool verbose = false,
      UnwrappedLine::OriginPrinterFunction origin_printer =
          UnwrappedLine::DefaultOriginPrinter)
      : node(n), verbose(verbose), origin_printer(std::move(origin_printer)) {}

  std::ostream &PrintTree(std::ostream &stream, int indent = 0) const;

  // The (sub)tree to display.
  const TokenPartitionTree &node;
  // If true, display inter-token information.
  bool verbose;

  UnwrappedLine::OriginPrinterFunction origin_printer;
};

std::ostream &operator<<(std::ostream &stream,
                         const TokenPartitionTreePrinter &printer);

// Returns true if any substring range spanned by the token partition 'range' is
// disabled by 'disabled_byte_ranges'.
// 'full_text' is the original string_view that spans the text from which
// tokens were lexed, and is used in byte-offset calculation.
bool AnyPartitionSubRangeIsDisabled(TokenPartitionRange range,
                                    absl::string_view full_text,
                                    const ByteOffsetSet &disabled_byte_ranges);

// Return ranges of subpartitions separated by blank lines.
// This does not modify the partition, but does return ranges of mutable
// iterators of partitions.
std::vector<TokenPartitionRange> GetSubpartitionsBetweenBlankLines(
    const TokenPartitionRange &);

// Transformations (modifying):

// Adds or removes a constant amount of indentation from entire token
// partition tree.  Relative indentation amount may be positive or negative.
// Final indentation will be at least 0, and never go negative.
void AdjustIndentationRelative(TokenPartitionTree *tree, int amount);

// Adjusts indentation to align root of partition tree to new indentation
// amount.
void AdjustIndentationAbsolute(TokenPartitionTree *tree, int amount);

// Returns the range of text spanned by tokens range.
absl::string_view StringSpanOfTokenRange(const FormatTokenRange &range);

// Mark ranges of tokens (corresponding to formatting-disabled lines) to
// have their original spacing preserved, except allow the first token
// to follow the formatter's calculated indentation.
void IndentButPreserveOtherSpacing(TokenPartitionRange partition_range,
                                   absl::string_view full_text,
                                   std::vector<PreFormatToken> *ftokens);

// Finalizes formatting of a partition with kAlreadyFormatted policy and
// optional kTokenModifer subpartitions.
// Spacing described by the partitions is applied to underlying tokens. All
// subpartitions of the passed node are removed.
void ApplyAlreadyFormattedPartitionPropertiesToTokens(
    TokenPartitionTree *already_formatted_partition_node,
    std::vector<PreFormatToken> *ftokens);

// Merges the two subpartitions of tree at index pos and pos+1.
void MergeConsecutiveSiblings(TokenPartitionTree *tree, size_t pos);

// Groups this leaf with the leaf partition that preceded it, which could be
// a distant relative. The grouping node is created in place of the preceding
// leaf.
// Both grouped partitions retain their indentation level and partition
// policies. The group partition's indentation level and partition policy is
// copied from the first partition in the group.
// Returns the grouped partition if operation succeeded, else nullptr.
TokenPartitionTree *GroupLeafWithPreviousLeaf(TokenPartitionTree *leaf);

// Merges this leaf into the leaf partition that preceded it, which could be
// a distant relative.  The leaf is destroyed in the process.
// The destination partition retains its indentation level and partition
// policies, but those of the leaf are discarded.
// (If you need that information, save it before moving the leaf.)
// Returns the parent of the leaf partition that was moved if the move
// occurred, else nullptr.
TokenPartitionTree *MergeLeafIntoPreviousLeaf(TokenPartitionTree *leaf);

// Merges this leaf into the leaf partition that follows it, which could be
// a distant relative.  The leaf is destroyed in the process.
// The destination partition retains its indentation level and partition
// policies, but those of the leaf are discarded.
// (If you need that information, save it before moving the leaf.)
// Returns the parent of the leaf partition that was moved if the move
// occurred, else nullptr.
TokenPartitionTree *MergeLeafIntoNextLeaf(TokenPartitionTree *leaf);

// Evaluates two partitioning schemes wrapped and appended first
// subpartition. Then reshapes node tree according to scheme with less
// grouping nodes (if both have same number of grouping nodes uses one
// with appended first subpartition).
//
// Example input:
// --------------
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[function fffffffffff (]) }
//   { (>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
//
// The special case of a singleton argument being flattened is also supported.
// --------------
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[function fffffffffff (]) }
//   { (>>>>>>[type_a aaaa ) ;]) }
// }
//
// Example outputs:
// ----------------
//
// style.column_limit = 100:
//
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[<auto>]) @{1,0,0}, policy: fit-else-expand
//     { (>>[function fffffffffff (]) }
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//     { (>>>>>>[type_e eeeeeeee ,]) }
//   }
//   { (>>>>>>>>>>>>>>>>>>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
//
// style.column_limit = 56:
//
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[<auto>]) @{1,0,0}, policy: fit-else-expand
//     { (>>[function fffffffffff (]) }
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//   }
//   { (>>>>>>>>>>>>>>>>>>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//   }
//   { (>>>>>>>>>>>>>>>>>>>>>>>[<auto>]) @{1,0,2}, policy: fit-else-expand
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
//
// style.column_limit = 40:
//
// { (>>[<auto>]) @{1,0}, policy: append-fitting-sub-partitions
//   { (>>[<auto>]) @{1,0,0}, policy: fit-else-expand
//     { (>>[function fffffffffff (]) }
//   }
//   { (>>>>>>[<auto>]) @{1,0,1}, policy: fit-else-expand
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//   }
//   { (>>>>>>[<auto>]) @{1,0,2}, policy: fit-else-expand
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//   }
//   { (>>>>>>[<auto>]) @{1,0,3}, policy: fit-else-expand
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
void ReshapeFittingSubpartitions(const verible::BasicFormatStyle &style,
                                 TokenPartitionTree *node);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_H_
