// Copyright 2017-2022 The Verible Authors.
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

// This file contains generic functions and trait classes for operating on tree
// structures.
//
// TreeNode concept
// ----------------
//
// Class `T` must contain at least the following member in order to fulfill the
// TreeNode concept:
//
// - `Container<T>& Children()`:
//   Returns reference to a STL-like container or a range containing the node's
//   children.
//   The Container must at least support `begin()` and `end()` methods.
//   Pointer and iterator stability is not required.
//
// To check whether class `T` fulfills the concept one can use the
// `TreeNodeTraits<T>` traits struct. It is defined only for classes fulfilling
// the TreeNode concept, what makes it suitable for SFINAE tests. For
// readability and consistency with Value and Parent checks (described below)
// there is a member `TreeNodeTraits<T>::available` which is always true.
//
// Optional members:
//
// - `T* Parent()`:
//   Returns pointer to a parent node or nullptr when the node is a tree root.
//   Types with this member can be detected by checking whether
//   `TreeNodeTraits<T>::Parent::available` is true.
//
// - `ValueType& Value()`:
//   Returns reference to a value stored in the node.
//   Types with this member can be detected by checking whether
//   `TreeNodeTraits<T>::Value::available` is true.
//
// - `subnodes_type` (typename):
//   Container type which should be used for storing collections of detached
//   child nodes.
//   It must be move-assignable to, and move-constructible from a value returned
//   by `Children()` method. It must provide at least the same STL container's
//   interface and iterator/pointer stability guarantees as a return type of
//   `Children()` method.
//   The use case where this alias is useful is when `Children()` returns a
//   container wrapped by some internal wrapper type that is either not
//   constructible outside of the node class or has some extra logic that is
//   not useful when used outside of a node. The `subnodes_type` in such case
//   should be an alias to the container's real type.
//   When the type is absent, it is assumed to be the type returned by
//   `Children()` method with reference and other modifiers removed.
//   It is available through `TreeNodeTraits<T>::Children::container_type`.

#ifndef VERIBLE_COMMON_UTIL_TREE_OPERATIONS_H_
#define VERIBLE_COMMON_UTIL_TREE_OPERATIONS_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iosfwd>  // IWYU pragma: keep
#include <iterator>
#include <numeric>
#include <set>
#include <utility>
#include <vector>

#include "common/util/logging.h"
#include "common/util/spacer.h"
#include "common/util/type_traits.h"

