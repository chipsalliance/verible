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

// Contains a suite of functions for operating on SyntaxTrees

#ifndef VERIBLE_COMMON_TEXT_TREE_UTILS_H_
#define VERIBLE_COMMON_TEXT_TREE_UTILS_H_

#include <cstddef>
#include <functional>
#include <iosfwd>

#include "absl/strings/string_view.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/visitors.h"
#include "common/util/logging.h"
#include "common/util/type_traits.h"

namespace verible {

// Returns the leftmost/rightmost leaf contained in Symbol.
// null_opt is returned if no leaves are found.
// If symbol is a leaf node, then it is its own rightmost/leftmost leaf
// Otherwise, recursively try to find leftmost/rightmost leaf by searching
//   through node's children.
const SyntaxTreeLeaf* GetLeftmostLeaf(const Symbol& symbol);
const SyntaxTreeLeaf* GetRightmostLeaf(const Symbol& symbol);

// Returns the range of text spanned by a Symbol, which could be a subtree.
absl::string_view StringSpanOfSymbol(const Symbol& symbol);

// Variant that takes the left-bound of lsym, and right-bound of rsym.
absl::string_view StringSpanOfSymbol(const Symbol& lsym, const Symbol& rsym);

// Returns a SyntaxTreeNode down_casted from a Symbol.
const SyntaxTreeNode& SymbolCastToNode(const Symbol&);
// Mutable variant.
SyntaxTreeNode& SymbolCastToNode(Symbol&);

// The following no-op overloads allow SymbolCastToNode() to work with zero
// overhead when the argument type is statically known to be the same.
inline const SyntaxTreeNode& SymbolCastToNode(const SyntaxTreeNode& node) {
  return node;
}
inline SyntaxTreeNode& SymbolCastToNode(SyntaxTreeNode& node) { return node; }

// Returns a SyntaxTreeLeaf down_casted from a Symbol.
const SyntaxTreeLeaf& SymbolCastToLeaf(const Symbol&);

// Unwrap layers of only-child nodes until reaching a leaf or a node with
// multiple children.
const Symbol* DescendThroughSingletons(const Symbol& symbol);

// Succeeds if node's enum matches 'node_enum'.
// Returns same node reference, so that anywhere that expects a SyntaxTreeNode
// can be passed CheckNodeEnum(node, node_enum).
template <typename E>
const SyntaxTreeNode& CheckNodeEnum(const SyntaxTreeNode& node,
                                    E expected_node_enum) {
  // Uses operator<<(std::ostream&, E) for diagnostics.
  CHECK_EQ(E(node.Tag().tag), expected_node_enum);
  return node;
}
// Mutable variant.
template <typename E>
SyntaxTreeNode& CheckNodeEnum(SyntaxTreeNode& node, E expected_node_enum) {
  // Uses operator<<(std::ostream&, E) for diagnostics.
  CHECK_EQ(E(node.Tag().tag), expected_node_enum);
  return node;
}

template <typename E>
const SyntaxTreeLeaf& CheckLeafEnum(const SyntaxTreeLeaf& leaf,
                                    E expected_token_enum) {
  // Uses operator<<(std::ostream&, E) for diagnostics.
  CHECK_EQ(E(leaf.get().token_enum()), expected_token_enum);
  return leaf;
}

namespace internal {
template <typename S>
void MustBeCSTSymbolOrNode(S&) {
  typedef typename std::remove_const<S>::type base_type;
  static_assert(std::is_same<base_type, Symbol>::value ||
                    std::is_same<base_type, SyntaxTreeNode>::value,
                "");
}
}  // namespace internal

// Succeeds if symbol is a node enumerated 'node_enum'.
// Returns a casted reference on success.
// Constness is deduced from S and reflected in the return type.
// S can be {const,non-const}x{Symbol,SyntaxTreeNode}.
template <typename E, typename S>
typename match_const<SyntaxTreeNode, S>::type& CheckSymbolAsNode(S& symbol,
                                                                 E node_enum) {
  internal::MustBeCSTSymbolOrNode(symbol);
  return CheckNodeEnum(SymbolCastToNode(symbol), node_enum);
}

// Succeeds if symbol is a leaf enumerated 'leaf_enum'.
// Returns a casted reference on success.
template <typename E>
const SyntaxTreeLeaf& CheckSymbolAsLeaf(const Symbol& symbol, E token_enum) {
  return CheckLeafEnum(SymbolCastToLeaf(symbol), token_enum);
}

// Succeeds if symbol is a node, or nullptr (returning nullptr).
template <typename SPtr>
const SyntaxTreeNode* CheckOptionalSymbolAsNode(const SPtr& symbol) {
  if (symbol == nullptr) return nullptr;
  return &SymbolCastToNode(*symbol);
}

// Succeeds if symbol is nullptr (returning nullptr), or it is a node
// enumerated 'node_enum' (returns casted non-nullptr).
template <typename SPtr, typename E>
const SyntaxTreeNode* CheckOptionalSymbolAsNode(const SPtr& symbol,
                                                E node_enum) {
  if (symbol == nullptr) return nullptr;
  return &CheckSymbolAsNode(*symbol, node_enum);
}

// Specialization for nullptr_t.
template <typename E>
const SyntaxTreeNode* CheckOptionalSymbolAsNode(const std::nullptr_t& symbol,
                                                E) {
  return nullptr;
}

// Succeeds if symbol is nullptr (returning nullptr), or it is a leaf
// enumerated 'token_enum' (returns casted non-nullptr).
template <typename SPtr, typename E>
const SyntaxTreeLeaf* CheckOptionalSymbolAsLeaf(const SPtr& symbol,
                                                E token_enum) {
  if (symbol == nullptr) return nullptr;
  return &CheckSymbolAsLeaf(*symbol, token_enum);
}

// Specialization for nullptr_t.
template <typename E>
const SyntaxTreeLeaf* CheckOptionalSymbolAsLeaf(const std::nullptr_t& symbol,
                                                E) {
  return nullptr;
}

// Extracts a particular child of a node by position, verifying the parent's
// node enumeration.
// S can be {const,non-const}x{Symbol,SyntaxTreeNode}
// constness is deduced from S and reflected in the return type.
template <typename E, typename S>
typename match_const<Symbol, S>::type* GetSubtreeAsSymbol(
    S& symbol, E parent_must_be_node_enum, size_t child_position) {
  internal::MustBeCSTSymbolOrNode(symbol);
  return CheckNodeEnum(SymbolCastToNode(symbol),
                       parent_must_be_node_enum)[child_position]
      .get();
}

// Same as GetSubtreeAsSymbol, but casts the result to a node.
// S can be {const,non-const}x{Symbol,SyntaxTreeNode}
// constness is deduced from S and reflected in the return type.
template <class S, class E>
typename match_const<SyntaxTreeNode, S>::type& GetSubtreeAsNode(
    S& symbol, E parent_must_be_node_enum, size_t child_position) {
  internal::MustBeCSTSymbolOrNode(symbol);
  return SymbolCastToNode(*ABSL_DIE_IF_NULL(
      GetSubtreeAsSymbol(symbol, parent_must_be_node_enum, child_position)));
}

// This variant further checks the returned node's enumeration.
// S can be {const,non-const}x{Symbol,SyntaxTreeNode}
// constness is deduced from S and reflected in the return type.
template <class S, class E>
typename match_const<SyntaxTreeNode, S>::type& GetSubtreeAsNode(
    S& symbol, E parent_must_be_node_enum, size_t child_position,
    E child_must_be_node_enum) {
  internal::MustBeCSTSymbolOrNode(symbol);
  return CheckNodeEnum(
      GetSubtreeAsNode(symbol, parent_must_be_node_enum, child_position),
      child_must_be_node_enum);
}

// Same as GetSubtreeAsSymbol, but casts the result to a leaf.
template <class S, class E>
const SyntaxTreeLeaf& GetSubtreeAsLeaf(const S& symbol,
                                       E parent_must_be_node_enum,
                                       size_t child_position) {
  internal::MustBeCSTSymbolOrNode(symbol);
  return SymbolCastToLeaf(*ABSL_DIE_IF_NULL(
      GetSubtreeAsSymbol(symbol, parent_must_be_node_enum, child_position)));
}

template <class S, class E>
E GetSubtreeNodeEnum(const S& symbol, E parent_must_be_node_enum,
                     size_t child_position) {
  internal::MustBeCSTSymbolOrNode(symbol);
  return static_cast<E>(
      GetSubtreeAsNode(symbol, parent_must_be_node_enum, child_position)
          .Tag()
          .tag);
}

using TreePredicate = std::function<bool(const Symbol&)>;

// Returns the first syntax tree leaf or node that matches the given predicate.
// tree must not be null. Both the tree and the returned tree are intended to
// be mutable.
ConcreteSyntaxTree* FindFirstSubtreeMutable(ConcreteSyntaxTree* tree,
                                            const TreePredicate&);

// Returns the first syntax tree leaf or node that matches the given predicate.
// tree must not be null. This is for non-mutating searches.
const Symbol* FindFirstSubtree(const Symbol*, const TreePredicate&);

// Returns the first syntax tree node whose token starts at or after
// the given first_token_offset, or nullptr if not found.
// tree must not be null.
// If the offset points to the middle of a token, then it will find the
// subtree that starts with the next whole token.
// Nodes without leaves will never be considered because they have no location.
// Both the tree and the returned tree are intended to be mutable.
ConcreteSyntaxTree* FindSubtreeStartingAtOffset(ConcreteSyntaxTree* tree,
                                                const char* first_token_offset);

// Cuts out all nodes and leaves that start at or past the given offset.
// This only looks at leaves' location offsets, and not actual text.
// Any subtree node (in a rightmost position) that becomes empty as the result
// of recursive pruning will also be pruned.
// tree must not be null.
// This will never prune away the root node.
void PruneSyntaxTreeAfterOffset(ConcreteSyntaxTree* tree, const char* offset);

// Returns the pointer to the largest subtree wholly contained
// inside the text range spanned by trim_range.
// tree must not be null.  Tokens outside of this range are discarded.
// If there are multiple eligible subtrees in range, then this chooses the
// first one.
ConcreteSyntaxTree* ZoomSyntaxTree(ConcreteSyntaxTree* tree,
                                   absl::string_view trim_range);

// Same as ZoomSyntaxTree(), except that it modifies 'tree' in-place.
void TrimSyntaxTree(ConcreteSyntaxTree* tree, absl::string_view trim_range);

using LeafMutator = std::function<void(TokenInfo*)>;

// Applies the mutator transformation to every leaf (token) in the syntax tree.
// tree may not be null.
void MutateLeaves(ConcreteSyntaxTree* tree, const LeafMutator& mutator);

//
// Set of tree printing functions
//

// RawSymbolPrinter is good for illustrating the structure of a tree, without
// concern for the interpretation of node/leaf enumerations and token locations.
// Nodes are rendered with proper indendation.
class RawSymbolPrinter : public SymbolVisitor {
 public:
  explicit RawSymbolPrinter(std::ostream* stream) : stream_(stream) {}

