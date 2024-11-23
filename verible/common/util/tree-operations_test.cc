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

#include "verible/common/util/tree-operations.h"

#include <cstdlib>
#include <initializer_list>
#include <list>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/strings/str_cat.h"  // IWYU pragma: keep (not in all pp-branches)
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"  // IWYU pragma: keep
#include "gtest/gtest.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"
#include "verible/common/util/type-traits.h"

/*
  The compiler takes a long time to compile this file, so we divide it
  into shards, in each shard only compiling a subsection, determined by the
  value in the COMPILATION_SHARD define.

                        How to assign shards
  Run a separate compile for each shard, and look at reported time by bazel:

  for f in 1 2 3 4 5 6 ; do
      bazel build -c opt //verible/common/util:tree-operations_${f}_test
  done

  ... then choose shard in #if CURRENT_SHARD() sections until
  compilation times are roughly balanced.
*/

// If COMPILATION_SHARD is set, this is used to only compile part of the code.
#ifdef COMPILATION_SHARD
#define CURRENT_SHARD_IS(s) s == COMPILATION_SHARD
#else
#define CURRENT_SHARD_IS(s) 1
#endif

namespace verible {
namespace {

// Recursively verifies two trees.
// Not intended for direct use - use the other VerifyTree with
// EXPECT_PRED_FORMAT2 instead.
template <typename T>
testing::AssertionResult VerifyTree(const T &actual, const T &expected,
                                    const std::vector<size_t> &path) {
  const bool id_ok = actual.id() == expected.id();
  const bool children_count_ok =
      actual.Children().size() == expected.Children().size();
  if (!id_ok || !children_count_ok) {
    auto err = testing::AssertionFailure();
    err << "Node mismatch at path: {"
        << absl::StrJoin(path.begin(), path.end(), ",") << "}\n";

    if (!id_ok) {
      err << "Invalid ID:\n"
          << "  Actual:   \"" << actual.id() << "\"\n"
          << "  Expected: \"" << expected.id() << "\"\n";
    }
    if (!children_count_ok) {
      err << "Invalid Children count:\n"
          << "  Actual:   " << actual.Children().size() << "\n"
          << "  Expected: " << expected.Children().size() << "\n";
    }
    return err;
  }
  if (!actual.Children().empty()) {
    auto actual_child = actual.Children().begin();
    auto expected_child = expected.Children().begin();
    auto child_path = path;
    child_path.push_back(0);
    for (size_t i = 0; i < actual.Children().size(); ++i) {
      const auto result =
          VerifyTree(*actual_child, *expected_child, child_path);
      if (!result) return result;
      ++actual_child;
      ++expected_child;
      ++child_path.back();
    }
  }
  return testing::AssertionSuccess();
}

// Recursively verifies two trees.
// This verification function is intended for use with EXPECT_PRED_FORMAT2.
template <typename T>
testing::AssertionResult VerifyTree(const char *actual_expr,
                                    const char *expected_expr, const T &actual,
                                    const T &expected) {
  auto result = VerifyTree(actual, expected, {});
  if (!result) {
    result << "\n"
           << "Actual tree:\n"
           << actual << "\n"
           << "Expected tree:\n"
           << expected << "\n";
  }
  return result;
}

// Alias to `ThisType` if `Derived` is void; alias to `Derived` otherwise.
template <class Derived, class ThisType>
using DerivedOrThis =
    std::conditional_t<std::is_void_v<Derived>, ThisType, Derived>;

// Definitions of sample tree node types:

template <template <class...> class Container, class Derived = void>
class SimpleNode {
  using ThisType = DerivedOrThis<Derived, SimpleNode<Container, Derived>>;

 public:
  using ChildrenType = Container<ThisType>;

  explicit SimpleNode(absl::string_view id, ChildrenType &&children = {})
      : children_(std::move(children)), id_(id) {
    Relink();
  }

  ChildrenType &Children() { return children_; }
  const ChildrenType &Children() const { return children_; }

  // Debug/test functions:

  const std::string &id() const { return id_; }

  void set_id(std::string &&new_id) { id_ = new_id; }

  bool operator==(const ThisType &other) const { return this == &other; }
  bool operator!=(const ThisType &other) const { return !(*this == other); }

  friend std::ostream &operator<<(std::ostream &stream, const ThisType &self) {
    self.PrintRecursively(stream, 0);
    return stream;
  }

  friend std::ostream &operator<<(std::ostream &stream,
                                  const ThisType *self_ptr) {
    if (self_ptr == nullptr) {
      return stream << "nullptr";
    }
    return stream << absl::StreamFormat("%p (%s; parent=%p)\n", self_ptr,
                                        self_ptr->id_, self_ptr->parent_);
  }

  // Updates parent pointers in children.
  void Relink() {
    for (auto &child : children_) {
      child.Relink();
      child.parent_ = static_cast<ThisType *>(this);
    }
  }

 protected:
  void PrintRecursively(std::ostream &stream, size_t depth = 0) const {
    stream << verible::Spacer(4 * depth)
           << absl::StreamFormat("@%p (%s; parent=%p)\n",
                                 static_cast<const ThisType *>(this), id_,
                                 parent_);
    for (const auto &child : Children()) {
      child.PrintRecursively(stream, depth + 1);
    }
  }

  ChildrenType children_;
  std::string id_;              // Exposed in subclasses as value
  ThisType *parent_ = nullptr;  // Exposed in subclasses
};

template <template <class...> class Container, class Derived = void>
class NodeWithValue
    : public SimpleNode<
          Container,
          DerivedOrThis<Derived, NodeWithValue<Container, Derived>>> {
  using ThisType = DerivedOrThis<Derived, NodeWithValue>;
  using Base = SimpleNode<Container, ThisType>;

 public:
  using Base::Base;
  using typename Base::ChildrenType;

  std::string &Value() { return this->id_; }
  const std::string &Value() const { return this->id_; }
};

template <template <class...> class Container, class Derived = void>
class NodeWithParent
    : public SimpleNode<
          Container,
          DerivedOrThis<Derived, NodeWithParent<Container, Derived>>> {
  using ThisType = DerivedOrThis<Derived, NodeWithParent>;
  using Base = SimpleNode<Container, ThisType>;

 public:
  using Base::Base;
  using typename Base::ChildrenType;

  ThisType *Parent() { return this->parent_; }
  const ThisType *Parent() const { return this->parent_; }
};

template <template <class...> class Container, class Derived = void>
class NodeWithParentAndValue
    : public NodeWithParent<
          Container,
          DerivedOrThis<Derived, NodeWithParentAndValue<Container, Derived>>> {
  using ThisType = DerivedOrThis<Derived, NodeWithParentAndValue>;
  using Base = NodeWithParent<Container, ThisType>;

 public:
  using Base::Base;
  using typename Base::ChildrenType;

  std::string &Value() { return this->id_; }
  const std::string &Value() const { return this->id_; }
};

// "Other" node type. Has different value type and is not related to other test
// trees.
class IntNode {
 public:
  IntNode() = default;
  explicit IntNode(int value, std::initializer_list<IntNode> children = {})
      : value_(value), children_(children) {}

  const int &Value() const { return value_; }

  const std::vector<IntNode> &Children() const { return children_; }
  std::vector<IntNode> &Children() { return children_; }

  const int &id() const { return value_; }

#if CURRENT_SHARD_IS(6)
  friend std::ostream &operator<<(std::ostream &stream, const IntNode &self) {
    self.PrintRecursively(stream, 0);
    return stream;
  }

  friend std::ostream &operator<<(std::ostream &stream,
                                  const IntNode *self_ptr) {
    if (self_ptr == nullptr) {
      return stream << "nullptr";
    }
    return stream << absl::StreamFormat("%p (%d)\n", self_ptr,
                                        self_ptr->value_);
  }
#endif

 private:
  void PrintRecursively(std::ostream &stream, size_t depth = 0) const {
    stream << verible::Spacer(4 * depth)
           << absl::StreamFormat("@%p (%d)\n", this, value_);
    for (const auto &child : Children()) {
      child.PrintRecursively(stream, depth + 1);
    }
  }

  int value_;
  std::vector<IntNode> children_;
};

// Test suites:

template <class Node>
class TreeTest : public ::testing::Test {
 public:
  TreeTest() = default;

  using N = Node;
  N root = N("root", {N("0"),                                          //
                      N("1", {N("1.0")}),                              //
                      N("2", {N("2.0", {N("2.0.0")}),                  //
                              N("2.1", {N("2.1.0")})}),                //
                      N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                        N("3.0.1", {N("3.0.1.0")})}),  //
                              N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                        N("3.1.1", {N("3.1.1.0")})}),  //
                              N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                        N("3.2.1", {N("3.2.1.0")})})})});

  Node &NodeAt(std::initializer_list<size_t> child_indexes) {
    Node *subnode = &root;
    for (auto index : child_indexes) {
      CHECK_GE(index, 0);
      CHECK_LT(index, subnode->Children().size());
      subnode = &*std::next(subnode->Children().begin(), index);
    }
    return *subnode;
  }
};

template <class Node>
class SimpleNodeTest : public TreeTest<Node> {};

