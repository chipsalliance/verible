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

#include "verible/common/text/tree-utils.h"

#include <cstddef>
#include <memory>
#include <ostream>
#include <sstream>  // IWYU pragma: keep  // for ostringstream

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/text/tree-compare.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

// Helper function for reaching only-child node.
const Symbol *ExpectOnlyChild(const Symbol &node) {
  return down_cast<const SyntaxTreeNode &>(node)[0].get();
}

// Tests that leaf is considered an only-child when descending.
TEST(DescendThroughSingletonsTest, LeafOnly) {
  SymbolPtr leaf = Leaf(0, "text");
  EXPECT_EQ(leaf.get(), DescendThroughSingletons(*leaf));
}

// Tests that descending stops at childless node.
TEST(DescendThroughSingletonsTest, EmptyNode) {
  SymbolPtr node = Node();
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

// Tests that descending stops at a node with a null-child.
TEST(DescendThroughSingletonsTest, NodeNullChild) {
  SymbolPtr node = Node(nullptr);
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

// Tests that descending reaches a leaf single-child.
TEST(DescendThroughSingletonsTest, NodeLeaf) {
  SymbolPtr node = Node(Leaf(0, "text"));
  EXPECT_EQ(ExpectOnlyChild(*node), DescendThroughSingletons(*node));
}

// Tests that descending reaches a node single-child.
TEST(DescendThroughSingletonsTest, NodeNode) {
  SymbolPtr node = Node(Node());
  EXPECT_EQ(ExpectOnlyChild(*node), DescendThroughSingletons(*node));
}

// Tests that descending reaches a single-grandchild leaf.
TEST(DescendThroughSingletonsTest, NodeNodeLeaf) {
  SymbolPtr node = Node(Node(Leaf(0, "text")));
  EXPECT_EQ(ExpectOnlyChild(*ExpectOnlyChild(*node)),
            DescendThroughSingletons(*node));
}

// Tests that descending reaches a single-grandchild node.
TEST(DescendThroughSingletonsTest, NodeNodeNode) {
  SymbolPtr node = Node(Node(Node()));
  EXPECT_EQ(ExpectOnlyChild(*ExpectOnlyChild(*node)),
            DescendThroughSingletons(*node));
}

// Tests that descending stops as a node with multiple children leaves.
TEST(DescendThroughSingletonsTest, NodeTwoLeaves) {
  SymbolPtr node = Node(Leaf(0, "more"), Leaf(0, "text"));
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

// Tests that descending stops as a node with multiple children nodes.
TEST(DescendThroughSingletonsTest, NodeTwoSubNodes) {
  SymbolPtr node = Node(Node(), Node());
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendThroughSingletonsTest, NodeFirstChildLeafSecondChildNull) {
  SymbolPtr node = Node(Leaf(0, "text"), nullptr);
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendThroughSingletonsTest, NodeFirstChildNullSecondChildLeaf) {
  SymbolPtr node = Node(nullptr, Leaf(0, "text"));
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendThroughSingletonsTest, NodeFirstChildNullSecondChildNode) {
  SymbolPtr node = Node(nullptr, Node());
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendThroughSingletonsTest, NodeFirstChildNodeSecondChildNull) {
  SymbolPtr node = Node(Node(), nullptr);
  EXPECT_EQ(node.get(), DescendThroughSingletons(*node));
}

static constexpr absl::string_view kTestToken[] = {
    "test_token1",
    "test_token2",
    "test_token3",
    "test_token4",
};

TEST(GetLeftmostLeafTest, LeafOnly) {
  SymbolPtr leaf = Leaf(0, kTestToken[0]);
  auto leaf_opt = GetLeftmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[0]);
}

TEST(GetLeftmostLeafTest, EmptyNode) {
  SymbolPtr leaf = Node();
  auto leaf_opt = GetLeftmostLeaf(*leaf);
  EXPECT_FALSE(leaf_opt);
}

TEST(GetLeftmostLeafTest, SingleChild) {
  SymbolPtr leaf = Node(Leaf(0, kTestToken[1]));
  auto leaf_opt = GetLeftmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[1]);
}

TEST(GetLeftmostLeafTest, SingleChildWithNull) {
  SymbolPtr leaf = Node(nullptr, Leaf(0, kTestToken[1]));
  auto leaf_opt = GetLeftmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[1]);
}

TEST(GetLeftmostLeafTest, SingleChildNullFromNode) {
  SymbolPtr leaf = Node(Node(nullptr), Node(Leaf(0, kTestToken[1])));
  auto leaf_opt = GetLeftmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[1]);
}

TEST(GetLeftmostLeafTest, ComplexTree) {
  SymbolPtr leaf = Node(Node(Leaf(0, kTestToken[0])), Leaf(0, kTestToken[1]),
                        Node(Leaf(0, kTestToken[2]), Leaf(0, kTestToken[3])));
  auto leaf_opt = GetLeftmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[0]);
}

TEST(GetRightmostLeafTest, LeafOnly) {
  SymbolPtr leaf = Leaf(0, kTestToken[0]);
  auto leaf_opt = GetRightmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[0]);
}

TEST(GetRightmostLeafTest, EmptyNode) {
  SymbolPtr leaf = Node();
  auto leaf_opt = GetRightmostLeaf(*leaf);
  EXPECT_FALSE(leaf_opt);
}

TEST(GetRightmostLeafTest, SingleChild) {
  SymbolPtr leaf = Node(Leaf(0, kTestToken[2]));
  auto leaf_opt = GetRightmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[2]);
}

TEST(GetRightmostLeafTest, SingleChildWithNull) {
  SymbolPtr leaf = Node(Leaf(0, kTestToken[2]), nullptr);
  auto leaf_opt = GetRightmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[2]);
}

TEST(GetRightmostLeafTest, SingleChildNullFromNode) {
  SymbolPtr leaf = Node(Node(Leaf(0, kTestToken[2])), Node(nullptr));
  auto leaf_opt = GetRightmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[2]);
}

TEST(GetRightmostLeafTest, ComplexTree) {
  SymbolPtr leaf = Node(Node(Leaf(0, kTestToken[0])), Leaf(0, kTestToken[1]),
                        Node(Leaf(0, kTestToken[2]), Leaf(0, kTestToken[3])));
  auto leaf_opt = GetRightmostLeaf(*leaf);
  EXPECT_TRUE(leaf_opt);
  EXPECT_EQ(leaf_opt->get().text(), kTestToken[3]);
}

TEST(StringSpanOfSymbolTest, EmptyTree) {
  SymbolPtr symbol = Node();
  const auto range = StringSpanOfSymbol(*symbol);
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range, "");
}

TEST(StringSpanOfSymbolTest, DeepEmptyTree) {
  SymbolPtr symbol = Node(Node(Node(Node())));
  const auto range = StringSpanOfSymbol(*symbol);
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range, "");
}

TEST(StringSpanOfSymbolTest, LeafOnlyEmptyText) {
  constexpr absl::string_view text;
  SymbolPtr symbol = Leaf(1, text);
  const auto range = StringSpanOfSymbol(*symbol);
  EXPECT_TRUE(range.empty());
  EXPECT_TRUE(BoundsEqual(range, text));
}

TEST(StringSpanOfSymbolTest, LeafOnlyNonemptyText) {
  constexpr absl::string_view text("asdfg");
  SymbolPtr symbol = Leaf(1, text);
  const auto range = StringSpanOfSymbol(*symbol);
  EXPECT_TRUE(BoundsEqual(range, text));
}

TEST(StringSpanOfSymbolTest, DeepLeafOnlyEmptyText) {
  constexpr absl::string_view text;
  SymbolPtr symbol = Node(Node(Leaf(1, text)));
  const auto range = StringSpanOfSymbol(*symbol);
  EXPECT_TRUE(BoundsEqual(range, text));
}

