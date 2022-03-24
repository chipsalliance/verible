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

#ifndef VERIBLE_COMMON_UTIL_EXPANDABLE_TREE_VIEW_H_
#define VERIBLE_COMMON_UTIL_EXPANDABLE_TREE_VIEW_H_

#include <cstddef>
#include <functional>
#include <iterator>

#include "common/util/logging.h"
#include "common/util/tree_operations.h"
#include "common/util/vector_tree.h"

namespace verible {

// TreeViewNodeInfo is a support structure for ExpandableTreeView.
// This is the information that is attached to every node in an
// ExpandableTreeView, and also contains a pointer to every corresponding
// node in the viewed VectorTree.
template <class WrappedNodeType>
class TreeViewNodeInfo {
  using TreeTraits = TreeNodeTraits<WrappedNodeType>;
  using value_const_reference = typename TreeTraits::Value::const_reference;

 public:
  explicit TreeViewNodeInfo(const WrappedNodeType& node) : node_(&node) {}

  void Unexpand() { expand_ = false; }
  void Expand() { expand_ = true; }
  bool IsExpanded() const { return expand_; }

  value_const_reference Value() const { return node_->Value(); }

 private:
  // Immutable pointer to corresponding VectorTree node.
  // This promises to never modify the node's data.
  const WrappedNodeType* const node_;

  // Current view of this particular subtree node.
  // If true, then traverse children_, otherwise visit this node as one element.
  // Never visit both this node and its children.
  bool expand_ = true;
};

// ExpandableTreeView is a read-only view of a VectorTree structure.
// Whereas VectorTree contains the information for a fully-expanded tree,
// this class presents a potentially collapsed view thereof.
// Whereas VectorTree represents the maximum extent to which a
// hierarchical view *could* be expanded, this view class allows control
// over which nodes are expanded.  This allows expansions decisions to
// occur dynamically.  At any given level, you work with either a single
// object (flat), or a sequence of objects that in some way represents
// or expands upon the single object -- equivalence is subject to
// interpretation.
//
//   Illustrated example: partitions:
//     none expanded:                   |<--                        -->|
//     one-level expanded:              |<-       ->|<-      ->|<-   ->|
//     selectively expanded:            |           |    |  |  |       |
//     fully expanded (VectorTree):     |  |    |   | |  |  |  |   |   |
//
//   No matter which view, the whole is representative of the sum of the parts.
//
// Implementation detail:
// ExpandableTreeView itself uses a VectorTree to store a parallel tree
// of references to another VectorTree, thanks to similiarity in structure.
// Structural modifications to the original tree may invalidate an entire tree
// view, due to the use of pointers in TreeViewNodeInfo node type.
template <class WrappedNodeType>
class ExpandableTreeView {
  using TreeTraits = TreeNodeTraits<WrappedNodeType>;

  using value_type = typename TreeTraits::Value::type;
  using this_type = ExpandableTreeView;

  // The type of the tree being pointed to.
  // This class treats the underlying tree as read-only.
  using tree_type = WrappedNodeType;

  // Struct type that contains a pointer to the original tree type.
  // Every node in the original tree type corresponds to a node in the
  // view impl_type.
  using node_type = TreeViewNodeInfo<WrappedNodeType>;

  // The tree implementation type for the collection of expandable tree
  // nodes that parallel the original tree.
  using impl_type = VectorTree<node_type>;

 public:
  class iterator;

  using const_iterator = iterator;

  // Constructs (recursively) a fully-expanded view from the input tree.
  explicit ExpandableTreeView(const tree_type& tree)
      : view_(
            Transform<impl_type>(tree, [](const tree_type& other) -> node_type {
              // Initialize a view node using the address of corresponding node
              // in the other tree.
              return node_type(other);
            })) {
    // Guarantee structural equivalence with original tree.
    CHECK(StructureEqual(view_, tree).left == nullptr);
  }

  // TODO(fangism): implement later as needed
  ExpandableTreeView(const this_type&) = delete;
  ExpandableTreeView(this_type&&) = delete;

  ~ExpandableTreeView() = default;

  // Accessors

  const node_type& Value() const { return view_.Value(); }
  node_type& Value() { return view_.Value(); }