using SimpleNodeTestTypes =
    ::testing::Types<SimpleNode<std::vector>,              //
                     NodeWithValue<std::vector>,           //
                     NodeWithParent<std::vector>,          //
                     NodeWithParentAndValue<std::vector>,  //

                     SimpleNode<std::list>,              //
                     NodeWithValue<std::list>,           //
                     NodeWithParent<std::list>,          //
                     NodeWithParentAndValue<std::list>,  //

                     const SimpleNode<std::vector>,              //
                     const NodeWithValue<std::vector>,           //
                     const NodeWithParent<std::vector>,          //
                     const NodeWithParentAndValue<std::vector>,  //

                     const SimpleNode<std::list>,      //
                     const NodeWithValue<std::list>,   //
                     const NodeWithParent<std::list>,  //
                     const NodeWithParentAndValue<std::list>>;
TYPED_TEST_SUITE(SimpleNodeTest, SimpleNodeTestTypes);

template <class Node>
class NodeWithValueTest : public TreeTest<Node> {};

using NodeWithValueTestTypes =
    ::testing::Types<NodeWithValue<std::vector>,           //
                     NodeWithParentAndValue<std::vector>,  //

                     NodeWithValue<std::list>,           //
                     NodeWithParentAndValue<std::list>,  //

                     const NodeWithValue<std::vector>,           //
                     const NodeWithParentAndValue<std::vector>,  //

                     const NodeWithValue<std::list>,  //
                     const NodeWithParentAndValue<std::list>>;
TYPED_TEST_SUITE(NodeWithValueTest, NodeWithValueTestTypes);

template <class Node>
class NodeWithParentTest : public TreeTest<Node> {};

using NodeWithParentTestTypes =
    ::testing::Types<NodeWithParent<std::vector>,          //
                     NodeWithParentAndValue<std::vector>,  //

                     NodeWithParent<std::list>,          //
                     NodeWithParentAndValue<std::list>,  //

                     const NodeWithParent<std::vector>,          //
                     const NodeWithParentAndValue<std::vector>,  //

                     const NodeWithParent<std::list>,  //
                     const NodeWithParentAndValue<std::list>>;
TYPED_TEST_SUITE(NodeWithParentTest, NodeWithParentTestTypes);

template <class Node>
class NodeWithParentAndValueTest : public TreeTest<Node> {};

using NodeWithParentAndValueTestTypes =
    ::testing::Types<NodeWithParentAndValue<std::vector>,        //
                     NodeWithParentAndValue<std::list>,          //
                     const NodeWithParentAndValue<std::vector>,  //
                     const NodeWithParentAndValue<std::list>>;
TYPED_TEST_SUITE(NodeWithParentAndValueTest, NodeWithParentAndValueTestTypes);

// Tests:

TEST(Misc, TreeNodeTraits) {
  struct NodeWithCustomContainerType {
    using subnodes_type = std::initializer_list<NodeWithCustomContainerType>;

    const std::vector<NodeWithCustomContainerType> &Children() const {
      return children_;
    }

   private:
    std::vector<NodeWithCustomContainerType> children_;
  };

  {
    using Traits = TreeNodeTraits<NodeWithCustomContainerType>;
    static_assert(Traits::available == true);
    static_assert(Traits::Children::available == true);

    static_assert(
        std::is_same_v<typename Traits::Children::container_type,
                       std::initializer_list<NodeWithCustomContainerType>>);
  }
  {
    using Traits = TreeNodeTraits<const NodeWithCustomContainerType>;
    static_assert(Traits::available == true);
    static_assert(Traits::Children::available == true);

    static_assert(
        std::is_same_v<typename Traits::Children::container_type,
                       std::initializer_list<NodeWithCustomContainerType>>);
  }
}

