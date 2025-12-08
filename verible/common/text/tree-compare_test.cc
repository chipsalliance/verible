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

#include "verible/common/text/tree-compare.h"

#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/tree-builder-test-util.h"

namespace verible {
namespace {

// Test empty tree equality
TEST(TreeEqualityTest, EmptyTreeEqualByEnum) {
  SymbolPtr tree1 = nullptr;
  SymbolPtr tree2 = nullptr;
  EXPECT_TRUE(EqualTreesByEnum(tree1.get(), tree2.get()));
}

TEST(TreeEqualityTest, EmptyTreeEqualByEnumString) {
  SymbolPtr tree1 = nullptr;
  SymbolPtr tree2 = nullptr;

  EXPECT_TRUE(EqualTreesByEnumString(tree1.get(), tree2.get()));
}

TEST(TreeEqualityTest, EmptyTreeNotLeavesByEnum) {
  SymbolPtr tree1 = Leaf(3, "bar");
  SymbolPtr tree2 = nullptr;
  SymbolPtr tree3 = Leaf(5, "bar");
  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree3.get()));
}

TEST(TreeEqualityTest, EmptyTreeNotLeavesByEnumString) {
  SymbolPtr tree1 = Leaf(4, "foo");
  SymbolPtr tree2 = nullptr;
  SymbolPtr tree3 = Leaf(3, "bar");

  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree3.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree2.get(), tree3.get()));
}

// Test leaf equality
TEST(TreeEqualityTest, LeavesEqualByEnum) {
  SymbolPtr tree1 = Leaf(3, "bar");
  SymbolPtr tree2 = Leaf(3, "foo");
  EXPECT_TRUE(EqualTreesByEnum(tree1.get(), tree2.get()));
  EXPECT_TRUE(EqualTreesByEnum(tree2.get(), tree1.get()));
}

TEST(TreeEqualityTest, LeavesEqualByEnumString) {
  SymbolPtr tree1 = Leaf(3, "foo");
  SymbolPtr tree2 = Leaf(3, "foo");

  EXPECT_TRUE(EqualTreesByEnumString(tree1.get(), tree2.get()));
  EXPECT_TRUE(EqualTreesByEnumString(tree2.get(), tree1.get()));
}

TEST(TreeEqualityTest, LeavesNotEqualByEnum) {
  SymbolPtr tree1 = Leaf(3, "bar");
  SymbolPtr tree2 = Leaf(5, "bar");
  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnum(tree2.get(), tree1.get()));
}

TEST(TreeEqualityTest, LeavesNotEqualByEnumString) {
  SymbolPtr tree1 = Leaf(4, "foo");
  SymbolPtr tree2 = Leaf(3, "foo");
  SymbolPtr tree3 = Leaf(3, "bar");

  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree3.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree2.get(), tree3.get()));
}

// Test Empty SyntaxTreeNode Equality
TEST(TreeEqualityTest, EmptyNodesEqualByEnum) {
  SymbolPtr tree1 = Node();
  SymbolPtr tree2 = Node();
  EXPECT_TRUE(EqualTreesByEnum(tree1.get(), tree2.get()));
}

TEST(TreeEqualityTest, EmptyNodesEqualByEnumString) {
  SymbolPtr tree1 = Node();
  SymbolPtr tree2 = Node();

  EXPECT_TRUE(EqualTreesByEnumString(tree1.get(), tree2.get()));
}

TEST(TreeEqualityTest, EmptyNodesNotEqualByEnum) {
  SymbolPtr tree1 = Leaf(3, "bar");
  SymbolPtr tree2 = Node();
  SymbolPtr tree3 = Leaf(5, "bar");
  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree3.get()));
}

TEST(TreeEqualityTest, EmptyNodesNotEqualByEnumString) {
  SymbolPtr tree1 = Leaf(4, "foo");
  SymbolPtr tree2 = Node();
  SymbolPtr tree3 = Leaf(3, "bar");

  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree3.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree2.get(), tree3.get()));
}

// Test SyntaxTreeNode equality
TEST(TreeEqualityTest, NonEmptyNodesEqualByEnum) {
  SymbolPtr tree1 = Node(Leaf(1, "a"), Leaf(2, "b"));
  SymbolPtr tree2 = Node(Leaf(1, "c"), Leaf(2, "c"));
  EXPECT_TRUE(EqualTreesByEnum(tree1.get(), tree2.get()));
  EXPECT_TRUE(EqualTreesByEnum(tree2.get(), tree1.get()));
}

TEST(TreeEqualityTest, NonEmptyNodesEqualByEnumString) {
  SymbolPtr tree1 = Node(Leaf(1, "bar"), Leaf(2, "foo"));
  SymbolPtr tree2 = Node(Leaf(1, "bar"), Leaf(2, "foo"));

  EXPECT_TRUE(EqualTreesByEnumString(tree1.get(), tree2.get()));
  EXPECT_TRUE(EqualTreesByEnumString(tree2.get(), tree1.get()));
}

TEST(TreeEqualityTest, NonEmptyNodesNotEqualByEnum) {
  constexpr std::string_view foo;
  SymbolPtr tree1 = Node(Leaf(1, foo), Leaf(2, foo));
  SymbolPtr tree2 = Node(Leaf(1, foo), Leaf(2, foo), Leaf(3, foo));
  SymbolPtr tree3 = Node(Leaf(3, foo), Leaf(1, foo), Leaf(2, foo));

  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree2.get(), tree1.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree3.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree3.get(), tree1.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree2.get(), tree3.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree3.get(), tree2.get()));
}