  // Directly access children nodes by index.
  const impl_type& operator[](size_t i) const { return view_.Children()[i]; }
  impl_type& operator[](size_t i) { return view_.Children()[i]; }

  // Iteration

  iterator begin() const { return iterator(first_unexpanded_child(view_)); }
  iterator end() const { return iterator(nullptr); }

  // Transformation

  // Apply a mutating transformation to this tree view, pre-order traversal.
  void ApplyPreOrder(const std::function<void(impl_type&)>& f) {
    verible::ApplyPreOrder(view_, f);
  }

  // Apply a mutating transformation to this tree view, post-order traversal.
  void ApplyPostOrder(const std::function<void(impl_type&)>& f) {
    verible::ApplyPostOrder(view_, f);
  }

  // Properties

 private:  // This section is only directly accessible to the iterator class.
  // Descend to the first node that is not expanded.
  // Recall that expanded nodes should *only* visit children, not self.
  // This behaves a lot like VectorTree::LeftmostDescendant(), only the
  // termination condition is different.
  static const impl_type* first_unexpanded_child(const impl_type& current) {
    const auto& info = current.Value();
    if (info.IsExpanded() && !is_leaf(current)) {
      // Let compiler to tail-call optimize self-recursion.
      return first_unexpanded_child(current.Children().front());
    } else {
      return &current;
    }
  }

  // Helper function for iterating to next node in sequence, which could be
  // sibling of the same parent or even a 'cousin'.  (Naming this
  // next_sibling_or_cousin just sounds too verbose...)
  // This visits either a node or its children, but never both.
  // This behaves a lot like VectorTree::NextLeaf().
  // \precondition this->parent_->expand_ is true (for all ancestors),
  //   otherwise we would have never reached this node.
  static const impl_type* next_sibling(const impl_type& current) {
    if (current.Parent() != nullptr) {
      // Find the next sibling, if there is one.
      const size_t birth_rank = verible::BirthRank(current);
      const size_t next_rank = birth_rank + 1;
      if (next_rank == current.Parent()->Children().size()) {
        // This is the last child of the group.
        // Find the nearest parent that has a next child (ascending).
        auto* next_ancestor = next_sibling(*current.Parent());
        if (next_ancestor != nullptr) {
          return first_unexpanded_child(*next_ancestor);
        } else {
          return nullptr;
        }
      } else {
        // More children follow this one.
        return first_unexpanded_child(current.Parent()->Children()[next_rank]);
      }
    } else {
      // Root node has no next sibling, this is the end().
      return nullptr;
    }
  }

 private:
  // Parallel tree whose nodes point to original tree passed in constructor.
  // As a 'view' class, the original tree is never modified from this view.
  // Changes that restructure the original tree can invalidate an entire
  // tree view, so it is best to freeze a tree before constructing a view from
  // it.
  impl_type view_;
};

// This iterator class implements a mostly-conventional N-ary tree iterator.
// The major difference is that this is tailored to ExpandableTreeView (node),
// in which its IsExpanded() property determines whether or not a particular
// node is visited unexpanded, or that node's children is visited; iteration
// never visits both a node AND its children, only one or the other.
template <class WrappedNodeType>
class ExpandableTreeView<WrappedNodeType>::iterator
    : public std::iterator<std::forward_iterator_tag, const value_type> {
  friend class ExpandableTreeView<WrappedNodeType>;  // grant access to private
                                                     // constructor

 private:
  explicit iterator(const impl_type* node) : node_(node) {}

 public:
  iterator(const iterator&) = default;

  // pre-increment
  iterator& operator++() {
    node_ = next_sibling(*node_);
    return *this;
  }

  // post-increment
  iterator operator++(int) {
    auto iter = *this;
    ++(*this);
    return iter;
  }

  // TODO(fangism): implement decrement operators, to support bidirectionality

  bool operator==(const iterator& rhs) const { return node_ == rhs.node_; }

  bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

  const value_type& operator*() const { return node_->Value().Value(); }
  const value_type* operator->() const { return &node_->Value().Value(); }

 private:
  // From the node_ pointer alone, we have sufficient information to iterate.
  // For now, this supports forward-only iteration because, once this becomes
  // nullptr, there is no way to go backward.
  const impl_type* node_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_EXPANDABLE_TREE_VIEW_H_