#if CURRENT_SHARD_IS(1)
TYPED_TEST(SimpleNodeTest, TreeNodeTraits) {
  // This test passes if it compiles without errors.
  using Traits = TreeNodeTraits<TypeParam>;

  static_assert(Traits::available == true);
  static_assert(Traits::Children::available == true);

  static_assert(std::is_same_v<typename Traits::Children::container_type,
                               typename TypeParam::ChildrenType>);
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(NodeWithValueTest, TreeNodeTraits) {
  // This test passes if it compiles without errors.
  using Traits = TreeNodeTraits<TypeParam>;

  static_assert(Traits::Value::available == true);

  static_assert(std::is_same_v<typename Traits::Value::type, std::string>);
  static_assert(
      std::is_same_v<typename Traits::Value::reference,
                     verible::match_const_t<std::string, TypeParam> &>);
  static_assert(std::is_same_v<typename Traits::Value::const_reference,
                               const std::string &>);
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(NodeWithParentTest, TreeNodeTraits) {
  // This test passes if it compiles without errors.
  using Traits = TreeNodeTraits<TypeParam>;

  static_assert(Traits::Parent::available == true);
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(SimpleNodeTest, is_leaf) {
  EXPECT_FALSE(is_leaf(this->root));
  EXPECT_TRUE(is_leaf(this->NodeAt({0})));
  EXPECT_FALSE(is_leaf(this->NodeAt({1})));
  EXPECT_TRUE(is_leaf(this->NodeAt({1, 0})));
  EXPECT_FALSE(is_leaf(this->NodeAt({2})));
  EXPECT_FALSE(is_leaf(this->NodeAt({2, 0})));
  EXPECT_TRUE(is_leaf(this->NodeAt({2, 0, 0})));
  EXPECT_FALSE(is_leaf(this->NodeAt({2, 1})));
  EXPECT_TRUE(is_leaf(this->NodeAt({2, 1, 0})));
  EXPECT_FALSE(is_leaf(this->NodeAt({3})));
  EXPECT_FALSE(is_leaf(this->NodeAt({3, 0})));
  EXPECT_FALSE(is_leaf(this->NodeAt({3, 0, 0})));
  EXPECT_TRUE(is_leaf(this->NodeAt({3, 0, 0, 0})));
  EXPECT_FALSE(is_leaf(this->NodeAt({3, 0, 1})));
  EXPECT_TRUE(is_leaf(this->NodeAt({3, 0, 1, 0})));
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(SimpleNodeTest, DescendPath) {
  {
    const std::vector<size_t> path = {0};
    EXPECT_EQ(DescendPath(this->root, path.begin(), path.end()),
              this->NodeAt({0}));
  }
  {
    const std::vector<size_t> path = {1, 0};
    EXPECT_EQ(DescendPath(this->root, path.begin(), path.end()),
              this->NodeAt({1, 0}));
  }
  {
    const std::vector<size_t> path = {1};
    EXPECT_EQ(DescendPath(this->NodeAt({2}), path.begin(), path.end()),
              this->NodeAt({2, 1}));
  }
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(SimpleNodeTest, LeftmostDescendant) {
  EXPECT_EQ(LeftmostDescendant(this->root), this->root.Children().front());
  EXPECT_EQ(LeftmostDescendant(this->root.Children().back()),
            this->NodeAt({3, 0, 0, 0}));
  EXPECT_EQ(LeftmostDescendant(this->NodeAt({2, 1, 0})),
            this->NodeAt({2, 1, 0}));
}
#endif

#if CURRENT_SHARD_IS(2)
TYPED_TEST(SimpleNodeTest, RightmostDescendant) {
  EXPECT_EQ(RightmostDescendant(this->root), this->NodeAt({3, 2, 1, 0}));
  EXPECT_EQ(RightmostDescendant(this->root.Children().back()),
            this->NodeAt({3, 2, 1, 0}));
  EXPECT_EQ(RightmostDescendant(this->NodeAt({2, 1, 0})),
            this->NodeAt({2, 1, 0}));
}
#endif

#if CURRENT_SHARD_IS(2)
TYPED_TEST(SimpleNodeTest, ApplyPreOrderWithNode) {
  using NodeRef = std::add_lvalue_reference_t<TypeParam>;
  {
    static const std::vector<std::string> expected_visited_values = {
        "root",    "0",       "1",     "1.0",     "2",     "2.0",     "2.0.0",
        "2.1",     "2.1.0",   "3",     "3.0",     "3.0.0", "3.0.0.0", "3.0.1",
        "3.0.1.0", "3.1",     "3.1.0", "3.1.0.0", "3.1.1", "3.1.1.0", "3.2",
        "3.2.0",   "3.2.0.0", "3.2.1", "3.2.1.0"};
    std::vector<std::string> visited_values;
    ApplyPreOrder(this->root,
                  [&](NodeRef node) { visited_values.push_back(node.id()); });
    EXPECT_EQ(visited_values, expected_visited_values);
  }

  // Test for mutable nodes only
  if constexpr (!std::is_const_v<TypeParam>) {
    // Modify
    ApplyPreOrder(this->root, [&](NodeRef node) {
      node.set_id(absl::StrCat(node.id(), "-new"));
    });
    // Verify
    static const std::vector<std::string> expected_visited_values = {
        "root-new", "0-new",     "1-new",       "1.0-new",   "2-new",
        "2.0-new",  "2.0.0-new", "2.1-new",     "2.1.0-new", "3-new",
        "3.0-new",  "3.0.0-new", "3.0.0.0-new", "3.0.1-new", "3.0.1.0-new",
        "3.1-new",  "3.1.0-new", "3.1.0.0-new", "3.1.1-new", "3.1.1.0-new",
        "3.2-new",  "3.2.0-new", "3.2.0.0-new", "3.2.1-new", "3.2.1.0-new"};
    std::vector<std::string> visited_values;
    ApplyPreOrder(this->root,
                  [&](NodeRef node) { visited_values.push_back(node.id()); });

    EXPECT_EQ(visited_values, expected_visited_values);
  }
}
#endif

#if CURRENT_SHARD_IS(2)
TYPED_TEST(SimpleNodeTest, ApplyPostOrderWithNode) {
  using NodeRef = std::add_lvalue_reference_t<TypeParam>;
  {
    static const std::vector<std::string> expected_visited_values = {
        "0",     "1.0",     "1",     "2.0.0",   "2.0",     "2.1.0", "2.1",
        "2",     "3.0.0.0", "3.0.0", "3.0.1.0", "3.0.1",   "3.0",   "3.1.0.0",
        "3.1.0", "3.1.1.0", "3.1.1", "3.1",     "3.2.0.0", "3.2.0", "3.2.1.0",
        "3.2.1", "3.2",     "3",     "root"};
    std::vector<std::string> visited_values;
    ApplyPostOrder(this->root,
                   [&](NodeRef node) { visited_values.push_back(node.id()); });
    EXPECT_EQ(visited_values, expected_visited_values);
  }
  // Test for mutable nodes only
  if constexpr (!std::is_const_v<TypeParam>) {
    // Modify
    ApplyPostOrder(this->root, [&](NodeRef node) {
      node.set_id(absl::StrCat(node.id(), "-new"));
    });
    // Verify
    static const std::vector<std::string> expected_visited_values = {
        "0-new",       "1.0-new",   "1-new",   "2.0.0-new",   "2.0-new",
        "2.1.0-new",   "2.1-new",   "2-new",   "3.0.0.0-new", "3.0.0-new",
        "3.0.1.0-new", "3.0.1-new", "3.0-new", "3.1.0.0-new", "3.1.0-new",
        "3.1.1.0-new", "3.1.1-new", "3.1-new", "3.2.0.0-new", "3.2.0-new",
        "3.2.1.0-new", "3.2.1-new", "3.2-new", "3-new",       "root-new"};
    std::vector<std::string> visited_values;
    ApplyPostOrder(this->root,
                   [&](NodeRef node) { visited_values.push_back(node.id()); });

    EXPECT_EQ(visited_values, expected_visited_values);
  }
}
#endif

#if CURRENT_SHARD_IS(2)
TYPED_TEST(NodeWithValueTest, ApplyPreOrderWithValue) {
  {
    static const std::vector<std::string> expected_visited_values = {
        "root",    "0",       "1",     "1.0",     "2",     "2.0",     "2.0.0",
        "2.1",     "2.1.0",   "3",     "3.0",     "3.0.0", "3.0.0.0", "3.0.1",
        "3.0.1.0", "3.1",     "3.1.0", "3.1.0.0", "3.1.1", "3.1.1.0", "3.2",
        "3.2.0",   "3.2.0.0", "3.2.1", "3.2.1.0"};
    std::vector<std::string> visited_values;
    ApplyPreOrder(this->root, [&](const std::string &value) {
      visited_values.push_back(value);
    });
    EXPECT_EQ(visited_values, expected_visited_values);
  }

  // Test for mutable nodes only
  if constexpr (!std::is_const_v<TypeParam>) {
    // Modify
    ApplyPreOrder(this->root,
                  [&](std::string &value) { absl::StrAppend(&value, "-new"); });
    // Verify
    static const std::vector<std::string> expected_visited_values = {
        "root-new", "0-new",     "1-new",       "1.0-new",   "2-new",
        "2.0-new",  "2.0.0-new", "2.1-new",     "2.1.0-new", "3-new",
        "3.0-new",  "3.0.0-new", "3.0.0.0-new", "3.0.1-new", "3.0.1.0-new",
        "3.1-new",  "3.1.0-new", "3.1.0.0-new", "3.1.1-new", "3.1.1.0-new",
        "3.2-new",  "3.2.0-new", "3.2.0.0-new", "3.2.1-new", "3.2.1.0-new"};
    std::vector<std::string> visited_values;
    ApplyPreOrder(this->root, [&](const std::string &value) {
      visited_values.push_back(value);
    });

    EXPECT_EQ(visited_values, expected_visited_values);
  }
}
#endif

#if CURRENT_SHARD_IS(2)
TYPED_TEST(NodeWithValueTest, ApplyPostOrderWithValue) {
  {
    static const std::vector<std::string> expected_visited_values = {
        "0",     "1.0",     "1",     "2.0.0",   "2.0",     "2.1.0", "2.1",
        "2",     "3.0.0.0", "3.0.0", "3.0.1.0", "3.0.1",   "3.0",   "3.1.0.0",
        "3.1.0", "3.1.1.0", "3.1.1", "3.1",     "3.2.0.0", "3.2.0", "3.2.1.0",
        "3.2.1", "3.2",     "3",     "root"};
    std::vector<std::string> visited_values;
    ApplyPostOrder(this->root, [&](const std::string &value) {
      visited_values.push_back(value);
    });
    EXPECT_EQ(visited_values, expected_visited_values);
  }

  // Test for mutable nodes only
  if constexpr (!std::is_const_v<TypeParam>) {
    // Modify
    ApplyPostOrder(this->root, [&](std::string &value) {
      absl::StrAppend(&value, "-new");
    });
    // Verify
    static const std::vector<std::string> expected_visited_values = {
        "0-new",       "1.0-new",   "1-new",   "2.0.0-new",   "2.0-new",
        "2.1.0-new",   "2.1-new",   "2-new",   "3.0.0.0-new", "3.0.0-new",
        "3.0.1.0-new", "3.0.1-new", "3.0-new", "3.1.0.0-new", "3.1.0-new",
        "3.1.1.0-new", "3.1.1-new", "3.1-new", "3.2.0.0-new", "3.2.0-new",
        "3.2.1.0-new", "3.2.1-new", "3.2-new", "3-new",       "root-new"};
    std::vector<std::string> visited_values;
    ApplyPostOrder(this->root, [&](const std::string &value) {
      visited_values.push_back(value);
    });

    EXPECT_EQ(visited_values, expected_visited_values);
  }
}
#endif

#if CURRENT_SHARD_IS(2)
TYPED_TEST(NodeWithParentTest, BirthRank) {
  EXPECT_EQ(BirthRank(this->root), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({1})), 1);
  EXPECT_EQ(BirthRank(this->NodeAt({1, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({2})), 2);
  EXPECT_EQ(BirthRank(this->NodeAt({2, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({2, 0, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({2, 1})), 1);
  EXPECT_EQ(BirthRank(this->NodeAt({2, 1, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3})), 3);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 0, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 0, 0, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 0, 1})), 1);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 0, 1, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 1})), 1);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 1, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 1, 0, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 1, 1})), 1);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 1, 1, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 2})), 2);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 2, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 2, 0, 0})), 0);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 2, 1})), 1);
  EXPECT_EQ(BirthRank(this->NodeAt({3, 2, 1, 0})), 0);
}
#endif

#if CURRENT_SHARD_IS(3)
TYPED_TEST(NodeWithParentTest, NumAncestors) {
  EXPECT_EQ(NumAncestors(this->root), 0);
  EXPECT_EQ(NumAncestors(this->NodeAt({0})), 1);
  EXPECT_EQ(NumAncestors(this->NodeAt({1})), 1);
  EXPECT_EQ(NumAncestors(this->NodeAt({1, 0})), 2);
  EXPECT_EQ(NumAncestors(this->NodeAt({2})), 1);
  EXPECT_EQ(NumAncestors(this->NodeAt({2, 0})), 2);
  EXPECT_EQ(NumAncestors(this->NodeAt({2, 0, 0})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({2, 1})), 2);
  EXPECT_EQ(NumAncestors(this->NodeAt({2, 1, 0})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({3})), 1);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 0})), 2);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 0, 0})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 0, 0, 0})), 4);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 0, 1})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 0, 1, 0})), 4);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 1})), 2);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 1, 0})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 1, 0, 0})), 4);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 1, 1})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 1, 1, 0})), 4);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 2})), 2);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 2, 0})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 2, 0, 0})), 4);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 2, 1})), 3);
  EXPECT_EQ(NumAncestors(this->NodeAt({3, 2, 1, 0})), 4);
}
#endif

#if CURRENT_SHARD_IS(3)
TYPED_TEST(NodeWithParentTest, Root) {
  EXPECT_EQ(Root(this->root), this->root);
  EXPECT_EQ(Root(this->NodeAt({0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({1})), this->root);
  EXPECT_EQ(Root(this->NodeAt({1, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({2})), this->root);
  EXPECT_EQ(Root(this->NodeAt({2, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({2, 0, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({2, 1})), this->root);
  EXPECT_EQ(Root(this->NodeAt({2, 1, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 0, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 0, 0, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 0, 1})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 0, 1, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 1})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 1, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 1, 0, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 1, 1})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 1, 1, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 2})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 2, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 2, 0, 0})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 2, 1})), this->root);
  EXPECT_EQ(Root(this->NodeAt({3, 2, 1, 0})), this->root);
}
#endif

