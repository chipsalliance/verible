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
#include "common/util/type_traits.h"

namespace verible {

// VectorTree is a hierarchical representation of information.
// While it may be useful to maintain some invariant relationship between
// parents and children nodes, it is not required for this class.
// The traversal methods ApplyPreOrder/ApplyPostOrder could be used to
// maintain or verify parent-child invariants.
// The VectorTree class is itself also the same as the vector_tree_internally
// used node class; i.e. there is no separate node class.
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
class VectorTree {
  typedef VectorTree<T> this_type;

  // Forward declaration
  class ChildrenList;

 public:
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
    children_.reserve(sizeof...(args));
    (children_.emplace_back(std::forward<Args>(args)), ...);
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

  // Accessors

  T& Value() { return node_value_; }

  const T& Value() const { return node_value_; }

  this_type* Parent() { return parent_; }

  const this_type* Parent() const { return parent_; }

  ChildrenList& Children() { return children_; }

  const ChildrenList& Children() const { return children_; }

 private:
  // Returns true if parent-child links are valid in entire tree.
  bool CheckIntegrity() const {
    for (const auto& child : children_) {
      CHECK_EQ(child.Parent(), this)
          << "Inconsistency: child's parent does not point back to this node!";
      if (!child.CheckIntegrity()) return false;
    }
    return true;
  }

  // A wrapper of a sequence container for storing VectorTree nodes that sets
  // correct parent pointer in each inserted node. The reference to the
  // "correct" parent is passed to a constructor.
  //
  // The sole purpose of this class is to function as a children list in
  // VectorTree, both as a storage and as a public interface for tree
  // manipulation (through reference).
  //
  // This class handles parent pointer assignment for all cases where the
  // children list itself is modified. However, it does not clear or otherwise
  // change parent pointer in removed nodes.
  class ChildrenList : ContainerProxyBase<ChildrenList, subnodes_type> {
    using Base = ContainerProxyBase<ChildrenList, subnodes_type>;
    friend Base;

   public:
    using typename Base::container_type;

    // Import (via `using`) ContainerProxy members supported by std::vector.
    USING_CONTAINER_PROXY_STD_VECTOR_MEMBERS(Base)

    // Move-cast to wrapped container's type. Moves out the container.
    explicit operator container_type() && { return std::move(container_); }

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

    // Allow construction, assignment and direct access to `container_` inside
    // VectorTree.
    friend VectorTree;

    // Hide constructors and assignments from the world. This object is created
    // and assigned-to only in VectorTree.

    explicit ChildrenList(VectorTree& node) : node_(node) {}

    // Construction requires parent node reference.
    ChildrenList(const ChildrenList&) = delete;

    ChildrenList(VectorTree& node, const ChildrenList& other)
        : node_(node), container_(other.container_) {
      LinkChildrenToParent(container_);
    }

    ChildrenList& operator=(const ChildrenList& other) {
      container_ = other.container_;
      LinkChildrenToParent(container_);
      return *this;
    }

    // Construction requires parent node reference.
    ChildrenList(ChildrenList&&) = delete;

    ChildrenList(VectorTree& node, ChildrenList&& other) noexcept
        : node_(node), container_(std::move(other.container_)) {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      LinkChildrenToParent(container_);
    }

    ChildrenList& operator=(ChildrenList&& other) noexcept {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      container_ = std::move(other.container_);
      LinkChildrenToParent(container_);
      return *this;
    }

    // Reference to a VectorTree node in which this object represents a list of
    // children.
    // TODO(mglb): try to get rid of this reference. See:
    // https://github.com/chipsalliance/verible/pull/1252#discussion_r825196108
    // Also look at MapTree::KeyValuePair() - `offsetof` is already used there
    // so it could be user here too.
    VectorTree& node_;

    // Actual data container where the nodes are stored.
    subnodes_type container_;
  };

  // Singular value stored at this node.
  value_type node_value_;

  // Pointer up to parent node.
  // Only the root node of a tree has a nullptr parent_.
  // This value is managed by ChildrenList, constructors, and
  // operator=(). There should be no need to set it manually in other places.
  this_type* parent_ = nullptr;

  // Array of nodes/subtrees.
  ChildrenList children_;
};

// Provide ADL-enabled overload for use by swap implementations.
template <class T>
void swap(VectorTree<T>& left, VectorTree<T>& right) {
  left.swap(right);
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_VECTOR_TREE_H_
