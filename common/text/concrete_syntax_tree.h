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

// ConcreteSyntaxTree represents the structure of a body of text.
//
// This header also provides the following (owernship-transferring) functions
// for constructing syntax trees in semantic action blocks:
//
//   $$ = MakeNode($1, $2, ...);
//   $$ = MakeTaggedNode(kTag, $1, $2, ...);
//   $$ = ExtendNode($1, $2, ...);
//   $$ = MakeNode($1, ForwardChildren($2), $3, ...);
//
// As ownership is transferred exclusively, the pointers left behind are
// null as a result.
//
// These functions are intended for use only in <language>.yc semantic actions.
//
// The std::move is automated for the sake of easy tree building.
// Without the automation, the user would have to write:
//   $$ = MakeNode(std::move($1), std::move($2), ...);
// which would be less readable than the above.

#ifndef VERIBLE_COMMON_TEXT_CONCRETE_SYNTAX_TREE_H_
#define VERIBLE_COMMON_TEXT_CONCRETE_SYNTAX_TREE_H_

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "common/text/constants.h"
#include "common/text/symbol.h"
#include "common/text/tree_compare.h"
#include "common/text/visitors.h"
#include "common/util/casts.h"
#include "common/util/logging.h"

namespace verible {

// Using unique_ptr in the symbol stack requires careful moving.
using SymbolPtr = std::unique_ptr<Symbol>;

// Currently, a tree *is* a tree-node, but this may change in the future.
// Treat this as an opaque type.
using ConcreteSyntaxTree = SymbolPtr;

// Helper class for transferring ownership of children, which is
// used as an overload to SyntaxTreeNode::AppendChild.
// This takes over ownership of the symbol pointer.
//    $$ = MakeNode($1, $2, ForwardChildren($3), $4);
struct ForwardChildren {
  explicit ForwardChildren(SymbolPtr& symbol) : node(std::move(symbol)) {}
  SymbolPtr node;
};

// SyntaxTreeNode is a language-agnostic node structure, supporting an
// arbitrary number of children.  The 'tag' field is a node type enumeration
// used by various language front-ends.
class SyntaxTreeNode : public Symbol {
 public:
  explicit SyntaxTreeNode(const int tag = kUntagged) : tag_(tag), children_() {}

  const std::vector<SymbolPtr>& children() const { return children_; }
  std::vector<SymbolPtr>& mutable_children() { return children_; }

  // Transfer ownership of argument to this object.
  // Call MakeNode or ExtendNode instead of calling this directly.
  void AppendChild(SymbolPtr child) { children_.push_back(std::move(child)); }

  // Transfer ownership of argument's children to this object.
  // Call MakeNode or ExtendNode instead of calling this directly.
  // If node is actually a leaf, just append the leaf.
  void AppendChild(ForwardChildren forwarded_children) {
    if (forwarded_children.node == nullptr) return;
    auto* node = dynamic_cast<SyntaxTreeNode*>(forwarded_children.node.get());
    if (node == nullptr) {
      // Could be a SyntaxTreeLeaf, for instance.
      children_.push_back(std::move(forwarded_children.node));
      return;
    }
    const auto new_size = children_.size() + node->children_.size();
    children_.reserve(new_size);
    for (auto& child : node->children_) {
      children_.push_back(std::move(child));
    }
    // Remove all the vacated children slots left in the parent.
    node->children_.clear();
  }

  // This no-op case is the base case for the variadic Append.
  void Append() const {}

  // Ownership of all arguments is transferred to this object.
  // Call MakeNode or ExtendNode instead of calling Append directly.
  template <typename T, typename... Args>
  void Append(T&& t, Args&&... args) {
    AppendChild(std::move(t));            // Append the first.
    Append(std::forward<Args>(args)...);  // Append the rest.
  }

  // Children accessor (mutable).
  SymbolPtr& operator[](const size_t i);

  // Children accessor (const).
  const SymbolPtr& operator[](const size_t i) const;

  // Compares this node to an arbitrary symbol using the compare_tokens
  // function.
  bool equals(const Symbol* symbol,
              const TokenComparator& compare_tokens) const override;

  // Compares this node to another node.
  // Checks for recursive equality among all children of both nodes.
  bool equals(const SyntaxTreeNode* node,
              const TokenComparator& compare_tokens) const;