#if CURRENT_SHARD_IS(3)
TYPED_TEST(NodeWithParentTest, IsFirstChild) {
  EXPECT_TRUE(IsFirstChild(this->root));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({1})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({1, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({2})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({2, 0})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({2, 0, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({2, 1})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({2, 1, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({3})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 0})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 0, 0})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 0, 0, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({3, 0, 1})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 0, 1, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({3, 1})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 1, 0})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 1, 0, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({3, 1, 1})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 1, 1, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({3, 2})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 2, 0})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 2, 0, 0})));
  EXPECT_FALSE(IsFirstChild(this->NodeAt({3, 2, 1})));
  EXPECT_TRUE(IsFirstChild(this->NodeAt({3, 2, 1, 0})));
}
#endif

#if CURRENT_SHARD_IS(3)
TYPED_TEST(NodeWithParentTest, IsLastChild) {
  EXPECT_TRUE(IsLastChild(this->root));
  EXPECT_FALSE(IsLastChild(this->NodeAt({0})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({1})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({1, 0})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({2})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({2, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({2, 0, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({2, 1})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({2, 1, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({3, 0})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({3, 0, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 0, 0, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 0, 1})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 0, 1, 0})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({3, 1})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({3, 1, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 1, 0, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 1, 1})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 1, 1, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 2})));
  EXPECT_FALSE(IsLastChild(this->NodeAt({3, 2, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 2, 0, 0})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 2, 1})));
  EXPECT_TRUE(IsLastChild(this->NodeAt({3, 2, 1, 0})));
}
#endif

#if CURRENT_SHARD_IS(3)
TYPED_TEST(NodeWithParentTest, HasAncestor) {
  EXPECT_FALSE(HasAncestor(this->root, &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({1}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({1, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 0, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 1}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 1, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 0, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 0, 0, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 0, 1}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 0, 1, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 1}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 1, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 1, 0, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 1, 1}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 1, 1, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 2}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 2, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 2, 0, 0}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 2, 1}), &this->root));
  EXPECT_TRUE(HasAncestor(this->NodeAt({3, 2, 1, 0}), &this->root));

  EXPECT_FALSE(HasAncestor(this->root, &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({1}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({1, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({2}), &this->NodeAt({2})));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 0}), &this->NodeAt({2})));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 0, 0}), &this->NodeAt({2})));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 1}), &this->NodeAt({2})));
  EXPECT_TRUE(HasAncestor(this->NodeAt({2, 1, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 0, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 1}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 1, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 0, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 1}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 1, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 0, 0}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 1}), &this->NodeAt({2})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 1, 0}), &this->NodeAt({2})));

  EXPECT_FALSE(HasAncestor(this->root, &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({1}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({1, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({2}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({2, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({2, 0, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({2, 1}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({2, 1, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 0, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 1}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 0, 1, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 0, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 1}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 1, 1, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 0, 0}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 1}), &this->NodeAt({1, 0})));
  EXPECT_FALSE(HasAncestor(this->NodeAt({3, 2, 1, 0}), &this->NodeAt({1, 0})));
}

template <class TestSuite>
void TestPath(TestSuite *test_suite, std::initializer_list<size_t> node_path) {
  auto &node = test_suite->NodeAt(node_path);
  {
    std::vector<size_t> calculated_path;
    Path(node, calculated_path);
    EXPECT_THAT(calculated_path, ::testing::ElementsAreArray(node_path))
        << "Path container: std::vector<size_t>";
  }
  {
    std::list<size_t> calculated_path;
    Path(node, calculated_path);
    EXPECT_THAT(calculated_path, ::testing::ElementsAreArray(node_path))
        << "Path container: std::list<size_t>";
  }
}
#endif

#if CURRENT_SHARD_IS(3)
TYPED_TEST(NodeWithParentTest, Path) {
  TestPath(this, {});
  TestPath(this, {0});
  TestPath(this, {1});
  TestPath(this, {1, 0});
  TestPath(this, {2});
  TestPath(this, {2, 0});
  TestPath(this, {2, 0, 0});
  TestPath(this, {2, 1});
  TestPath(this, {2, 1, 0});
  TestPath(this, {3});
  TestPath(this, {3, 0});
  TestPath(this, {3, 0, 0});
  TestPath(this, {3, 0, 0, 0});
  TestPath(this, {3, 0, 1});
  TestPath(this, {3, 0, 1, 0});
  TestPath(this, {3, 1});
  TestPath(this, {3, 1, 0});
  TestPath(this, {3, 1, 0, 0});
  TestPath(this, {3, 1, 1});
  TestPath(this, {3, 1, 1, 0});
  TestPath(this, {3, 2});
  TestPath(this, {3, 2, 0});
  TestPath(this, {3, 2, 0, 0});
  TestPath(this, {3, 2, 1});
  TestPath(this, {3, 2, 1, 0});
}
#endif

template <class TestSuite>
void TestNodePath(TestSuite *test_suite,
                  std::initializer_list<size_t> node_path,
                  absl::string_view expected_string) {
  auto &node = test_suite->NodeAt(node_path);
  std::ostringstream output;
  output << NodePath(node);
  EXPECT_EQ(output.str(), expected_string);
}

#if CURRENT_SHARD_IS(4)
TYPED_TEST(NodeWithParentTest, NodePath) {
  TestNodePath(this, {}, "{}");
  TestNodePath(this, {0}, "{0}");
  TestNodePath(this, {1}, "{1}");
  TestNodePath(this, {1, 0}, "{1,0}");
  TestNodePath(this, {2}, "{2}");
  TestNodePath(this, {2, 0}, "{2,0}");
  TestNodePath(this, {2, 0, 0}, "{2,0,0}");
  TestNodePath(this, {2, 1}, "{2,1}");
  TestNodePath(this, {2, 1, 0}, "{2,1,0}");
  TestNodePath(this, {3}, "{3}");
  TestNodePath(this, {3, 0}, "{3,0}");
  TestNodePath(this, {3, 0, 0}, "{3,0,0}");
  TestNodePath(this, {3, 0, 0, 0}, "{3,0,0,0}");
  TestNodePath(this, {3, 0, 1}, "{3,0,1}");
  TestNodePath(this, {3, 0, 1, 0}, "{3,0,1,0}");
  TestNodePath(this, {3, 1}, "{3,1}");
  TestNodePath(this, {3, 1, 0}, "{3,1,0}");
  TestNodePath(this, {3, 1, 0, 0}, "{3,1,0,0}");
  TestNodePath(this, {3, 1, 1}, "{3,1,1}");
  TestNodePath(this, {3, 1, 1, 0}, "{3,1,1,0}");
  TestNodePath(this, {3, 2}, "{3,2}");
  TestNodePath(this, {3, 2, 0}, "{3,2,0}");
  TestNodePath(this, {3, 2, 0, 0}, "{3,2,0,0}");
  TestNodePath(this, {3, 2, 1}, "{3,2,1}");
  TestNodePath(this, {3, 2, 1, 0}, "{3,2,1,0}");
}
#endif

#if CURRENT_SHARD_IS(4)
TYPED_TEST(NodeWithValueTest, PrintTreeWithCustomPrinter) {
  {
    std::ostringstream output;
    PrintTree(
        this->root, &output,
        [](std::ostream &stream, const std::string &value) -> std::ostream & {
          return stream << "value=" << value;
        });
    static const absl::string_view expected_output =
        "{ (value=root)\n"
        "  { (value=0) }\n"
        "  { (value=1)\n"
        "    { (value=1.0) }\n"
        "  }\n"
        "  { (value=2)\n"
        "    { (value=2.0)\n"
        "      { (value=2.0.0) }\n"
        "    }\n"
        "    { (value=2.1)\n"
        "      { (value=2.1.0) }\n"
        "    }\n"
        "  }\n"
        "  { (value=3)\n"
        "    { (value=3.0)\n"
        "      { (value=3.0.0)\n"
        "        { (value=3.0.0.0) }\n"
        "      }\n"
        "      { (value=3.0.1)\n"
        "        { (value=3.0.1.0) }\n"
        "      }\n"
        "    }\n"
        "    { (value=3.1)\n"
        "      { (value=3.1.0)\n"
        "        { (value=3.1.0.0) }\n"
        "      }\n"
        "      { (value=3.1.1)\n"
        "        { (value=3.1.1.0) }\n"
        "      }\n"
        "    }\n"
        "    { (value=3.2)\n"
        "      { (value=3.2.0)\n"
        "        { (value=3.2.0.0) }\n"
        "      }\n"
        "      { (value=3.2.1)\n"
        "        { (value=3.2.1.0) }\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}";
    EXPECT_EQ(output.str(), expected_output);
  }
  {
    std::ostringstream output;
    PrintTree(
        this->NodeAt({3, 1}), &output,
        [](std::ostream &stream, const std::string &value) -> std::ostream & {
          return stream << "value=" << value;
        });
    static const absl::string_view expected_output =
        "{ (value=3.1)\n"
        "  { (value=3.1.0)\n"
        "    { (value=3.1.0.0) }\n"
        "  }\n"
        "  { (value=3.1.1)\n"
        "    { (value=3.1.1.0) }\n"
        "  }\n"
        "}";
    EXPECT_EQ(output.str(), expected_output);
  }
}
#endif

#if CURRENT_SHARD_IS(4)
TYPED_TEST(NodeWithValueTest, PrintTree) {
  {
    std::ostringstream output;
    PrintTree(this->root, &output);
    static const absl::string_view expected_output =
        "{ (root)\n"
        "  { (0) }\n"
        "  { (1)\n"
        "    { (1.0) }\n"
        "  }\n"
        "  { (2)\n"
        "    { (2.0)\n"
        "      { (2.0.0) }\n"
        "    }\n"
        "    { (2.1)\n"
        "      { (2.1.0) }\n"
        "    }\n"
        "  }\n"
        "  { (3)\n"
        "    { (3.0)\n"
        "      { (3.0.0)\n"
        "        { (3.0.0.0) }\n"
        "      }\n"
        "      { (3.0.1)\n"
        "        { (3.0.1.0) }\n"
        "      }\n"
        "    }\n"
        "    { (3.1)\n"
        "      { (3.1.0)\n"
        "        { (3.1.0.0) }\n"
        "      }\n"
        "      { (3.1.1)\n"
        "        { (3.1.1.0) }\n"
        "      }\n"
        "    }\n"
        "    { (3.2)\n"
        "      { (3.2.0)\n"
        "        { (3.2.0.0) }\n"
        "      }\n"
        "      { (3.2.1)\n"
        "        { (3.2.1.0) }\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}";
    EXPECT_EQ(output.str(), expected_output);
  }
  {
    std::ostringstream output;
    PrintTree(this->NodeAt({3, 1}), &output);
    static const absl::string_view expected_output =
        "{ (3.1)\n"
        "  { (3.1.0)\n"
        "    { (3.1.0.0) }\n"
        "  }\n"
        "  { (3.1.1)\n"
        "    { (3.1.1.0) }\n"
        "  }\n"
        "}";
    EXPECT_EQ(output.str(), expected_output);
  }
}
#endif

#if CURRENT_SHARD_IS(4)
TYPED_TEST(NodeWithParentTest, NextSibling) {
  EXPECT_EQ(NextSibling(this->root), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({0})), &this->NodeAt({1}));
  EXPECT_EQ(NextSibling(this->NodeAt({1})), &this->NodeAt({2}));
  EXPECT_EQ(NextSibling(this->NodeAt({1, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({2})), &this->NodeAt({3}));
  EXPECT_EQ(NextSibling(this->NodeAt({2, 0})), &this->NodeAt({2, 1}));
  EXPECT_EQ(NextSibling(this->NodeAt({2, 0, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({2, 1})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({2, 1, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 0})), &this->NodeAt({3, 1}));
  EXPECT_EQ(NextSibling(this->NodeAt({3, 0, 0})), &this->NodeAt({3, 0, 1}));
  EXPECT_EQ(NextSibling(this->NodeAt({3, 0, 0, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 0, 1})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 0, 1, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 1})), &this->NodeAt({3, 2}));
  EXPECT_EQ(NextSibling(this->NodeAt({3, 1, 0})), &this->NodeAt({3, 1, 1}));
  EXPECT_EQ(NextSibling(this->NodeAt({3, 1, 0, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 1, 1})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 1, 1, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 2})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 2, 0})), &this->NodeAt({3, 2, 1}));
  EXPECT_EQ(NextSibling(this->NodeAt({3, 2, 0, 0})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 2, 1})), nullptr);
  EXPECT_EQ(NextSibling(this->NodeAt({3, 2, 1, 0})), nullptr);
}
#endif

