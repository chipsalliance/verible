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

#ifndef VERIBLE_COMMON_UTIL_VECTOR_TREE_H_
#define VERIBLE_COMMON_UTIL_VECTOR_TREE_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iosfwd>  // IWYU pragma: keep
#include <iterator>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

#include "common/util/container_proxy.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"

namespace verible {

// This helper class contains private template functions for many pairs
// of const/mutable methods of VectorTree.  This reduces the amount of code
// duplication for similar methods.
// In all template functions, type TP is either 'VectorTree' or
// 'const VectorTree', and is inferrable from template argument deduction.
// Recursive calls automatically call the correct variant.
class _VectorTreeImpl {
 protected:
  // Returns pointer root node, following ancestry chain.
  template <typename TP>
  static TP* _Root(TP* node) {
    while (node->Parent() != nullptr) {
      node = node->Parent();
    }
    return node;
  }

  // Returns pointer to next sibling if it exists, else nullptr.
  template <typename TP>
  static TP* _NextSibling(TP* node) {
    if (node->Parent() == nullptr) {
      return nullptr;
    }
    const size_t birth_rank = node->BirthRank();
    const size_t next_rank = birth_rank + 1;
    if (next_rank == node->Parent()->Children().size()) {
      return nullptr;  // This is the last child of the Parent().
    }
    // More children follow this one.
    return &node->Parent()->Children()[next_rank];
  }

  // Returns pointer to previous sibling if it exists, else nullptr.
  template <typename TP>
  static TP* _PreviousSibling(TP* node) {
    if (node->Parent() == nullptr) {
      return nullptr;
    }
    const size_t birth_rank = node->BirthRank();
    if (birth_rank == 0) {
      return nullptr;
    }
    // More children precede this one.
    return &node->Parent()->Children()[birth_rank - 1];
  }

  // Descend through children using indices specified by iterator range.
  // Iter type dereferences to an integral value.
  // This works on any internal node, not just the root.
  template <typename TP, typename Iter>
  static TP& _DescendPath(TP* node, Iter start, Iter end) {
    for (; start != end; ++start) {
      auto& children = node->Children();
      node = &children[*start];  // descend
    }
    return *node;
  }

  // Returns the node reached by descending through Children().front().
  template <typename TP>
  static TP* _LeftmostDescendant(TP* node) {
    while (!node->is_leaf()) {
      node = &node->Children().front();
    }
    return node;
  }

  // Returns the node reached by descending through Children().back().
  template <typename TP>
  static TP* _RightmostDescendant(TP* node) {
    while (!node->is_leaf()) {
      node = &node->Children().back();
    }
    return node;
  }

  // Navigates to the next leaf (node without Children()) in the tree
  // (if it exists), else returns nullptr.
  template <typename TP>
  static TP* _NextLeaf(TP* node) {
    auto* parent = node->Parent();
    if (parent == nullptr) {
      // Root node has no next sibling, this is the end().
      return nullptr;
    }

    // Find the next sibling, if there is one.
    auto& siblings = parent->Children();
    const size_t birth_rank = node->BirthRank();
    const size_t next_rank = birth_rank + 1;
    if (next_rank != siblings.size()) {
      // More children follow this one.
      return siblings[next_rank].LeftmostDescendant();
    }

    // This is the last child of the group.
    // Find the nearest parent that has a next child (ascending).
    // TODO(fangism): rewrite without recursion
    auto* next_ancestor = parent->NextLeaf();
    if (next_ancestor == nullptr) return nullptr;

    // next_ancestor is the NearestCommonAncestor() to the original
    // node and the resulting node.
    return next_ancestor->LeftmostDescendant();
  }

  // Navigates to the previous leaf (node without Children()) in the tree
  // (if it exists), else returns nullptr.
  template <typename TP>
  static TP* _PreviousLeaf(TP* node) {
    auto* parent = node->Parent();
    if (parent == nullptr) {
      // Root node has no previous sibling, this is the reverse-end().
      return nullptr;
    }

    // Find the next sibling, if there is one.
    auto& siblings = parent->Children();
    const size_t birth_rank = node->BirthRank();
    if (birth_rank > 0) {
      // More children precede this one.
      return siblings[birth_rank - 1].RightmostDescendant();
    }

    // This is the first child of the group.
    // Find the nearest parent that has a previous child (descending).
    // TODO(fangism): rewrite without recursion
    auto* prev_ancestor = parent->PreviousLeaf();
    if (prev_ancestor == nullptr) return nullptr;

    // prev_ancestor is the NearestCommonAncestor() to the original
    // node and the resulting node.
    return prev_ancestor->RightmostDescendant();
  }

