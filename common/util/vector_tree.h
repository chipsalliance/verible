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
#include "common/util/type_traits.h"

namespace verible {

namespace vector_tree_internal {

struct UnavailableFeatureTraits {
  static inline constexpr bool available = false;
};

struct FeatureTraits {
  static inline constexpr bool available = true;
};

template <
    typename Node,  //
    typename Container_ = decltype(std::declval<Node>().Children()),
    typename ChildNode_ = decltype(*std::declval<Node>().Children().begin()),
    typename = std::void_t<decltype(std::declval<Node>().Children().end())>>
struct TreeNodeChildrenTraits : FeatureTraits {
  // TODO(mglb):
  // - children_reference
  // - children_const_reference

  // TODO(mglb): use subnodes_type if available?
  using container_type = std::remove_reference_t<Container_>;
};

template <typename Node,  //
          typename Value_ = decltype(std::declval<Node>().Value())>
struct TreeNodeValueTraits : FeatureTraits {
  using type = std::remove_reference_t<Value_>;
};

template <typename Node,  //
          typename Parent_ = decltype(*std::declval<Node>().Parent()),
          typename = std::void_t<TreeNodeChildrenTraits<Parent_>>>
struct TreeNodeParentTraits : FeatureTraits {};

}  // namespace vector_tree_internal

template <class Node,  //
          typename Children_ =
              vector_tree_internal::TreeNodeChildrenTraits<Node>>
struct TreeNodeTraits : vector_tree_internal::FeatureTraits {
  // optional
  using Parent =
      detected_or_t<vector_tree_internal::UnavailableFeatureTraits,
                    vector_tree_internal::TreeNodeParentTraits, Node>;
  // optional
  using Value = detected_or_t<vector_tree_internal::UnavailableFeatureTraits,
                              vector_tree_internal::TreeNodeValueTraits, Node>;
  // required
  using Children = Children_;
};

namespace vector_tree_internal {

template <class T>
inline static size_t BirthRank(const T& node, std::input_iterator_tag) {
  if (node.Parent() != nullptr) {
    size_t index = 0;
    for (const auto& child : node.Parent()->Children()) {
      if (&node == &child) return index;
      ++index;
    }
  }
  return 0;
}

template <class T>
inline static size_t BirthRank(const T& node, std::random_access_iterator_tag) {
  if (node.Parent() != nullptr) {
    return std::distance(&(node.Parent()->Children().front()), &node);
  }
  return 0;
}

}  // namespace vector_tree_internal

// Returns the index of this node relative to parent's children.
// An only-child or first-child will have birth rank 0.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
size_t BirthRank(const T& node) {
  using Iterator = decltype(node.Parent()->Children().begin());
  return vector_tree_internal::BirthRank(
      node, typename std::iterator_traits<Iterator>::iterator_category());
}

// Returns the number of parents between this node and the root.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
size_t NumAncestors(const T& node) {
  size_t depth = 0;
  for (const T* p = node.Parent(); p != nullptr; p = p->Parent()) {
    ++depth;
  }
  return depth;
}

// Returns true if 'other' is an ancestor of this node, in other words,
// this node is descended from 'other'.
// This method could have been named IsDescendedFrom().
// nullptr is never considered an ancestor of any node.
// 'this' node is not considered an ancestor of itself.
template <class T, class AncestorType,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
bool HasAncestor(const T& node, const AncestorType other) {
  if (other == nullptr) return false;
  for (const auto* p = node.Parent(); p != nullptr; p = p->Parent()) {
    if (p == other) return true;
  }
  return false;
}

// Returns pointer to the tree root, the greatest ancestor of this node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T* Root(T& node) {
  T* root = &node;
  while (root->Parent() != nullptr) {
    root = root->Parent();
  }
  return root;
}

template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
bool is_leaf(T& node) {
  return node.Children().empty();
}