#if CURRENT_SHARD_IS(4)
TYPED_TEST(NodeWithParentTest, PreviousSibling) {
  EXPECT_EQ(PreviousSibling(this->root), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({1})), &this->NodeAt({0}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({1, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({2})), &this->NodeAt({1}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({2, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({2, 0, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({2, 1})), &this->NodeAt({2, 0}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({2, 1, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3})), &this->NodeAt({2}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 0, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 0, 0, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 0, 1})), &this->NodeAt({3, 0, 0}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 0, 1, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 1})), &this->NodeAt({3, 0}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 1, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 1, 0, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 1, 1})), &this->NodeAt({3, 1, 0}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 1, 1, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 2})), &this->NodeAt({3, 1}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 2, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 2, 0, 0})), nullptr);
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 2, 1})), &this->NodeAt({3, 2, 0}));
  EXPECT_EQ(PreviousSibling(this->NodeAt({3, 2, 1, 0})), nullptr);
}
#endif

#if CURRENT_SHARD_IS(4)
TYPED_TEST(NodeWithParentTest, NextLeaf) {
  EXPECT_EQ(NextLeaf(this->root), nullptr);
  EXPECT_EQ(NextLeaf(this->NodeAt({0})), &this->NodeAt({1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({1})), &this->NodeAt({2, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({1, 0})), &this->NodeAt({2, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({2})), &this->NodeAt({3, 0, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({2, 0})), &this->NodeAt({2, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({2, 0, 0})), &this->NodeAt({2, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({2, 1})), &this->NodeAt({3, 0, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({2, 1, 0})), &this->NodeAt({3, 0, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3})), nullptr);
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 0})), &this->NodeAt({3, 1, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 0, 0})), &this->NodeAt({3, 0, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 0, 0, 0})), &this->NodeAt({3, 0, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 0, 1})), &this->NodeAt({3, 1, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 0, 1, 0})), &this->NodeAt({3, 1, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 1})), &this->NodeAt({3, 2, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 1, 0})), &this->NodeAt({3, 1, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 1, 0, 0})), &this->NodeAt({3, 1, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 1, 1})), &this->NodeAt({3, 2, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 1, 1, 0})), &this->NodeAt({3, 2, 0, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 2})), nullptr);
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 2, 0})), &this->NodeAt({3, 2, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 2, 0, 0})), &this->NodeAt({3, 2, 1, 0}));
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 2, 1})), nullptr);
  EXPECT_EQ(NextLeaf(this->NodeAt({3, 2, 1, 0})), nullptr);
}
#endif

#if CURRENT_SHARD_IS(5)
TYPED_TEST(NodeWithParentTest, PreviousLeaf) {
  EXPECT_EQ(PreviousLeaf(this->root), nullptr);
  EXPECT_EQ(PreviousLeaf(this->NodeAt({0})), nullptr);
  EXPECT_EQ(PreviousLeaf(this->NodeAt({1})), &this->NodeAt({0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({1, 0})), &this->NodeAt({0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({2})), &this->NodeAt({1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({2, 0})), &this->NodeAt({1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({2, 0, 0})), &this->NodeAt({1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({2, 1})), &this->NodeAt({2, 0, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({2, 1, 0})), &this->NodeAt({2, 0, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3})), &this->NodeAt({2, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 0})), &this->NodeAt({2, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 0, 0})), &this->NodeAt({2, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 0, 0, 0})), &this->NodeAt({2, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 0, 1})), &this->NodeAt({3, 0, 0, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 0, 1, 0})),
            &this->NodeAt({3, 0, 0, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 1})), &this->NodeAt({3, 0, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 1, 0})), &this->NodeAt({3, 0, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 1, 0, 0})),
            &this->NodeAt({3, 0, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 1, 1})), &this->NodeAt({3, 1, 0, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 1, 1, 0})),
            &this->NodeAt({3, 1, 0, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 2})), &this->NodeAt({3, 1, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 2, 0})), &this->NodeAt({3, 1, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 2, 0, 0})),
            &this->NodeAt({3, 1, 1, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 2, 1})), &this->NodeAt({3, 2, 0, 0}));
  EXPECT_EQ(PreviousLeaf(this->NodeAt({3, 2, 1, 0})),
            &this->NodeAt({3, 2, 0, 0}));
}
#endif

#if CURRENT_SHARD_IS(5)
TYPED_TEST(NodeWithParentTest, RemoveSelfFromParent) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    RemoveSelfFromParent(this->NodeAt({2}));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1", {N("1.0")}),                              //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                     N("3.1.1", {N("3.1.1.0")})}),  //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));
    this->root.Relink();
    RemoveSelfFromParent(this->NodeAt({2, 1}));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1", {N("1.0")}),                              //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));
    this->root.Relink();
    RemoveSelfFromParent(this->NodeAt({0}));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("1", {N("1.0")}),                              //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));
  }
}
#endif