TEST(StringSpanOfSymbolTest, TwoLeavesOneTree) {
  constexpr absl::string_view text("aaabbb");
  SymbolPtr symbol =
      Node(Node(Leaf(1, text.substr(0, 3))), Node(Leaf(2, text.substr(3, 3))));
  const auto range = StringSpanOfSymbol(*symbol);
  EXPECT_TRUE(BoundsEqual(range, text));
}

TEST(StringSpanOfSymbolTest, TwoAbuttingLeavesTwoTrees) {
  constexpr absl::string_view text("aaabbb");
  SymbolPtr lsymbol = Node(Node(Leaf(1, text.substr(0, 3))));
  SymbolPtr rsymbol = Node(Node(Leaf(1, text.substr(3, 3))));
  const auto range = StringSpanOfSymbol(*lsymbol, *rsymbol);
  EXPECT_TRUE(BoundsEqual(range, text));
}

TEST(StringSpanOfSymbolTest, TwoDisjointLeavesTwoTrees) {
  constexpr absl::string_view text("aaabbb");
  SymbolPtr lsymbol = Node(Node(Leaf(1, text.substr(0, 2))));
  SymbolPtr rsymbol = Node(Node(Leaf(1, text.substr(4, 2))));
  const auto range = StringSpanOfSymbol(*lsymbol, *rsymbol);
  EXPECT_TRUE(BoundsEqual(range, text));
}

TEST(TreePrintTest, RawPrint) {
  constexpr absl::string_view text("leaf 1 leaf 2 leaf 3 leaf 4");
  SymbolPtr tree = Node(Leaf(0, text.substr(0, 6)),        //
                        Node(                              //
                            Leaf(1, text.substr(7, 6)),    //
                            Leaf(2, text.substr(14, 6))),  //
                        Leaf(3, text.substr(21, 6)));
  // Output excludes byte offsets.
  constexpr absl::string_view expected =
      "Node @0 {\n"
      "  Leaf @0 (#0: \"leaf 1\")\n"
      "  Node @1 {\n"
      "    Leaf @0 (#1: \"leaf 2\")\n"
      "    Leaf @1 (#2: \"leaf 3\")\n"
      "  }\n"
      "  Leaf @2 (#3: \"leaf 4\")\n"
      "}\n";
  std::ostringstream stream;
  stream << RawTreePrinter(*tree);
  EXPECT_EQ(stream.str(), expected);
}

TEST(TreePrintTest, RawPrintNullptrPrinting) {
  constexpr absl::string_view text("leaf 1 leaf 2 leaf 3 leaf 4");
  SymbolPtr tree = Node(nullptr,                           //
                        Leaf(0, text.substr(0, 6)),        //
                        nullptr,                           //
                        Node(                              //
                            Leaf(1, text.substr(7, 6)),    //
                            nullptr,                       //
                            Leaf(2, text.substr(14, 6))),  //
                        nullptr,                           //
                        Leaf(3, text.substr(21, 6)),       //
                        nullptr);
  {
    // Output excludes byte offsets.
    constexpr absl::string_view expected =
        "Node @0 {\n"
        "  Leaf @1 (#0: \"leaf 1\")\n"
        "  Node @3 {\n"
        "    Leaf @0 (#1: \"leaf 2\")\n"
        "    Leaf @2 (#2: \"leaf 3\")\n"
        "  }\n"
        "  Leaf @5 (#3: \"leaf 4\")\n"
        "}\n";
    std::ostringstream stream;
    stream << RawTreePrinter(*tree);
    EXPECT_EQ(stream.str(), expected);
  }
  {
    // Now the same, but with nullptr printing enabled.
    constexpr absl::string_view expected =
        "Node @0 {\n"
        "  NULL @0\n"
        "  Leaf @1 (#0: \"leaf 1\")\n"
        "  NULL @2\n"
        "  Node @3 {\n"
        "    Leaf @0 (#1: \"leaf 2\")\n"
        "    NULL @1\n"
        "    Leaf @2 (#2: \"leaf 3\")\n"
        "  }\n"
        "  NULL @4\n"
        "  Leaf @5 (#3: \"leaf 4\")\n"
        "  NULL @6\n"
        "}\n";
    std::ostringstream stream;
    stream << RawTreePrinter(*tree, true);
    EXPECT_EQ(stream.str(), expected);
  }
}

TEST(TreePrintTest, PrettyPrint) {
  constexpr absl::string_view text("leaf 1 leaf 2 leaf 3 leaf 4");
  SymbolPtr tree = Node(                 //
      Leaf(0, text.substr(0, 6)),        //
      Node(                              //
          Leaf(1, text.substr(7, 6)),    //
          Leaf(2, text.substr(14, 6))),  //
      Leaf(3, text.substr(21, 6)));
  // Output includes byte offsets.
  constexpr absl::string_view expected =
      "Node @0 {\n"
      "  Leaf @0 (#0 @0-6: \"leaf 1\")\n"
      "  Node @1 {\n"
      "    Leaf @0 (#1 @7-13: \"leaf 2\")\n"
      "    Leaf @1 (#2 @14-20: \"leaf 3\")\n"
      "  }\n"
      "  Leaf @2 (#3 @21-27: \"leaf 4\")\n"
      "}\n";

  const TokenInfo::Context context(text);
  {
    std::ostringstream stream;
    PrettyPrintTree(*tree, context, &stream);
    EXPECT_EQ(stream.str(), expected);
  }
  {
    std::ostringstream stream;
    stream << TreePrettyPrinter(*tree, context);
    EXPECT_EQ(stream.str(), expected);
  }
}

TEST(TreePrintTest, PrettyPrintSkipNullptrs) {
  constexpr absl::string_view text("leaf 1 leaf 2 leaf 3 leaf 4");
  SymbolPtr tree = Node(                 //
      Leaf(0, text.substr(0, 6)),        //
      nullptr,                           //
      nullptr,                           //
      Node(                              //
          nullptr,                       //
          Leaf(1, text.substr(7, 6)),    //
          nullptr,                       //
          Leaf(2, text.substr(14, 6))),  //
      nullptr,                           //
      nullptr,                           //
      Leaf(3, text.substr(21, 6)));
  // Output includes byte offsets.
  constexpr absl::string_view expected =
      "Node @0 {\n"
      "  Leaf @0 (#0 @0-6: \"leaf 1\")\n"
      "  Node @3 {\n"
      "    Leaf @1 (#1 @7-13: \"leaf 2\")\n"
      "    Leaf @3 (#2 @14-20: \"leaf 3\")\n"
      "  }\n"
      "  Leaf @6 (#3 @21-27: \"leaf 4\")\n"
      "}\n";

  const TokenInfo::Context context(text);
  {
    std::ostringstream stream;
    PrettyPrintTree(*tree, context, &stream);
    EXPECT_EQ(stream.str(), expected);
  }
  {
    std::ostringstream stream;
    stream << TreePrettyPrinter(*tree, context);
    EXPECT_EQ(stream.str(), expected);
  }
}

// FindFirstSubtreeMutable tests:

// Test that 1-node tree and always-true predicate yields the root node.
TEST(FindFirstSubtreeMutableTest, OneNodePredicateTrue) {
  SymbolPtr tree = Node();
  SymbolPtr *result =
      FindFirstSubtreeMutable(&tree, [](const Symbol &) { return true; });
  EXPECT_EQ(result, &tree);
}

// Test that 1-node tree and always-false predicate yields no match.
TEST(FindFirstSubtreeMutableTest, OneNodePredicateFalse) {
  SymbolPtr tree = Node();
  SymbolPtr *result =
      FindFirstSubtreeMutable(&tree, [](const Symbol &) { return false; });
  EXPECT_EQ(result, nullptr);
}

// Test that 2-node tree and always-true predicate yields the root node.
TEST(FindFirstSubtreeMutableTest, TwoLevelPredicateTrue) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr *result =
      FindFirstSubtreeMutable(&tree, [](const Symbol &) { return true; });
  EXPECT_EQ(result, &tree);
}