// Returns the closest common ancestor to this and the other, else nullptr.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T* NearestCommonAncestor(T& node_a, T& node_b) {
  T* a = &node_a;
  T* b = &node_b;
  std::set<T*> ancestors_a, ancestors_b;
  // In alternation, insert a/b into its respective set of ancestors,
  // and check for membership in the other ancestor set.
  // Return as soon as one is found in the other's set of ancestors.
  while (a != nullptr || b != nullptr) {
    if (a != nullptr) {
      if (ancestors_b.find(a) != ancestors_b.end()) {
        return a;
      }
      ancestors_a.insert(a);
      a = a->Parent();
    }
    if (b != nullptr) {
      if (ancestors_a.find(b) != ancestors_a.end()) {
        return b;
      }
      ancestors_b.insert(b);
      b = b->Parent();
    }
  }
  // Once this point is reached, there are no common ancestors.
  return nullptr;
}

// Construct a path of BirthRank()s from root to this.
// Root node's 'path' is empty.  Passing the resulting path to
// root.DescendPath() gets you back to this node.
// PathType can be any container with a push_back() interface.
template <class T, class PathType,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
void Path(const T& node, PathType& path) {
  if (node.Parent() != nullptr) {
    Path(*node.Parent(), path);
    path.push_back(verible::BirthRank(node));
  }
}

// Descend through children using indices specified by iterator range.
// Iter type dereferences to an integral value.
// This works on any vector_tree_internal node, not just the root.
template <class T, class Iterator,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
T& DescendPath(T& node, Iterator start, Iterator end) {
  auto* current_node = &node;
  for (auto iter = start; iter != end; ++iter) {
    auto& children = current_node->Children();
    const auto index = *iter;
    CHECK_GE(index, 0);
    CHECK_LT(index, children.size());
    current_node = &children[index];  // descend
  }
  return *current_node;
}

// Returns the node reached by descending through Children().front().
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
T* LeftmostDescendant(T& node) {
  T* leaf = &node;
  while (!leaf->Children().empty()) {
    leaf = &leaf->Children().front();
  }
  return leaf;
}

// Returns the node reached by descending through Children().back().
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
T* RightmostDescendant(T& node) {
  T* leaf = &node;
  while (!leaf->Children().empty()) {
    leaf = &leaf->Children().back();
  }
  return leaf;
}

// Returns true if this node has no parent, or it has BirthRank 0.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
bool IsFirstChild(const T& node) {
  if (node.Parent() == nullptr) return true;
  return &node.Parent()->Children().front() == &node;
}

// Returns true if this node has no parent, or it is the last child of its
// parent.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
bool IsLastChild(const T& node) {
  if (node.Parent() == nullptr) return true;
  return &node.Parent()->Children().back() == &node;
}

// Navigates to the next leaf (node without Children()) in the tree
// (if it exists), else returns nullptr.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T* NextLeaf(T& node) {
  auto* parent = node.Parent();
  if (parent == nullptr) {
    // Root node has no next sibling, this is the end().
    return nullptr;
  }

  // Find the next sibling, if there is one.
  auto& siblings = parent->Children();
  const size_t birth_rank = BirthRank(node);
  const size_t next_rank = birth_rank + 1;
  if (next_rank != siblings.size()) {
    // More children follow this one.
    return LeftmostDescendant(siblings[next_rank]);
  }

  // This is the last child of the group.
  // Find the nearest parent that has a next child (ascending).
  // TODO(fangism): rewrite without recursion
  auto* next_ancestor = NextLeaf(*parent);
  if (next_ancestor == nullptr) return nullptr;

  // next_ancestor is the NearestCommonAncestor() to the original
  // node and the resulting node.
  return LeftmostDescendant(*next_ancestor);
}

// Navigates to the previous leaf (node without Children()) in the tree
// (if it exists), else returns nullptr.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T* PreviousLeaf(T& node) {
  auto* parent = node.Parent();
  if (parent == nullptr) {
    // Root node has no previous sibling, this is the reverse-end().
    return nullptr;
  }

  // Find the next sibling, if there is one.
  auto& siblings = parent->Children();
  const size_t birth_rank = BirthRank(node);
  if (birth_rank > 0) {
    // More children precede this one.
    return RightmostDescendant(siblings[birth_rank - 1]);
  }

  // This is the first child of the group.
  // Find the nearest parent that has a previous child (descending).
  // TODO(fangism): rewrite without recursion
  auto* prev_ancestor = PreviousLeaf(*parent);
  if (prev_ancestor == nullptr) return nullptr;

  // prev_ancestor is the NearestCommonAncestor() to the original
  // node and the resulting node.
  return RightmostDescendant(*prev_ancestor);
}