  // Returns the nearest common ancestor node to two nodes if a common ancestor
  // exists, else nullptr.
  // Run time: let L and R be the number of ancestors of left and right,
  // respectively.  In the worst case, there will be L and R set insertions,
  // so O(L lg L) + I(R lg R) and L and R membership checks, so O(L lg R) + O(R
  // lg L).  With K = max(L, R), overall this is no worse than O(K lg K).
  template <typename TP>
  static TP* _NearestCommonAncestor(TP* left, TP* right) {
    std::set<TP*> left_ancestors, right_ancestors;
    // In alternation, insert left/right into its respective set of ancestors,
    // and check for membership in the other ancestor set.
    // Return as soon as one is found in the other's set of ancestors.
    while (left != nullptr || right != nullptr) {
      if (left != nullptr) {
        left_ancestors.insert(left);
        if (right_ancestors.find(left) != right_ancestors.end()) {
          return left;
        }
        left = left->Parent();
      }
      if (right != nullptr) {
        right_ancestors.insert(right);
        if (left_ancestors.find(right) != left_ancestors.end()) {
          return right;
        }
        right = right->Parent();
      }
    }
    // Once this point is reached, there are no common ancestors.
    return nullptr;
  }
};

namespace vector_tree_internal {

// A wrapper of a sequence container for storing VectorTree nodes that sets
// correct parent pointer in each inserted node. The reference to the "correct"
// parent is passed to a constructor.
//
// The sole purpose of this class is to function as a children list in
// VectorTree, both as a storage and as a public interface for tree manipulation
// (through reference).
//
// This class handles parent pointer assignment for all cases where the children
// list itself is modified. However, it does not clear or otherwise change
// parent pointer in removed nodes.
template <class Container>
class VectorTreeChildrenList
    : ContainerProxyBase<VectorTreeChildrenList<Container>, Container> {
  using Base = ContainerProxyBase<VectorTreeChildrenList<Container>, Container>;
  friend Base;

  using ThisType = VectorTreeChildrenList<Container>;

  using VectorTreeType = typename Container::value_type;
  friend VectorTreeType;

 public:
  using typename Base::container_type;

  // Sequence Container interface

  using typename Base::value_type;

  using typename Base::const_reference;
  using typename Base::reference;

  using typename Base::const_iterator;
  using typename Base::iterator;

  using typename Base::difference_type;
  using typename Base::size_type;

  using typename Base::const_reverse_iterator;
  using typename Base::reverse_iterator;

  using Base::begin;
  using Base::cbegin;
  using Base::cend;
  using Base::end;

  using Base::crbegin;
  using Base::crend;
  using Base::rbegin;
  using Base::rend;

  using Base::back;
  using Base::front;
  using Base::operator[];
  using Base::at;

  using Base::empty;
  using Base::max_size;
  using Base::size;

  using Base::emplace_back;
  using Base::push_back;

  using Base::emplace_front;
  using Base::push_front;

  using Base::emplace;
  using Base::insert;

  using Base::clear;
  using Base::erase;
  using Base::pop_back;
  using Base::pop_front;

  using Base::assign;
  using Base::operator=;
  using Base::swap;

  using Base::capacity;
  using Base::reserve;
  using Base::resize;

 protected:
  // ContainerProxy interface

  container_type& underlying_container() { return container_; }
  const container_type& underlying_container() const { return container_; }

  void ElementsInserted(iterator first, iterator last) {
    LinkChildrenToParent(iterator_range(first, last));
  }

  // Unused:
  // void ElementsBeingRemoved(iterator first, iterator last)

  // Unused:
  // void ElementsBeingReplaced()

  void ElementsWereReplaced() { LinkChildrenToParent(container_); }

 private:
  // Sets parent pointer of nodes from `children` range to address of `node_`.
  template <class Range>
  void LinkChildrenToParent(Range&& children) {
    for (auto& child : children) {
      child.parent_ = &node_;
    }
  }

  // Hide constructors and assignments from the world. This object is created
  // and assigned-to only in VectorTree.

  explicit VectorTreeChildrenList(VectorTreeType& node) : node_(node) {}

  // Construction requires parent node reference.
  VectorTreeChildrenList(const VectorTreeChildrenList&) = delete;

  VectorTreeChildrenList(VectorTreeType& node,
                         const VectorTreeChildrenList& other)
      : node_(node), container_(other.container_) {
    LinkChildrenToParent(container_);
  }

  VectorTreeChildrenList& operator=(const VectorTreeChildrenList& other) {
    container_ = other.container_;
    LinkChildrenToParent(container_);
    return *this;
  }

  // Construction requires parent node reference.
  VectorTreeChildrenList(VectorTreeChildrenList&&) = delete;

  VectorTreeChildrenList(VectorTreeType& node,
                         VectorTreeChildrenList&& other) noexcept
      : node_(node), container_(std::move(other.container_)) {
    // Note: `other` is not notified about the change because it ends up in
    // undefined state as a result of the move.
    LinkChildrenToParent(container_);
  }

  VectorTreeChildrenList& operator=(VectorTreeChildrenList&& other) noexcept {
    // Note: `other` is not notified about the change because it ends up in
    // undefined state as a result of the move.
    container_ = std::move(other.container_);
    LinkChildrenToParent(container_);
    return *this;
  }

  // Reference to a VectorTree node in which this object represents a list of
  // children.
  VectorTreeType& node_;

  // Actual data container where the nodes are stored.
  Container container_;
};

}  // namespace vector_tree_internal

// VectorTree is a hierarchical representation of information.
// While it may be useful to maintain some invariant relationship between
// parents and children nodes, it is not required for this class.
// The traversal methods ApplyPreOrder/ApplyPostOrder could be used to
// maintain or verify parent-child invariants.
// The VectorTree class is itself also the same as the internally used node
// class; i.e. there is no separate node class.
//
// Example applications (with some parent-child invariant relationship):
//
// * Range interval tree -- A numeric range from [0:N] could be sub-divided
//   into smaller ranges [0:k], [k:N] for some 0 < k < N, or multiple
//   monotonically increasing k's.  This class is a generalization of an
//   interval tree concept.
//
// * Lexical tokens output -- some tokens could be further tokenized or
//   expanded, but the choice of view depends on consumer and application.
//   This can be useful for embedding snippets of one language within another;
//   further lexing/parsing of an unexpanded substring can be deferred.
//
// * Lexical token range partitions -- token sub-range partitioning is a
//   critical step in a formatting strategy; the decision to partition
//   a particular range may be difficult to determine statically, so it
//   is best left undecided until a later heuristic pass.
//
// Construction:
// This implementation gives the user the liberty to construct the tree
// nodes in any order and any manner.  A top-down construction may guarantee
// that a parent is well-formed before creating its children, whereas a
// bottom-up construction completes creation of children before the enveloping
// parent.
template <typename T>
class VectorTree : private _VectorTreeImpl {
  typedef VectorTree<T> this_type;
  typedef _VectorTreeImpl impl_type;