#if CURRENT_SHARD_IS(5)
TYPED_TEST(SimpleNodeTest, HoistOnlyChild) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    HoistOnlyChild(this->NodeAt({2, 0}));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1", {N("1.0")}),                              //
                   N("2", {N("2.0.0"),                              //
                           N("2.1", {N("2.1.0")})}),                //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                     N("3.1.1", {N("3.1.1.0")})}),  //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));

    HoistOnlyChild(this->NodeAt({3}));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1", {N("1.0")}),                              //
                   N("2", {N("2.0.0"),                              //
                           N("2.1", {N("2.1.0")})}),                //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                     N("3.1.1", {N("3.1.1.0")})}),  //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));

    // remove "3.1" and "3.2"
    this->NodeAt({3}).Children().erase(
        std::next(this->NodeAt({3}).Children().begin(), 1),
        this->NodeAt({3}).Children().end());
    // remove "3.0.1"
    this->NodeAt({3, 0}).Children().erase(
        std::next(this->NodeAt({3, 0}).Children().begin(), 1));

    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                            //
                   N("1", {N("1.0")}),                //
                   N("2", {N("2.0.0"),                //
                           N("2.1", {N("2.1.0")})}),  //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")})})})}));

    HoistOnlyChild(this->NodeAt({3}));
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                            //
                                   N("1", {N("1.0")}),                //
                                   N("2", {N("2.0.0"),                //
                                           N("2.1", {N("2.1.0")})}),  //
                                   N("3.0", {N("3.0.0", {N("3.0.0.0")})})}));

    HoistOnlyChild(this->NodeAt({3}));
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                            //
                                   N("1", {N("1.0")}),                //
                                   N("2", {N("2.0.0"),                //
                                           N("2.1", {N("2.1.0")})}),  //
                                   N("3.0.0", {N("3.0.0.0")})}));

    HoistOnlyChild(this->NodeAt({3}));
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                            //
                                   N("1", {N("1.0")}),                //
                                   N("2", {N("2.0.0"),                //
                                           N("2.1", {N("2.1.0")})}),  //
                                   N("3.0.0.0")}));
  }
}
#endif

#if CURRENT_SHARD_IS(5)
TYPED_TEST(SimpleNodeTest, FlattenOnce) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    FlattenOnce(this->NodeAt({3}));
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                              //
                                   N("1", {N("1.0")}),                  //
                                   N("2", {N("2.0", {N("2.0.0")}),      //
                                           N("2.1", {N("2.1.0")})}),    //
                                   N("3", {N("3.0.0", {N("3.0.0.0")}),  //
                                           N("3.0.1", {N("3.0.1.0")}),  //
                                           N("3.1.0", {N("3.1.0.0")}),  //
                                           N("3.1.1", {N("3.1.1.0")}),  //
                                           N("3.2.0", {N("3.2.0.0")}),  //
                                           N("3.2.1", {N("3.2.1.0")})})}));

    // This should change nothing
    FlattenOnce(this->NodeAt({0}));
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                              //
                                   N("1", {N("1.0")}),                  //
                                   N("2", {N("2.0", {N("2.0.0")}),      //
                                           N("2.1", {N("2.1.0")})}),    //
                                   N("3", {N("3.0.0", {N("3.0.0.0")}),  //
                                           N("3.0.1", {N("3.0.1.0")}),  //
                                           N("3.1.0", {N("3.1.0.0")}),  //
                                           N("3.1.1", {N("3.1.1.0")}),  //
                                           N("3.2.0", {N("3.2.0.0")}),  //
                                           N("3.2.1", {N("3.2.1.0")})})}));

    FlattenOnce(this->root);
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {                             //
                                   N("1.0"),                    //
                                   N("2.0", {N("2.0.0")}),      //
                                   N("2.1", {N("2.1.0")}),      //
                                   N("3.0.0", {N("3.0.0.0")}),  //
                                   N("3.0.1", {N("3.0.1.0")}),  //
                                   N("3.1.0", {N("3.1.0.0")}),  //
                                   N("3.1.1", {N("3.1.1.0")}),  //
                                   N("3.2.0", {N("3.2.0.0")}),  //
                                   N("3.2.1", {N("3.2.1.0")})}));
  }
}
#endif

#if CURRENT_SHARD_IS(5)
TYPED_TEST(SimpleNodeTest, FlattenOnlyChildrenWithChildren) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    FlattenOnlyChildrenWithChildren(this->root);
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                                  //
                                   N("1.0"),                                //
                                   N("2.0", {N("2.0.0")}),                  //
                                   N("2.1", {N("2.1.0")}),                  //
                                   N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                             N("3.0.1", {N("3.0.1.0")})}),  //
                                   N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                             N("3.1.1", {N("3.1.1.0")})}),  //
                                   N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                             N("3.2.1", {N("3.2.1.0")})})}));

    // This should change nothing
    FlattenOnlyChildrenWithChildren(this->NodeAt({0}));
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                                  //
                                   N("1.0"),                                //
                                   N("2.0", {N("2.0.0")}),                  //
                                   N("2.1", {N("2.1.0")}),                  //
                                   N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                             N("3.0.1", {N("3.0.1.0")})}),  //
                                   N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                             N("3.1.1", {N("3.1.1.0")})}),  //
                                   N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                             N("3.2.1", {N("3.2.1.0")})})}));

    FlattenOnlyChildrenWithChildren(this->root);
    EXPECT_PRED_FORMAT2(VerifyTree, this->root,
                        N("root", {N("0"),                      //
                                   N("1.0"),                    //
                                   N("2.0.0"),                  //
                                   N("2.1.0"),                  //
                                   N("3.0.0", {N("3.0.0.0")}),  //
                                   N("3.0.1", {N("3.0.1.0")}),  //
                                   N("3.1.0", {N("3.1.0.0")}),  //
                                   N("3.1.1", {N("3.1.1.0")}),  //
                                   N("3.2.0", {N("3.2.0.0")}),  //
                                   N("3.2.1", {N("3.2.1.0")})}));
  }
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(SimpleNodeTest, FlattenOneChild) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    FlattenOneChild(this->NodeAt({3}), 1);
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1", {N("1.0")}),                              //
                   N("2", {N("2.0", {N("2.0.0")}),                  //
                           N("2.1", {N("2.1.0")})}),                //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.1.0", {N("3.1.0.0")}),              //
                           N("3.1.1", {N("3.1.1.0")}),              //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));

    FlattenOneChild(this->NodeAt({1}), 0);
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1"),                                          //
                   N("2", {N("2.0", {N("2.0.0")}),                  //
                           N("2.1", {N("2.1.0")})}),                //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.1.0", {N("3.1.0.0")}),              //
                           N("3.1.1", {N("3.1.1.0")}),              //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));
  }
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(NodeWithValueTest, MergeConsecutiveSiblings) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    MergeConsecutiveSiblings(this->NodeAt({3}), 1,
                             [](std::string *v0, const std::string &v1) {
                               absl::StrAppend(v0, "+", v1);
                             });
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                            //
                   N("1", {N("1.0")}),                                //
                   N("2", {N("2.0", {N("2.0.0")}),                    //
                           N("2.1", {N("2.1.0")})}),                  //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),      //
                                     N("3.0.1", {N("3.0.1.0")})}),    //
                           N("3.1+3.2", {N("3.1.0", {N("3.1.0.0")}),  //
                                         N("3.1.1", {N("3.1.1.0")}),  //
                                         N("3.2.0", {N("3.2.0.0")}),  //
                                         N("3.2.1", {N("3.2.1.0")})})})}));

    MergeConsecutiveSiblings(this->NodeAt({3}), 0,
                             [](std::string *v0, const std::string &v1) {
                               absl::StrAppend(v0, "+", v1);
                             });
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                                //
                   N("1", {N("1.0")}),                                    //
                   N("2", {N("2.0", {N("2.0.0")}),                        //
                           N("2.1", {N("2.1.0")})}),                      //
                   N("3", {N("3.0+3.1+3.2", {N("3.0.0", {N("3.0.0.0")}),  //
                                             N("3.0.1", {N("3.0.1.0")}),  //
                                             N("3.1.0", {N("3.1.0.0")}),  //
                                             N("3.1.1", {N("3.1.1.0")}),  //
                                             N("3.2.0", {N("3.2.0.0")}),  //
                                             N("3.2.1", {N("3.2.1.0")})})})}));

    MergeConsecutiveSiblings(this->root, 0,
                             [](std::string *v0, const std::string &v1) {
                               absl::StrAppend(v0, "+", v1);
                             });
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0+1", {N("1.0")}),                                  //
                   N("2", {N("2.0", {N("2.0.0")}),                        //
                           N("2.1", {N("2.1.0")})}),                      //
                   N("3", {N("3.0+3.1+3.2", {N("3.0.0", {N("3.0.0.0")}),  //
                                             N("3.0.1", {N("3.0.1.0")}),  //
                                             N("3.1.0", {N("3.1.0.0")}),  //
                                             N("3.1.1", {N("3.1.1.0")}),  //
                                             N("3.2.0", {N("3.2.0.0")}),  //
                                             N("3.2.1", {N("3.2.1.0")})})})}));
  }
}
#endif