// Test that 2-node tree and always-false predicate yields nullptr.
TEST(FindFirstSubtreeMutableTest, TwoLevelPredicateFalse) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr *result =
      FindFirstSubtreeMutable(&tree, [](const Symbol &) { return false; });
  EXPECT_EQ(result, nullptr);
}

bool IsNodeTagged(const Symbol &s, int tag) {
  const SymbolTag t = s.Tag();
  return t.kind == SymbolKind::kNode && t.tag == tag;
}

bool IsLeafTagged(const Symbol &s, int tag) {
  const SymbolTag t = s.Tag();
  return t.kind == SymbolKind::kLeaf && t.tag == tag;
}

// Test that tree and predicate can match the root node.
TEST(FindFirstSubtreeMutableTest, TwoLevelNodeTagIs1) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsNodeTagged(s, 1); });
  EXPECT_EQ(result, &tree);
}

// Test that tree and predicate can skip over null nodes.
TEST(FindFirstSubtreeMutableTest, SkipNulls) {
  SymbolPtr tree = TNode(1, nullptr, nullptr, TNode(2), nullptr);
  SymbolPtr expect = TNode(2);
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result->get(), expect.get()));
}

// Test that tree and predicate can match no node.
TEST(FindFirstSubtreeMutableTest, TwoLevelNoNodeMatch) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsLeafTagged(s, 3); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match no node with null leaves.
TEST(FindFirstSubtreeMutableTest, TwoLevelNoNodeMatchNullLeaves) {
  SymbolPtr tree = TNode(1, nullptr, nullptr);
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsLeafTagged(s, 4); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match no leaf.
TEST(FindFirstSubtreeMutableTest, TwoLevelNoLeafTag) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsLeafTagged(s, 1); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match an inner node.
TEST(FindFirstSubtreeMutableTest, TwoLevelNodeTagIs2) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr expect = TNode(2);
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result->get(), expect.get()));
}

// Test that tree and predicate can match an inner leaf.
TEST(FindFirstSubtreeMutableTest, TwoLevelLeafTagIs2) {
  SymbolPtr tree = TNode(1, XLeaf(2));
  SymbolPtr expect = XLeaf(2);
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsLeafTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result->get(), expect.get()));
}

// Test that tree and predicate can match no leaf.
TEST(FindFirstSubtreeMutableTest, TwoLevelLeafTagNoMatch) {
  SymbolPtr tree = TNode(1, XLeaf(2), XLeaf(3));
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsLeafTagged(s, 1); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate finds the first match out of many.
TEST(FindFirstSubtreeMutableTest, MatchFirstOfSiblings) {
  SymbolPtr tree = TNode(1, TNode(2, TNode(3)), TNode(2, TNode(4)));
  SymbolPtr expect = TNode(2, TNode(3));
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result->get(), expect.get()));
}

// Test that tree and predicate finds the first match in-order.
TEST(FindFirstSubtreeMutableTest, MatchFirstInOrder) {
  SymbolPtr tree = TNode(1, TNode(2, TNode(3)), TNode(3, TNode(4)));
  SymbolPtr expect = TNode(3);
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsNodeTagged(s, 3); });
  EXPECT_TRUE(EqualTreesByEnum(result->get(), expect.get()));
}

// Test that tree and predicate finds the first match in-order.
TEST(FindFirstSubtreeMutableTest, MatchLeaf) {
  SymbolPtr tree = TNode(1, TNode(2, XLeaf(3)), TNode(3, TNode(4)));
  SymbolPtr expect = XLeaf(3);
  SymbolPtr *result = FindFirstSubtreeMutable(
      &tree, [](const Symbol &s) { return IsLeafTagged(s, 3); });
  EXPECT_TRUE(EqualTreesByEnum(result->get(), expect.get()));
}

// FindFirstSubtree test

// Test that 1-node tree and always-true predicate yields the root node.
TEST(FindFirstSubtreeTest, OneNodePredicateTrue) {
  SymbolPtr tree = Node();
  const Symbol *result =
      FindFirstSubtree(tree.get(), [](const Symbol &) { return true; });
  EXPECT_EQ(result, tree.get());
}

// Test that 1-node tree and always-false predicate yields no match.
TEST(FindFirstSubtreeTest, OneNodePredicateFalse) {
  SymbolPtr tree = Node();
  const Symbol *result =
      FindFirstSubtree(tree.get(), [](const Symbol &) { return false; });
  EXPECT_EQ(result, nullptr);
}

// Test that 2-node tree and always-true predicate yields the root node.
TEST(FindFirstSubtreeTest, TwoLevelPredicateTrue) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result =
      FindFirstSubtree(tree.get(), [](const Symbol &) { return true; });
  EXPECT_EQ(result, tree.get());
}

// Test that 2-node tree and always-false predicate yields nullptr.
TEST(FindFirstSubtreeTest, TwoLevelPredicateFalse) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result =
      FindFirstSubtree(tree.get(), [](const Symbol &) { return false; });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match the root node.
TEST(FindFirstSubtreeTest, TwoLevelNodeTagIs1) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 1); });
  EXPECT_EQ(result, tree.get());
}

// Test that tree and predicate can skip over null nodes.
TEST(FindFirstSubtreeTest, SkipNulls) {
  SymbolPtr tree = TNode(1, nullptr, nullptr, TNode(2), nullptr);
  SymbolPtr expect = TNode(2);
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate can match no node.
TEST(FindFirstSubtreeTest, TwoLevelNoNodeMatch) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 3); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match no node with null leaves.
TEST(FindFirstSubtreeTest, TwoLevelNoNodeMatchNullLeaves) {
  SymbolPtr tree = TNode(1, nullptr, nullptr);
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 4); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match no leaf.
TEST(FindFirstSubtreeTest, TwoLevelNoLeafTag) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 1); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match an inner node.
TEST(FindFirstSubtreeTest, TwoLevelNodeTagIs2) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr expect = TNode(2);
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate can match an inner leaf.
TEST(FindFirstSubtreeTest, TwoLevelLeafTagIs2) {
  SymbolPtr tree = TNode(1, XLeaf(2));
  SymbolPtr expect = XLeaf(2);
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate can match no leaf.
TEST(FindFirstSubtreeTest, TwoLevelLeafTagNoMatch) {
  SymbolPtr tree = TNode(1, XLeaf(2), XLeaf(3));
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 1); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate finds the first match out of many.
TEST(FindFirstSubtreeTest, MatchFirstOfSiblings) {
  SymbolPtr tree = TNode(1, TNode(2, TNode(3)), TNode(2, TNode(4)));
  SymbolPtr expect = TNode(2, TNode(3));
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate finds the first match in-order.
TEST(FindFirstSubtreeTest, MatchFirstInOrder) {
  SymbolPtr tree = TNode(1, TNode(2, TNode(3)), TNode(3, TNode(4)));
  SymbolPtr expect = TNode(3);
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 3); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate finds the first match in-order.
TEST(FindFirstSubtreeTest, MatchLeaf) {
  SymbolPtr tree = TNode(1, TNode(2, XLeaf(3)), TNode(3, TNode(4)));
  SymbolPtr expect = XLeaf(3);
  const Symbol *result = FindFirstSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 3); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// FindLastSubtree test

// Test that 1-node tree and always-true predicate yields the root node.
TEST(FindLastSubtreeTest, OneNodePredicateTrue) {
  SymbolPtr tree = Node();
  const Symbol *result =
      FindLastSubtree(tree.get(), [](const Symbol &) { return true; });
  EXPECT_EQ(result, tree.get());
}

// Test that 1-node tree and always-false predicate yields no match.
TEST(FindLastSubtreeTest, OneNodePredicateFalse) {
  SymbolPtr tree = Node();
  const Symbol *result =
      FindLastSubtree(tree.get(), [](const Symbol &) { return false; });
  EXPECT_EQ(result, nullptr);
}

// Test that 2-node tree and always-true predicate yields the second node.
TEST(FindLastSubtreeTest, TwoLevelPredicateTrue) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr expect = TNode(2);
  const Symbol *result =
      FindLastSubtree(tree.get(), [](const Symbol &) { return true; });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that 2-node tree and always-false predicate yields nullptr.
TEST(FindLastSubtreeTest, TwoLevelPredicateFalse) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result =
      FindLastSubtree(tree.get(), [](const Symbol &) { return false; });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match the root node.
TEST(FindLastSubtreeTest, TwoLevelNodeTagIs1) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 1); });
  EXPECT_EQ(result, tree.get());
}