namespace verible {

namespace tree_operations_internal {

// TreeNodeTraits implementation details:

// Alias to Node::subnodes_type if it exists.
// Intended for use with detected_or_t.
template <typename Node,  //
          typename Type_ = typename Node::subnodes_type>
using TreeNodeSubnodesType = Type_;

// Defined when `Node` contains `Children()` method which returns reference to
// a STL-like container with nodes.
// TODO(mglb): Add support for children containers storing Node pointers.
template <typename Node,  //
          typename ChildrenType_ = decltype(std::declval<Node>().Children()),
          typename =
              std::void_t<decltype(*std::declval<Node>().Children().begin()),
                          decltype(std::declval<Node>().Children().end())>>
struct TreeNodeChildrenTraits : FeatureTraits {
  // Container type which should be used for storing arrays of detached
  // child nodes. It must be move-assignable to, and move-constructible from,
  // a value returned by `Children()` method.
  using container_type = verible::remove_cvref_t<
      detected_or_t<ChildrenType_, TreeNodeSubnodesType, Node>>;
};

// Defined when `Node` contains `Value()` method.
template <typename Node,  //
          typename Value_ = decltype(std::declval<Node>().Value())>
struct TreeNodeValueTraits : FeatureTraits {
  // Type of data returned by `Value()` method, without reference and const.
  using type = std::remove_const_t<std::remove_reference_t<Value_>>;
  // Reference to a type returned by `Value()` matching its constness.
  using reference = std::add_lvalue_reference_t<Value_>;
  // Const reference to a type returned by `Value()`
  using const_reference = std::add_lvalue_reference_t<std::add_const_t<type>>;
};

// Defined when `Node` contains `Parent()` method which dereferences to a type
// fulfilling NodeTraits concept.
template <typename Node,  //
          typename Parent_ = decltype(*std::declval<Node>().Parent()),
          typename = std::void_t<TreeNodeChildrenTraits<Parent_>>>
struct TreeNodeParentTraits : FeatureTraits {};

// BirthRank implementation details:

// BirthRank implementation supporting any container.
// This overload is a linear-time operation.
template <class T>
size_t BirthRank(const T& node, std::input_iterator_tag) {
  if (node.Parent() != nullptr) {
    size_t index = 0;
    for (const auto& child : node.Parent()->Children()) {
      if (&node == &child) return index;
      ++index;
    }
  }
  return 0;
}

// BirthRank optimized for random access containers.
template <class T>
size_t BirthRank(const T& node, std::random_access_iterator_tag) {
  if (node.Parent() != nullptr) {
    return std::distance(&*(node.Parent()->Children().begin()), &node);
  }
  return 0;
}

// Conditional container operations:

// Calls `container.reserve(new_cap)` if container supports this method.
template <class Container>
auto /* void */ ReserveIfSupported(Container& container,
                                   typename Container::size_type new_cap)
    -> std::void_t<decltype(container.reserve(new_cap))> {
  container.reserve(new_cap);
}

// No-op candidate used when Container doesn't provide `reserve()` method.
template <class Container>
void ReserveIfSupported(Container&, ...) {}

}  // namespace tree_operations_internal

// TreeNodeTraits:

// Traits of a type fulfilling TreeNode concept.
// `TreeNodeTraits<T>` is defined for every class `T` fulfilling the TreeNode
// concept. It can be used in SFINAE tests.
template <class Node,  //
          typename Children_ =
              tree_operations_internal::TreeNodeChildrenTraits<Node>>
struct TreeNodeTraits : FeatureTraits {
  using Parent =
      detected_or_t<UnavailableFeatureTraits,
                    tree_operations_internal::TreeNodeParentTraits, Node>;
  using Value =
      detected_or_t<UnavailableFeatureTraits,
                    tree_operations_internal::TreeNodeValueTraits, Node>;
  using Children = Children_;
};

// Functions operating on tree nodes:

// Returns true when `node` is a leaf, false otherwise.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
bool is_leaf(const T& node) {
  return node.Children().empty();
}

// Descend through children using indices specified by iterator range.
// Iterator type must dereference to an integral value.
// This works on any internal node, not just the root.
// This implementation fatally exits if any indices are out-of-range.
// The const-ness of the returned reference matches the const-ness of the node
// argument.
template <class T, class Iterator,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
T& DescendPath(T& node, Iterator start, Iterator end) {
  auto* current_node = &node;
  for (auto iter = start; iter != end; ++iter) {
    auto& children = current_node->Children();
    const auto index = *iter;
    CHECK_GE(index, 0);
    CHECK_LT(index, std::size(children));
    current_node = &*std::next(children.begin(), index);  // descend
  }
  return *current_node;
}

// Returns the node reached by descending through *(Children().begin()).
// The const-ness of the returned reference matches the const-ness of the node
// argumnent.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
T& LeftmostDescendant(T& node) {
  T* leaf = &node;
  while (!leaf->Children().empty()) {
    leaf = &*leaf->Children().begin();
  }
  return *leaf;
}

// Returns the node reached by descending through Children().back().
// The const-ness of the returned reference matches the const-ness of the node
// argumnent.
//
// Requires `back()` method in the children container.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr,
          std::void_t<decltype(std::declval<T>().Children().back())>* = nullptr>
T& RightmostDescendant(T& node) {
  T* leaf = &node;
  while (!leaf->Children().empty()) {
    leaf = &leaf->Children().back();
  }
  return *leaf;
}

// std::function type representing a printer function in PrintTree.
template <class Node>
using PrintTreePrinterFunction = std::function<std::ostream&(
    std::ostream&, typename TreeNodeTraits<Node>::Value::const_reference)>;

// Pretty-print in tree-form.  Value() is enclosed in parens, and the whole
// node is enclosed in braces.
// This variant supports a custom value 'printer'.
//
// Requires `Value()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available>* = nullptr>
std::ostream& PrintTree(const T& node, std::ostream* stream,
                        const PrintTreePrinterFunction<T>& printer,
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

// Pretty-print tree, using the default stream printer, which requires that
// operator<<(std::ostream&, const value_type&) is defined.
//
// Requires `Value()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available>* = nullptr>
std::ostream& PrintTree(const T& node, std::ostream* stream,
                        size_t indent = 0) {
  using ConstValueRef = typename TreeNodeTraits<T>::Value::const_reference;
  return PrintTree(
      node, stream,
      [](std::ostream& s, ConstValueRef v) -> std::ostream& { return s << v; },
      indent);
}

// Stream operator overload that calls PrintTree with given stream and node.
//
// Requires `Value()` method in the node.
template <typename T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available>* = nullptr>
std::ostream& operator<<(std::ostream& stream, const T& node) {
  return PrintTree(node, &stream);
}

// Function-application traversals.
// - Apply in pre-order vs. post-order.
// - Apply to attached value at each node vs. the whole node itself.
//   Whole node gives access to immediate children (and subtree).
// - Apply function is const vs. mutating.
// All combinations of the above are provided.

// std::function type representing function called on each node in
// Apply(Pre|Post)Order.
template <class Node>
using ApplyOnNodeFunction =
    std::function<void(std::add_lvalue_reference_t<Node>)>;

// Visits all tree nodes in pre-order traversal applying function to all nodes.
// Useful for checking invariants between parents and their children.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
void ApplyPreOrder(T& node, const ApplyOnNodeFunction<T>& f) {
  f(node);
  for (auto& child : node.Children()) ApplyPreOrder(child, f);
}

// Visits all tree nodes in post-order traversal applying function to all nodes.
// Useful for checking invariants between parents and their children.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available>* = nullptr>
void ApplyPostOrder(T& node, const ApplyOnNodeFunction<T>& f) {
  for (auto& child : node.Children()) ApplyPostOrder(child, f);
  f(node);
}

// std::function type representing function called on each node's value in
// Apply(Pre|Post)Order.
template <class Node>
using ApplyOnNodeValueFunction =
    std::function<void(typename TreeNodeTraits<Node>::Value::reference)>;

// This variant of ApplyPreOrder expects a function on the underlying
// value_type (const&).
//
// Requires `Value()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available>* = nullptr>
void ApplyPreOrder(T& node, const ApplyOnNodeValueFunction<T>& f) {
  f(node.Value());
  for (auto& child : node.Children()) ApplyPreOrder(child, f);
}

// This variant of ApplyPostOrder expects a function on the underlying
// value_type.
//
// Requires `Value()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available>* = nullptr>
void ApplyPostOrder(
    T& node,
    const std::function<void(typename TreeNodeTraits<T>::Value::reference)>&
        f) {
  for (auto& child : node.Children()) ApplyPostOrder(child, f);
  f(node.Value());
}

// Returns the index of this node relative to parent's children.
// An only-child, first-child, and a tree root will have birth rank 0.
// This function is aware of children container type: it uses `std::distance()`
// to calculate node index in random access containers (which usually performs
// in O(1)), and O(n) loop for other containers.
//
// Requires `Parent()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
size_t BirthRank(const T& node) {
  using Iterator = decltype(node.Parent()->Children().begin());
  return tree_operations_internal::BirthRank(
      node, typename std::iterator_traits<Iterator>::iterator_category());
}

// Returns the number of parents between this node and the root.
//
// Requires `Parent()` method in the node.
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
//
// Requires `Parent()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
bool HasAncestor(const T& node, const T* other) {
  if (other == nullptr) return false;
  for (const auto* p = node.Parent(); p != nullptr; p = p->Parent()) {
    if (p == other) return true;
  }
  return false;
}

// Overload for nullptr. Always returns false.
//
// Requires `Parent()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
constexpr bool HasAncestor(const T& node, std::nullptr_t) {
  return false;
}

// Returns reference to the tree root, the greatest ancestor of this node.
// The const-ness of the returned reference matches the const-ness of the node
// argumnent.
//
// Requires `Parent()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T& Root(T& node) {
  T* root = &node;
  while (root->Parent() != nullptr) {
    root = root->Parent();
  }
  return *root;
}

// Returns the closest common ancestor to this and the other, else nullptr.
// The const-ness of the returned pointer matches the const-ness of the node
// argumnent.
//
// Requires `Parent()` method in the node.
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

// Returns true if this node has no parent, or it has BirthRank 0.
//
// Requires `Parent()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
bool IsFirstChild(const T& node) {
  if (node.Parent() == nullptr) return true;
  return &*node.Parent()->Children().begin() == &node;
}

// Returns true if this node has no parent, or it is the last child of its
// parent.
//
// Requires `back()` method in the children container.
// Requires `Parent()` method in the node.
template <
    class T,  //
    std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr,
    std::void_t<decltype(std::declval<const T>().Children().back())>* = nullptr>
bool IsLastChild(const T& node) {
  if (node.Parent() == nullptr) return true;
  return &node.Parent()->Children().back() == &node;
}

// Navigates to the next leaf (node without Children()) in the tree
// (if it exists), else returns nullptr.
// The const-ness of the returned pointer matches the const-ness of the node
// argumnent.
//
// Requires `Parent()` method in the node.
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
  if (next_rank != std::size(siblings)) {
    // More children follow this one.
    return &LeftmostDescendant(*std::next(siblings.begin(), next_rank));
  }