 public:
  using VectorTreeChildrenList =
      vector_tree_internal::VectorTreeChildrenList<std::vector<this_type>>;
  friend VectorTreeChildrenList;

  // Self-recursive type that represents children in an expanded view.
  typedef std::vector<this_type> subnodes_type;
  typedef T value_type;

  VectorTree() : children_(*this) {}

  // Deep copy-constructor.
  VectorTree(const this_type& other)
      : node_value_(other.node_value_),
        parent_(other.parent_),
        children_(*this, other.children_) {}

  VectorTree(this_type&& other) noexcept
      : node_value_(std::move(other.node_value_)),
        parent_(other.parent_),
        children_(*this, std::move(other.children_)) {}

  // This constructor can be used to recursively build trees.
  // e.g.
  //   // looks like function-call, but just invokes constructor:
  //   typedef VectorTree<Foo> FooNode;
  //   auto foo_tree = FooNode({value-initializer},
  //        FooNode({value-initializer}, /* children nodes... */ ),
  //        FooNode({value-initializer}, /* children nodes... */ )
  //   );
  template <typename... Args>
  explicit VectorTree(const value_type& v, Args&&... args)
      : node_value_(v), children_(*this) {
    AdoptSubtree(std::forward<Args>(args)...);
  }

  template <typename... Args>
  explicit VectorTree(value_type&& v, Args&&... args)
      : node_value_(std::move(v)), children_(*this) {
    children_.reserve(sizeof...(args));
    (children_.emplace_back(std::forward<Args>(args)), ...);
  }

  ~VectorTree() { CHECK(CheckIntegrity()); }

  // Swaps values and subtrees of two nodes.
  // This operation is safe for unrelated trees (no common ancestor).
  // This operation is safe when the two nodes share a common ancestor,
  // excluding the case where one node is a direct ancestor of the other.
  // TODO(fangism): Add a proper check for this property, and test.
  void swap(this_type& other) {
    std::swap(node_value_, other.node_value_);
    children_.swap(other.children_);  // efficient O(1) vector::swap
                                      // + O(|children|) linking to parent
  }

