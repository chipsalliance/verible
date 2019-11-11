// Copyright 2017-2019 The Verible Authors.
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
#include <vector>

#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/vector_tree.h"

namespace verible {

// Opaque type alias for a hierarchically partitioned format token stream.
// Objects of this type maintain the following invariants:
//   1) The format token range spanned by any tree node (UnwrappedLine) is
//      equal to that of its children.
//   2) Adjacent siblings begin/end iterators are equal (continuity).
using TokenPartitionTree = VectorTree<UnwrappedLine>;

// Analyses (non-modifying):

// Verifies the invariant properties of TokenPartitionTree at a single node.
// The 'base' argument is used to calculate iterator distances relative to the
// start of the format token array that is the basis for UnwrappedLine token
// ranges.  This function fails with a fatal-error like CHECK() if the
// invariants do not hold true.
void VerifyTreeNodeFormatTokenRanges(
    const TokenPartitionTree& node,
    std::vector<PreFormatToken>::const_iterator base);

// Verifies TokenPartitionTree invariants at every node in the tree,
// which covers and entire hierarchical partition of format tokens.
void VerifyFullTreeFormatTokenRanges(
    const TokenPartitionTree& tree,
    std::vector<PreFormatToken>::const_iterator base);

// Returns the largest leaf partitions of tokens, ordered by number of tokens
// spanned.
std::vector<const UnwrappedLine*> FindLargestPartitions(
    const TokenPartitionTree& token_partitions, size_t num_partitions);

// Custom printer, alternative to the default stream operator<<.
// Modeled after VectorTree<>::PrintTree, but suppresses printing of the
// tokens for non-leaf nodes because a node's token range always spans
// that of all of its children.
// Usage: stream << TokenPartitionTreePrinter(tree) << std::endl;
struct TokenPartitionTreePrinter {
  explicit TokenPartitionTreePrinter(const TokenPartitionTree& n) : node(n) {}

  std::ostream& PrintTree(std::ostream& stream, int indent = 0) const;

  const TokenPartitionTree& node;
};

std::ostream& operator<<(std::ostream& stream,
                         const TokenPartitionTreePrinter& printer);

// Transformations (modifying):

// Moves the rightmost leaf to the leaf partition that precedes it.
void MoveLastLeafIntoPreviousSibling(TokenPartitionTree*);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_H_