// Function-application traversals.
//   * Apply in pre-order vs. post-order.
//   * Apply to attached value at each node vs. the whole node itself.
//     Whole node gives access to immediate children (and subtree).
//   * Apply function is const vs. mutating.
// All combinations of the above are provided.

// Visits all tree nodes in pre-order traversal applying function to all nodes.
// Useful for checking invariants between parents and their children.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
void ApplyPreOrder(
    T& node, const std::function<void(std::add_lvalue_reference_t<T>)>& f) {
  f(node);
  for (auto& child : node.Children()) ApplyPreOrder(child, f);
}

// This variant of ApplyPreOrder expects a function on the underlying
// value_type (const&).
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available>* = nullptr>
void ApplyPreOrder(
    T& node,
    const std::function<void(
        std::add_lvalue_reference_t<typename TreeNodeTraits<T>::Value::type>)>&
        f) {
  f(node.Value());
  for (auto& child : node.Children()) ApplyPreOrder(child, f);
}

// Visits all tree nodes in post-order traversal applying function to all nodes.
// Useful for checking invariants between parents and their children.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
void ApplyPostOrder(
    T& node, const std::function<void(std::add_lvalue_reference_t<T>)>& f) {
  for (auto& child : node.Children()) ApplyPostOrder(child, f);
  f(node);
}

// This variant of ApplyPostOrder expects a function on the underlying
// value_type.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available>* = nullptr>
void ApplyPostOrder(
    T& node,
    const std::function<void(
        std::add_lvalue_reference_t<typename TreeNodeTraits<T>::Value::type>)>&
        f) {
  for (auto& child : node.Children()) ApplyPostOrder(child, f);
  f(node.Value());
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
template <
    class DstTree, class SrcTree, class SrcNodeToDstValueFunc,
    class DstValue_ =
        std::invoke_result_t<SrcNodeToDstValueFunc, const SrcTree&>,  //
    std::enable_if_t<TreeNodeTraits<DstTree>::Value::available &&
                     TreeNodeTraits<SrcTree>::Value::available &&
                     std::is_constructible_v<DstTree, DstValue_>>* = nullptr>
DstTree Transform(const SrcTree& src_node, const SrcNodeToDstValueFunc& f) {
  // Using invoke() to allow passing SrcTree's method pointers as `f`
  DstTree dst_node(std::invoke(f, src_node));
  dst_node.Children().reserve(src_node.Children().size());
  for (const auto& child : src_node.Children()) {
    AdoptSubtree(dst_node, Transform<DstTree>(child, f));
  }
  return dst_node;
}

// Returns the next sibling node if it exists, else nullptr.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T* NextSibling(T& node) {
  if (node.Parent() == nullptr) {
    return nullptr;
  }
  const size_t birth_rank = BirthRank(node);
  const size_t next_rank = birth_rank + 1;
  if (next_rank == node.Parent()->Children().size()) {
    return nullptr;  // This is the last child of the Parent().
  }
  // More children follow this one.
  return &node.Parent()->Children()[next_rank];
}

// Returns the previous sibling node if it exists, else nullptr.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T* PreviousSibling(T& node) {
  if (node.Parent() == nullptr) {
    return nullptr;
  }
  const size_t birth_rank = BirthRank(node);
  if (birth_rank == 0) {
    return nullptr;
  }
  // More children precede this one.
  return &node.Parent()->Children()[birth_rank - 1];
}

// Removes this node from its parent, and shifts ths siblings that follow this
// node lower in BirthRank().
// Any iterators that pointed to this node or its later siblings are
// invalidated.
// This node is destroyed in the process.
// This operation is only valid on non-root nodes.
// It is the caller's responsibility to maintain invariants before
// destroying this node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available &&
                           !std::is_const_v<T>>* = nullptr>
void RemoveSelfFromParent(T& node) {
  auto& siblings = ABSL_DIE_IF_NULL(node.Parent())->Children();
  auto self_iter = siblings.begin() + verible::BirthRank(node);
  CHECK_EQ(&*self_iter, &node);
  siblings.erase(self_iter);
}