  // This is the last child of the group.
  // Find the nearest parent that has a next child (ascending).
  // TODO(fangism): rewrite without recursion
  auto* next_ancestor = NextLeaf(*parent);
  if (next_ancestor == nullptr) return nullptr;

  // next_ancestor is the NearestCommonAncestor() to the original
  // node and the resulting node.
  return &LeftmostDescendant(*next_ancestor);
}

// Navigates to the previous leaf (node without Children()) in the tree
// (if it exists), else returns nullptr.
// The const-ness of the returned pointer matches the const-ness of the node
// argumnent.
//
// Requires `Parent()` method in the node.
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
    return &RightmostDescendant(*std::next(siblings.begin(), birth_rank - 1));
  }

  // This is the first child of the group.
  // Find the nearest parent that has a previous child (descending).
  // TODO(fangism): rewrite without recursion
  auto* prev_ancestor = PreviousLeaf(*parent);
  if (prev_ancestor == nullptr) return nullptr;

  // prev_ancestor is the NearestCommonAncestor() to the original
  // node and the resulting node.
  return &RightmostDescendant(*prev_ancestor);
}

// Returns the next sibling node if it exists, else nullptr.
// The const-ness of the returned pointer matches the const-ness of the node
// argumnent.
//
// Requires `Parent()` method in the node.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
T* NextSibling(T& node) {
  if (node.Parent() == nullptr) {
    return nullptr;
  }
  const size_t birth_rank = BirthRank(node);
  const size_t next_rank = birth_rank + 1;
  if (next_rank == std::size(node.Parent()->Children())) {
    return nullptr;  // This is the last child of the Parent().
  }
  // More children follow this one.
  return &*std::next(node.Parent()->Children().begin(), next_rank);
}