TEST(TreeEqualityTest, NonEmptyNodesNotEqualByEnumString) {
  constexpr std::string_view foo("Foo"), bar("Bar");
  SymbolPtr tree1 = Node(Leaf(1, bar), Leaf(2, foo));
  SymbolPtr tree2 = Node(Leaf(1, foo), Leaf(2, bar));
  SymbolPtr tree3 = Node(Leaf(3, foo), Leaf(1, foo), Leaf(2, bar));

  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree2.get(), tree1.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree1.get(), tree3.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree3.get(), tree1.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree2.get(), tree3.get()));
  EXPECT_FALSE(EqualTreesByEnumString(tree3.get(), tree2.get()));
}

// Test arbitrary structure equality
TEST(TreeEqualityTest, ManyLayeredTreeEqual) {
  SymbolPtr tree1 =
      Node(Node(XLeaf(1), nullptr, XLeaf(2)), XLeaf(1), XLeaf(2), nullptr);
  SymbolPtr tree2 =
      Node(Node(XLeaf(1), nullptr, XLeaf(2)), XLeaf(1), XLeaf(2), nullptr);

  EXPECT_TRUE(EqualTreesByEnum(tree1.get(), tree2.get()));
  EXPECT_TRUE(EqualTreesByEnum(tree2.get(), tree1.get()));
}

TEST(TreeEqualityTest, SameStructureTreeNotEqual) {
  SymbolPtr tree1 =
      Node(Node(XLeaf(1), nullptr, XLeaf(3)), XLeaf(1), XLeaf(2), nullptr);
  SymbolPtr tree2 =
      Node(Node(XLeaf(1), nullptr, XLeaf(2)), XLeaf(1), XLeaf(2), nullptr);

  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTreesByEnum(tree2.get(), tree1.get()));
}

TEST(TreeEqualityTest, DifferentStructureTreeNotEqual) {
  SymbolPtr tree1 = Node(Node(XLeaf(1), nullptr, XLeaf(3)), XLeaf(1), XLeaf(2),
                         nullptr, Node(XLeaf(2)));
  SymbolPtr tree2 = Node(Node(XLeaf(1), nullptr, XLeaf(2)), XLeaf(1), XLeaf(2),
                         nullptr, nullptr);

  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree2.get()));
}

TEST(TreeEqualityTest, SubTreeNotEqual) {
  SymbolPtr tree1 = Node(Node(XLeaf(1), nullptr, XLeaf(3)), XLeaf(1), XLeaf(2),
                         nullptr, Node(XLeaf(2)));
  SymbolPtr tree2 = Node(Node(Node(XLeaf(1), nullptr, XLeaf(3)), XLeaf(1),
                              XLeaf(2), nullptr, Node(XLeaf(2))));
  EXPECT_FALSE(EqualTreesByEnum(tree1.get(), tree2.get()));
}

// Test exact token-by-token equality.
TEST(TreeEqualityTest, ExactEqualPerfectMatch) {
  constexpr std::string_view foo("foo"), bar("bar");
  SymbolPtr tree1 = Node(Leaf(1, bar), Leaf(2, foo));
  SymbolPtr tree2 = Node(Leaf(1, bar), Leaf(2, foo));
  EXPECT_TRUE(EqualTrees(tree1.get(), tree2.get()));
  EXPECT_TRUE(EqualTrees(tree2.get(), tree1.get()));
}

// Test for mismatch on different leaf tag.
TEST(TreeEqualityTest, ExactEqualMismatchLeafTag) {
  constexpr std::string_view foo("foo"), bar("bar");
  SymbolPtr tree1 = Node(Leaf(1, bar), Leaf(2, foo));
  SymbolPtr tree2 = Node(Leaf(1, bar), Leaf(3, foo));
  EXPECT_FALSE(EqualTrees(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTrees(tree2.get(), tree1.get()));
}

// Test for mismatch on different token location.
TEST(TreeEqualityTest, ExactEqualMismatchTokenLocation) {
  constexpr std::string_view bar("barbar"), foo("foo");
  // guarantee different ranges
  const std::string_view bar1(bar.substr(0, 3)), bar2(bar.substr(3, 3));
  SymbolPtr tree1 = Node(Leaf(1, bar1), Leaf(2, foo));
  SymbolPtr tree2 = Node(Leaf(1, bar2), Leaf(2, foo));
  EXPECT_FALSE(EqualTrees(tree1.get(), tree2.get()));
  EXPECT_FALSE(EqualTrees(tree2.get(), tree1.get()));
}

// Test for mismatch on different token text.
TEST(TreeEqualityTest, ExactEqualMismatchTokenText) {
  constexpr std::string_view bar("bar"), foo1("foo"), foo2("f00");
  SymbolPtr tree1 = Node(Leaf(1, bar), Leaf(2, foo1));
  SymbolPtr tree2 = Node(Leaf(1, bar), Leaf(2, foo2));
  EXPECT_FALSE(EqualTrees(tree1.get(), tree2.get()));
}

}  // namespace
}  // namespace verible
