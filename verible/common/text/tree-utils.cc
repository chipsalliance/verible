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

#include "verible/common/text/tree-utils.h"

#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/visitors.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"
#include "verible/common/util/value-saver.h"

namespace verible {

const Symbol *DescendThroughSingletons(const Symbol &symbol) {
  if (symbol.Kind() == SymbolKind::kLeaf) {
    return &symbol;
  }
  // else is a kNode
  const auto &node = SymbolCastToNode(symbol);
  if (node.size() == 1 && node.front() != nullptr) {
    // If only child is non-null, descend.
    return DescendThroughSingletons(*node.front());
    // TODO(fangism): rewrite non-recursively.
  }
  return &symbol;
}

const SyntaxTreeLeaf *GetRightmostLeaf(const Symbol &symbol) {
  if (symbol.Kind() == SymbolKind::kLeaf) {
    return &SymbolCastToLeaf(symbol);
  }

  const auto &node = SymbolCastToNode(symbol);

  for (const auto &child : const_reversed_view(node.children())) {
    if (child != nullptr) {
      const auto *leaf = GetRightmostLeaf(*child);
      if (leaf != nullptr) {
        return leaf;
      }
    }
  }

  return nullptr;
}

const SyntaxTreeLeaf *GetLeftmostLeaf(const Symbol &symbol) {
  if (symbol.Kind() == SymbolKind::kLeaf) {
    return &SymbolCastToLeaf(symbol);
  }

  const auto &node = SymbolCastToNode(symbol);

  for (const auto &child : node.children()) {
    if (child != nullptr) {
      const auto *leaf = GetLeftmostLeaf(*child);
      if (leaf != nullptr) {
        return leaf;
      }
    }
  }

  return nullptr;
}

std::string_view StringSpanOfSymbol(const Symbol &symbol) {
  return StringSpanOfSymbol(symbol, symbol);
}

std::string_view StringSpanOfSymbol(const Symbol &lsym, const Symbol &rsym) {
  const auto *left = GetLeftmostLeaf(lsym);
  const auto *right = GetRightmostLeaf(rsym);
  if (left != nullptr && right != nullptr) {
    const auto range_begin = left->get().text().begin();
    const auto range_end = right->get().text().end();
    return std::string_view(&*range_begin,
                            std::distance(range_begin, range_end));
  }
  return "";
}

const SyntaxTreeNode &SymbolCastToNode(const Symbol &symbol) {
  // Assert the symbol is a node.
  CHECK_EQ(symbol.Kind(), SymbolKind::kNode)
      << "got: " << RawTreePrinter(symbol);
  return down_cast<const SyntaxTreeNode &>(symbol);
}

SyntaxTreeNode &SymbolCastToNode(Symbol &symbol) {
  // Assert the symbol is a node.
  CHECK_EQ(symbol.Kind(), SymbolKind::kNode)
      << "got: " << RawTreePrinter(symbol);
  return down_cast<SyntaxTreeNode &>(symbol);
}

const SyntaxTreeLeaf &SymbolCastToLeaf(const Symbol &symbol) {
  // Assert the symbol is a leaf.
  CHECK_EQ(symbol.Kind(), SymbolKind::kLeaf)
      << "got: " << RawTreePrinter(symbol);
  return down_cast<const SyntaxTreeLeaf &>(symbol);
}

namespace {
// FirstSubtreeFinderMutable is a visitor class that supports the implementation
// of FindFirstSubtreeMutable().  It is derived from
// MutableTreeVisitorRecursive because it is intended for use with pruning and
// modifying syntax trees.
class FirstSubtreeFinderMutable : public MutableTreeVisitorRecursive {
 public:
  explicit FirstSubtreeFinderMutable(const TreePredicate &predicate)
      : predicate_(predicate) {}