// Returns the previous sibling node if it exists, else nullptr.
// The const-ness of the returned pointer matches the const-ness of the node
// argumnent.
//
// Requires `Parent()` method in the node.
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
  return &*std::next(node.Parent()->Children().begin(), birth_rank - 1);
}

// Removes this node from its parent, and shifts ths siblings that follow this
// node lower in BirthRank().
// Any iterators that pointed to this node or its later siblings are
// invalidated.
// This node is destroyed in the process.
// This operation is only valid on non-root nodes.
// It is the caller's responsibility to maintain invariants before
// destroying this node.
//
// Requires `Parent()` method in the node.
// Requires `erase()` method in the children container.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available &&
                           !std::is_const_v<T>>* = nullptr>
void RemoveSelfFromParent(T& node) {
  auto& siblings = ABSL_DIE_IF_NULL(node.Parent())->Children();
  auto self_iter = std::next(siblings.begin(), verible::BirthRank(node));
  CHECK_EQ(&*self_iter, &node);
  siblings.erase(self_iter);
}

// Appends one or more sub-trees at this level.
// Variadic template handles one argument at a time.
// This invalidates previous iterators/pointers to sibling children.
//
// Requires `push_back()` method in the children container.
template <class T, typename... AdoptedNodeN>
std::enable_if_t<TreeNodeTraits<T>::available && !std::is_const_v<T> &&
                 (std::is_convertible_v<std::decay_t<AdoptedNodeN>, T> && ...)>
AdoptSubtree(T& node, AdoptedNodeN&&... node_n) {
  using tree_operations_internal::ReserveIfSupported;
  ReserveIfSupported(node.Children(),
                     std::size(node.Children()) + sizeof...(node_n));
  (node.Children().push_back(std::forward<AdoptedNodeN>(node_n)), ...);
}

// This node takes/moves subtrees from another node (concatenates).
// There need not be any relationship between this node and the other.
//
// Requires `push_back()` and `clear()` method in the children container.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void AdoptSubtreesFrom(T& node, T* other) {
  using tree_operations_internal::ReserveIfSupported;
  auto& src_children = other->Children();
  ReserveIfSupported(node.Children(),
                     std::size(node.Children()) + std::size(src_children));
  for (auto& src_child : src_children) {
    node.Children().push_back(std::move(src_child));
  }
  other->Children().clear();
}

