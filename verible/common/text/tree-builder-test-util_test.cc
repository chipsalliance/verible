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

#include "verible/common/text/tree-builder-test-util.h"

#include "gtest/gtest.h"
#include "verible/common/text/symbol-ptr.h"
#include "verible/common/text/tree-utils.h"

namespace verible {
namespace {

// Tests descending when root is a leaf.
TEST(DescendPathTest, LeafOnly) {
  SymbolPtr leaf = Leaf(0, "text");
  EXPECT_EQ(leaf.get(), DescendPath(*leaf, {}));
  EXPECT_DEATH(DescendPath(*leaf, {0}), "");
}

// Tests that descending stops at childless node.
TEST(DescendPathTest, EmptyNode) {
  SymbolPtr node = Node();
  EXPECT_EQ(node.get(), DescendPath(*node, {}));
  EXPECT_DEATH(DescendPath(*node, {0}), "");
}

// Tests that descent can return nullptr nodes.
TEST(DescendPathTest, NodeNullChild) {
  SymbolPtr node = Node(nullptr);
  EXPECT_EQ(DescendPath(*node, {0}), nullptr);
  EXPECT_DEATH(DescendPath(*node, {1}), "");  // out-of-bounds
}

// Tests that descending reaches a leaf single-child.
TEST(DescendPathTest, NodeLeaf) {
  SymbolPtr node = Node(Leaf(0, "text"));
  EXPECT_NE(&SymbolCastToLeaf(*DescendPath(*node, {0})), nullptr);
  EXPECT_DEATH(DescendPath(*node, {0, 0}), "");  // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {1}), "");     // out-of-bounds
}

// Tests that descending reaches a node single-child.
TEST(DescendPathTest, NodeNode) {
  SymbolPtr node = Node(Node());
  EXPECT_NE(&SymbolCastToNode(*DescendPath(*node, {0})), nullptr);
  EXPECT_DEATH(DescendPath(*node, {0, 0}), "");  // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {1}), "");     // out-of-bounds
}

// Tests that descending reaches a single-grandchild leaf.
TEST(DescendPathTest, NodeNodeLeaf) {
  SymbolPtr node = Node(Node(Leaf(0, "text")));
  EXPECT_NE(&SymbolCastToLeaf(*DescendPath(*node, {0, 0})), nullptr);
  EXPECT_DEATH(DescendPath(*node, {0, 0, 0}), "");  // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {0, 1}), "");     // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {1}), "");        // out-of-bounds
}

// Tests that descending reaches a single-grandchild node.
TEST(DescendPathTest, NodeNodeNode) {
  SymbolPtr node = Node(Node(Node()));
  EXPECT_NE(&SymbolCastToNode(*DescendPath(*node, {0, 0})), nullptr);
  EXPECT_DEATH(DescendPath(*node, {0, 0, 0}), "");  // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {0, 1}), "");     // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {1}), "");        // out-of-bounds
}

// Tests that descending reaches two terminal leaves.
TEST(DescendPathTest, NodeTwoLeaves) {
  SymbolPtr node = Node(Leaf(0, "more"), Leaf(0, "text"));
  const auto *leaf0 = &SymbolCastToLeaf(*DescendPath(*node, {0}));
  const auto *leaf1 = &SymbolCastToLeaf(*DescendPath(*node, {1}));
  EXPECT_NE(leaf0, nullptr);
  EXPECT_NE(leaf1, nullptr);
  EXPECT_NE(leaf0, leaf1);
  EXPECT_DEATH(DescendPath(*node, {2}), "");  // out-of-bounds
}

// Tests that descending stops as a node with multiple children nodes.
TEST(DescendPathTest, NodeTwoSubNodes) {
  SymbolPtr node = Node(Node(), Node());
  const auto *subnode0 = &SymbolCastToNode(*DescendPath(*node, {0}));
  const auto *subnode1 = &SymbolCastToNode(*DescendPath(*node, {1}));
  EXPECT_NE(subnode0, nullptr);
  EXPECT_NE(subnode1, nullptr);
  EXPECT_NE(subnode0, subnode1);
  EXPECT_DEATH(DescendPath(*node, {2}), "");  // out-of-bounds
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendPathTest, NodeFirstChildLeafSecondChildNull) {
  SymbolPtr node = Node(Leaf(0, "text"), nullptr);
  const auto *leaf0 = &SymbolCastToLeaf(*DescendPath(*node, {0}));
  const auto *leaf1 = DescendPath(*node, {1});
  EXPECT_NE(leaf0, nullptr);
  EXPECT_EQ(leaf1, nullptr);
  EXPECT_DEATH(DescendPath(*node, {2}), "");     // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {0, 0}), "");  // out-of-bounds
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendPathTest, NodeFirstChildNullSecondChildLeaf) {
  SymbolPtr node = Node(nullptr, Leaf(0, "text"));
  const auto *leaf0 = DescendPath(*node, {0});
  const auto *leaf1 = &SymbolCastToLeaf(*DescendPath(*node, {1}));
  EXPECT_EQ(leaf0, nullptr);
  EXPECT_NE(leaf1, nullptr);
  EXPECT_DEATH(DescendPath(*node, {2}), "");     // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {1, 0}), "");  // out-of-bounds
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendPathTest, NodeFirstChildNullSecondChildNode) {
  SymbolPtr node = Node(nullptr, Node());
  const auto *subnode0 = DescendPath(*node, {0});
  const auto *subnode1 = &SymbolCastToNode(*DescendPath(*node, {1}));
  EXPECT_EQ(subnode0, nullptr);
  EXPECT_NE(subnode1, nullptr);
  EXPECT_DEATH(DescendPath(*node, {2}), "");     // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {1, 0}), "");  // out-of-bounds
}

// Tests that descending stops as a node with multiple children, some null.
TEST(DescendPathTest, NodeFirstChildNodeSecondChildNull) {
  SymbolPtr node = Node(Node(), nullptr);
  const auto *subnode0 = &SymbolCastToNode(*DescendPath(*node, {0}));
  const auto *subnode1 = DescendPath(*node, {1});
  EXPECT_NE(subnode0, nullptr);
  EXPECT_EQ(subnode1, nullptr);
  EXPECT_DEATH(DescendPath(*node, {2}), "");     // out-of-bounds
  EXPECT_DEATH(DescendPath(*node, {0, 0}), "");  // out-of-bounds
}

}  // namespace
}  // namespace verible