// Test that tree and predicate can skip over null nodes.
TEST(FindLastSubtreeTest, SkipNulls) {
  SymbolPtr tree = TNode(1, nullptr, nullptr, TNode(2), nullptr);
  SymbolPtr expect = TNode(2);
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate can match no node.
TEST(FindLastSubtreeTest, TwoLevelNoNodeMatch) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 3); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match no node with null leaves.
TEST(FindLastSubtreeTest, TwoLevelNoNodeMatchNullLeaves) {
  SymbolPtr tree = TNode(1, nullptr, nullptr);
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 4); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match no leaf.
TEST(FindLastSubtreeTest, TwoLevelNoLeafTag) {
  SymbolPtr tree = TNode(1, TNode(2));
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 1); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate can match an inner node.
TEST(FindLastSubtreeTest, TwoLevelNodeTagIs2) {
  SymbolPtr tree = TNode(1, TNode(2));
  SymbolPtr expect = TNode(2);
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate can match an inner leaf.
TEST(FindLastSubtreeTest, TwoLevelLeafTagIs2) {
  SymbolPtr tree = TNode(1, XLeaf(2));
  SymbolPtr expect = XLeaf(2);
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate can match no leaf.
TEST(FindLastSubtreeTest, TwoLevelLeafTagNoMatch) {
  SymbolPtr tree = TNode(1, XLeaf(2), XLeaf(3));
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsLeafTagged(s, 1); });
  EXPECT_EQ(result, nullptr);
}

// Test that tree and predicate finds the last matching subtree.
TEST(FindLastSubtreeTest, MatchLastOfSiblings) {
  SymbolPtr tree = TNode(1, TNode(2, TNode(2, TNode(2))), TNode(2, TNode(3)));
  SymbolPtr expect = TNode(2, TNode(3));
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 2); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate finds the last match in-order.
TEST(FindLastSubtreeTest, MatchLastInOrder) {
  SymbolPtr tree = TNode(1, TNode(2, TNode(3)), TNode(3, TNode(4)));
  SymbolPtr expect = TNode(3, TNode(4));
  const Symbol *result = FindLastSubtree(
      tree.get(), [](const Symbol &s) { return IsNodeTagged(s, 3); });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// Test that tree and predicate finds the last match in-order.
TEST(FindLastSubtreeTest, MatchLeaf) {
  SymbolPtr tree = TNode(1, TNode(2, XLeaf(3)), TNode(3, TNode(4, XLeaf(4))));
  SymbolPtr expect = XLeaf(4);
  const Symbol *result = FindLastSubtree(tree.get(), [](const Symbol &s) {
    return IsLeafTagged(s, 3) || IsLeafTagged(s, 4);
  });
  EXPECT_TRUE(EqualTreesByEnum(result, expect.get()));
}

// FindSubtreeStartingAtOffset tests

constexpr absl::string_view kFindSubtreeTestText("abcdef");
const absl::string_view kFindSubtreeTestSubstring(
    kFindSubtreeTestText.substr(1, 3));

struct FindSubtreeStartingAtOffsetTest : public testing::Test {
  SymbolPtr tree;
  FindSubtreeStartingAtOffsetTest()
      : tree(Leaf(0, kFindSubtreeTestSubstring)) {}
};

// Test that a single leaf yields itself when it starts < offset.
TEST_F(FindSubtreeStartingAtOffsetTest, LeafOnlyOffsetLessThan) {
  SymbolPtr *subtree =
      FindSubtreeStartingAtOffset(&tree, kFindSubtreeTestText.begin());
  EXPECT_EQ(*subtree, tree);
}

// Test that a single leaf yields itself when it starts == offset.
TEST_F(FindSubtreeStartingAtOffsetTest, LeafOnlyOffsetLeftEqual) {
  SymbolPtr *subtree =
      FindSubtreeStartingAtOffset(&tree, kFindSubtreeTestText.begin() + 1);
  EXPECT_EQ(*subtree, tree);
}

// Test that a single leaf yields null when it starts > offset.
TEST_F(FindSubtreeStartingAtOffsetTest, LeafOnlyOffsetGreaterThan) {
  SymbolPtr *subtree =
      FindSubtreeStartingAtOffset(&tree, kFindSubtreeTestText.begin() + 2);
  EXPECT_EQ(subtree, nullptr);
}

// Allow to look beyond kBaseText by cutting it out of a larger string.
constexpr absl::string_view kBaseTextPadded("_abcdefghijklmnopqrst_");
constexpr absl::string_view kBaseText = {kBaseTextPadded.data() + 1,
                                         kBaseTextPadded.length() - 2};

// Return a tree with monotonically increasing token locations.
// This is suitable for tests that require only location offsets,
// and do not need actual text.
SymbolPtr FakeSyntaxTree() {
  // Leaf(tag, substr)
  return TNode(0,                                      // noformat
               TNode(1,                                // noformat
                     Leaf(0, kBaseText.substr(0, 5))   // noformat
                     ),                                // noformat
               Leaf(0, kBaseText.substr(5, 3)),        // noformat
               TNode(2,                                // noformat
                     Leaf(0, kBaseText.substr(8, 2)),  // noformat
                     Leaf(0, kBaseText.substr(12, 5))  // noformat
                     )                                 // noformat
  );
}

struct FindSubtreeStartingAtOffsetFakeTreeTest : public testing::Test {
  SymbolPtr tree;
  FindSubtreeStartingAtOffsetFakeTreeTest() : tree(FakeSyntaxTree()) {}
};

// Test that a whole tree is returned when offset is less than leftmost token.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetLessThanFirstToken) {
  // Note: begin() - 1 points to a valid location due to kBaseTextPadded
  SymbolPtr *subtree =
      FindSubtreeStartingAtOffset(&tree, kBaseText.begin() - 1);
  EXPECT_EQ(*subtree, tree);
}

// Test that a whole tree is returned when offset starts at leftmost token.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetAtFirstToken) {
  SymbolPtr *subtree = FindSubtreeStartingAtOffset(&tree, kBaseText.begin());
  EXPECT_EQ(*subtree, tree);
}

// Test that an offset past last token yields nullptr.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetPastLastToken) {
  SymbolPtr *subtree =
      FindSubtreeStartingAtOffset(&tree, kBaseText.begin() + 18);
  EXPECT_EQ(subtree, nullptr);
}

// Test that an offset in middle of last token yields nullptr.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetInsideLastToken) {
  SymbolPtr *subtree =
      FindSubtreeStartingAtOffset(&tree, kBaseText.begin() + 13);
  EXPECT_EQ(subtree, nullptr);
}

// Test that a subtree is returned when offset starts inside leftmost token.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetInsideFirstToken) {
  SymbolPtr expect = Leaf(0, kBaseText.substr(0, 5));
  for (int offset = 1; offset <= 4; ++offset) {
    SymbolPtr *subtree =
        FindSubtreeStartingAtOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTreesByEnum(subtree->get(), expect.get()));
  }
}

// Test that a subtree is returned when offset starts inside leftmost token.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetInsideMiddleSubtree) {
  SymbolPtr expect = Leaf(0, kBaseText.substr(5, 3));
  SymbolPtr *subtree =
      FindSubtreeStartingAtOffset(&tree, kBaseText.begin() + 5);
  EXPECT_TRUE(EqualTreesByEnum(subtree->get(), expect.get()));
}

