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

#include "common/formatting/token_partition_tree.h"

#include <cstddef>
#include <iterator>
#include <vector>

#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line.h"
#include "common/util/container_iterator_range.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"
#include "common/util/top_n.h"
#include "common/util/vector_tree.h"

namespace verible {

void VerifyTreeNodeFormatTokenRanges(
    const TokenPartitionTree& node,
    std::vector<PreFormatToken>::const_iterator base) {
  VLOG(4) << __FUNCTION__ << " @ node path: " << NodePath(node);

  // Converting an iterator to an index is easier for debugging.
  auto TokenIndex = [=](std::vector<PreFormatToken>::const_iterator iter) {
    return std::distance(base, iter);
  };

  const auto& children = node.Children();
  if (!children.empty()) {
    {
      // Hierarchy invariant: parent's range == range spanned by children.
      // Check against first child's begin, and last child's end.
      const auto& parent_range = node.Value().TokensRange();
      // Translates ranges' iterators into positional indices.
      const int parent_begin = TokenIndex(parent_range.begin());
      const int parent_end = TokenIndex(parent_range.end());
      const int children_begin =
          TokenIndex(children.front().Value().TokensRange().begin());
      const int children_end =
          TokenIndex(children.back().Value().TokensRange().end());
      CHECK_EQ(parent_begin, children_begin);
      CHECK_EQ(parent_end, children_end);
    }
    {
      // Sibling continuity invariant:
      // The end() of one child is the begin() of the next child.
      auto iter = children.begin();
      const auto end = children.end();
      auto prev_upper_bound = iter->Value().TokensRange().end();
      for (++iter; iter != end; ++iter) {
        const auto child_range = iter->Value().TokensRange();
        const int current_begin = TokenIndex(child_range.begin());
        const int previous_end = TokenIndex(prev_upper_bound);
        CHECK_EQ(current_begin, previous_end);
        prev_upper_bound = child_range.end();
      }
    }
  }
  VLOG(4) << __FUNCTION__ << " (verified)";
}

void VerifyFullTreeFormatTokenRanges(
    const TokenPartitionTree& tree,
    std::vector<PreFormatToken>::const_iterator base) {
  VLOG(4) << __FUNCTION__ << '\n' << TokenPartitionTreePrinter{tree};
  tree.ApplyPreOrder([=](const TokenPartitionTree& node) {
    VerifyTreeNodeFormatTokenRanges(node, base);
  });
}

struct SizeCompare {
  bool operator()(const UnwrappedLine* left, const UnwrappedLine* right) const {
    return left->Size() > right->Size();
  }
};

std::vector<const UnwrappedLine*> FindLargestPartitions(
    const TokenPartitionTree& token_partitions, size_t num_partitions) {
  // Sort UnwrappedLines from leaf partitions by size.
  using partition_set_type = verible::TopN<const UnwrappedLine*, SizeCompare>;
  partition_set_type partitions(num_partitions);
  token_partitions.ApplyPreOrder([&partitions](const TokenPartitionTree& node) {
    if (node.Children().empty()) {  // only look at leaf partitions
      partitions.push(&node.Value());
    }
  });
  return partitions.Take();
}

std::ostream& TokenPartitionTreePrinter::PrintTree(std::ostream& stream,
                                                   int indent) const {
  const auto& value = node.Value();
  const auto& children = node.Children();
  stream << Spacer(indent) << "{ ";
  if (children.empty()) {
    stream << '(' << value << ") }";
  } else {
    stream << '(' << Spacer(value.IndentationSpaces()) << "[<auto>]) @"
           << NodePath(node) << '\n';
    // token range spans all of children nodes
    for (const auto& child : children) {
      TokenPartitionTreePrinter(child).PrintTree(stream, indent + 2) << '\n';
    }
    stream << Spacer(indent) << '}';
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const TokenPartitionTreePrinter& printer) {
  return printer.PrintTree(stream);
}

TokenPartitionTree* MoveLastLeafIntoPreviousSibling(TokenPartitionTree* tree) {
  auto* rightmost_leaf = tree->RightmostDescendant();
  auto* target_leaf = rightmost_leaf->PreviousLeaf();
  if (target_leaf == nullptr) return nullptr;

  // if 'tree' is not an ancestor of target_leaf, do not modify.
  if (!target_leaf->ContainsAncestor(tree)) return nullptr;

  // Verify continuity of token ranges between adjacent leaves.
  CHECK(target_leaf->Value().TokensRange().end() ==
        rightmost_leaf->Value().TokensRange().begin());

  auto* rightmost_leaf_parent = rightmost_leaf->Parent();
  {
    // Extend the upper-bound of the previous leaf partition to cover the
    // partition that is about to be removed.
    const auto range_end = rightmost_leaf->Value().TokensRange().end();

    // Update ancestry chain of the updated leaf, to maintain the parent-child
    // range equivalence invariant.
    auto* node = target_leaf;
    while (node != tree) {
      node->Value().SpanUpToToken(range_end);
      node = node->Parent();
    }
    node->Value().SpanUpToToken(range_end);

    // Remove the obsolete partition, rightmost_leaf.
    // Caution: Existing references to the obsolete partition will be
    // invalidated!
    rightmost_leaf_parent->Children().pop_back();
  }

  // Sanity check invariants.
  VerifyFullTreeFormatTokenRanges(
      *tree, tree->LeftmostDescendant()->Value().TokensRange().begin());

  return rightmost_leaf_parent;
}

}  // namespace verible