// Recursively transform a SrcTree to DstTree.
// The resulting tree is always StructureEqual() to the original tree.
//
// Usage:
//   auto tree_other = orig_tree.Transform<OtherType>(
//       [](const SrcTree& node) { return ... });
//
// Requires `push_back()` method in the children container.
template <
    class DstTree, class SrcTree, class SrcNodeToDstValueFunc,
    class DstValue_ =
        std::invoke_result_t<SrcNodeToDstValueFunc, const SrcTree&>,  //
    std::enable_if_t<TreeNodeTraits<DstTree>::available &&
                     TreeNodeTraits<SrcTree>::available &&
                     std::is_constructible_v<DstTree, DstValue_>>* = nullptr>
DstTree Transform(const SrcTree& src_node, const SrcNodeToDstValueFunc& f) {
  using tree_operations_internal::ReserveIfSupported;
  // Using invoke() to allow passing SrcTree's method pointers as `f`
  DstTree dst_node(std::invoke(f, src_node));
  ReserveIfSupported(dst_node.Children(), std::size(src_node.Children()));
  for (const auto& child : src_node.Children()) {
    AdoptSubtree(dst_node, Transform<DstTree>(child, f));
  }
  return dst_node;
}

// If this node has exactly one child, replace this node with that child
// and return true, otherwise, do nothing and return false.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
bool HoistOnlyChild(T& node) {
  if (std::size(node.Children()) != 1) return false;
  // Can't do this directly, as assignment to node destroys its child
  // (`only`) before it is moved.
  auto only = std::move(*node.Children().begin());
  node = std::move(only);
  return true;
}

// Combines the Nth and (N+1) sibling using a custom function 'joiner' on the
// nodes' values, and the Nth sibling will adopt N+1's children.
// The 'joiner' function does: *left = f(*left, *right);
// The (N+1) sibling will be erased in the process, and every sibling
// thereafter will be shifted back one position. Depending on a children
// container type this can invalidate all interators after position N and
// interators to Nth node children. This applies e.g. for vector-like
// containers, with contiguous storage.
//
// Requires `Value()` method in the node.
// Requires `push_back()` and `erase()` methods in the children container.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::Value::available &&
                           !std::is_const_v<T>>* = nullptr>
void MergeConsecutiveSiblings(
    T& node, size_t N,
    std::function<void(typename TreeNodeTraits<T>::Value::type*,
                       typename TreeNodeTraits<T>::Value::const_reference)>
        joiner) {
  CHECK_LT(N + 1, std::size(node.Children()));

  // Combine value into node[N].
  const auto nth_child = std::next(node.Children().begin(), N);
  const auto next_child = std::next(nth_child);
  joiner(&nth_child->Value(), next_child->Value());

  // Move-concatenate children to node[N].
  verible::AdoptSubtreesFrom(*nth_child, &*next_child);

  // Shift-left children_ by 1 beyond N.
  node.Children().erase(next_child);  // done via move-assignment
}

