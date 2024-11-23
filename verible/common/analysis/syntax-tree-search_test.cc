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

#include "verible/common/analysis/syntax-tree-search.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/matcher-builders.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/text/tree-utils.h"

namespace verible {
namespace {

// Matcher class for finding a specific node type.
template <int NodeTag>
using NodeMatcher = matcher::TagMatchBuilder<SymbolKind::kNode, int, NodeTag>;

// Matcher class for finding a specific leaf type.
template <int LeafTag>
using LeafMatcher = matcher::TagMatchBuilder<SymbolKind::kLeaf, int, LeafTag>;

// Tests that node with same tag matches.
TEST(SearchSyntaxTreeTest, RootOnlyNodeMatch) {
  auto tree = TNode(0);
  auto matcher_builder = NodeMatcher<0>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(*tree, matcher);
  EXPECT_EQ(matches.size(), 1);
  EXPECT_EQ(matches.front().match, tree.get());
}

// Tests that node and leaf do not match.
TEST(SearchSyntaxTreeTest, RootOnlyLeafNotMatch) {
  auto tree = TNode(0);
  auto matcher_builder = LeafMatcher<0>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(*tree, matcher);
  EXPECT_TRUE(matches.empty());
}

// Tests that leaf with same tag matches.
TEST(SearchSyntaxTreeTest, RootOnlyLeafMatch) {
  auto tree = XLeaf(0);
  auto matcher_builder = LeafMatcher<0>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(*tree, matcher);
  EXPECT_EQ(matches.size(), 1);
  EXPECT_EQ(matches.front().match, tree.get());
}

// Tests that node and leaf do not match.
TEST(SearchSyntaxTreeTest, RootOnlyNodeNotMatch) {
  auto tree = XLeaf(0);
  auto matcher_builder = NodeMatcher<0>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(*tree, matcher);
  EXPECT_TRUE(matches.empty());
}

// Tests that multiple matching leaves are found.
TEST(SearchSyntaxTreeTest, DeepLeafMatch) {
  auto tree = Node(Node(XLeaf(2), XLeaf(3)), Node(XLeaf(4), XLeaf(2)));
  auto matcher_builder = LeafMatcher<2>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(*tree, matcher);
  EXPECT_EQ(matches.size(), 2);
  EXPECT_EQ(&SymbolCastToLeaf(*matches.front().match),
            &SymbolCastToLeaf(*DescendPath(*tree, {0, 0})));
  EXPECT_EQ(&SymbolCastToLeaf(*matches.back().match),
            &SymbolCastToLeaf(*DescendPath(*tree, {1, 1})));
}

// Tests that multiple matching nodes are found.
TEST(SearchSyntaxTreeTest, DeepNodeMatch) {
  auto tree = Node(Node(TNode(3), XLeaf(3)), Node(XLeaf(4), TNode(3)));
  auto matcher_builder = NodeMatcher<3>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(*tree, matcher);
  EXPECT_EQ(matches.size(), 2);
  EXPECT_EQ(&SymbolCastToNode(*matches.front().match),
            &SymbolCastToNode(*DescendPath(*tree, {0, 0})));
  EXPECT_EQ(&SymbolCastToNode(*matches.back().match),
            &SymbolCastToNode(*DescendPath(*tree, {1, 1})));
}

// Tests that multiple matching nested nodes are found.
TEST(SearchSyntaxTreeTest, NestedNodeMatch) {
  auto tree = Node(TNode(1, TNode(3), TNode(1)), Node(XLeaf(4), TNode(3)));
  auto matcher_builder = NodeMatcher<1>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(*tree, matcher);
  EXPECT_EQ(matches.size(), 2);
  EXPECT_EQ(&SymbolCastToNode(*matches.front().match),
            &SymbolCastToNode(*DescendPath(*tree, {0})));
  EXPECT_EQ(&SymbolCastToNode(*matches.back().match),
            &SymbolCastToNode(*DescendPath(*tree, {0, 1})));
}

// Tests that false predicate filters out matches.
TEST(SearchSyntaxTreeTest, RootOnlyNodeMatchFalsePredicate) {
  auto tree = TNode(0);
  auto matcher_builder = NodeMatcher<0>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(
      *tree, matcher, [](const SyntaxTreeContext &) { return false; });
  EXPECT_TRUE(matches.empty());
}

// Tests that true predicate preserves matches.
TEST(SearchSyntaxTreeTest, RootOnlyNodeMatchTruePredicate) {
  auto tree = TNode(0);
  auto matcher_builder = NodeMatcher<0>();
  auto matcher = matcher_builder();
  auto matches = SearchSyntaxTree(
      *tree, matcher, [](const SyntaxTreeContext &) { return true; });
  EXPECT_EQ(matches.size(), 1);
  EXPECT_EQ(&SymbolCastToNode(*matches.front().match), tree.get());
}

}  // namespace
}  // namespace verible