  // Copy value and children, but relink new children to this node.
  VectorTree& operator=(const this_type& source) {
    if (this == &source) return *this;
    node_value_ = source.node_value_;
    children_ = source.children_;
    return *this;
  }

  // Explicit move-assignability needed for vector::erase()
  // No need to change parent links when children keep same parent.
  VectorTree& operator=(this_type&& source) noexcept {
    node_value_ = std::move(source.node_value_);
    children_ = std::move(source.children_);
    return *this;
  }

  // Hint the implementation how many children are to be expected.
  void SetExpectedChildrenUpperBound(size_t s) { children_.reserve(s); }

  // Builders

  // Appends a new child node to the tree at this level.
  // 'this' node is the parent of the new child.
  // Returns a pointer to the newly added child.
  // This invalidates previous iterators/pointers to sibling children.
  template <typename... Args>
  this_type* NewChild(Args&&... args) {
    Children().emplace_back(std::forward<Args>(args)...);
    return &Children().back();
  }

  // Appends a new child node to the parent of this node.
  // Returns a pointer to the newly added sibling.
  // This invalidates previous iterators/pointers to sibling children.
  template <typename... Args>
  this_type* NewSibling(Args&&... args) {
    return ABSL_DIE_IF_NULL(parent_)->NewChild(std::forward<Args>(args)...);
  }

  // Appends one or more sub-trees at this level.
  // Variadic template handles one argument at a time.
  // This invalidates previous iterators/pointers to sibling children.
  template <typename... AdoptedNodeN>
  void AdoptSubtree(AdoptedNodeN&&... node_n) {
    children_.reserve(children_.size() + sizeof...(node_n));
    (children_.push_back(std::forward<AdoptedNodeN>(node_n)), ...);
  }

  // This node takes/moves subtrees from another node (concatenates).
  // There need not be any relationship between this node and the other.
  void AdoptSubtreesFrom(this_type* other) {
    auto& src_children = other->children_;
    children_.reserve(children_.size() + src_children.size());
    for (auto& src_child : src_children) {
      children_.push_back(std::move(src_child));
    }
    other->children_.clear();
  }

  // Accessors

  T& Value() { return node_value_; }

  const T& Value() const { return node_value_; }

  this_type* Parent() { return parent_; }

  const this_type* Parent() const { return parent_; }

  VectorTreeChildrenList& Children() { return children_; }

  const VectorTreeChildrenList& Children() const { return children_; }

  bool is_leaf() const { return children_.empty(); }

  // Properties

  // Returns the index of this node relative to parent's children.
  // An only-child or first-child will have birth rank 0.
  size_t BirthRank() const {
    if (parent_ != nullptr) {
      // Parent() must have Children(); this is one of them.
      // Storage of siblings must be contiguous, so that pointer
      // distance translates to index.
      return std::distance(&Parent()->Children().front(), this);
    }
    return 0;
  }

  // Returns the number of parents between this node and the root.
  size_t NumAncestors() const {
    size_t depth = 0;
    for (const this_type* iter = Parent(); iter != nullptr;
         iter = iter->Parent()) {
      ++depth;
    }
    return depth;
  }

  // Returns true if 'other' is an ancestor of this node, in other words,
  // this node is descended from 'other'.
  // This method could have been named IsDescendedFrom().
  // nullptr is never considered an ancestor of any node.
  // 'this' node is not considered an ancestor of itself.
  bool HasAncestor(const this_type* other) const {
    if (other == nullptr) return false;
    for (const this_type* iter = Parent(); iter != nullptr;
         iter = iter->Parent()) {
      if (iter == other) return true;
    }
    return false;
  }

  // Returns pointer to the tree root, the greatest ancestor of this node.
  const this_type* Root() const { return impl_type::_Root(this); }

  // Returns mutable pointer to the tree root.
  this_type* Root() { return impl_type::_Root(this); }

  // Returns the closest common ancestor to this and the other, else nullptr.
  // This is the const pointer overload.
  const this_type* NearestCommonAncestor(const this_type* other) const {
    return impl_type::_NearestCommonAncestor(this, other);
  }

  // Returns the closest common ancestor to this and the other, else nullptr.
  // This is the mutable pointer overload.
  this_type* NearestCommonAncestor(this_type* other) {
    return impl_type::_NearestCommonAncestor(this, other);
  }