// Replace all direct children of this node with concatenated grandchildren.
// Retains the value of this node.  Discards direct childrens' values.
//
// Requires `insert()` and `erase()` methods in the children container.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void FlattenOnce(T& node) {
  const int grandchildren_count = std::transform_reduce(
      node.Children().begin(), node.Children().end(), 0, std::plus<>(),
      [](const T& gc) { return std::size(gc.Children()); });
  using tree_operations_internal::ReserveIfSupported;

  // Build new children list in a standalone container, then move-assign it to
  // this node's children container.
  using Container = typename TreeNodeTraits<T>::Children::container_type;
  Container grandchildren;
  ReserveIfSupported(grandchildren, grandchildren_count);

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
//
// Requires `push_back()` method in the children container.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void FlattenOnlyChildrenWithChildren(
    T& node, std::vector<size_t>* new_offsets = nullptr) {
  const int new_children_count = std::transform_reduce(
      node.Children().begin(), node.Children().end(), 0, std::plus<>(),
      [](const T& gc) {
        return std::max<size_t>(std::size(gc.Children()), 1u);
      });
  using tree_operations_internal::ReserveIfSupported;

  // Build new children list in a standalone container, then move-assign it to
  // this node's children container.
  using Container = typename TreeNodeTraits<T>::Children::container_type;
  Container new_children;

  ReserveIfSupported(new_children, new_children_count);

  if (new_offsets != nullptr) {
    new_offsets->clear();
    new_offsets->reserve(std::size(node.Children()));
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
//
// Requires `insert()` and `erase()` methods in the children container.
template <class T,  //
          std::enable_if_t<TreeNodeTraits<T>::available &&
                           !std::is_const_v<T>>* = nullptr>
void FlattenOneChild(T& node, size_t i) {
  const size_t original_size = std::size(node.Children());
  CHECK_LT(i, original_size);

  auto ith_child = std::next(node.Children().begin(), i);

  if (is_leaf(*ith_child)) {
    // Empty list of grandchildren, just remove the child.
    node.Children().erase(ith_child);
    return;
  }

  // Move-insert all grandchildren except the first one after the child.
  node.Children().insert(
      std::next(ith_child, 1),
      std::make_move_iterator(std::next(ith_child->Children().begin(), 1)),
      std::make_move_iterator(ith_child->Children().end()));
  // Possible reallocation and iterator invalidation above; update iterator.
  ith_child = std::next(node.Children().begin(), i);
  // Move the first grandchild into the child's place. Can't do this directly,
  // as assignment to *ith_child destroys the grandchild before it is moved.
  auto first_granchild = std::move(*ith_child->Children().begin());
  *ith_child = std::move(first_granchild);
}

// Construct a path of BirthRank()s from root to this.
// Root node's 'path' is empty.  Passing the resulting path to
// root.DescendPath() gets you back to this node.
// PathType can be any container with a `push_back()` interface.
//
// Requires `Parent()` method in the node.
template <class T, class PathType,  //
          std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
void Path(const T& node, PathType& path) {
  if (node.Parent() != nullptr) {
    Path(*node.Parent(), path);
    path.push_back(verible::BirthRank(node));
  }
}

// Stream-printable representation of the location of a node under its
// greatest ancestor (root).
// Usage: stream << NodePath(node);
//
// Requires `Parent()` method in the node.
struct NodePath {
  template <class T,
            std::enable_if_t<TreeNodeTraits<T>::Parent::available>* = nullptr>
  explicit NodePath(const T& node) {
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
template <typename LT, typename RT,
          std::enable_if_t<TreeNodeTraits<LT>::available &&
                           TreeNodeTraits<RT>::available>* = nullptr>
struct TreeNodePair {
  TreeNodePair() {}
  TreeNodePair(const LT* l, const RT* r) : left(l), right(r) {}
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
//
// Requires `Value()` method in the node.
template <typename LT, typename RT,
          std::enable_if_t<TreeNodeTraits<LT>::Value::available &&
                           TreeNodeTraits<RT>::Value::available>* = nullptr>
TreeNodePair<LT, RT> DeepEqual(
    const LT& left, const RT& right,
    const std::function<
        bool(typename TreeNodeTraits<LT>::Value::const_reference,
             typename TreeNodeTraits<RT>::Value::const_reference)>& comp) {
  using result_type = TreeNodePair<LT, RT>;
  // Node value comparison at current level.
  if (!comp(left.Value(), right.Value())) {
    return result_type{&left, &right};
  }

  // Subtree comparison: check number of children first, returning early if
  // different.
  const auto& left_children = left.Children();
  const auto& right_children = right.Children();
  if (std::size(left_children) != std::size(right_children)) {
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
//
// Requires `Value()` method in the node.
template <typename LT, typename RT,
          std::enable_if_t<TreeNodeTraits<LT>::Value::available &&
                           TreeNodeTraits<RT>::Value::available>* = nullptr>
TreeNodePair<LT, RT> DeepEqual(const LT& left, const RT& right) {
  using LValueRef = typename TreeNodeTraits<LT>::Value::const_reference;
  using RValueRef = typename TreeNodeTraits<RT>::Value::const_reference;
  return DeepEqual(left, right,
                   [](LValueRef l, RValueRef r) { return l == r; });
}

// If both trees are structurally the same, node-for-node, returns a pair of
// nullptrs.  Otherwise, returns with pointers to the first encountered nodes
// that differ in substructure.  Traversal order: pre-order.
// Implementation: this is just a degenerate form of DeepEqual, where the value
// comparison is ignored (always returns true).
//
// Requires `Value()` method in the node.
template <typename LT, typename RT,
          std::enable_if_t<TreeNodeTraits<LT>::Value::available &&
                           TreeNodeTraits<RT>::Value::available>* = nullptr>
TreeNodePair<LT, RT> StructureEqual(const LT& left, const RT& right) {
  using LValueRef = typename TreeNodeTraits<LT>::Value::const_reference;
  using RValueRef = typename TreeNodeTraits<RT>::Value::const_reference;
  // Ignore node values by always treating them as equal.
  return DeepEqual(left, right, [](LValueRef, RValueRef) { return true; });
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_TREE_OPERATIONS_H_