  void Visit(const SyntaxTreeNode &node, SymbolPtr *symbol_ptr) final {
    CHECK_EQ(symbol_ptr->get(), &node);  // symbol_ptr owns node.
    if (result_ == nullptr) {
      // If this node matches, return it, and skip evaluating children.
      if (predicate_(node)) {
        result_ = symbol_ptr;
      } else {
        // Cast the mutable copy of the node pointer (same object as &node).
        auto *const mutable_node =
            down_cast<SyntaxTreeNode *>(symbol_ptr->get());
        for (SymbolPtr &child : mutable_node->mutable_children()) {
          if (child != nullptr) {
            child->Accept(this, &child);
          }
          // Stop as soon as first result is found.
          if (result_ != nullptr) return;
        }
      }
    }
  }

  void Visit(const SyntaxTreeLeaf &leaf, SymbolPtr *symbol_ptr) final {
    CHECK_EQ(symbol_ptr->get(), &leaf);  // symbol_ptr owns leaf.
    // If already have a result, stop checking and return right away.
    if (result_ == nullptr) {
      if (predicate_(leaf)) {
        result_ = symbol_ptr;
      }
    }
  }

  ConcreteSyntaxTree *result() const { return result_; }

 private:
  // Matching criterion.
  TreePredicate predicate_;

  // Contains first matching result found or nullptr if no match is found.
  ConcreteSyntaxTree *result_ = nullptr;
};

// FirstSubtreeFinder is a visitor class that supports the implementation of
// FindFirstSubtree().  It is derived from SymbolVisitor so that it's able to
// exit early if a match is found.
class FirstSubtreeFinder : public SymbolVisitor {
 public:
  explicit FirstSubtreeFinder(const TreePredicate &predicate)
      : predicate_(predicate) {}

  void Visit(const SyntaxTreeNode &node) final {
    if (result_ == nullptr) {
      // If this node matches, return it, and skip evaluating children.
      if (predicate_(node)) {
        result_ = &node;
      } else {
        for (const SymbolPtr &child : node.children()) {
          if (child != nullptr) {
            child->Accept(this);
          }
          // Stop as soon as first result is found.
          if (result_ != nullptr) return;
        }
      }
    }
  }

  void Visit(const SyntaxTreeLeaf &leaf) final {
    // If already have a result, stop checking and return right away.
    if (result_ == nullptr) {
      if (predicate_(leaf)) {
        result_ = &leaf;
      }
    }
  }

  const Symbol *result() const { return result_; }

 private:
  // Matching criterion.
  TreePredicate predicate_;

  // Contains first matching result found or nullptr if no match is found.
  const Symbol *result_ = nullptr;
};

// A visitor that finds the last matching node. Inherits from
// TreeVisitorRecursive, as it needs to visit all nodes in the tree.
class LastSubtreeFinder : public TreeVisitorRecursive {
 public:
  explicit LastSubtreeFinder(const TreePredicate &predicate)
      : predicate_(predicate) {}

  void Visit(const SyntaxTreeNode &node) final {
    if (predicate_(node)) result_ = &node;
  }

  void Visit(const SyntaxTreeLeaf &leaf) final {
    if (predicate_(leaf)) result_ = &leaf;
  }

  const Symbol *result() const { return result_; }

 private:
  // Matching criterion.
  TreePredicate predicate_;