  // Construct a path of BirthRank()s from root to this.
  // Root node's 'path' is empty.  Passing the resulting path to
  // root.DescendPath() gets you back to this node.
  // PathType can be any container with a push_back() interface.
  template <class PathType>
  void Path(PathType& path) const {
    if (Parent() != nullptr) {
      Parent()->Path(path);
      path.push_back(BirthRank());
    }
  }

  // Descend through children using indices specified by iterator range.
  // Iter type dereferences to an integral value.
  // This works on any internal node, not just the root.
  template <class Iter>
  const this_type& DescendPath(Iter start, Iter end) const {
    return impl_type::_DescendPath(this, start, end);
  }

  // Returns the next sibling node if it exists, else nullptr.
  const this_type* NextSibling() const { return impl_type::_NextSibling(this); }

  // Returns the next sibling node if it exists, else nullptr.
  this_type* NextSibling() { return impl_type::_NextSibling(this); }

  // Returns the previous sibling node if it exists, else nullptr.
  const this_type* PreviousSibling() const {
    return impl_type::_PreviousSibling(this);
  }

  // Returns the previous sibling node if it exists, else nullptr.
  this_type* PreviousSibling() { return impl_type::_PreviousSibling(this); }

  // Mutable variant of DescendPath.
  template <class Iter>
  this_type& DescendPath(Iter start, Iter end) {
    return impl_type::_DescendPath(this, start, end);
  }

  // Returns the node reached by descending through Children().front().
  const this_type* LeftmostDescendant() const {
    return impl_type::_LeftmostDescendant(this);
  }

  // Returns the node reached by descending through Children().front(), mutable
  // variant.
  this_type* LeftmostDescendant() {
    return impl_type::_LeftmostDescendant(this);
  }

  // Returns the node reached by descending through Children().back().
  const this_type* RightmostDescendant() const {
    return impl_type::_RightmostDescendant(this);
  }

  // Returns the node reached by descending through Children().back(), mutable
  // variant.
  this_type* RightmostDescendant() {
    return impl_type::_RightmostDescendant(this);
  }

  // Returns true if this node has no parent, or it has BirthRank 0.
  bool IsFirstChild() const {
    if (Parent() == nullptr) return true;
    return BirthRank() == 0;
  }

  // Returns true if this node has no parent, or it is the last child of its
  // parent.
  bool IsLastChild() const {
    if (Parent() == nullptr) return true;
    return BirthRank() == Parent()->Children().size() - 1;
  }

  // Navigates to the next leaf (node without Children()) in the tree
  // (if it exists), else returns nullptr.
  const this_type* NextLeaf() const { return impl_type::_NextLeaf(this); }

  // Mutable variant of NextLeaf().
  this_type* NextLeaf() { return impl_type::_NextLeaf(this); }

  // Navigates to the previous leaf (node without Children()) in the tree
  // (if it exists), else returns nullptr.
  const this_type* PreviousLeaf() const {
    return impl_type::_PreviousLeaf(this);
  }

  // Mutable variant of PreviousLeaf().
  this_type* PreviousLeaf() { return impl_type::_PreviousLeaf(this); }

  // Removes this node from its parent, and shifts ths siblings that follow this
  // node lower in BirthRank().
  // Any iterators that pointed to this node or its later siblings are
  // invalidated.
  // This node is destroyed in the process.
  // This operation is only valid on non-root nodes.
  // It is the caller's responsibility to maintain invariants before
  // destroying this node.
  void RemoveSelfFromParent() {
    auto& siblings = ABSL_DIE_IF_NULL(Parent())->Children();
    auto self_iter = siblings.begin() + BirthRank();
    CHECK_EQ(&*self_iter, this);
    siblings.erase(self_iter);
  }

  // TODO(fangism): provide unidirectional iterator views, forward and reversed,
  // using NextLeaf() and PreviousLeaf().

  // Returns true if parent-child links are valid in entire tree.
  bool CheckIntegrity() const {
    for (const auto& child : children_) {
      CHECK_EQ(child.Parent(), this)
          << "Inconsistency: child's parent does not point back to this node!";
      if (!child.CheckIntegrity()) return false;
    }
    return true;
  }

  // Function-application traversals.
  //   * Apply in pre-order vs. post-order.
  //   * Apply to attached value at each node vs. the whole node itself.
  //     Whole node gives access to immediate children (and subtree).
  //   * Apply function is const vs. mutating.
  // All combinations of the above are provided.

