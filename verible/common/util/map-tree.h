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

#ifndef VERIBLE_COMMON_UTIL_MAP_TREE_H_
#define VERIBLE_COMMON_UTIL_MAP_TREE_H_

#include <cstddef>
#include <functional>
#include <iostream>
#include <map>
#include <utility>

#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"

namespace verible {

// MapTree is a hierarchical tree representation of values, where branches are
// associated with keys.
// This is one implementation of a 'trie' or 'prefix tree' data structure.
// There are minimal constraints on the key and value types:
//   K is the key type of each node.
//     K needs to be copy-able.
//   V is the type of data stored at each node.
//     V only needs to be move-able, and need not be copy-able.
//     Copy-ability is only needed when deep-copying the tree.
//   KeyComp is the comparator for K for ordering.
//     KeyComp can be a heterogenous lookup comparator (C++14).
//
// Key-value pairs are co-located together, and are iterator-stable, meaning
// that insertion/deletion operations do not invalidate existing iterators
// or existing pointers to other nodes in the same family tree.
// Insertion/deletion/search operations are O(lg N).
//
// Nodes maintain links to their parent (except the root node), so upwards
// navigation toward the root is always possible.
//
// Navigation:
//
//   MapTree (parent) <----------------.
//     |                                |
//     | .Find(key) -> iterator         |
//     |                                | .Parent() -> MapTree*
//     v  key_value_type (pair)         |
//   +=============+==================+ /
//   | .first:     | .second:         |/
//   |     key     |  MapTree (child) |
//   |   key_type  |  node_value_type |
//   +=============+==================+
//        ^                 |
//        |                 |
//         \---------------/
//              .Key() -> key_type*
//
// Example applications:
// * Dictionaries, where K is a single char.
// * Hierarchical symbol tables, where K is string-like.
//   * Lookup operations could use upward search.
// * Subcommand menus, where K is string-like.
// * File-system like structures, with string-like K.
//   * Navigation through parent directories uses upward links.
//
template <typename K, typename V, typename KeyComp = std::less<K>>
class MapTree {
  using this_type = MapTree<K, V, KeyComp>;

  // Self-recursive type that holds subtrees.
  // A std::map is chosen for key-value co-location and iterator stability.
  // We chose std::map over std::set to allow values to be mutable, while keys
  // remain const.
  using subtrees_type = std::map<K, this_type, KeyComp>;

 public:
  using key_type = K;
  using node_value_type = V;
  using key_value_type = typename subtrees_type::value_type;  // pair
  using iterator = typename subtrees_type::iterator;
  using const_iterator = typename subtrees_type::const_iterator;

  MapTree() = default;

  // deep-copy, requires node_value_type to be copy-able.
  MapTree(const MapTree &other)
      : node_value_(other.node_value_),
        subtrees_(other.subtrees_),
        // new copy is disconnected from original parent and is a new root
        parent_(nullptr) {
    Relink();
  }

  // move (with relink)
  MapTree(MapTree &&other) noexcept
      : node_value_(std::move(other.node_value_)),
        subtrees_(std::move(other.subtrees_)),
        // Retain existing parent.
        parent_(other.parent_) {
    Relink();
  }

  // TODO(fangism): implement assignments as needed
  MapTree &operator=(const MapTree &) = delete;
  MapTree &operator=(MapTree &&) = delete;

  // Recursively initialize trees (copy node value).
  // Example:
  //   using M = MapTree<...>;
  //   using P = M::key_value_type;  // pair
  //   M tree(
  //     value0,
  //     P{key1, M(value1,
  //               P{key2, M(value2)},
  //               P{key3, M(value3)})},
  //     P{key4, M(value4,
  //               P{key5, M(value5)},
  //               P{key6, M(value6)})}
  //   );
  template <typename... Args>
  explicit MapTree(const node_value_type &v, Args &&...args)
      : node_value_(v), subtrees_() {
    EmplacePairs(std::forward<Args>(args)...);
  }

  // Recursively initialize trees (move node value).
  // See example above, using node_value_type copy.
  template <typename... Args>
  explicit MapTree(node_value_type &&v, Args &&...args)
      : node_value_(std::move(v)), subtrees_() {
    EmplacePairs(std::forward<Args>(args)...);
  }

  ~MapTree() { CHECK(CheckIntegrity()); }

  void swap(this_type &other) noexcept {
    std::swap(node_value_, other.node_value_);
    subtrees_.swap(other.subtrees_);
    Relink();
    other.Relink();
  }