// Appends one or more sub-trees at this level.
// Variadic template handles one argument at a time.
// This invalidates previous iterators/pointers to sibling children.
template <class T, typename... AdoptedNodeN>
std::enable_if_t<TreeNodeTraits<T>::available && !std::is_const_v<T> &&
                 (std::is_convertible_v<std::decay_t<AdoptedNodeN>, T> && ...)>
AdoptSubtree(T& node, AdoptedNodeN&&... node_n) {
  node.Children().reserve(node.Children().size() + sizeof...(node_n));
  (node.Children().push_back(std::forward<AdoptedNodeN>(node_n)), ...);
}

// This node takes/moves subtrees from another node (concatenates).
// There need not be any relationship between this node and the other.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void AdoptSubtreesFrom(T& node, T* other) {
  auto& src_children = other->Children();
  node.Children().reserve(node.Children().size() + src_children.size());
  for (auto& src_child : src_children) {
    node.Children().push_back(std::move(src_child));
  }
  other->Children().clear();
}

// If this node has exactly one child, replace this node with that child
// and return true, otherwise, do nothing and return false.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available &&
                           !std::is_const_v<T>>* = nullptr>
bool HoistOnlyChild(T& node) {
  if (node.Children().size() != 1) return false;

  auto& only = node.Children().front();
  node.Value() = std::move(only.Value());
  // Can't do this directly, as assignment to children_ destroys its child
  // (`only`) before it is moved.
  // TODO(mglb): Find another solution; see public move constructor/assignment
  // in ChildrenList.
  auto new_children = std::move(only.Children());
  node.Children() = std::move(new_children);

  return true;
}

// Combines the Nth and (N+1) sibling using a custom function 'joiner' on the
// nodes' values, and the Nth sibling will adopt N+1's children.
// The 'joiner' function does: *left = f(*left, *right);
// The (N+1) sibling will be erased in the process, and every sibling
// thereafter will be shifted back one position (same inefficiency as shifting
// vector contents).  This invalidates all iterators after position N,
// and iterators to the Nth node's children (possible realloc).
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available &&
                           !std::is_const_v<T>>* = nullptr>
void MergeConsecutiveSiblings(
    T& node, size_t N,
    std::function<void(typename TreeNodeTraits<T>::Value::type*,
                       const typename TreeNodeTraits<T>::Value::type&)>
        joiner) {
  CHECK_LT(N + 1, node.Children().size());

  // Combine value into node[N].
  joiner(&node.Children()[N].Value(), node.Children()[N + 1].Value());

  // Move-concatenate children to node[N].
  const auto next_iter = node.Children().begin() + N + 1;
  verible::AdoptSubtreesFrom(node.Children()[N], &*next_iter);

  // Shift-left children_ by 1 beyond N.
  node.Children().erase(next_iter);  // done via move-assignment
}

// Replace all direct children of this node with concatenated grandchildren.
// Retains the value of this node.  Discards direct childrens' values.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void FlattenOnce(T& node) {
  const int grandchildren_count = std::transform_reduce(
      node.Children().begin(), node.Children().end(), 0, std::plus<>(),
      [](const T& gc) { return gc.Children().size(); });

  // Build new children list in a standalone vector, then move-assign it to
  // this node's children vector.
  // FIXME(mglb): use TreeNodeTraits<T>::Children::container_type
  std::vector<std::decay_t<T>> grandchildren;
  grandchildren.reserve(grandchildren_count);

  for (auto& child : node.Children()) {
    for (auto& grandchild : child.Children()) {
      grandchildren.push_back(std::move(grandchild));
    }
  }
  node.Children() = std::move(grandchildren);
}