  // Contains last matching result found or nullptr if no match is found.
  const Symbol *result_ = nullptr;
};
}  // namespace

ConcreteSyntaxTree *FindFirstSubtreeMutable(ConcreteSyntaxTree *tree,
                                            const TreePredicate &pred) {
  if (*ABSL_DIE_IF_NULL(tree) == nullptr) return nullptr;
  FirstSubtreeFinderMutable finder(pred);
  (*tree)->Accept(&finder, tree);
  return finder.result();
}

const Symbol *FindFirstSubtree(const Symbol *tree, const TreePredicate &pred) {
  if (tree == nullptr) return nullptr;
  FirstSubtreeFinder finder(pred);
  tree->Accept(&finder);
  return finder.result();
}

const Symbol *FindLastSubtree(const Symbol *tree, const TreePredicate &pred) {
  if (tree == nullptr) return nullptr;
  LastSubtreeFinder finder(pred);
  tree->Accept(&finder);
  return finder.result();
}

ConcreteSyntaxTree *FindSubtreeStartingAtOffset(
    ConcreteSyntaxTree *tree,
    std::string_view::const_iterator first_token_offset) {
  auto predicate = [=](const Symbol &s) {
    const SyntaxTreeLeaf *leftmost = GetLeftmostLeaf(s);
    if (leftmost != nullptr) {
      if (std::distance(first_token_offset, leftmost->get().text().begin()) >=
          0) {
        return true;
      }
    }
    return false;
  };
  ConcreteSyntaxTree *result =
      FindFirstSubtreeMutable(ABSL_DIE_IF_NULL(tree), predicate);
  // This cannot return a null tree node because it would have been skipped
  // by FirstSubtreeFinderMutable.
  if (result != nullptr) CHECK(*result != nullptr);
  return result;
}

// Helper function for PruneSyntaxTreeAfterOffset
namespace {
// Returns true if this node should be deleted by parent (pop_back).
bool PruneTreeFromRight(ConcreteSyntaxTree *tree,
                        std::string_view::const_iterator offset) {
  const auto kind = (*ABSL_DIE_IF_NULL(tree))->Kind();
  switch (kind) {
    case SymbolKind::kLeaf: {
      auto *leaf = down_cast<SyntaxTreeLeaf *>(tree->get());
      return std::distance(offset, leaf->get().text().end()) > 0;
    }
    case SymbolKind::kNode: {
      auto &node = down_cast<SyntaxTreeNode &>(*tree->get());
      int prune_count = 0;
      for (auto &child : const_reversed_view(node.mutable_children())) {
        if (child == nullptr) {
          ++prune_count;
        } else {
          if (PruneTreeFromRight(&child, offset)) {
            ++prune_count;
          } else {
            // Since token locations are monotonic, we can stop checking
            // as soon as the above function returns false.
            break;
          }
        }
      }
      // To avoid iterator invalidation issues in the loop, we erase at end.
      while (prune_count--) {
        node.pop_back();
      }
      // If no children remain, tell caller to delete this node.
      return node.empty();
    }
  }

  std::cerr << "Unhandled SymbolKind: " << static_cast<int>(kind);
  abort();
}
}  // namespace

void PruneSyntaxTreeAfterOffset(ConcreteSyntaxTree *tree,
                                std::string_view::const_iterator offset) {
  PruneTreeFromRight(tree, offset);
}

// Helper functions for ZoomSyntaxTree
namespace {
// Return the upper bound offset of the rightmost token in the tree.
std::string_view::const_iterator RightmostOffset(const Symbol &symbol) {
  const SyntaxTreeLeaf *leaf_ptr = verible::GetRightmostLeaf(symbol);
  return ABSL_DIE_IF_NULL(leaf_ptr)->get().text().end();
}

// Return the first non-null child node/leaf of the immediate subtree.
ConcreteSyntaxTree *LeftSubtree(ConcreteSyntaxTree *tree) {
  if ((ABSL_DIE_IF_NULL(*tree))->Kind() == verible::SymbolKind::kLeaf) {
    // Leaves don't have subtrees.
    return nullptr;
  }
  SyntaxTreeNode *const tree_node = down_cast<SyntaxTreeNode *>(tree->get());
  for (auto &child : tree_node->mutable_children()) {
    if (child != nullptr) return &child;
  }
  return nullptr;
}
}  // namespace

ConcreteSyntaxTree *ZoomSyntaxTree(ConcreteSyntaxTree *tree,
                                   std::string_view trim_range) {
  if (*tree == nullptr) return nullptr;

  const auto left_offset = trim_range.begin();
  // Find shallowest syntax tree node that starts at the given byte offset.
  ConcreteSyntaxTree *match =
      FindSubtreeStartingAtOffset(ABSL_DIE_IF_NULL(tree), left_offset);

  // Take leftmost subtree until its right bound falls within offset.
  const auto right_offset = trim_range.end();
  while (match != nullptr && *match != nullptr &&
         RightmostOffset(*ABSL_DIE_IF_NULL(*match)) > right_offset) {
    match = LeftSubtree(match);
  }
  return match;
}

void TrimSyntaxTree(ConcreteSyntaxTree *tree, std::string_view trim_range) {
  auto *replacement = ZoomSyntaxTree(tree, trim_range);
  if (replacement == nullptr || *replacement == nullptr) {
    *tree = nullptr;
  } else {
    *tree = std::move(*replacement);
  }
}

namespace {
// Applies one transformation to every leaf (token) in the syntax tree.
class LeafMutatorVisitor : public MutableTreeVisitorRecursive {
 public:
  // Maintains a reference but not ownership of the mutator, so the
  // mutator must outlive this object.
  explicit LeafMutatorVisitor(const LeafMutator *mutator)
      : leaf_mutator_(*mutator) {}

