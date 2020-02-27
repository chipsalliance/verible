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

#include "common/formatting/token_partition_tree.h"

#include <cstddef>
#include <iterator>
#include <vector>

#include "common/formatting/format_token.h"
#include "common/formatting/line_wrap_searcher.h"
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
    const TokenPartitionTreePrinter node_printer(node);
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
      CHECK_EQ(parent_begin, children_begin) << "node:\n" << node_printer;
      CHECK_EQ(parent_end, children_end) << "node:\n" << node_printer;
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
        CHECK_EQ(current_begin, previous_end) << "node:\n" << node_printer;
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
    stream << '(';
    value.AsCode(&stream, verbose);
    stream << ") }";
  } else {
    stream << '('
           << Spacer(value.IndentationSpaces(),
                     UnwrappedLine::kIndentationMarker)
           << "[<auto>]) @" << NodePath(node)
           << ", policy: " << value.PartitionPolicy() << '\n';
    // token range spans all of children nodes
    for (const auto& child : children) {
      TokenPartitionTreePrinter(child, verbose).PrintTree(stream, indent + 2)
          << '\n';
    }
    stream << Spacer(indent) << '}';
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const TokenPartitionTreePrinter& printer) {
  return printer.PrintTree(stream);
}

void MergeConsecutiveSiblings(TokenPartitionTree* tree, size_t pos) {
  // Effectively concatenate unwrapped line ranges of sibling subpartitions.
  tree->MergeConsecutiveSiblings(
      pos, [](UnwrappedLine* left, const UnwrappedLine& right) {
        // Verify token range continuity.
        CHECK(left->TokensRange().end() == right.TokensRange().begin());
        left->SpanUpToToken(right.TokensRange().end());
      });
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

//
// TokenPartitionTree class wrapper used by AppendFittingSubpartitions and
// ReshapeFittingSubpartitions for partition reshaping purposes.
//
class TokenPartitionTreeWrapper {
 public:
  TokenPartitionTreeWrapper(const TokenPartitionTree& node) : node_(&node) {}

  // Grouping node with no corresponding TokenPartitionTree node
  TokenPartitionTreeWrapper(const UnwrappedLine& unwrapped_line) : node_(nullptr) {
    unwrapped_line_ = std::unique_ptr<UnwrappedLine>(new UnwrappedLine(unwrapped_line));
  }

  // At least one of node_ or unwrapped_line_ should not be nullptr
  TokenPartitionTreeWrapper() = delete;

  TokenPartitionTreeWrapper(const TokenPartitionTreeWrapper& other) {
    CHECK((other.node_ != nullptr) || (other.unwrapped_line_ != nullptr));

    if (other.node_) {
      node_ = other.node_;
    } else {
      node_ = nullptr;
      unwrapped_line_ = std::unique_ptr<UnwrappedLine>(
          new UnwrappedLine(*other.unwrapped_line_));
    }
  }

  // Return wrapped node value or concatenation of subnodes values
  const UnwrappedLine& Value() const {
    if (node_) {
      return node_->Value();
    } else {
      return *unwrapped_line_;
    }
  }

  // Concatenate subnodes value with other node value
  UnwrappedLine Value(const TokenPartitionTree& other) const {
    CHECK((node_ == nullptr) && (unwrapped_line_ != nullptr));
    UnwrappedLine uw = *unwrapped_line_;
    uw.SpanUpToToken(other.Value().TokensRange().end());
    return uw;
  }

  // Update concatenated value of subnodes
  void Update(const VectorTree<TokenPartitionTreeWrapper>* child) {
    const auto& token_partition_tree_wrapper = child->Value();
    const auto& uwline = token_partition_tree_wrapper.Value();

    unwrapped_line_->SpanUpToToken(uwline.TokensRange().end());
  }

  // Fix concatenated value indentation
  void SetIndentationSpaces(int indent) {
    CHECK((node_ == nullptr) && (unwrapped_line_ != nullptr));
    unwrapped_line_->SetIndentationSpaces(indent);
  }

  // Return address to wrapped node
  const TokenPartitionTree* Node() const {
    return node_;
  }

 private:
  // Wrapped node
  const TokenPartitionTree* node_;

  // Concatenated value of subnodes
  std::unique_ptr<UnwrappedLine> unwrapped_line_;
};

// Reshapes unfitted_partitions and returns result via fitted_partitions.
// Function creates VectorTree<> with additional level of grouping.
// Function expects two partitions: first used for computing indentation
// second with subpartitions, e.g.
// { (>>[<auto>]) @{1,0} // tree root
//   { (>>[function fffffffffff (]) } // first subpartition
//   { (>>>>>>[<auto>]) @{1,0,1}, // root node with subpartitions
//     { (>>>>>>[type_a aaaa ,]) }
//     { (>>>>>>[type_b bbbbb ,]) }
//     { (>>>>>>[type_c cccccc ,]) }
//     { (>>>>>>[type_d dddddddd ,]) }
//     { (>>>>>>[type_e eeeeeeee ,]) }
//     { (>>>>>>[type_f ffff ) ;]) }
//   }
// }
// Function is iterating over subpartitions and tries to append
// them if fits in line. Parameter wrap_first_subpartition is used to determine
// whether wrap first subpartition or not.
// Return value signalise whether first subpartition was wrapped.
static bool AppendFittingSubpartitions(
    VectorTree<TokenPartitionTreeWrapper>* fitted_partitions,
    const TokenPartitionTree& unfitted_partitions,
    const verible::BasicFormatStyle& style,
    bool wrap_first_subpartition) {
  bool wrapped_first_subpartition;

  // expects two partitions
  CHECK_EQ(unfitted_partitions.Children().size(), 2);

  // first with header
  const auto& header = unfitted_partitions.Children()[0];

  // arguments
  const auto& args = unfitted_partitions.Children()[1].Children();

  // at least one argument
  CHECK_GE(args.size(), 1);

  // Create first partition group
  // and populate it with function name (e.g. { [function foo (] })
  auto* group = fitted_partitions->NewChild(header.Value());
  auto* child = group->NewChild(header);

  int indent;

  // Try appending first argument
  auto group_value_arg = group->Value().Value(args[0]);
  if (wrap_first_subpartition || (verible::FitsOnLine(std::move(group_value_arg), style).fits == false)) {
    // Use wrap indentation
    indent = style.wrap_spaces + group_value_arg.IndentationSpaces();

    // wrap line
    group = group->NewSibling(args[0].Value()); // start new group
    child = group->NewChild(args[0]); // append not fitting 1st argument
    group->Value().SetIndentationSpaces(indent);

    // Wrapped first argument
    wrapped_first_subpartition = true;
  } else {
    // Compute new indentation level based on first partition
    const auto& group_value = group->Value().Value();
    const UnwrappedLine& uwline = group_value;
    indent = verible::FitsOnLine(uwline, style).final_column;

    // Append first argument to current group
    child = group->NewChild(args[0]);
    group->Value().Update(child);
    // keep group indentation

    // Appended first argument
    wrapped_first_subpartition = false;
  }

  for (size_t idx = 1 ; idx < args.size() ; ++idx) {
    const auto& arg = args[idx];

    // Every group should have at least one child
    CHECK_GT(group->Children().size(), 0);

    // Try appending current argument to current line
    UnwrappedLine uwline = group->Value().Value(arg);
    if (verible::FitsOnLine(uwline, style).fits) {
      // Fits, appending child
      child = group->NewChild(arg);
      group->Value().Update(child);
    } else {
      // Does not fit, start new group with current child
      group = group->NewSibling(arg.Value());
      child = group->NewChild(arg);
      // no need to update because group was created
      // with current child value

      // Fix group indentation
      group->Value().SetIndentationSpaces(indent);
    }
  }

  return wrapped_first_subpartition;
}

void ReshapeFittingSubpartitions(TokenPartitionTree* node,
                                 const verible::BasicFormatStyle& style) {
  VectorTree<TokenPartitionTreeWrapper>* fitted_tree = nullptr;

  // Leaf or simple node, e.g. '[function foo ( ) ;]'
  if ((node->Children().size() == 0) || (node->Children().size() == 1)) {
    // Nothing to to do
    return ;
  }

  // Partition with arguments should have at least one argument
  const auto& args = node->Children()[1].Children();
  CHECK_GT(args.size(), 0);

  VectorTree<TokenPartitionTreeWrapper> unwrapped_tree(node->Value());
  VectorTree<TokenPartitionTreeWrapper> wrapped_tree(node->Value());

  // Format unwrapped_lines. At first without forced wrap after first line
  bool wrapped_first_token = AppendFittingSubpartitions(&unwrapped_tree,
                                                        *node, style, false);

  if (wrapped_first_token) {
    // First token was forced to wrap so there's no need to
    // generate wrapped version (it has to be wrapped)
    fitted_tree = &unwrapped_tree;
  } else {
    // Generate wrapped version to compare results.
    // Below function passes-trough lines that doesn't fit
    // e.g. very looooooooooong arguments with length over column limit
    // and leaves optimization to line_wrap_searcher.
    // In this approach generated result may not be
    // exactly correct beacause of additional line break done later.
    AppendFittingSubpartitions(&wrapped_tree, *node, style, true);

    // Compare number of grouping nodes
    // If number of grouped node is equal then prefer unwrapped result
    if (unwrapped_tree.Children().size() <= wrapped_tree.Children().size()) {
      fitted_tree = &unwrapped_tree;
    } else {
      fitted_tree = &wrapped_tree;
    }
  }

  // Rebuild TokenPartitionTree
  TokenPartitionTree temporary_tree(node->Value());

  // Iterate over partition groups
  for (const auto& itr : fitted_tree->Children()) {
    auto uwline = itr.Value().Value();
    // Partitions groups should fit in line but we're
    // leaving final decision to ExpandableTreeView
    uwline.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

    // Create new grouping node
    auto* group = temporary_tree.NewChild(std::move(uwline));

    // Iterate over partitions in group
    for (const auto& partition : itr.Children()) {
      // access partition_node_type
      const auto* node = partition.Value().Node();

      // Append child (warning contains original indentation)
      group->NewChild(std::move(*node));
    }
  }

  // Remove moved nodes
  node->Children().clear();

  // Move back from temporary tree
  node->AdoptSubtreesFrom(&temporary_tree);
}

}  // namespace verible