  // Visits all tree nodes in pre-order traversal applying function to all nodes
  // (non-modifying).  Useful for checking invariants between parents and
  // their children.
  void ApplyPreOrder(const std::function<void(const this_type&)>& f) const {
    f(*this);
    for (const auto& child : Children()) {
      child.ApplyPreOrder(f);
    }
  }

  // This variant of ApplyPreOrder expects a function on the underlying
  // value_type (const&).
  void ApplyPreOrder(const std::function<void(const value_type&)>& f) const {
    ApplyPreOrder([&f](const this_type& t) { f(t.Value()); });
  }

  // Visits all tree nodes in pre-order traversal applying function to all nodes
  // (modifying).
  void ApplyPreOrder(const std::function<void(this_type&)>& f) {
    f(*this);
    for (auto& child : Children()) {
      child.ApplyPreOrder(f);
    }
  }

  // Visits all tree nodes in pre-order traversal applying function to all node
  // values (modifying).  Useful for applying transformations.
  void ApplyPreOrder(const std::function<void(value_type&)>& f) {
    ApplyPreOrder([&f](this_type& t) { f(t.Value()); });
  }

  // Visits all tree nodes in post-order traversal applying function to all
  // nodes (non-modifying).  Useful for checking invariants between parents and
  // their children.
  void ApplyPostOrder(const std::function<void(const this_type&)>& f) const {
    for (const auto& child : Children()) {
      child.ApplyPostOrder(f);
    }
    f(*this);
  }

  // This variant of ApplyPostOrder expects a function on the underlying
  // value_type (const&).
  void ApplyPostOrder(const std::function<void(const value_type&)>& f) const {
    ApplyPostOrder([&f](const this_type& t) { f(t.Value()); });
  }

  // Visits all tree nodes in post-order traversal applying function to all
  // nodes (modifying).  Useful for applying transformations.
  void ApplyPostOrder(const std::function<void(this_type&)>& f) {
    for (auto& child : Children()) {
      child.ApplyPostOrder(f);
    }
    f(*this);
  }

  // Visits all tree nodes in post-order traversal applying function to all node
  // values (modifying).  Useful for applying transformations.
  void ApplyPostOrder(const std::function<void(value_type&)>& f) {
    ApplyPostOrder([&f](this_type& t) { f(t.Value()); });
  }

  // Recursively transform a VectorTree<T> to VectorTree<S>.
  // This cannot be expressed as a constructor because template constructor
  // calls must be type-deducible from its constructor args, and there's no
  // way to explicitly invoke a constructor with specific template args.
  // The resulting tree is always StructureEqual() to the original tree.
  //
  // Usage:
  //   auto tree_other = orig_tree.Transform<OtherType>(
  //       [](const VectorTree<OrigType>& node) { return ... });
  template <typename S>
  VectorTree<S> Transform(
      const std::function<S(const this_type& node)>& f) const {
    VectorTree<S> return_tree(f(*this));  // sets Value()
    return_tree.SetExpectedChildrenUpperBound(Children().size());
    // Construct children subtree.
    for (const auto& child : Children()) {
      return_tree.AdoptSubtree(child.template Transform<S>(f));
    }
    return return_tree;  // Rely on copy-elision.
  }

  // If this node has exactly one child, replace this node with that child
  // and return true, otherwise, do nothing and return false.
  bool HoistOnlyChild() {
    if (Children().size() != 1) return false;

    auto& only = Children().front();
    node_value_ = std::move(only.node_value_);
    // Can't do this directly, as assignment to children_ destroys its child
    // (`only`) before it is moved.
    auto new_children = std::move(only.children_.container_);
    children_ = std::move(new_children);

    return true;
  }

  // Combines the Nth and (N+1) sibling using a custom function 'joiner' on the
  // nodes' values, and the Nth sibling will adopt N+1's children.
  // The 'joiner' function does: *left = f(*left, *right);
  // The (N+1) sibling will be erased in the process, and every sibling
  // thereafter will be shifted back one position (same inefficiency as shifting
  // vector contents).  This invalidates all iterators after position N,
  // and iterators to the Nth node's children (possible realloc).
  void MergeConsecutiveSiblings(
      size_t N, std::function<void(value_type*, const value_type&)> joiner) {
    CHECK_LT(N + 1, children_.size());

    // Combine value into node[N].
    joiner(&Children()[N].node_value_, Children()[N + 1].node_value_);

    // Move-concatenate children to node[N].
    const auto next_iter = children_.begin() + N + 1;
    Children()[N].AdoptSubtreesFrom(&*next_iter);

    // Shift-left children_ by 1 beyond N.
    children_.erase(next_iter);  // done via move-assignment
  }