#if CURRENT_SHARD_IS(1)
TYPED_TEST(NodeWithParentTest, NearestCommonAncestor) {
  EXPECT_EQ(NearestCommonAncestor(  //
                this->NodeAt({0}),  //
                this->NodeAt({1})),
            &this->NodeAt({}));

  EXPECT_EQ(NearestCommonAncestor(     //
                this->NodeAt({3, 1}),  //
                this->NodeAt({3, 2, 1, 0})),
            &this->NodeAt({3}));

  EXPECT_EQ(NearestCommonAncestor(     //
                this->NodeAt({3, 1}),  //
                this->NodeAt({3, 1, 1, 0})),
            &this->NodeAt({3, 1}));

  EXPECT_EQ(NearestCommonAncestor(        //
                this->NodeAt({2, 0, 0}),  //
                this->NodeAt({3, 2, 1, 0})),
            &this->NodeAt({}));

  EXPECT_EQ(NearestCommonAncestor(  //
                this->NodeAt({}),   //
                this->NodeAt({3, 2, 1, 0})),
            &this->NodeAt({}));

  using N = TypeParam;
  N other_tree = N("", {N("A"),                                    //
                        N("C", {N("CA")}),                         //
                        N("G", {N("GA", {N("GAA")}),               //
                                N("GC", {N("GCA")})}),             //
                        N("T", {N("TA", {N("TAA", {N("TAAA")}),    //
                                         N("TAC", {N("TACA")})}),  //
                                N("TC", {N("TCA", {N("TCAA")}),    //
                                         N("TCC", {N("TCCA")})}),  //
                                N("TG", {N("TGA", {N("TGAA")}),    //
                                         N("TGC", {N("TGCA")})})})});

  EXPECT_EQ(NearestCommonAncestor(              //
                other_tree.Children().front(),  //
                this->NodeAt({0})),
            nullptr);
}
#endif

#if CURRENT_SHARD_IS(2)
TYPED_TEST(SimpleNodeTest, AdoptSubtree) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    AdoptSubtree(this->NodeAt({2}), N("2.2"));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1", {N("1.0")}),                              //
                   N("2", {N("2.0", {N("2.0.0")}),                  //
                           N("2.1", {N("2.1.0")}),                  //
                           N("2.2")}),                              //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                     N("3.1.1", {N("3.1.1.0")})}),  //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));

    AdoptSubtree(this->NodeAt({2}), N("2.3", {N("2.3.0"), N("2.3.1")}),
                 N("2.4"));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                          //
                   N("1", {N("1.0")}),                              //
                   N("2", {N("2.0", {N("2.0.0")}),                  //
                           N("2.1", {N("2.1.0")}),                  //
                           N("2.2"),                                //
                           N("2.3", {N("2.3.0"), N("2.3.1")}),      //
                           N("2.4")}),                              //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                     N("3.0.1", {N("3.0.1.0")})}),  //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                     N("3.1.1", {N("3.1.1.0")})}),  //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                     N("3.2.1", {N("3.2.1.0")})})})}));

    N other_tree = N("", {N("A"),                                    //
                          N("C", {N("CA")}),                         //
                          N("G", {N("GA", {N("GAA")}),               //
                                  N("GC", {N("GCA")})}),             //
                          N("T", {N("TA", {N("TAA", {N("TAAA")}),    //
                                           N("TAC", {N("TACA")})}),  //
                                  N("TC", {N("TCA", {N("TCAA")}),    //
                                           N("TCC", {N("TCCA")})}),  //
                                  N("TG", {N("TGA", {N("TGAA")}),    //
                                           N("TGC", {N("TGCA")})})})});

    AdoptSubtree(this->root, std::move(other_tree));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                            //
                   N("1", {N("1.0")}),                                //
                   N("2", {N("2.0", {N("2.0.0")}),                    //
                           N("2.1", {N("2.1.0")}),                    //
                           N("2.2"),                                  //
                           N("2.3", {N("2.3.0"), N("2.3.1")}),        //
                           N("2.4")}),                                //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),      //
                                     N("3.0.1", {N("3.0.1.0")})}),    //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),      //
                                     N("3.1.1", {N("3.1.1.0")})}),    //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),      //
                                     N("3.2.1", {N("3.2.1.0")})})}),  //
                   N("", {N("A"),                                     //
                          N("C", {N("CA")}),                          //
                          N("G", {N("GA", {N("GAA")}),                //
                                  N("GC", {N("GCA")})}),              //
                          N("T", {N("TA", {N("TAA", {N("TAAA")}),     //
                                           N("TAC", {N("TACA")})}),   //
                                  N("TC", {N("TCA", {N("TCAA")}),     //
                                           N("TCC", {N("TCCA")})}),   //
                                  N("TG", {N("TGA", {N("TGAA")}),     //
                                           N("TGC", {N("TGCA")})})})})}));
  }
}
#endif

#if CURRENT_SHARD_IS(4)
TYPED_TEST(SimpleNodeTest, AdoptSubtreesFrom) {
  using N = TypeParam;
  // Const nodes are not supported
  if constexpr (!std::is_const_v<TypeParam>) {
    N other_tree = N("", {N("A"),                                    //
                          N("C", {N("CA")}),                         //
                          N("G", {N("GA", {N("GAA")}),               //
                                  N("GC", {N("GCA")})}),             //
                          N("T", {N("TA", {N("TAA", {N("TAAA")}),    //
                                           N("TAC", {N("TACA")})}),  //
                                  N("TC", {N("TCA", {N("TCAA")}),    //
                                           N("TCC", {N("TCCA")})}),  //
                                  N("TG", {N("TGA", {N("TGAA")}),    //
                                           N("TGC", {N("TGCA")})})})});

    AdoptSubtreesFrom(this->root, &other_tree);
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                            //
                   N("1", {N("1.0")}),                                //
                   N("2", {N("2.0", {N("2.0.0")}),                    //
                           N("2.1", {N("2.1.0")})}),                  //
                   N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),      //
                                     N("3.0.1", {N("3.0.1.0")})}),    //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),      //
                                     N("3.1.1", {N("3.1.1.0")})}),    //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),      //
                                     N("3.2.1", {N("3.2.1.0")})})}),  //
                   N("A"),                                            //
                   N("C", {N("CA")}),                                 //
                   N("G", {N("GA", {N("GAA")}),                       //
                           N("GC", {N("GCA")})}),                     //
                   N("T", {N("TA", {N("TAA", {N("TAAA")}),            //
                                    N("TAC", {N("TACA")})}),          //
                           N("TC", {N("TCA", {N("TCAA")}),            //
                                    N("TCC", {N("TCCA")})}),          //
                           N("TG", {N("TGA", {N("TGAA")}),            //
                                    N("TGC", {N("TGCA")})})})}));
    EXPECT_PRED_FORMAT2(VerifyTree, other_tree, N(""));

    AdoptSubtreesFrom(this->NodeAt({1, 0}), &this->NodeAt({3, 0}));
    EXPECT_PRED_FORMAT2(
        VerifyTree, this->root,
        N("root", {N("0"),                                            //
                   N("1", {N("1.0", {N("3.0.0", {N("3.0.0.0")}),      //
                                     N("3.0.1", {N("3.0.1.0")})})}),  //
                   N("2", {N("2.0", {N("2.0.0")}),                    //
                           N("2.1", {N("2.1.0")})}),                  //
                   N("3", {N("3.0"),                                  //
                           N("3.1", {N("3.1.0", {N("3.1.0.0")}),      //
                                     N("3.1.1", {N("3.1.1.0")})}),    //
                           N("3.2", {N("3.2.0", {N("3.2.0.0")}),      //
                                     N("3.2.1", {N("3.2.1.0")})})}),  //
                   N("A"),                                            //
                   N("C", {N("CA")}),                                 //
                   N("G", {N("GA", {N("GAA")}),                       //
                           N("GC", {N("GCA")})}),                     //
                   N("T", {N("TA", {N("TAA", {N("TAAA")}),            //
                                    N("TAC", {N("TACA")})}),          //
                           N("TC", {N("TCA", {N("TCAA")}),            //
                                    N("TCC", {N("TCCA")})}),          //
                           N("TG", {N("TGA", {N("TGAA")}),            //
                                    N("TGC", {N("TGCA")})})})}));
  }
}
#endif

#if CURRENT_SHARD_IS(6)
TYPED_TEST(SimpleNodeTest, Transform) {
  using N = TypeParam;

  IntNode id_lengths_tree = Transform<IntNode>(
      this->root, [](const N &node) -> int { return node.id().size(); });

  EXPECT_PRED_FORMAT2(
      VerifyTree, id_lengths_tree,
      IntNode(4, {IntNode(1),                                          //
                  IntNode(1, {IntNode(3)}),                            //
                  IntNode(1, {IntNode(3, {IntNode(5)}),                //
                              IntNode(3, {IntNode(5)})}),              //
                  IntNode(1, {IntNode(3, {IntNode(5, {IntNode(7)}),    //
                                          IntNode(5, {IntNode(7)})}),  //
                              IntNode(3, {IntNode(5, {IntNode(7)}),    //
                                          IntNode(5, {IntNode(7)})}),  //
                              IntNode(3, {IntNode(5, {IntNode(7)}),    //
                                          IntNode(5, {IntNode(7)})})})}));

  N censored_id_tree = Transform<std::remove_const_t<N>>(
      id_lengths_tree, [](const IntNode &node) -> std::string {
        return std::string(node.id(), 'x');
      });
  EXPECT_PRED_FORMAT2(
      VerifyTree, censored_id_tree,
      N("xxxx", {N("x"),                                          //
                 N("x", {N("xxx")}),                              //
                 N("x", {N("xxx", {N("xxxxx")}),                  //
                         N("xxx", {N("xxxxx")})}),                //
                 N("x", {N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                   N("xxxxx", {N("xxxxxxx")})}),  //
                         N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                   N("xxxxx", {N("xxxxxxx")})}),  //
                         N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                   N("xxxxx", {N("xxxxxxx")})})})}));
}
#endif