// Test that a subtree is returned when offset starts at last subtree.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetAtLastSubtree) {
  SymbolPtr expect = TNode(2,                                // noformat
                           Leaf(0, kBaseText.substr(8, 2)),  // noformat
                           Leaf(0, kBaseText.substr(12, 5))  // noformat
  );
  for (int offset = 6; offset <= 8; ++offset) {
    SymbolPtr *subtree =
        FindSubtreeStartingAtOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTreesByEnum(subtree->get(), expect.get()));
  }
}

// Test that a subtree is returned when offset starts at last leaf.
TEST_F(FindSubtreeStartingAtOffsetFakeTreeTest, TreeOffsetAtLastLeaf) {
  SymbolPtr expect = Leaf(0, kBaseText.substr(8, 2));
  for (int offset = 9; offset <= 12; ++offset) {
    SymbolPtr *subtree =
        FindSubtreeStartingAtOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTreesByEnum(subtree->get(), expect.get()));
  }
}

// MutateLeaves tests

// Example LeafMutator
void SetLeafEnum(TokenInfo *token) { token->set_token_enum(9); }

// Test that null unique_ptr is ignored.
TEST(MutateLeavesTest, UniqueNullPtr) {
  SymbolPtr tree;
  MutateLeaves(&tree, SetLeafEnum);
}

// Test that a node with no leaves is unchanged.
TEST(MutateLeavesTest, OneNode) {
  SymbolPtr tree = Node();
  SymbolPtr expect = Node();
  MutateLeaves(&tree, SetLeafEnum);
  EXPECT_TRUE(EqualTreesByEnum(tree.get(), expect.get()));
}

// Test that a node with nullptr subnodes is unchanged.
TEST(MutateLeavesTest, TreeWithNulls) {
  SymbolPtr tree = Node(nullptr, Node(nullptr, Node()), nullptr);
  SymbolPtr expect = Node(nullptr, Node(nullptr, Node()), nullptr);
  MutateLeaves(&tree, SetLeafEnum);
  EXPECT_TRUE(EqualTreesByEnum(tree.get(), expect.get()));
}

// Test that a tree with no leaves is unchanged.
TEST(MutateLeavesTest, NodesOnly) {
  SymbolPtr tree = TNode(3, TNode(2, TNode(4)), TNode(1));
  SymbolPtr expect = TNode(3, TNode(2, TNode(4)), TNode(1));
  MutateLeaves(&tree, SetLeafEnum);
  EXPECT_TRUE(EqualTreesByEnum(tree.get(), expect.get()));
}

// Test that a single leaf at the root is transformed.
TEST(MutateLeavesTest, OneLeaf) {
  SymbolPtr tree = XLeaf(0);
  SymbolPtr expect = XLeaf(9);
  MutateLeaves(&tree, SetLeafEnum);
  EXPECT_TRUE(EqualTreesByEnum(tree.get(), expect.get()));
}

// Test that a single leaf deep in the tree is transformed.
TEST(MutateLeavesTest, NodeAndLeaf) {
  SymbolPtr tree = Node(Node(XLeaf(0)));
  SymbolPtr expect = Node(Node(XLeaf(9)));
  MutateLeaves(&tree, SetLeafEnum);
  EXPECT_TRUE(EqualTreesByEnum(tree.get(), expect.get()));
}

// Test that a single leaf deep in the tree with nulls is transformed.
TEST(MutateLeavesTest, NodeAndLeafAndNulls) {
  SymbolPtr tree = Node(nullptr, Node(nullptr, XLeaf(0), nullptr), XLeaf(2));
  SymbolPtr expect = Node(nullptr, Node(nullptr, XLeaf(9), nullptr), XLeaf(9));
  MutateLeaves(&tree, SetLeafEnum);
  EXPECT_TRUE(EqualTreesByEnum(tree.get(), expect.get()));
}

// Test that all leaves deep in the tree are transformed.
TEST(MutateLeavesTest, NodeAndLeaves) {
  SymbolPtr tree = TNode(0, TNode(8, XLeaf(0), TNode(4, XLeaf(1), XLeaf(3))));
  SymbolPtr expect = TNode(0, TNode(8, XLeaf(9), TNode(4, XLeaf(9), XLeaf(9))));
  MutateLeaves(&tree, SetLeafEnum);
  EXPECT_TRUE(EqualTreesByEnum(tree.get(), expect.get()));
}

// PruneSyntaxTreeAfterOffset tests

// Test that a leafless root node is not pruned.
TEST(PruneSyntaxTreeAfterOffsetTest, LeaflessRootNode) {
  constexpr absl::string_view text;
  SymbolPtr tree = Node();
  SymbolPtr expect = Node();  // distinct copy
  PruneSyntaxTreeAfterOffset(&tree, text.begin());
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
}

// Test that a root leaf is never pruned.
TEST(PruneSyntaxTreeAfterOffsetTest, RootLeafNeverRemoved) {
  constexpr absl::string_view text("baz");
  SymbolPtr tree = Leaf(1, text);
  SymbolPtr expect = Leaf(1, text);  // distinct copy
  PruneSyntaxTreeAfterOffset(&tree, text.begin());
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
}

// Test that a leafless tree is pruned down to the root.
TEST(PruneSyntaxTreeAfterOffsetTest, EmptyNodes) {
  constexpr absl::string_view text("baz");
  SymbolPtr tree = Node(TNode(1), TNode(1, TNode(0), TNode(3)), TNode(2));
  SymbolPtr expect = Node();
  PruneSyntaxTreeAfterOffset(&tree, text.begin());
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
}

// Test that a offset greater than rightmost location prunes nothing.
TEST(PruneSyntaxTreeAfterOffsetTest, AtEndPrunesNothing) {
  SymbolPtr tree = FakeSyntaxTree();
  SymbolPtr expect = FakeSyntaxTree();  // distinct copy
  PruneSyntaxTreeAfterOffset(&tree, kBaseText.begin() + 17);
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
}

// Test that trailing nullptr nodes are pruned.
TEST(PruneSyntaxTreeAfterOffsetTest, PruneTrailingNullNodes) {
  constexpr absl::string_view text("foo bar");
  const absl::string_view foo(text.substr(0, 3)), bar(text.substr(4, 3));
  SymbolPtr tree = TNode(0,             // noformat
                         nullptr,       // noformat
                         Leaf(1, foo),  // noformat
                         nullptr,       // noformat
                         Leaf(2, bar),  // noformat
                         nullptr        // noformat
  );
  const SymbolPtr expect = TNode(0,            // noformat
                                 nullptr,      // noformat
                                 Leaf(1, foo)  // noformat
  );
  PruneSyntaxTreeAfterOffset(&tree, text.begin() + 4);
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
}

// Test that trailing nullptr nodes are pruned recursively.
TEST(PruneSyntaxTreeAfterOffsetTest, PruneTrailingNullNodesRecursive) {
  constexpr absl::string_view text("foo bar BQ");
  const absl::string_view foo(text.substr(0, 3)), bar(text.substr(4, 3)),
      bq(text.substr(8, 2));
  SymbolPtr tree = TNode(0,                   // noformat
                         nullptr,             // noformat
                         Leaf(1, foo),        // noformat
                         nullptr,             // noformat
                         TNode(2,             // noformat
                               Leaf(2, bar),  // noformat
                               Leaf(2, bq)    // noformat
                               ),             // noformat
                         nullptr              // noformat
  );
  SymbolPtr expect = TNode(0,            // noformat
                           nullptr,      // noformat
                           Leaf(1, foo)  // noformat
  );
  PruneSyntaxTreeAfterOffset(&tree, text.begin() + 4);
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
}