  // Replace all direct children of this node with concatenated grandchildren.
  // Retains the value of this node.  Discards direct childrens' values.
  void FlattenOnce() {
    const int grandchildren_count = std::transform_reduce(
        children_.begin(), children_.end(), 0, std::plus<>(),
        [](const this_type& gc) { return gc.Children().size(); });

    // Build new children list in a standalone vector, then move-assign it to
    // this node's children vector.
    subnodes_type grandchildren;
    grandchildren.reserve(grandchildren_count);

    for (auto& child : children_) {
      for (auto& grandchild : child.children_) {
        grandchildren.push_back(std::move(grandchild));
      }
    }
    children_ = std::move(grandchildren);
  }

  // For every child, if that child has grandchildren, replace that child with
  // its grandchildren, else preserve that child.
  // If new_offsets is provided, populate that array with indices into the
  // resulting children that correspond to the start locations of the original
  // children's children.  This lets the caller reference and operate on
  // subranges of the original set of grandchildren.
  void FlattenOnlyChildrenWithChildren(
      std::vector<size_t>* new_offsets = nullptr) {
    const int new_children_count = std::transform_reduce(
        children_.begin(), children_.end(), 0, std::plus<>(),
        [](const this_type& gc) {
          return std::max<size_t>(gc.Children().size(), 1u);
        });

    // Build new children list in a standalone vector, then move-assign it to
    // this node's children vector.
    subnodes_type new_children;
    new_children.reserve(new_children_count);

    if (new_offsets != nullptr) {
      new_offsets->clear();
      new_offsets->reserve(children_.size());
    }

    size_t new_index = 0;
    for (auto& child : children_) {
      if (new_offsets) new_offsets->push_back(new_index);
      if (child.children_.empty()) {
        // Use child node
        new_children.push_back(std::move(child));
        ++new_index;
      } else {
        // Use grandchildren
        for (auto& grandchild : child.children_) {
          new_children.push_back(std::move(grandchild));
          ++new_index;
        }
      }
    }
    children_ = std::move(new_children);
  }

  // Replace the i'th child with its children.  This may result in increasing
  // the number of direct children of this node.
  void FlattenOneChild(size_t i) {
    const size_t original_size = Children().size();
    CHECK_LT(i, original_size);

    auto ith_child = children_.begin() + i;

    if (ith_child->is_leaf()) {
      // Empty list of grandchildren, just remove the child.
      children_.erase(ith_child);
      return;
    }

    // Move-insert all grandchildren except the first one after the child.
    children_.insert(ith_child + 1,
                     std::make_move_iterator(ith_child->children_.begin() + 1),
                     std::make_move_iterator(ith_child->children_.end()));
    // Possible reallocation and iterator invalidation above; update iterator.
    ith_child = children_.begin() + i;
    // Move the first grandchild into the child's place. Can't do this directly,
    // as assignment to *ith_child destroys the grandchild before it is moved.
    auto first_granchild = std::move(ith_child->children_.front());
    *ith_child = std::move(first_granchild);
  }

  // Pretty-print in tree-form.  Value() is enclosed in parens, and the whole
  // node is enclosed in braces.
  // This variant supports a custom value 'printer'.
  std::ostream& PrintTree(
      std::ostream* stream,
      const std::function<std::ostream&(std::ostream&, const value_type&)>&
          printer,
      size_t indent = 0) const {
    printer(*stream << Spacer(indent) << "{ (", Value()) << ')';
    if (Children().empty()) {
      *stream << " }";
    } else {
      *stream << '\n';
      for (const auto& child : Children()) {
        child.PrintTree(stream, printer, indent + 2) << '\n';
      }
      *stream << Spacer(indent) << '}';
    }
    return *stream;
  }

  // Pretty-print tree, using the default stream printer, which requires that
  // operator<<(std::ostream&, const value_type&) is defined.
  std::ostream& PrintTree(std::ostream* stream, size_t indent = 0) const {
    return PrintTree(
        stream,
        [](std::ostream& s, const value_type& v) -> std::ostream& {
          return s << v;
        },
        indent);
  }

 private:
  // Singular value stored at this node.
  value_type node_value_;

  // Pointer up to parent node.
  // Only the root node of a tree has a nullptr parent_.
  // This value is managed by VectorTreeChildrenList, constructors, and
  // operator=(). There should be no need to set it manually in other places.
  this_type* parent_ = nullptr;