// For every child, if that child has grandchildren, replace that child with
// its grandchildren, else preserve that child.
// If new_offsets is provided, populate that array with indices into the
// resulting children that correspond to the start locations of the original
// children's children.  This lets the caller reference and operate on
// subranges of the original set of grandchildren.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void FlattenOnlyChildrenWithChildren(
    T& node, std::vector<size_t>* new_offsets = nullptr) {
  const int new_children_count = std::transform_reduce(
      node.Children().begin(), node.Children().end(), 0, std::plus<>(),
      [](const T& gc) { return std::max<size_t>(gc.Children().size(), 1u); });

  // Build new children list in a standalone vector, then move-assign it to
  // this node's children vector.
  // FIXME(mglb): use TreeNodeTraits<T>::Children::container_type
  std::vector<std::decay_t<T>> new_children;
  new_children.reserve(new_children_count);

  if (new_offsets != nullptr) {
    new_offsets->clear();
    new_offsets->reserve(node.Children().size());
  }

  size_t new_index = 0;
  for (auto& child : node.Children()) {
    if (new_offsets) new_offsets->push_back(new_index);
    if (child.Children().empty()) {
      // Use child node
      new_children.push_back(std::move(child));
      ++new_index;
    } else {
      // Use grandchildren
      for (auto& grandchild : child.Children()) {
        new_children.push_back(std::move(grandchild));
        ++new_index;
      }
    }
  }
  node.Children() = std::move(new_children);
}

// Replace the i'th child with its children.  This may result in increasing
// the number of direct children of this node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void FlattenOneChild(T& node, size_t i) {
  const size_t original_size = node.Children().size();
  CHECK_LT(i, original_size);

  auto ith_child = node.Children().begin() + i;

  if (is_leaf(*ith_child)) {
    // Empty list of grandchildren, just remove the child.
    node.Children().erase(ith_child);
    return;
  }

  // Move-insert all grandchildren except the first one after the child.
  node.Children().insert(
      ith_child + 1, std::make_move_iterator(ith_child->Children().begin() + 1),
      std::make_move_iterator(ith_child->Children().end()));
  // Possible reallocation and iterator invalidation above; update iterator.
  ith_child = node.Children().begin() + i;
  // Move the first grandchild into the child's place. Can't do this directly,
  // as assignment to *ith_child destroys the grandchild before it is moved.
  auto first_granchild = std::move(ith_child->Children().front());
  *ith_child = std::move(first_granchild);
}

// Pretty-print in tree-form.  Value() is enclosed in parens, and the whole
// node is enclosed in braces.
// This variant supports a custom value 'printer'.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
std::ostream& PrintTree(
    const T& node, std::ostream* stream,
    const std::function<std::ostream&(
        std::ostream&, std::add_lvalue_reference_t<decltype(node.Value())>)>&
        printer,
    size_t indent = 0) {
  printer(*stream << Spacer(indent) << "{ (", node.Value()) << ')';
  if (node.Children().empty()) {
    *stream << " }";
  } else {
    *stream << '\n';
    for (const auto& child : node.Children()) {
      PrintTree(child, stream, printer, indent + 2) << '\n';
    }
    *stream << Spacer(indent) << '}';
  }
  return *stream;
}

template <typename T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
std::ostream& operator<<(std::ostream& stream, const T& node) {
  return PrintTree(node, &stream);
}

// Pretty-print tree, using the default stream printer, which requires that
// operator<<(std::ostream&, const value_type&) is defined.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
std::ostream& PrintTree(const T& node, std::ostream* stream,
                        size_t indent = 0) {
  return PrintTree(
      node, stream,
      [](std::ostream& s, std::add_lvalue_reference_t<decltype(node.Value())> v)
          -> std::ostream& { return s << v; },
      indent);
}

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

    // TODO(mglb): implemented for HoistOnlyChild(); consider another solution
    ChildrenList(ChildrenList&& other) noexcept
        : node_(other.node_), container_(std::move(other.container_)) {}

    // TODO(mglb): made public for HoistOnlyChild(); consider another solution
    ChildrenList& operator=(ChildrenList&& other) noexcept {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      container_ = std::move(other.container_);
      LinkChildrenToParent(container_);
      return *this;
    }

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

    ChildrenList(VectorTree& node, ChildrenList&& other) noexcept
        : node_(node), container_(std::move(other.container_)) {
      // Note: `other` is not notified about the change because it ends up in
      // undefined state as a result of the move.
      LinkChildrenToParent(container_);
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

// Stream-printable representation of the location of a node under its
// greatest ancestor (root).
// Usage: stream << NodePath(vector_tree);
struct NodePath {
  template <class TreeType>
  explicit NodePath(const TreeType& node) {
    verible::Path(node, path);
  }
  std::vector<size_t> path;
};

std::ostream& operator<<(std::ostream& stream, const NodePath& p);

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