  void Visit(const SyntaxTreeNode &, SymbolPtr *) final {}

  // Transforms a single leaf.
  void Visit(const SyntaxTreeLeaf &leaf, SymbolPtr *leaf_owner) final {
    CHECK_EQ(leaf_owner->get(), &leaf);
    auto *const mutable_leaf = down_cast<SyntaxTreeLeaf *>(leaf_owner->get());
    leaf_mutator_(ABSL_DIE_IF_NULL(mutable_leaf)->get_mutable());
  }

 private:
  // Mutation to apply to every leaf token.
  const LeafMutator &leaf_mutator_;
};
}  // namespace

void MutateLeaves(ConcreteSyntaxTree *tree, const LeafMutator &mutator) {
  if (*ABSL_DIE_IF_NULL(tree) != nullptr) {
    LeafMutatorVisitor visitor(&mutator);
    (*tree)->Accept(&visitor, tree);
  }
}

//
// Implementation of printing functions
//

std::ostream &RawSymbolPrinter::auto_indent() {
  return *stream_ << Spacer(indent_, ' ');
}

void RawSymbolPrinter::Visit(const SyntaxTreeLeaf &leaf) {
  leaf.get().ToStream(auto_indent() << "Leaf @" << child_rank_ << ' ')
      << std::endl;
}

void PrettyPrinter::Visit(const SyntaxTreeLeaf &leaf) {
  leaf.get().ToStream(auto_indent() << "Leaf @" << child_rank_ << ' ', context_)
      << std::endl;
}

void RawSymbolPrinter::Visit(const SyntaxTreeNode &node) {
  std::string tag_info;
  const int tag = node.Tag().tag;
  if (tag != 0) tag_info = absl::StrCat("(tag: ", tag, ") ");

  auto_indent() << "Node @" << child_rank_ << ' ' << tag_info << "{"
                << std::endl;

  {
    const ValueSaver<int> value_saver(&indent_, indent_ + 2);
    const ValueSaver<int> rank_saver(&child_rank_, 0);
    for (const auto &child : node.children()) {
      if (child) {
        child->Accept(this);
      } else if (print_null_nodes_) {
        auto_indent() << "NULL @" << child_rank_ << std::endl;
      }
      // Note that nullptrs will appear as gaps in the child rank sequence.
      // nullptr nodes in tail position are not shown.
      ++child_rank_;
    }
  }
  auto_indent() << "}" << std::endl;
}

std::ostream &RawTreePrinter::Print(std::ostream &stream) const {
  RawSymbolPrinter printer(&stream, print_null_nodes_);
  root_.Accept(&printer);
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const RawTreePrinter &printer) {
  return printer.Print(stream);
}

void PrettyPrintTree(const Symbol &root, const TokenInfo::Context &context,
                     std::ostream *stream) {
  PrettyPrinter printer(stream, context);
  root.Accept(&printer);
}

std::ostream &TreePrettyPrinter::Print(std::ostream &stream) const {
  PrettyPrintTree(root_, context_, &stream);
  return stream;
}

std::ostream &operator<<(std::ostream &stream,
                         const TreePrettyPrinter &printer) {
  return printer.Print(stream);
}

}  // namespace verible