#if CURRENT_SHARD_IS(6)
TYPED_TEST(NodeWithValueTest, DeepEqual) {
  using N = TypeParam;

  {
    N copy = this->root;
    const auto diff = DeepEqual(this->root, copy);
    EXPECT_EQ(diff.left, nullptr);
    EXPECT_EQ(diff.right, nullptr);
  }
  {
    N censored_id_tree =
        N("xxxx", {N("x"),                                          //
                   N("x", {N("xxx")}),                              //
                   N("x", {N("xxx", {N("xxxxx")}),                  //
                           N("xxx", {N("xxxxx")})}),                //
                   N("x", {N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                     N("xxxxx", {N("xxxxxxx")})}),  //
                           N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                     N("xxxxx", {N("xxxxxxx")})}),  //
                           N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                     N("xxxxx", {N("xxxxxxx")})})})});
    const auto diff = DeepEqual(this->root, censored_id_tree,
                                [](const std::string &l, const std::string &r) {
                                  return l.size() == r.size();
                                });
    EXPECT_EQ(diff.left, nullptr);
    EXPECT_EQ(diff.right, nullptr);
  }
  {
    IntNode id_lengths_tree =
        IntNode(4, {IntNode(1),                                          //
                    IntNode(1, {IntNode(3)}),                            //
                    IntNode(1, {IntNode(3, {IntNode(5)}),                //
                                IntNode(3, {IntNode(5)})}),              //
                    IntNode(1, {IntNode(3, {IntNode(5, {IntNode(7)}),    //
                                            IntNode(5, {IntNode(7)})}),  //
                                IntNode(3, {IntNode(5, {IntNode(7)}),    //
                                            IntNode(5, {IntNode(7)})}),  //
                                IntNode(3, {IntNode(5, {IntNode(7)}),    //
                                            IntNode(5, {IntNode(7)})})})});
    const auto diff = DeepEqual(this->root, id_lengths_tree,
                                [](const std::string &l, const int &r) {
                                  return static_cast<int>(l.size()) == r;
                                });
    EXPECT_EQ(diff.left, nullptr);
    EXPECT_EQ(diff.right, nullptr);
  }
  {
    std::remove_const_t<N> copy = this->root;
    auto n2 = std::next(copy.Children().begin(), 2);
    auto n21 = std::next(n2->Children().begin(), 1);
    auto n3 = std::next(copy.Children().begin(), 3);
    auto n31 = std::next(n3->Children().begin(), 1);
    n21->set_id("foo");
    n31->set_id("bar");
    const auto diff = DeepEqual(this->root, copy);
    EXPECT_EQ(diff.left, &this->NodeAt({2, 1}));
    EXPECT_EQ(diff.right, &*n21);
  }
  {
    N censored_id_tree =
        N("xxxx", {N("x"),                                          //
                   N("x", {N("xxx")}),                              //
                   N("x", {N("xxx", {N("xxxxx")}),                  //
                           N("xxx", {N("xxxxx")})}),                //
                   N("x", {N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                     N("xxxxx", {N("xxxxxxx")})}),  //
                           N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                     N("xxxxx", {N("xxxxxxx")})}),  //
                           N("xxx", {N("xxxxx", {N("xxxxxxx")}),    //
                                     N("xxxxx", {N("WRONG")})})})});
    auto n3 = std::next(censored_id_tree.Children().begin(), 3);
    auto n32 = std::next(n3->Children().begin(), 2);
    auto n321 = std::next(n32->Children().begin(), 1);
    auto n3210 = n321->Children().begin();
    const auto diff = DeepEqual(this->root, censored_id_tree,
                                [](const std::string &l, const std::string &r) {
                                  return l.size() == r.size();
                                });
    EXPECT_EQ(diff.left, &this->NodeAt({3, 2, 1, 0}));
    EXPECT_EQ(diff.right, &*n3210);
  }
  {
    IntNode id_lengths_tree =
        IntNode(4, {IntNode(42),                                            //
                    IntNode(1, {IntNode(3)}),                               //
                    IntNode(1, {IntNode(3, {IntNode(5)}),                   //
                                IntNode(3, {IntNode(5)})}),                 //
                    IntNode(1, {IntNode(3, {IntNode(5, {IntNode(7)}),       //
                                            IntNode(5, {IntNode(7)})}),     //
                                IntNode(3, {IntNode(5, {IntNode(7)}),       //
                                            IntNode(5, {IntNode(9999)})}),  //
                                IntNode(3, {IntNode(5, {IntNode(7)}),       //
                                            IntNode(999, {IntNode(7)})})})});
    const auto diff = DeepEqual(this->root, id_lengths_tree,
                                [](const std::string &l, const int &r) {
                                  return static_cast<int>(l.size()) == r;
                                });
    EXPECT_EQ(diff.left, &this->NodeAt({0}));
    EXPECT_EQ(diff.right, &*id_lengths_tree.Children().begin());
  }
  {
    N other = N("root", {N("0"),              //
                         N("1", {N("1.0")}),  //
                         // Missing subtree:
                         // N("2", {N("2.0", {N("2.0.0")}),
                         //         N("2.1", {N("2.1.0")})}),
                         N("3", {N("3.0", {N("3.0.0", {N("3.0.0.0")}),    //
                                           N("3.0.1", {N("3.0.1.0")})}),  //
                                 N("3.1", {N("3.1.0", {N("3.1.0.0")}),    //
                                           N("3.1.1", {N("3.1.1.0")})}),  //
                                 N("3.2", {N("3.2.0", {N("3.2.0.0")}),    //
                                           N("3.2.1", {N("3.2.1.0")})})})});
    const auto diff = DeepEqual(this->root, other);
    EXPECT_EQ(diff.left, &this->root);
    EXPECT_EQ(diff.right, &other);
  }
}
#endif

#if CURRENT_SHARD_IS(6)
TYPED_TEST(NodeWithValueTest, StructureEqual) {
  using N = IntNode;
  {
    static const IntNode matching_tree =
        N(0, {N(1),                              //
              N(2, {N(21)}),                     //
              N(3, {N(31, {N(311)}),             //
                    N(32, {N(321)})}),           //
              N(4, {N(41, {N(411, {N(4111)}),    //
                           N(412, {N(4121)})}),  //
                    N(42, {N(421, {N(4211)}),    //
                           N(422, {N(4221)})}),  //
                    N(43, {N(431, {N(4311)}),    //
                           N(432, {N(4321)})})})});

    auto result = StructureEqual(this->root, matching_tree);
    EXPECT_EQ(result.left, nullptr);
    EXPECT_EQ(result.right, nullptr);
  }
  {
    static const IntNode matching_tree =
        N(0, {N(1),                              //
              N(2, {N(21)}),                     //
              N(3, {N(31, {N(311)}),             //
                    N(32, {N(321)})}),           //
              N(4, {N(41, {N(411, {N(4111)}),    //
                           N(412, {N(4121)})}),  //
                    // Missing subtree:
                    // N(42, {N(421, {N(4211)}),
                    //        N(422, {N(4221)})}),
                    N(43, {N(431, {N(4311)}),  //
                           N(432, {N(4321)})})})});

    auto result = StructureEqual(this->root, matching_tree);
    EXPECT_EQ(result.left, &this->NodeAt({3}));
    EXPECT_EQ(result.right, &matching_tree.Children()[3]);
  }
  {
    static const IntNode matching_tree =
        N(0, {N(1),                   //
              N(2, {N(21)}),          //
              N(3, {N(31, {N(311)}),  //
                    N(32, {N(321)}),  //
                    // Extra subtree:
                    N(33, {N(331)})}),           //
              N(4, {N(41, {N(411, {N(4111)}),    //
                           N(412, {N(4121)})}),  //
                    N(42, {N(421, {N(4211)}),    //
                           N(422, {N(4221)})}),  //
                    N(43, {N(431, {N(4311)}),    //
                           N(432, {N(4321)})})})});

    auto result = StructureEqual(this->root, matching_tree);
    EXPECT_EQ(result.left, &this->NodeAt({2}));
    EXPECT_EQ(result.right, &matching_tree.Children()[2]);
  }
  {
    static const IntNode matching_tree =
        N(0, {N(1),                   //
              N(2, {N(21)}),          //
              N(3, {N(31, {N(311)}),  //
                    N(32, {N(321)}),  //
                    // Extra subtree:
                    N(33, {N(331)})}),           //
              N(4, {N(41, {N(411, {N(4111)}),    //
                           N(412, {N(4121)})}),  //
                    // Missing subtree:
                    // N(42, {N(421, {N(4211)}),
                    //        N(422, {N(4221)})}),
                    N(43, {N(431, {N(4311)}),  //
                           N(432, {N(4321)})})})});

    auto result = StructureEqual(this->root, matching_tree);
    EXPECT_EQ(result.left, &this->NodeAt({2}));
    EXPECT_EQ(result.right, &matching_tree.Children()[2]);
  }
}
#endif
}  // namespace
}  // namespace verible