  // Validation

  // Ensure parent-child mutual linkage invariant.
  // This property should hold after any mutating operation.
  bool CheckIntegrity() const {
    for (const auto &node : *this) {
      CHECK_EQ(node.second.Parent(), this)
          << "Inconsistency: child's parent does not point back to this node!";
      if (!node.second.CheckIntegrity()) return false;
    }
    return true;
  }

  // Insertion

  // Inserts a node at 'key' if it doesn't already exist.
  // 'args' are type V's constructor arguments, forwarded.
  // Returns (iterator, bool), where iterator points to the element at 'key'
  // whether or not it was newly inserted or already there, and true to indicate
  // that it was newly created, false if an entry already existed at that key.
  template <typename... Args>
  std::pair<iterator, bool> TryEmplace(const key_type &key, Args &&...args) {
    const auto p = subtrees_.emplace(
        key_value_type{key, node_value_type(std::forward<Args>(args)...)});
    if (p.second) {
      // Link child to parent.
      p.first->second.parent_ = this;
    }
    return p;
  }

  // No-op base case for variadic EmplacePairs().
  void EmplacePairs() const {}

  // Appends one or more sub-trees at this level.
  // Variadic template handles one argument at a time.
  // F and Args should each be key-value pairs (key_value_type).
  // If there are duplicate keys, only the first of each key will be taken,
  // while the other duplicates will be dropped.
  template <typename F, typename... Args>
  void EmplacePairs(F &&first, Args &&...args) {
    const auto p = subtrees_.emplace(std::forward<F>(first));
    if (p.second) {
      // Link child to parent.
      p.first->second.parent_ = this;
    }
    // Emplace the remaining items.
    EmplacePairs(std::forward<Args>(args)...);
  }

  // TODO(fangism): TryEmplaceHint(), like map::emplace_hint.

  // Erasure
  // TODO(fangism): void Erase(key);

  // Iteration/Navigation

  this_type *Parent() { return parent_; }
  const this_type *Parent() const { return parent_; }

  iterator begin() { return subtrees_.begin(); }
  const_iterator begin() const { return subtrees_.begin(); }
  iterator end() { return subtrees_.end(); }
  const_iterator end() const { return subtrees_.end(); }

  bool is_leaf() const { return Children().empty(); }

