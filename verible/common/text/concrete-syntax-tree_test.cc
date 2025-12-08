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

// Sanity tests for tree node construction functions.

#include "verible/common/text/concrete-syntax-tree.h"

#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/text/tree-compare.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"

namespace verible {

namespace {

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

static void SinkValue(SymbolPtr) {
  // Do nothing, just consume and expire the pointer.
}

static SyntaxTreeNode *CheckTree(const SymbolPtr &ptr) {
  CHECK(ptr->Kind() == SymbolKind::kNode);
  return down_cast<SyntaxTreeNode *>(ptr.get());
}

// Test that MatchesTag matches correctly.
TEST(SyntaxTreeNodeMatchesTag, Matches) {
  auto node = MakeTaggedNode(3);
  EXPECT_TRUE(CheckTree(node)->MatchesTag(3));
}

// Test that MatchesTag does not match.
TEST(SyntaxTreeNodeMatchesTag, NotMatches) {
  auto node = MakeTaggedNode(4);
  EXPECT_FALSE(CheckTree(node)->MatchesTag(3));
}

// Test that MatchesTagAnyOf matches correctly.
TEST(SyntaxTreeNodeMatchesTagAnyOf, Matches) {
  auto node = MakeTaggedNode(3);
  EXPECT_TRUE(CheckTree(node)->MatchesTagAnyOf({2, 3}));
  EXPECT_TRUE(CheckTree(node)->MatchesTagAnyOf({3, 4}));
  EXPECT_FALSE(CheckTree(node)->MatchesTagAnyOf({2, 4}));
}

TEST(SyntaxTreeNodeAppend, AppendVoid) {
  SyntaxTreeNode node;
  node.Append();
  EXPECT_THAT(node, IsEmpty());
}

// Test that std::move is automated.
TEST(SyntaxTreeNodeAppend, AppendChildReference) {
  SyntaxTreeNode node;
  SymbolPtr child;
  node.Append(child);
  EXPECT_THAT(node, SizeIs(1));
}

// Test that redundant move is accepted.
TEST(SyntaxTreeNodeAppend, AppendChildMoved) {
  SyntaxTreeNode node;
  SymbolPtr child;
  node.Append(std::move(child));
  EXPECT_THAT(node, SizeIs(1));
}

// Test that temporary value is properly forwarded.
TEST(SyntaxTreeNodeAppend, AppendChildTemporary) {
  SyntaxTreeNode node;
  node.Append(SymbolPtr());
  EXPECT_THAT(node, SizeIs(1));
}

// Test that nullptrs can be appended.
TEST(SyntaxTreeNodeAppend, AppendChildNullPtr) {
  SyntaxTreeNode node;
  node.Append(nullptr);
  EXPECT_THAT(node, SizeIs(1));
}

// Test that ownership is transferred to sink functions.
TEST(MakeNodeTest, EmptyConstructor) {
  auto node = MakeNode();
  EXPECT_THAT(node, NotNull());
  SinkValue(std::move(node));
  EXPECT_THAT(node, IsNull());
}

// Test that out-of-bounds is caught.
TEST(MakeNodeTest, ChildrenOutOfBounds) {
  SyntaxTreeNode node;
  EXPECT_DEATH(node[0], "");
}

// Test that out-of-bounds is caught (const).
TEST(MakeNodeTest, ChildrenOutOfBoundsConst) {
  const SyntaxTreeNode node;
  EXPECT_DEATH(node[0], "");
}

TEST(MakeNodeTest, SinkTemporary) { SinkValue(MakeNode()); }

// Test construction of tagged node.
TEST(MakeNodeTest, TaggedEmptyConstructor) {
  const int tag = 10;
  auto node = MakeTaggedNode(tag);
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(*CheckTree(node), IsEmpty());
}

// Test construction of tagged node.
TEST(MakeNodeTest, ImmediateTaggedEmptyConstructor) {
  auto node = MakeTaggedNode(20);
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(*CheckTree(node), IsEmpty());
}

// Test construction of untagged node with one child.
TEST(MakeNodeTest, SingleChild) {
  auto child = MakeNode();
  EXPECT_THAT(child, NotNull());
  auto parent = MakeNode(child);
  EXPECT_THAT(parent, NotNull());
  EXPECT_THAT(child, IsNull());
  EXPECT_THAT(*CheckTree(parent), SizeIs(1));
}

// Test construction of untagged node with one (temporary) child.
TEST(MakeNodeTest, SingleChildTemporary) {
  auto parent = MakeNode(MakeNode());
  EXPECT_THAT(parent, NotNull());
  EXPECT_THAT(*CheckTree(parent), SizeIs(1));
}

// Test construction of untagged node with multiple children.
TEST(MakeNodeTest, MultiChild) {
  auto child1 = MakeNode();
  auto child2 = MakeNode();
  auto child3 = MakeNode();
  EXPECT_THAT(child1, NotNull());
  EXPECT_THAT(child2, NotNull());
  EXPECT_THAT(child3, NotNull());
  auto parent = MakeNode(child1, child2, child3);
  ASSERT_THAT(parent, NotNull());
  EXPECT_THAT(child1, IsNull());
  EXPECT_THAT(child2, IsNull());
  EXPECT_THAT(child3, IsNull());
  EXPECT_THAT(*CheckTree(parent), SizeIs(3));
}

// Test ExtendNode with nothing to extend (base case).
TEST(ExtendNodeTest, ExtendNone) {
  auto seq = MakeNode();
  EXPECT_THAT(seq, NotNull());
  auto seq2 = ExtendNode(seq);
  EXPECT_THAT(seq2, NotNull());
  EXPECT_THAT(seq, IsNull());
  EXPECT_THAT(*CheckTree(seq2), IsEmpty());
}

// Test extending node with one child.
TEST(ExtendNodeTest, ExtendOne) {
  auto seq = MakeNode();
  auto item = MakeNode();
  EXPECT_THAT(seq, NotNull());
  EXPECT_THAT(item, NotNull());
  auto seq2 = ExtendNode(seq, item);
  EXPECT_THAT(seq2, NotNull());
  EXPECT_THAT(seq, IsNull());
  EXPECT_THAT(item, IsNull());
  EXPECT_THAT(*CheckTree(seq2), SizeIs(1));
}

// Test extending node with multiple children.
TEST(ExtendNodeTest, ExtendMulti) {
  auto seq = MakeNode();
  auto item1 = MakeNode();
  auto item2 = MakeNode();
  auto item3 = MakeNode();
  EXPECT_THAT(seq, NotNull());
  EXPECT_THAT(item1, NotNull());
  EXPECT_THAT(item2, NotNull());
  EXPECT_THAT(item3, NotNull());
  auto seq2 = ExtendNode(seq, item1, item2, item3);
  EXPECT_THAT(seq2, NotNull());
  EXPECT_THAT(seq, IsNull());
  EXPECT_THAT(item1, IsNull());
  EXPECT_THAT(item2, IsNull());
  EXPECT_THAT(item3, IsNull());
  EXPECT_THAT(*CheckTree(seq2), SizeIs(3));
}

// Test extending node with multiple (temporary) children.
TEST(ExtendNodeTest, ExtendMultiTemporary) {
  auto seq = MakeNode();
  EXPECT_THAT(seq, NotNull());
  auto seq2 = ExtendNode(seq, MakeNode(), MakeNode());
  ASSERT_THAT(seq2, NotNull());
  EXPECT_THAT(seq, IsNull());
  EXPECT_THAT(*CheckTree(seq2), SizeIs(2));
}

// Test extending node with temporary parent.
TEST(ExtendNodeTest, ExtendTemporaryNodeNoChildren) {
  auto seq = ExtendNode(MakeNode());
  ASSERT_THAT(seq, NotNull());
  EXPECT_THAT(*CheckTree(seq), IsEmpty());
}

// Test extending node with temporary parent with temporary children.
TEST(ExtendNodeTest, ExtendTemporaryNode) {
  auto seq = ExtendNode(MakeNode(), MakeNode(), MakeNode());
  ASSERT_THAT(seq, NotNull());
  EXPECT_THAT(*CheckTree(seq), SizeIs(2));
}

// Test forwarding empty set of children to new node.
TEST(SyntaxTreeNodeAppend, AdoptChildrenNone) {
  auto seq = MakeNode();
  auto parent = MakeNode(ForwardChildren(seq));
  EXPECT_THAT(seq, IsNull());
  EXPECT_THAT(*CheckTree(parent), IsEmpty());
}

// Test forwarding empty set of children to new node.
TEST(SyntaxTreeNodeAppend, AdoptLeaf) {
  SymbolPtr leaf(new SyntaxTreeLeaf(0, "abc"));
  auto parent = MakeNode(ForwardChildren(leaf));
  EXPECT_THAT(leaf, IsNull());
  EXPECT_THAT(*CheckTree(parent), SizeIs(1));
}

// Test forwarding set of children to new node, transferring ownership.
TEST(SyntaxTreeNodeAppend, AdoptChildren) {
  auto seq = MakeNode(MakeNode(), MakeNode(), MakeNode());
  auto parent = MakeNode(ForwardChildren(seq));
  EXPECT_THAT(seq, IsNull());
  auto parentnode = CheckTree(parent);
  EXPECT_THAT(*parentnode, SizeIs(3));
  for (const auto &child : parentnode->children()) {
    EXPECT_THAT(child, NotNull());
  }
}

// Test forwarding set of null children to new node.
TEST(SyntaxTreeNodeAppend, AdoptNullChildren) {
  auto seq = MakeNode(nullptr, nullptr);
  auto parent = MakeNode(ForwardChildren(seq));
  EXPECT_THAT(seq, IsNull());
  auto parentnode = CheckTree(parent);
  EXPECT_THAT(*parentnode, SizeIs(2));
  for (const auto &child : parentnode->children()) {
    EXPECT_THAT(child, IsNull());
  }
}

/*
// Test forwarding of children of null parent
TEST(SyntaxTreeNodeAppend, AdoptChildrenNullParent) {
  auto seq = nullptr;
  auto parent = MakeNode(ForwardChildren(seq));
  auto parentnode = CheckTree(parent);
  ASSERT_THAT(parentnode, NotNull());
  EXPECT_THAT(parentnode->children(), IsEmpty());
}

// Test forwarding of children of null parent
TEST(SyntaxTreeNodeAppend, AdoptChildrenNullParentDirect) {
  auto parent = MakeNode(ForwardChildren(nullptr));
  auto parentnode = CheckTree(parent);
  ASSERT_THAT(parentnode, NotNull());
  EXPECT_THAT(parentnode->children(), IsEmpty());
}
*/

// Test mix of forwarded and non-forwarded children.
TEST(SyntaxTreeNodeAppend, AdoptChildrenMixed) {
  auto seq = MakeNode(MakeNode(), MakeNode(), MakeNode());
  auto parent = MakeNode(MakeNode(), ForwardChildren(seq), MakeNode());
  EXPECT_THAT(seq, IsNull());
  auto parentnode = CheckTree(parent);
  ASSERT_THAT(parentnode, NotNull());
  EXPECT_THAT(*parentnode, SizeIs(5));
  for (const auto &child : parentnode->children()) {
    EXPECT_THAT(child, NotNull());
  }
}

// Test multiple of forwarded sets of children.
TEST(SyntaxTreeNodeAppend, AdoptChildrenMultiple) {
  auto seq = MakeNode(MakeNode(), MakeNode(), MakeNode());
  auto seq2 = MakeNode(MakeNode(), MakeNode(), MakeNode(), MakeNode());
  auto parent = MakeNode(ForwardChildren(seq), ForwardChildren(seq2));
  EXPECT_THAT(seq, IsNull());
  EXPECT_THAT(seq2, IsNull());
  auto parentnode = CheckTree(parent);
  ASSERT_THAT(parentnode, NotNull());
  EXPECT_THAT(*parentnode, SizeIs(7));
  for (const auto &child : parentnode->children()) {
    EXPECT_THAT(child, NotNull());
  }
}

// Test extending one set with forwarded children.
TEST(ExtendNodeTest, AdoptChildren) {
  auto seq = ExtendNode(MakeNode(), MakeNode(), MakeNode());
  auto parent = MakeNode(ForwardChildren(seq));
  EXPECT_THAT(seq, IsNull());
  auto parentnode = CheckTree(parent);
  EXPECT_THAT(*parentnode, SizeIs(2));
  for (const auto &child : parentnode->children()) {
    EXPECT_THAT(child, NotNull());
  }
}

// Test extending temporary parent with forwarded children.
TEST(ExtendNodeTest, AdoptChildrenMixed) {
  auto seq = MakeNode(MakeNode(), MakeNode(), MakeNode());
  auto parent = ExtendNode(MakeNode(), ForwardChildren(seq), MakeNode());
  EXPECT_THAT(seq, IsNull());
  auto parentnode = CheckTree(parent);
  EXPECT_THAT(*parentnode, SizeIs(4));
  for (const auto &child : parentnode->children()) {
    EXPECT_THAT(child, NotNull());
  }
}

// Tests setting a placeholder to a single leaf
TEST(ExtendNodeTest, SetChild0Size1) {
  auto expected = Node(Leaf(4, "z"));

  auto node1 = Node(nullptr);
  auto node2 = Leaf(4, "y");
  SetChild(node1, 0, node2);
  EXPECT_THAT(node1, NotNull());
  EXPECT_THAT(node2, IsNull());
  EXPECT_THAT(*CheckTree(node1), SizeIs(1));
  EXPECT_TRUE(EqualTreesByEnum(expected.get(), node1.get()));
}

// Tests setting a placeholder to a more complex tree
TEST(ExtendNodeTest, SetChild1Size2) {
  auto expected = Node(Leaf(1, "a"), Node(Leaf(4, "b")));

  auto node1 = Node(Leaf(1, "a"), nullptr);
  auto node2 = Node(Leaf(4, "b"));
  SetChild(node1, 1, node2);
  EXPECT_THAT(node1, NotNull());
  EXPECT_THAT(node2, IsNull());
  EXPECT_THAT(*CheckTree(node1), SizeIs(2));
  EXPECT_TRUE(EqualTreesByEnum(expected.get(), node1.get()));
}

// Tests that subscript operator works (mutable).
TEST(NodeSubscriptTest, MutableReference) {
  auto example = Node(Leaf(6, "6"), Leaf(7, "7"));
  auto &example_node = down_cast<SyntaxTreeNode &>(*example);
  EXPECT_EQ(example_node[0]->Tag().tag, 6);
  EXPECT_EQ(example_node[1]->Tag().tag, 7);
}

// Tests that subscript operator works (const).
TEST(NodeSubscriptTest, ConstReference) {
  auto example = Node(Leaf(8, "8"), Leaf(9, "9"));
  const auto &example_node = down_cast<const SyntaxTreeNode &>(*example);
  EXPECT_EQ(example_node[0]->Tag().tag, 8);
  EXPECT_EQ(example_node[1]->Tag().tag, 9);
}

}  // namespace
}  // namespace verible