  void Visit(const SyntaxTreeLeaf&) override;
  void Visit(const SyntaxTreeNode&) override;

 protected:
  // Output stream.
  std::ostream* stream_;

  // Indentation tracks current depth in tree.
  int indent_ = 0;

  // Each set of siblings is enumerated starting at 0.
  // This is set by parent nodes during traversal.
  int child_rank_ = 0;

  // Prints start of line with correct indentation.
  std::ostream& auto_indent();
};

// Streamable print adapter using RawSymbolPrinter.
// Usage: stream << RawTreePrinter(*tree_root);
class RawTreePrinter {
 public:
  explicit RawTreePrinter(const Symbol& root) : root_(root) {}

  std::ostream& Print(std::ostream&) const;

 private:
  const Symbol& root_;
};

std::ostream& operator<<(std::ostream&, const RawTreePrinter&);

// Tree printer that includes details about token text byte offsets relative
// to a given string buffer base, and using an enum translator.
class PrettyPrinter : public RawSymbolPrinter {
 public:
  PrettyPrinter(std::ostream* stream, const TokenInfo::Context& context)
      : RawSymbolPrinter(stream), context_(context) {}

  void Visit(const SyntaxTreeLeaf&) override;

 protected:
  // Range of text spanned by syntax tree, used for offset calculation.
  const TokenInfo::Context context_;
};

// Prints tree contained at root to stream
void PrettyPrintTree(const Symbol& root, const TokenInfo::Context& context,
                     std::ostream* stream);

// Streamable tree printing class.
// Usage: stream << TreePrettyPrinter(*tree_root, context);
class TreePrettyPrinter {
 public:
  TreePrettyPrinter(const Symbol& root, const TokenInfo::Context& context)
      : root_(root), context_(context) {}

  std::ostream& Print(std::ostream&) const;

 private:
  const Symbol& root_;
  const TokenInfo::Context& context_;
};

std::ostream& operator<<(std::ostream&, const TreePrettyPrinter&);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_UTILS_H_