// Test that offset pointing to middle of rightmost leaf prunes it.
TEST(PruneSyntaxTreeAfterOffsetTest, PruneRightmostLeaf) {
  SymbolPtr expect = TNode(0,                                      // noformat
                           TNode(1,                                // noformat
                                 Leaf(0, kBaseText.substr(0, 5))   // noformat
                                 ),                                // noformat
                           Leaf(0, kBaseText.substr(5, 3)),        // noformat
                           TNode(2,                                // noformat
                                 Leaf(0, kBaseText.substr(8, 2)))  // noformat
  );
  for (int offset = 10; offset <= 16; ++offset) {
    SymbolPtr tree = FakeSyntaxTree();
    PruneSyntaxTreeAfterOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
        << "failed at offset " << offset;
  }
}

// Test that offset pointing before rightmost leaf prunes it.
TEST(PruneSyntaxTreeAfterOffsetTest, PruneBeforeRightmostLeaf) {
  SymbolPtr expect = TNode(0,                                     // noformat
                           TNode(1,                               // noformat
                                 Leaf(0, kBaseText.substr(0, 5))  // noformat
                                 ),                               // noformat
                           Leaf(0, kBaseText.substr(5, 3)),       // noformat
                           TNode(2,                               // noformat
                                 Leaf(0, kBaseText.substr(8, 2))  // noformat
                                 )                                // noformat
  );
  for (int offset = 10; offset <= 11; ++offset) {
    SymbolPtr tree = FakeSyntaxTree();
    PruneSyntaxTreeAfterOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
        << "failed at offset " << offset;
  }
}

// Test that offset pointing to first node of rightmost subtree also removes
// that subtree because it becomes empty.
TEST(PruneSyntaxTreeAfterOffsetTest, PruneRightmostSubtree) {
  SymbolPtr expect = TNode(0,                                     // noformat
                           TNode(1,                               // noformat
                                 Leaf(0, kBaseText.substr(0, 5))  // noformat
                                 ),                               // noformat
                           Leaf(0, kBaseText.substr(5, 3))        // noformat
  );
  for (int offset = 8; offset <= 9; ++offset) {
    SymbolPtr tree = FakeSyntaxTree();
    PruneSyntaxTreeAfterOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
        << "failed at offset " << offset;
  }
}

// Test that offset pointing after first node of rightmost subtree preserves
// only that subtree.
TEST(PruneSyntaxTreeAfterOffsetTest, PreserveLeftmostSubtree) {
  SymbolPtr expect = TNode(0,                                     // noformat
                           TNode(1,                               // noformat
                                 Leaf(0, kBaseText.substr(0, 5))  // noformat
                                 )                                // noformat
  );
  for (int offset = 5; offset <= 7; ++offset) {
    SymbolPtr tree = FakeSyntaxTree();
    PruneSyntaxTreeAfterOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
        << "failed at offset " << offset;
  }
}

// Test that offset pointing to or before the first token clears out the
// entire tree.
TEST(PruneSyntaxTreeAfterOffsetTest, DeleteAll) {
  SymbolPtr expect = TNode(0);
  for (int offset = -1; offset <= 4; ++offset) {
    SymbolPtr tree = FakeSyntaxTree();
    PruneSyntaxTreeAfterOffset(&tree, kBaseText.begin() + offset);
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
        << "failed at offset " << offset;
  }
}

// TrimSyntaxTree tests

// Test that root node without leaves is cleared because no locations match.
TEST(TrimSyntaxTreeTest, RootNodeOnly) {
  constexpr absl::string_view range;
  SymbolPtr tree = Node();
  TrimSyntaxTree(&tree, range);
  EXPECT_EQ(tree, nullptr);
}

// Test that tree without leaves is cleared because no locations match.
TEST(TrimSyntaxTreeTest, TreeNoLeaves) {
  constexpr absl::string_view range;
  SymbolPtr tree = Node(TNode(4), TNode(3, TNode(1), TNode(2)), TNode(0));
  TrimSyntaxTree(&tree, range);
  EXPECT_EQ(tree, nullptr);
}

// Test that tree with one leaf is preserved in the enclosing range.
TEST(TrimSyntaxTreeTest, OneLeafNotTrimmed) {
  constexpr absl::string_view text("ddddddddddddddd");
  const absl::string_view token(text.substr(5, 5));
  const SymbolPtr expect = Node(TNode(3, Leaf(1, token)));
  for (int left = 4; left <= 5; ++left) {
    for (int right = 10; right <= 11; ++right) {
      SymbolPtr tree = Node(TNode(3, Leaf(1, token)));
      TrimSyntaxTree(&tree, text.substr(left, right - left));
      EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
    }
  }
}

// Test that tree with one leaf is trimmed correctly (right bound).
TEST(TrimSyntaxTreeTest, OneLeafTrimmedFromRight) {
  constexpr absl::string_view text("ddddddddddddddd");
  const absl::string_view token(text.substr(5, 5));
  for (int left = 4; left <= 5; ++left) {
    SymbolPtr tree = Node(TNode(3, Leaf(1, token)));
    TrimSyntaxTree(&tree, text.substr(left, 9 - left));
    EXPECT_EQ(tree, nullptr) << "Failed with left bound " << left;
  }
}

// Test that tree with one leaf is trimmed correctly (left bound).
TEST(TrimSyntaxTreeTest, OneLeafTrimmedFromLeft) {
  constexpr absl::string_view text("ddddddddddddddd");
  const absl::string_view token(text.substr(5, 5));
  for (int right = 10; right <= 11; ++right) {
    SymbolPtr tree = Node(TNode(3, Leaf(1, token)));
    TrimSyntaxTree(&tree, text.substr(6, right - 6));
    EXPECT_EQ(tree, nullptr) << "Failed with right bound " << right;
  }
}

// Test that out-of-range (on left) yields a null tree.
TEST(TrimSyntaxTreeTest, OutOfRangeLeft) {
  SymbolPtr tree = FakeSyntaxTree();
  // Can't test left starting at -1, because string_view raises a
  // bounds check error.
  TrimSyntaxTree(&tree, kBaseText.substr(0, 4));
  EXPECT_EQ(tree, nullptr) << "Failed with left bound 0";
}

// Test that out-of-range (on right) yields a null tree.
TEST(TrimSyntaxTreeTest, OutOfRangeRight) {
  constexpr absl::string_view text("dddddddddddddddddddddddddd");
  SymbolPtr tree = FakeSyntaxTree();
  // Can't test right beyond 17, because string_view raises a bounds
  // check error.
  TrimSyntaxTree(&tree, text.substr(13, 4));
  EXPECT_EQ(tree, nullptr) << "Failed with right bound 17";
}

// Test that an entire complex tree spanned by the range is preserved.
TEST(TrimSyntaxTreeTest, ComplexWholeTreePreserved) {
  SymbolPtr expect = FakeSyntaxTree();
  SymbolPtr tree = FakeSyntaxTree();
  // string_view checks bounds, so we cannot create ranges outside
  // of the original kBaseText string.
  TrimSyntaxTree(&tree, kBaseText.substr(0, 17));
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
      << "Failed with bounds 0,17";
}

// Test that a complex tree is narrowed to a subtree down the left.
TEST(TrimSyntaxTreeTest, ComplexLeftSubtreePreserved) {
  SymbolPtr expect = TNode(1,                               // noformat
                           Leaf(0, kBaseText.substr(0, 5))  // noformat
  );
  SymbolPtr tree = FakeSyntaxTree();
  for (int right = 5; right <= 17; ++right) {
    TrimSyntaxTree(&tree, kBaseText.substr(0, right));
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
        << "Failed with right bound " << right;
  }
}

// Test that a complex tree to the first of multiple range-eligible subtrees.
TEST(TrimSyntaxTreeTest, ComplexFirstEligibleSubtreePreserved) {
  SymbolPtr expect = Leaf(0, kBaseText.substr(5, 3));
  SymbolPtr tree = FakeSyntaxTree();
  for (int left = 1; left <= 5; ++left) {
    TrimSyntaxTree(&tree, kBaseText.substr(left, 18 - left));
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()))
        << "Failed with left bound " << left;
  }
}