  size_t NumAncestors() const {
    size_t depth = 0;
    for (const this_type *iter = Parent(); iter != nullptr;
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
  bool HasAncestor(const this_type *other) const {
    if (other == nullptr) return false;
    for (const this_type *iter = Parent(); iter != nullptr;
         iter = iter->Parent()) {
      if (iter == other) return true;
    }
    return false;
  }

  // Returns pointer to the tree root, the greatest ancestor of this node.
  const this_type *Root() const {
    const this_type *node = this;
    while (node->Parent() != nullptr) node = node->Parent();
    return node;
  }

  // Returns pointer to the key-value pair of which this node is the value.
  // Root nodes return nullptr because they have no associated key.
  const key_value_type *KeyValuePair() const {
    // Note: The use of offsetof below is technically undefined until C++17
    // because std::pair is not a standard layout type. However, all compilers
    // currently provide well-defined behavior as an extension (which is
    // demonstrated since constexpr evaluation must diagnose all undefined
    // behavior). However, GCC and Clang also warn about this use of offsetof,
    // which must be suppressed.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    // Compute offset of member &key_value_type::second (built-in function).
    static constexpr auto value_offset = offsetof(key_value_type, second);
    if (Parent() == nullptr) return nullptr;  // Root node has no key
    // All non-root nodes belong to a parent's map, and thus are
    // pair-co-located with a key.
    // Use pointer arithmetic to recover location of the key-value pair to which
    // this node value belongs.
    return reinterpret_cast<const key_value_type *>(
        reinterpret_cast<const char *>(this) - value_offset);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    // TODO(fangism): factor this out into member_offsetof() template function.
  }

  // Returns the key associated with this node (if this is not a root),
  // otherwise return nullptr.
  const key_type *Key() const {
    if (Parent() == nullptr) return nullptr;  // Root node has no key
    return &KeyValuePair()->first;
  }

  // Access

  const subtrees_type &Children() const { return subtrees_; }

  node_value_type &Value() { return node_value_; }
  const node_value_type &Value() const { return node_value_; }

  // Search

  // Returns an iterator located at 'key' or end() if not found.
  // O(lg N), same as underlying map type.
  template <typename AnyKey>
  iterator Find(AnyKey &&key) {
    // Forward to underlying map::find, enabling heterogenous lookup.
    return subtrees_.find(std::forward<AnyKey>(key));
  }

  // Returns a const_iterator located at 'key' or end() if not found.
  // O(lg N), same as underlying map type.
  template <typename AnyKey>
  const_iterator Find(AnyKey &&key) const {
    // Forward to underlying map::find, enabling heterogenous lookup.
    return subtrees_.find(std::forward<AnyKey>(key));
  }

  // Traversals
  //
  // Function-application traversals' variants:
  //   * Apply in pre-order vs. post-order.
  //   * Apply to attached value at each node vs. the whole node itself.
  //     Whole node gives access to immediate children (and subtree).
  //   * Apply function is const vs. mutating.
  // All combinations of the above are provided.
  // Note that keys can never be mutated.

  // Applies function 'f' to all nodes in this tree in a pre-order traversal.
  // Children are visited in key-order.
  void ApplyPreOrder(const std::function<void(const this_type &)> &f) const {
    f(*this);
    for (const auto &child : *this) {
      child.second.ApplyPreOrder(f);
    }
  }
  void ApplyPreOrder(const std::function<void(this_type &)> &f) {
    f(*this);
    for (auto &child : *this) {
      child.second.ApplyPreOrder(f);
    }
  }

  // This variant of ApplyPreOrder expects a function on the underlying
  // node_value_type (const&).
  void ApplyPreOrder(
      const std::function<void(const node_value_type &)> &f) const {
    ApplyPreOrder([&f](const this_type &t) { f(t.Value()); });
  }
  void ApplyPreOrder(const std::function<void(node_value_type &)> &f) {
    ApplyPreOrder([&f](this_type &t) { f(t.Value()); });
  }

  // Applies function 'f' to all nodes in this tree in a post-order traversal.
  // Children are visited in key-order.
  void ApplyPostOrder(const std::function<void(const this_type &)> &f) const {
    for (const auto &child : *this) {
      child.second.ApplyPostOrder(f);
    }
    f(*this);
  }
  void ApplyPostOrder(const std::function<void(this_type &)> &f) {
    for (auto &child : *this) {
      child.second.ApplyPostOrder(f);
    }
    f(*this);
  }

  // This variant of ApplyPostOrder expects a function on the underlying
  // node_value_type (const&).
  void ApplyPostOrder(
      const std::function<void(const node_value_type &)> &f) const {
    ApplyPostOrder([&f](const this_type &t) { f(t.Value()); });
  }
  void ApplyPostOrder(const std::function<void(node_value_type &)> &f) {
    ApplyPostOrder([&f](this_type &t) { f(t.Value()); });
  }

  // Transforms

  // Printing and formatting

  // Pretty-print tree, using a custom node_value printer function.
  // Keys are printed using operator<<.
  std::ostream &PrintTree(std::ostream &stream,
                          const std::function<std::ostream &(
                              std::ostream &, const node_value_type &,
                              size_t value_indent)> &printer,
                          size_t indent = 0) const {
    // Indentation will be printed before the key, not here.
    printer(stream << "{ (", Value(), indent) << ')';
    if (Children().empty()) {
      stream << " }";
    } else {
      stream << '\n';
      for (const auto &child : Children()) {
        stream << Spacer(indent + 2) << child.first << ": ";
        child.second.PrintTree(stream, printer, indent + 2) << '\n';
      }
      stream << Spacer(indent) << '}';
    }
    return stream;
  }

  // Pretty-print tree, using the default stream printer, which requires that
  // operator<<(std::ostream&, const node_value_type&) is defined.
  std::ostream &PrintTree(std::ostream &stream, size_t indent = 0) const {
    return PrintTree(
        stream,
        [](std::ostream &s, const node_value_type &v,
           size_t unused_indent) -> std::ostream & { return s << v; },
        indent);
  }

 private:  // methods
  // Establish parent-child links.
  void Relink() {
    for (auto &subtree : subtrees_) {
      subtree.second.parent_ = this;
    }
  }

 private:  // data
  // Singular value stored at this node.
  node_value_type node_value_;

  // Collection of subtrees
  subtrees_type subtrees_;

  // Pointer to parent node, or nullptr if this is a root node.
  this_type *parent_ = nullptr;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_MAP_TREE_H_
