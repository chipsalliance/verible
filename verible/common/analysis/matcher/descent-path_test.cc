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

#include "verible/common/analysis/matcher/descent-path.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/casts.h"

namespace verible {
namespace matcher {
namespace {

TEST(DescentPathTest, GetDescendantsFromPathNullTreeFail) {
  SymbolPtr embedded_null = TNode(1, nullptr, TNode(2, nullptr));
  DescentPath path = {NodeTag(2), NodeTag(3), NodeTag(4), LeafTag(100)};

  auto descendants = GetAllDescendantsFromPath(*embedded_null, path);
  EXPECT_EQ(descendants.size(), 0);
}

TEST(DescentPathTest, GetDescendantsFromPathEmbeddedNullPass) {
  SymbolPtr root = TNode(
      1, nullptr, TNode(2), nullptr, TNode(2, nullptr, XLeaf(10)), nullptr,
      TNode(2, nullptr, XLeaf(10), nullptr, TNode(100, nullptr)));
  DescentPath path = {NodeTag(2), LeafTag(10)};
  auto descendants = GetAllDescendantsFromPath(*root, path);
  EXPECT_EQ(descendants.size(), 2);

  const auto *leaf1 = down_cast<const SyntaxTreeLeaf *>(descendants[0]);
  const auto *leaf2 = down_cast<const SyntaxTreeLeaf *>(descendants[1]);

  ASSERT_NE(leaf1, nullptr);
  ASSERT_NE(leaf2, nullptr);
  EXPECT_EQ(leaf1->get().token_enum(), 10);
  EXPECT_EQ(leaf2->get().token_enum(), 10);
}

TEST(DescentPathTest, GetDescendantsFromPathSingle) {
  SymbolPtr root = TNode(1, XLeaf(2), nullptr, TNode(2, XLeaf(10)));
  DescentPath path = {NodeTag(2), LeafTag(10)};
  auto descendants = GetAllDescendantsFromPath(*root, path);
  EXPECT_EQ(descendants.size(), 1);

  const auto *leaf = down_cast<const SyntaxTreeLeaf *>(descendants[0]);
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(leaf->get().token_enum(), 10);
}

TEST(DescentPathTest, GetDescendantsFromPathMultiple) {
  SymbolPtr root = TNode(1, TNode(2, TNode(100), TNode(100)));
  DescentPath path = {NodeTag(2), NodeTag(100)};
  auto descendants = GetAllDescendantsFromPath(*root, path);
  EXPECT_EQ(descendants.size(), 2);

  const auto *node1 = down_cast<const SyntaxTreeNode *>(descendants[0]);
  const auto *node2 = down_cast<const SyntaxTreeNode *>(descendants[1]);

  ASSERT_NE(node1, nullptr);
  ASSERT_NE(node2, nullptr);
  EXPECT_TRUE(node1->MatchesTag(100));
  EXPECT_TRUE(node2->MatchesTag(100));
}

TEST(DescentPathTest, GetDescendantsFromPathMultiplePaths) {
  SymbolPtr root =
      TNode(1, TNode(2), TNode(2, XLeaf(10)), TNode(2, XLeaf(10), TNode(100)));
  DescentPath path = {NodeTag(2), LeafTag(10)};
  auto descendants = GetAllDescendantsFromPath(*root, path);
  EXPECT_EQ(descendants.size(), 2);

  const auto *leaf1 = down_cast<const SyntaxTreeLeaf *>(descendants[0]);
  const auto *leaf2 = down_cast<const SyntaxTreeLeaf *>(descendants[1]);

  ASSERT_NE(leaf1, nullptr);
  ASSERT_NE(leaf2, nullptr);
  EXPECT_EQ(leaf1->get().token_enum(), 10);
  EXPECT_EQ(leaf2->get().token_enum(), 10);
}

TEST(DescentPathTest, GetDescendantsFromPathFailureGapInPath) {
  SymbolPtr root = TNode(1, TNode(2, TNode(3, XLeaf(100))));
  DescentPath path = {NodeTag(2), LeafTag(100)};
  auto descendants = GetAllDescendantsFromPath(*root, path);
  EXPECT_EQ(descendants.size(), 0);
}

TEST(DescentPathTest, GetDescendantsFromPathFailurePathTooLong) {
  SymbolPtr root = TNode(1, TNode(2, TNode(3, XLeaf(100))));
  DescentPath path = {NodeTag(2), NodeTag(3), NodeTag(4), LeafTag(100)};
  auto descendants = GetAllDescendantsFromPath(*root, path);
  EXPECT_EQ(descendants.size(), 0);
}

}  // namespace
}  // namespace matcher
}  // namespace verible