// Test that a complex tree is narrowed to a subtree down the right.
TEST(TrimSyntaxTreeTest, ComplexRightSubtreePreserved) {
  SymbolPtr expect = TNode(2,                                // noformat
                           Leaf(0, kBaseText.substr(8, 2)),  // noformat
                           Leaf(0, kBaseText.substr(12, 5))  // noformat
  );
  SymbolPtr tree = FakeSyntaxTree();
  TrimSyntaxTree(&tree, kBaseText.substr(8, 10));
  EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
}

// Test narrowing down tree that is neither leftmost nor rightmost, and neither
// bound is an endpoint.
TEST(TrimSyntaxTreeTest, ComplexMidSpanSubtree) {
  SymbolPtr expect = Leaf(0, kBaseText.substr(5, 3));
  SymbolPtr tree = FakeSyntaxTree();
  for (int right = 8; right <= 17; ++right) {
    SymbolPtr tree = FakeSyntaxTree();
    TrimSyntaxTree(&tree, kBaseText.substr(5, right - 5));
    EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
  }
}

// Test narrowing down tree that is neither leftmost nor rightmost, and neither
// bound is an endpoint.
TEST(TrimSyntaxTreeTest, ComplexMidSpanSubtree2) {
  SymbolPtr expect = Leaf(0, kBaseText.substr(8, 2));
  SymbolPtr tree = FakeSyntaxTree();
  for (int left = 6; left <= 8; ++left) {
    for (int right = 10; right <= 12; ++right) {
      SymbolPtr tree = FakeSyntaxTree();
      TrimSyntaxTree(&tree, kBaseText.substr(left, right - left));
      EXPECT_TRUE(EqualTrees(tree.get(), expect.get()));
    }
  }
}

TEST(SymbolCastToNodeTest, BasicTest) {
  const SyntaxTreeNode node_symbol;
  const auto &node = SymbolCastToNode(node_symbol);
  CHECK_EQ(node.Kind(), SymbolKind::kNode);
}

TEST(SymbolCastToNodeTest, BasicTestMutable) {
  SyntaxTreeNode node_symbol;
  SyntaxTreeNode &node = SymbolCastToNode(node_symbol);
  CHECK_EQ(node.Kind(), SymbolKind::kNode);
}

TEST(SymbolCastToNodeTest, InvalidInputLeaf) {
  const SyntaxTreeLeaf leaf_symbol(3, "foo");
  EXPECT_DEATH(SymbolCastToNode(leaf_symbol), "");
}

TEST(SymbolCastToNodeTest, InvalidInputLeafMutable) {
  SyntaxTreeLeaf leaf_symbol(3, "foo");
  EXPECT_DEATH(SymbolCastToNode(leaf_symbol), "");
}

TEST(SymbolCastToLeafTest, BasicTest) {
  SyntaxTreeLeaf leaf_symbol(3, "foo");
  const auto &leaf = SymbolCastToLeaf(leaf_symbol);
  CHECK_EQ(leaf.Kind(), SymbolKind::kLeaf);
}

TEST(SymbolCastToLeafTest, InvalidInputNode) {
  SyntaxTreeNode node_symbol;
  EXPECT_DEATH(SymbolCastToLeaf(node_symbol), "");
}

TEST(GetSubtreeAsSymbolTest, OutOfBounds) {
  auto root = TNode(1);
  EXPECT_EQ(GetSubtreeAsSymbol(*root, 1, 0), nullptr);
}

TEST(GetSubtreeAsSymbolTest, WrongParentIntegerTag) {
  auto root = TNode(1, TNode(4));
  EXPECT_EQ(GetSubtreeAsSymbol(*root, 2, 0), nullptr);
}

enum class FakeEnum {
  kZero,
  kOne,
  kTwo,
};

std::ostream &operator<<(std::ostream &stream, FakeEnum e) {
  switch (e) {
    case FakeEnum::kZero:
      stream << "zero";
      break;
    case FakeEnum::kOne:
      stream << "one";
      break;
    case FakeEnum::kTwo:
      stream << "two";
      break;
  }
  return stream;
}

TEST(MatchNodeEnumOrNullTest, MatchingEnum) {
  const auto root = TNode(FakeEnum::kTwo);
  EXPECT_NE(MatchNodeEnumOrNull(SymbolCastToNode(*root), FakeEnum::kTwo),
            nullptr);
}

TEST(MatchNodeEnumOrNullTest, NonMatchingEnum) {
  const auto root = TNode(FakeEnum::kTwo);
  EXPECT_EQ(MatchNodeEnumOrNull(SymbolCastToNode(*root), FakeEnum::kZero),
            nullptr);
}

TEST(MatchNodeEnumOrNullTest, MatchingEnumMutable) {
  auto root = TNode(FakeEnum::kTwo);
  MatchNodeEnumOrNull(SymbolCastToNode(*root), FakeEnum::kTwo);
}

TEST(MatchNodeEnumOrNullTest, NonMatchingEnumMutable) {
  auto root = TNode(FakeEnum::kTwo);
  EXPECT_EQ(MatchNodeEnumOrNull(SymbolCastToNode(*root), FakeEnum::kZero),
            nullptr);
}

TEST(MatchLeafEnumOrNullTest, MatchingEnum) {
  const auto root = Leaf(6, "six");
  MatchLeafEnumOrNull(SymbolCastToLeaf(*root), 6);
}

TEST(MatchLeafEnumOrNullTest, NonMatchingEnum) {
  const auto root = Leaf(6, "six");
  EXPECT_EQ(MatchLeafEnumOrNull(SymbolCastToLeaf(*root), 5), nullptr);
}

TEST(CheckSymbolAsNodeTest, MatchingEnum) {
  const auto root = TNode(FakeEnum::kZero);
  CheckSymbolAsNode(*root, FakeEnum::kZero);
}

TEST(CheckSymbolAsNodeTest, NonMatchingEnum) {
  const auto root = TNode(FakeEnum::kOne);
  EXPECT_DEATH(CheckSymbolAsNode(*root, FakeEnum::kZero), "");
}

TEST(CheckSymbolAsNodeTest, NotNode) {
  const auto root = Leaf(4, "x");
  EXPECT_DEATH(CheckSymbolAsNode(*root, FakeEnum::kZero), "");
}

TEST(CheckSymbolAsNodeTest, MatchingEnumMutable) {
  auto root = TNode(FakeEnum::kZero);
  CheckSymbolAsNode(*root, FakeEnum::kZero);
}

TEST(CheckSymbolAsNodeTest, NonMatchingEnumMutable) {
  auto root = TNode(FakeEnum::kOne);
  EXPECT_DEATH(CheckSymbolAsNode(*root, FakeEnum::kZero), "");
}

TEST(CheckSymbolAsNodeTest, NotNodeMutable) {
  auto root = Leaf(4, "x");
  EXPECT_DEATH(CheckSymbolAsNode(*root, FakeEnum::kZero), "");
}

TEST(CheckSymbolAsLeafTest, MatchingEnum) {
  const auto root = Leaf(4, "x");
  CheckSymbolAsLeaf(*root, 4);
}

TEST(CheckSymbolAsLeafTest, NonMatchingEnum) {
  const auto root = Leaf(4, "x");
  EXPECT_DEATH(CheckSymbolAsLeaf(*root, 2), "");
}

TEST(CheckSymbolAsLeafTest, NotLeaf) {
  const auto root = TNode(FakeEnum::kZero);
  EXPECT_DEATH(CheckSymbolAsLeaf(*root, FakeEnum::kZero), "");
}

TEST(CheckOptionalSymbolAsLeafTest, Nullptr) {
  constexpr std::nullptr_t n = nullptr;
  EXPECT_EQ(CheckOptionalSymbolAsLeaf(n, FakeEnum::kZero), nullptr);
}

