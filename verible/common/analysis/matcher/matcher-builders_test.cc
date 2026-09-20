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

#include "verible/common/analysis/matcher/matcher-builders.h"

#include <memory>

#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher-test-utils.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/casts.h"

namespace verible {
namespace matcher {
namespace {

// Collection of simple matchers used in test cases
constexpr TagMatchBuilder<SymbolKind::kNode, int, 5> Node5;
constexpr TagMatchBuilder<SymbolKind::kNode, int, 1> Node1;
constexpr TagMatchBuilder<SymbolKind::kLeaf, int, 1> Leaf1;
constexpr auto Path543 = MakePathMatcher(NodeTag(3), NodeTag(4), LeafTag(10));
constexpr auto PathNode1 = MakePathMatcher(NodeTag(1));
constexpr auto PathLeaf1 = MakePathMatcher(LeafTag(1));

const DynamicTagMatchBuilder DNode5{
    SymbolTag{.kind = SymbolKind::kNode, .tag = 5}};
const DynamicTagMatchBuilder DNode1{
    SymbolTag{.kind = SymbolKind::kNode, .tag = 1}};
const DynamicTagMatchBuilder DLeaf1{
    SymbolTag{.kind = SymbolKind::kLeaf, .tag = 1}};

TEST(MatcherBuildersTest, MatcherTestCases) {
  const MatcherTestCase test_cases[] = {
      // Test cases with embedded nulls in structure
      {.matcher = Node5(),
       .root = TNode(5, nullptr),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = Node5().Bind("foo"),
       .root = TNode(5, nullptr),
       .expected_result = true,
       .expected_bound_nodes = {{"foo", NodeTag(5)}}},
      {.matcher = Node5(),
       .root = TNode(6, nullptr),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = Node5(PathNode1()),
       .root = TNode(5, nullptr, TNode(1, nullptr)),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = Node5(PathNode1()),
       .root = TNode(5, nullptr, TNode(4, nullptr)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = Node5(PathNode1(Node1(PathNode1(Node1())))),
       .root = TNode(5, nullptr, TNode(1, nullptr)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = Node5(Path543().Bind("inner")).Bind("outer"),
       .root =
           TNode(5, nullptr, TNode(3, nullptr, TNode(4, nullptr, XLeaf(10)))),
       .expected_result = true,
       .expected_bound_nodes = {{"inner", LeafTag(10)}, {"outer", NodeTag(5)}}},

      {.matcher = Node5(),
       .root = TNode(5),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = Node5().Bind("foo"),
       .root = TNode(5),
       .expected_result = true,
       .expected_bound_nodes = {{"foo", NodeTag(5)}}},
      {.matcher = Node5(),
       .root = TNode(6),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = Node5(Path543().Bind("inner")).Bind("outer"),
       .root = TNode(5, TNode(3, TNode(4, XLeaf(10)))),
       .expected_result = true,
       .expected_bound_nodes = {{"inner", LeafTag(10)}, {"outer", NodeTag(5)}}},
      {.matcher = Node5(Path543().Bind("inner")).Bind("outer"),
       .root = TNode(6, TNode(3, TNode(4, XLeaf(10)))),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = Node5(PathNode1()),
       .root = TNode(5, TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = Node5(PathNode1()),
       .root = TNode(5, TNode(4)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = Node5(PathNode1(Node1(PathNode1(Node1())))),
       .root = TNode(5, TNode(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      // Complicated nested statement
      {.matcher = Node5(PathNode1(Node1(PathNode1(Node1())))),
       .root = TNode(5, TNode(1, TNode(1))),
       .expected_result = true,
       .expected_bound_nodes = {}},
      // A complicated nested statement with many binds
      {.matcher =
           Node5(
               PathNode1(Node1(PathLeaf1(Leaf1().Bind("leaf1")).Bind("pleaf1"))
                             .Bind("node1"))
                   .Bind("pnode1"))
               .Bind("node5"),
       .root = TNode(5, TNode(1, XLeaf(1))),
       .expected_result = true,
       .expected_bound_nodes = {{"node5", NodeTag(5)},
                                {"leaf1", LeafTag(1)},
                                {"pleaf1", LeafTag(1)},
                                {"node1", NodeTag(1)},
                                {"pnode1", NodeTag(1)}}},
      // TODO(jeremycs): update this when we add branching
      // Examines simple branching behavior
      {.matcher = Node5(PathNode1().Bind("node1")),
       .root = TNode(5, TNode(1), TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"node1", NodeTag(1)}}},
      // Repeated traversal to downward to leaves should fail
      {.matcher = Node5(PathLeaf1(PathLeaf1())),
       .root = TNode(5, XLeaf(1), XLeaf(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(MatcherBuildersTest, DynamicMatcherTestCases) {
  const MatcherTestCase test_cases[] = {
      // Test cases with embedded nulls in structure
      {.matcher = DNode5(),
       .root = TNode(5, nullptr),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = DNode5().Bind("foo"),
       .root = TNode(5, nullptr),
       .expected_result = true,
       .expected_bound_nodes = {{"foo", NodeTag(5)}}},
      {.matcher = DNode5(),
       .root = TNode(6, nullptr),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(PathNode1()),
       .root = TNode(5, nullptr, TNode(1, nullptr)),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(PathNode1()),
       .root = TNode(5, nullptr, TNode(4, nullptr)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(PathNode1(DNode1(PathNode1(DNode1())))),
       .root = TNode(5, nullptr, TNode(1, nullptr)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(Path543().Bind("inner")).Bind("outer"),
       .root =
           TNode(5, nullptr, TNode(3, nullptr, TNode(4, nullptr, XLeaf(10)))),
       .expected_result = true,
       .expected_bound_nodes = {{"inner", LeafTag(10)}, {"outer", NodeTag(5)}}},

      {.matcher = DNode5(),
       .root = TNode(5),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = DNode5().Bind("foo"),
       .root = TNode(5),
       .expected_result = true,
       .expected_bound_nodes = {{"foo", NodeTag(5)}}},
      {.matcher = DNode5(),
       .root = TNode(6),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(Path543().Bind("inner")).Bind("outer"),
       .root = TNode(5, TNode(3, TNode(4, XLeaf(10)))),
       .expected_result = true,
       .expected_bound_nodes = {{"inner", LeafTag(10)}, {"outer", NodeTag(5)}}},
      {.matcher = DNode5(Path543().Bind("inner")).Bind("outer"),
       .root = TNode(6, TNode(3, TNode(4, XLeaf(10)))),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(PathNode1()),
       .root = TNode(5, TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(PathNode1()),
       .root = TNode(5, TNode(4)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher = DNode5(PathNode1(DNode1(PathNode1(DNode1())))),
       .root = TNode(5, TNode(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      // Complicated nested statement
      {.matcher = DNode5(PathNode1(DNode1(PathNode1(DNode1())))),
       .root = TNode(5, TNode(1, TNode(1))),
       .expected_result = true,
       .expected_bound_nodes = {}},
      // A complicated nested statement with many binds
      {.matcher =
           DNode5(PathNode1(
                      DNode1(PathLeaf1(DLeaf1().Bind("leaf1")).Bind("pleaf1"))
                          .Bind("node1"))
                      .Bind("pnode1"))
               .Bind("node5"),
       .root = TNode(5, TNode(1, XLeaf(1))),
       .expected_result = true,
       .expected_bound_nodes = {{"node5", NodeTag(5)},
                                {"leaf1", LeafTag(1)},
                                {"pleaf1", LeafTag(1)},
                                {"node1", NodeTag(1)},
                                {"pnode1", NodeTag(1)}}},
      // TODO(jeremycs): update this when we add branching
      // Examines simple branching behavior
      {.matcher = DNode5(PathNode1().Bind("node1")),
       .root = TNode(5, TNode(1), TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"node1", NodeTag(1)}}},
      // Repeated traversal to downward to leaves should fail
      {.matcher = DNode5(PathLeaf1(PathLeaf1())),
       .root = TNode(5, XLeaf(1), XLeaf(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

// Basic test case for matching on a single node
TEST(MatcherBuildersTest, MatchTagSimple) {
  auto matcher = Node5();
  BoundSymbolManager bound_symbol_manager;

  auto should_match = TNode(5);
  EXPECT_TRUE(matcher.Matches(*should_match, &bound_symbol_manager));

  auto no_match = TNode(6);
  EXPECT_FALSE(matcher.Matches(*no_match, &bound_symbol_manager));
}

// Basic test case for pathed descent
TEST(MatcherBuildersTest, MatchPathSimple) {
  auto matcher = Node5(Path543().Bind("inner")).Bind("outer");
  BoundSymbolManager bound_symbol_manager;

  auto should_match = TNode(5, TNode(3, TNode(4, XLeaf(10))));
  EXPECT_TRUE(matcher.Matches(*should_match, &bound_symbol_manager));
  EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("outer"));
  EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("inner"));

  const auto *outer_match = down_cast<const SyntaxTreeNode *>(
      bound_symbol_manager.FindSymbol("outer"));
  const auto *inner_match = down_cast<const SyntaxTreeLeaf *>(
      bound_symbol_manager.FindSymbol("inner"));
  ASSERT_NE(outer_match, nullptr);
  ASSERT_NE(inner_match, nullptr);
  EXPECT_EQ(outer_match->Tag(), NodeTag(5));
  EXPECT_EQ(inner_match->Tag(), LeafTag(10));

  bound_symbol_manager.Clear();

  auto no_match = TNode(6);
  EXPECT_FALSE(matcher.Matches(*no_match, &bound_symbol_manager));
  EXPECT_EQ(bound_symbol_manager.Size(), 0);
}

}  // namespace
}  // namespace matcher
}  // namespace verible