  // Uses passed TreeVisitorRecursive to visit all children recursively,
  // then visit itself.
  void Accept(TreeVisitorRecursive* visitor) const override;
  void Accept(MutableTreeVisitorRecursive* visitor,
              SymbolPtr* this_owned) override;

  // Accepting a symbol visitor does not recursively visit children.
  void Accept(SymbolVisitor* visitor) const override;

  // Method override that returns the Kind of Symbol
  SymbolKind Kind() const override { return SymbolKind::kNode; }
  SymbolTag Tag() const override { return NodeTag(tag_); }

  // MatchesTag returns true if the tag value matches the argument.
  // This is designed to work with any enumeration type.
  template <typename EnumType>
  bool MatchesTag(EnumType e) const {
    return tag_ == static_cast<int>(e);
  }

  // TODO(fangism): when performance is needed, use a flat_set.
  template <typename EnumType>
  bool MatchesTagAnyOf(std::initializer_list<EnumType> enums) const {
    // Linear search.
    auto iter = std::find(enums.begin(), enums.end(), EnumType(tag_));
    return iter != enums.end();
  }

 private:
  // This tag would really prefer to be a language-specific node enumeration
  // type, but that would (IMHO) create unecessary templating.
  // Decision: Keep this a generic int.
  int tag_;

  // Sequence of pointers to subtrees and nodes.
  std::vector<SymbolPtr> children_;
};

// The following functions are intended for use in semantic action blocks
// in yacc/bison grammar files (.yc).

// Construct a syntax tree node with a tag.
// Ownership of all args is transferred, and consumed by the new node.
// Sample usage: $$ = MakeNode(TAG, $1, $2, $3);
template <typename... Args>
SymbolPtr MakeNode(Args&&... args) {
  std::unique_ptr<SyntaxTreeNode> node_pointer(new SyntaxTreeNode);
  node_pointer->Append(std::forward<Args>(args)...);
  return std::move(node_pointer);
}

// Construct a syntax tree node with a tag.
// Ownership of all args is transferred, and consumed by the new node.
// Sample usage:
//   $$ = MakeTaggedNode(TAG);  // empty, no children
//   $$ = MakeTaggedNode(TAG, $1, $2, $3);
template <typename Enum, typename... Args>
SymbolPtr MakeTaggedNode(const Enum tag, Args&&... args) {
  std::unique_ptr<SyntaxTreeNode> node_pointer(
      new SyntaxTreeNode(static_cast<int>(tag)));
  node_pointer->Append(std::forward<Args>(args)...);
  return std::move(node_pointer);
}

// Extend the children of an existing node.
// Equivalent to: $$ = std::move(ExtendNode($1, $2, $3));
// Ownership of all args is transferred, and consumed by the existing node.
// $1 is transferred to $$.
// Sample usage: $$ = ExtendNode($1, $2, $3);
template <typename T, typename... Args>
SymbolPtr ExtendNode(T&& list_ptr, Args&&... args) {
  return _ExtendNodeMoved(std::move(list_ptr), std::forward<Args>(args)...);
}

// Implementation detail, call ExtendNode instead for automatic std::move.
template <typename... Args>
SymbolPtr _ExtendNodeMoved(SymbolPtr list_ptr, Args&&... args) {
  CHECK(list_ptr->Kind() == SymbolKind::kNode);
  SyntaxTreeNode* node_pointer = down_cast<SyntaxTreeNode*>(list_ptr.get());
  node_pointer->Append(std::forward<Args>(args)...);
  return list_ptr;
}

// Helper function to deal with move semantics and argument forwarding
void SetChild_(const SymbolPtr& parent, int child_index, SymbolPtr new_child);

// Sets the child at child_index of parent to new_child.
// SetChild will crash when:
//   Child_index is out of range
//   Parent either a leaf or a nullptr
//   Preexisting data at target index is not a nullptr
// Equivalent to: parent->children[child_index] = std::move(new_child);
template <typename T>
void SetChild(const SymbolPtr& parent, int child_index, T&& new_child) {
  SetChild_(parent, child_index, std::move(new_child));
}

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_CONCRETE_SYNTAX_TREE_H_