TEST(CheckOptionalSymbolAsLeafTest, NullBarePtr) {
  const Symbol *root = nullptr;
  EXPECT_EQ(CheckOptionalSymbolAsLeaf(root, FakeEnum::kOne), nullptr);
}

TEST(CheckOptionalSymbolAsLeafTest, NullUniquePtr) {
  const std::unique_ptr<Symbol> root(nullptr);
  EXPECT_EQ(CheckOptionalSymbolAsLeaf(root, FakeEnum::kOne), nullptr);
}

TEST(CheckOptionalSymbolAsLeafTest, NullSharedPtr) {
  const std::shared_ptr<Symbol> root(nullptr);
  EXPECT_EQ(CheckOptionalSymbolAsLeaf(root, FakeEnum::kOne), nullptr);
}

TEST(CheckOptionalSymbolAsLeafTest, MatchingEnum) {
  const auto root = Leaf(6, "a");
  EXPECT_EQ(CheckOptionalSymbolAsLeaf(root, 6), &*root);
}

TEST(CheckOptionalSymbolAsLeafTest, NonMatchingEnum) {
  const auto root = Leaf(7, "a");
  EXPECT_DEATH(CheckOptionalSymbolAsLeaf(root, 6), "");
}

TEST(CheckOptionalSymbolAsLeafTest, NotLeaf) {
  const auto root = Leaf(4, "x");
  EXPECT_DEATH(CheckOptionalSymbolAsLeaf(root, FakeEnum::kZero), "");
}

TEST(CheckOptionalSymbolAsNodeTest, Nullptr) {
  constexpr std::nullptr_t n = nullptr;
  EXPECT_EQ(CheckOptionalSymbolAsNode(n, FakeEnum::kZero), nullptr);
}

TEST(CheckOptionalSymbolAsNodeTest, NullBarePtr) {
  const Symbol *root = nullptr;
  EXPECT_EQ(CheckOptionalSymbolAsNode(root, FakeEnum::kOne), nullptr);
}

TEST(CheckOptionalSymbolAsNodeTest, NullBarePtrNoEnum) {
  const Symbol *root = nullptr;
  EXPECT_EQ(CheckOptionalSymbolAsNode(root), nullptr);
}

TEST(CheckOptionalSymbolAsNodeTest, NullUniquePtr) {
  const std::unique_ptr<Symbol> root(nullptr);
  EXPECT_EQ(CheckOptionalSymbolAsNode(root, FakeEnum::kOne), nullptr);
}

TEST(CheckOptionalSymbolAsNodeTest, NullUniquePtrNoEnum) {
  const std::unique_ptr<Symbol> root(nullptr);
  EXPECT_EQ(CheckOptionalSymbolAsNode(root), nullptr);
}

TEST(CheckOptionalSymbolAsNodeTest, NullSharedPtr) {
  const std::shared_ptr<Symbol> root(nullptr);
  EXPECT_EQ(CheckOptionalSymbolAsNode(root, FakeEnum::kOne), nullptr);
}

TEST(CheckOptionalSymbolAsNodeTest, MatchingEnum) {
  const auto root = TNode(FakeEnum::kZero);
  EXPECT_EQ(CheckOptionalSymbolAsNode(root, FakeEnum::kZero), &*root);
}

TEST(CheckOptionalSymbolAsNodeTest, NonMatchingEnum) {
  const auto root = TNode(FakeEnum::kOne);
  EXPECT_DEATH(CheckOptionalSymbolAsNode(root, FakeEnum::kZero), "");
}

TEST(CheckOptionalSymbolAsNodeTest, DontCareEnum) {
  const auto root = TNode(FakeEnum::kOne);
  EXPECT_NE(CheckOptionalSymbolAsNode(root), nullptr);
}

TEST(CheckOptionalSymbolAsNodeTest, NotNode) {
  const auto root = Leaf(4, "x");
  EXPECT_DEATH(CheckOptionalSymbolAsNode(root, FakeEnum::kZero), "");
}

TEST(CheckOptionalSymbolAsNodeTest, NotNodeNoEnum) {
  const auto root = Leaf(4, "x");
  EXPECT_DEATH(CheckOptionalSymbolAsNode(root), "");
}

TEST(GetSubtreeAsSymbolTest, WrongParentEnumTag) {
  auto root = TNode(FakeEnum::kTwo, TNode(FakeEnum::kOne));
  EXPECT_EQ(GetSubtreeAsSymbol(*root, FakeEnum::kZero, 0), nullptr);
}

TEST(GetSubtreeAsSymbolTest, ValidAccessNode) {
  auto root = TNode(FakeEnum::kTwo, TNode(FakeEnum::kOne));
  const auto *child = GetSubtreeAsSymbol(*root, FakeEnum::kTwo, 0);
  const auto *child_node = down_cast<const SyntaxTreeNode *>(child);
  EXPECT_EQ(FakeEnum(child_node->Tag().tag), FakeEnum::kOne);
}

TEST(GetSubtreeAsSymbolTest, ValidAccessLeaf) {
  auto root = TNode(FakeEnum::kTwo, Leaf(6, "six"));
  const auto *child = GetSubtreeAsSymbol(*root, FakeEnum::kTwo, 0);
  const auto *child_leaf = down_cast<const SyntaxTreeLeaf *>(child);
  EXPECT_EQ(child_leaf->get().token_enum(), 6);
}

TEST(GetSubtreeAsSymbolTest, ValidAccessAtIndexOne) {
  auto root =
      TNode(FakeEnum::kTwo, TNode(FakeEnum::kZero), TNode(FakeEnum::kOne));
  const auto *child = GetSubtreeAsSymbol(*root, FakeEnum::kTwo, 1);
  const auto *child_node = down_cast<const SyntaxTreeNode *>(child);
  EXPECT_EQ(FakeEnum(child_node->Tag().tag), FakeEnum::kOne);
}

TEST(GetSubtreeAsNodeTest, ValidatedFoundNodeEnum) {
  auto root = TNode(FakeEnum::kZero, TNode(FakeEnum::kOne));
  const auto &child = GetSubtreeAsNode(*root, FakeEnum::kZero, 0);
  EXPECT_EQ(FakeEnum(child->Tag().tag), FakeEnum::kOne);
}

TEST(GetSubtreeAsNodeTest, GotLeafInsteadOfNode) {
  auto root = TNode(FakeEnum::kZero, Leaf(1, "foo"));
  EXPECT_EQ(GetSubtreeAsNode(*root, FakeEnum::kZero, 0), nullptr);
}

TEST(GetSubtreeAsNodeTest, ValidatedFoundNodeEnumChildMatches) {
  auto root = TNode(FakeEnum::kZero, TNode(FakeEnum::kOne));
  GetSubtreeAsNode(*root, FakeEnum::kZero, 0, FakeEnum::kOne);
  // Internal checks passed.
}

TEST(GetSubtreeAsNodeTest, ValidatedFoundNodeEnumChildMismatches) {
  auto root = TNode(FakeEnum::kZero, TNode(FakeEnum::kOne));
  EXPECT_EQ(GetSubtreeAsNode(*root, FakeEnum::kZero, 0, FakeEnum::kTwo),
            nullptr);
}

TEST(GetSubtreeAsLeafTest, ValidatedFoundLeaf) {
  auto root = TNode(FakeEnum::kZero, Leaf(7, "foo"));
  const SyntaxTreeLeaf *leaf = GetSubtreeAsLeaf(*root, FakeEnum::kZero, 0);
  EXPECT_EQ(leaf->get().token_enum(), 7);
}

TEST(GetSubtreeAsLeafTest, GotNodeInsteadOfLeaf) {
  auto root = TNode(FakeEnum::kZero, TNode(FakeEnum::kOne));
  EXPECT_DEATH(GetSubtreeAsLeaf(*root, FakeEnum::kZero, 0), "");
}

}  // namespace
}  // namespace verible