  // Array of nodes/subtrees.
  VectorTreeChildrenList children_;
};

// Stream-printable representation of the location of a node under its
// greatest ancestor (root).
// Usage: stream << NodePath(vector_tree);
struct NodePath {
  template <class T>
  explicit NodePath(const VectorTree<T>& node) {
    node.Path(path);
  }
  std::vector<size_t> path;
};

std::ostream& operator<<(std::ostream& stream, const NodePath& p);

template <typename T>
std::ostream& operator<<(std::ostream& stream, const VectorTree<T>& node) {
  return node.PrintTree(&stream);
}

// Binary operations

// Struct used to point to corresponding two nodes in different trees.
// This could be interpreted as a difference object.
// Both pointers should be nullptr when DeepEqual() finds no differences between
// any corresponding pair of tree nodes, or non-nullptr when some difference is
// found.  When they are both non-nullptr, left != right by some user-defined
// comparison function, and their ->Path()s are equivalent (as ensured by
// DeepEqual's simultaneous traversal).
// This is analogous to std::mismatch()'s return type: a pair of non-end()
// iterators pointing to the first difference found in a dual-linear
// traversal.
template <typename LT, typename RT>
struct VectorTreeNodePair {
  VectorTreeNodePair() {}
  VectorTreeNodePair(const LT* l, const RT* r) : left(l), right(r) {}
  const LT* left = nullptr;
  const RT* right = nullptr;
};

// Recursively compares two trees node-for-node, checking their values
// and substructure.  The value types of the trees being compared need not be
// the same, as long as there exists a comparison function for them.
// The comparison function should be an equality function, i.e. it should
// return true when values are considered equal, otherwise false.
// Traversal order: pre-order (compare parents before children)
// Returns pair of pointers pointing to first encountered difference,
// or pair of nullptr when everything matches.
// If you want more detail about value differences, then capture them in the
// comparison function's closure before returning.
template <typename LT, typename RT>
VectorTreeNodePair<LT, RT> DeepEqual(
    const LT& left, const RT& right,
    const std::function<bool(const typename LT::value_type&,
                             const typename RT::value_type&)>& comp) {
  using result_type = VectorTreeNodePair<LT, RT>;
  // Node value comparison at current level.
  if (!comp(left.Value(), right.Value())) {
    return result_type{&left, &right};
  }

  // Subtree comparison: check number of children first, returning early if
  // different.
  const auto& left_children = left.Children();
  const auto& right_children = right.Children();
  if (left_children.size() != right_children.size()) {
    return result_type{&left, &right};
  }

  // Number of children match.  Find first differing children.
  // The iterators returned by std::mismatch() do not propagate the
  // deep children nodes that differ, so we must use a lambda with capture.
  result_type first_diff;  // initially nullptrs
  (void)std::mismatch(left_children.begin(), left_children.end(),
                      right_children.begin(),
                      [&comp, &first_diff](const LT& l, const RT& r) -> bool {
                        const auto result = DeepEqual(l, r, comp);
                        if (result.left == nullptr) {
                          return true;
                        } else {
                          first_diff = result;  // Capture first difference.
                          return false;
                        }
                        // When this returns true, no further comparisons will
                        // be called, so the assignment to first_diff will
                        // contain the desired result.
                      });
  // When every subtree matches, first_diff will hold nullptrs.
  // Otherwise it will point to the first mismatched nodes.
  return first_diff;
}

// Overload for the case when left and right tree types have a defined equality
// operator.  This also works when the left and right types are the same.
template <typename LT, typename RT>
VectorTreeNodePair<LT, RT> DeepEqual(const LT& left, const RT& right) {
  return DeepEqual(left, right,
                   [](const typename LT::value_type& l,
                      const typename RT::value_type& r) { return l == r; });
}

// If both trees are structurally the same, node-for-node, returns a pair of
// nullptrs.  Otherwise, returns with pointers to the first encountered nodes
// that differ in substructure.  Traversal order: pre-order.
// Implementation: this is just a degenerate form of DeepEqual, where the value
// comparison is ignored (always returns true).
template <typename LT, typename RT>
VectorTreeNodePair<LT, RT> StructureEqual(const LT& left, const RT& right) {
  // Ignore node values by always treating them as equal.
  return DeepEqual(left, right,
                   [](const typename LT::value_type&,
                      const typename RT::value_type&) { return true; });
}

// Provide ADL-enabled overload for use by swap implementations.
template <class T>
void swap(VectorTree<T>& left, VectorTree<T>& right) {
  left.swap(right);
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_VECTOR_TREE_H_
