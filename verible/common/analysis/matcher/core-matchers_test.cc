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

#include "verible/common/analysis/matcher/core-matchers.h"

#include <memory>

#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher-builders.h"
#include "verible/common/analysis/matcher/matcher-test-utils.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-builder-test-util.h"

namespace verible {
namespace matcher {
namespace {

// Collection of simple matchers used in test cases
constexpr TagMatchBuilder<SymbolKind::kNode, int, 5> Node5;
constexpr TagMatchBuilder<SymbolKind::kNode, int, 1> Node1;
constexpr TagMatchBuilder<SymbolKind::kLeaf, int, 1> Leaf1;

constexpr auto PathNode1 = MakePathMatcher(NodeTag(1));
constexpr auto PathLeaf1 = MakePathMatcher(LeafTag(1));
constexpr auto PathNode2 = MakePathMatcher(NodeTag(2));
constexpr auto PathLeaf2 = MakePathMatcher(LeafTag(2));

// Basic test case for AnyOf
TEST(MatcherBuildersTest, AnyOfSimple) {
  const auto matchers = {
      Node5(AnyOf(PathLeaf1(), PathNode1())),
      Node5(AnyOf(PathNode1(), PathLeaf1())),  // commutative
  };

  for (const auto &matcher : matchers) {
    BoundSymbolManager bound_symbol_manager;
    {
      const auto should_match_leaf = TNode(5, XLeaf(1));
      EXPECT_TRUE(matcher.Matches(*should_match_leaf, &bound_symbol_manager));
    }
    {
      bound_symbol_manager.Clear();
      const auto should_match_node = TNode(5, TNode(1));
      EXPECT_TRUE(matcher.Matches(*should_match_node, &bound_symbol_manager));
    }
    {
      bound_symbol_manager.Clear();
      const auto should_match_both = TNode(5, TNode(1), XLeaf(1));
      EXPECT_TRUE(matcher.Matches(*should_match_both, &bound_symbol_manager));
    }
  }
}

// Basic test case for EachOf
TEST(MatcherBuildersTest, EachOfSimple) {
  const auto matchers = {
      Node5(EachOf(PathLeaf1().Bind("leaf1"), PathNode1().Bind("node1"))),
      // swap order (commutative):
      Node5(EachOf(PathNode1().Bind("node1"), PathLeaf1().Bind("leaf1"))),
  };

  for (const auto &matcher : matchers) {
    BoundSymbolManager bound_symbol_manager;
    {
      const auto should_match_leaf = TNode(5, XLeaf(1));
      EXPECT_TRUE(matcher.Matches(*should_match_leaf, &bound_symbol_manager));
      EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("leaf1"));
    }
    {
      bound_symbol_manager.Clear();
      const auto should_match_node = TNode(5, TNode(1));
      EXPECT_TRUE(matcher.Matches(*should_match_node, &bound_symbol_manager));
      EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("node1"));
    }
    {
      bound_symbol_manager.Clear();
      const auto should_match_both = TNode(5, TNode(1), XLeaf(1));
      EXPECT_TRUE(matcher.Matches(*should_match_both, &bound_symbol_manager));
      EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("leaf1"));
      EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("node1"));
    }
  }
}

// Basic test case for AllOf
TEST(MatcherBuildersTest, AllOfSimple) {
  const auto matchers = {
      Node5(AllOf(PathLeaf1(), PathNode1())),
      Node5(AllOf(PathNode1(), PathLeaf1())),  // commutative
  };

  for (const auto &matcher : matchers) {
    BoundSymbolManager bound_symbol_manager;
    {
      const auto should_match = TNode(5, XLeaf(1), XLeaf(2), TNode(1));
      EXPECT_TRUE(matcher.Matches(*should_match, &bound_symbol_manager));
    }
    {
      const auto no_match_node = TNode(5, XLeaf(1));
      EXPECT_FALSE(matcher.Matches(*no_match_node, &bound_symbol_manager));
    }
    {
      const auto no_match_leaf = TNode(5, TNode(1));
      EXPECT_FALSE(matcher.Matches(*no_match_leaf, &bound_symbol_manager));
    }
  }
}

// Basic test case for Unless
TEST(MatcherBuildersTest, UnlessSimple) {
  const auto matcher = Node5(Unless(PathNode1()));
  BoundSymbolManager bound_symbol_manager;

  {
    const auto no_match1 = TNode(5, XLeaf(1), XLeaf(2), TNode(1));
    EXPECT_FALSE(matcher.Matches(*no_match1, &bound_symbol_manager));
  }
  {
    const auto yes_match1 = TNode(5, XLeaf(1));
    EXPECT_TRUE(matcher.Matches(*yes_match1, &bound_symbol_manager));
  }
  {
    const auto no_match2 = TNode(5, TNode(1));
    EXPECT_FALSE(matcher.Matches(*no_match2, &bound_symbol_manager));
  }
}

TEST(CoreMatchers, AnyOfManyTests) {
  const MatcherTestCase test_cases[] = {
      // Only first inner matcher should Bind
      {.matcher =
           Node5(AnyOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       .root = TNode(5, TNode(1), XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)}}},
      {.matcher =
           Node5(AnyOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       .root = TNode(5, TNode(1), XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"XLeaf", LeafTag(1)}}},

      // Unmatched inner matchers should not bind
      {.matcher =
           Node5(AnyOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       .root = TNode(5, XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"XLeaf", LeafTag(1)}}},
      {.matcher =
           Node5(AnyOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       .root = TNode(5, TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)}}},

      // Unmatched inner matchers should not prevent binds elsewhere in bode
      {.matcher =
           Node5(Node5().Bind("first"),
                 AnyOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       .root = TNode(5, XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"XLeaf", LeafTag(1)}, {"first", NodeTag(5)}}},
      {.matcher = Node5(Node5().Bind("first"), AnyOf(PathLeaf1().Bind("XLeaf"),
                                                     PathNode1().Bind("Node")))
                      .Bind("outer"),
       .root = TNode(5, TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)},
                                {"first", NodeTag(5)},
                                {"outer", NodeTag(5)}}},

      // AnyOf should fail when all inner matchers fail
      {.matcher =
           Node5(AnyOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       .root = TNode(5, TNode(5), XLeaf(5)),
       .expected_result = false,
       .expected_bound_nodes = {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, EachOfManyTests) {
  const MatcherTestCase test_cases[] = {
      // All passing matchers should bind
      {.matcher =
           Node5(EachOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       .root = TNode(5, TNode(1), XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)}, {"XLeaf", LeafTag(1)}}},
      // Unmatched inner matchers should not bind
      {.matcher =
           Node5(EachOf(PathNode2().Bind("node2"), PathLeaf1().Bind("XLeaf"),
                        PathLeaf2().Bind("leaf2"), PathNode1().Bind("Node"))),
       .root = TNode(5, TNode(1), XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)}, {"XLeaf", LeafTag(1)}}},
      // Inner matchers of EachOf's inner matchers that pass should bind
      {.matcher =
           Node5(EachOf(PathNode1(Node1().Bind("InnerNode")).Bind("Node"),
                        PathLeaf1(Leaf1().Bind("InnerLeaf")).Bind("XLeaf"))),
       .root = TNode(5, TNode(1), XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)},
                                {"XLeaf", LeafTag(1)},
                                {"InnerNode", NodeTag(1)},
                                {"InnerLeaf", LeafTag(1)}}},

      // Unmatched inner matchers should not prevent binds elsewhere
      {.matcher =
           Node5(Node5().Bind("first"),
                 EachOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       .root = TNode(5, XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"XLeaf", LeafTag(1)}, {"first", NodeTag(5)}}},
      {.matcher = Node5(Node5().Bind("first"), EachOf(PathLeaf1().Bind("XLeaf"),
                                                      PathNode1().Bind("Node")))
                      .Bind("outer"),
       .root = TNode(5, TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)},
                                {"first", NodeTag(5)},
                                {"outer", NodeTag(5)}}},

      // EachOf should fail when all inner matchers fail
      {.matcher =
           Node5(EachOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       .root = TNode(5, TNode(5), XLeaf(5)),
       .expected_result = false,
       .expected_bound_nodes = {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, AllOfManyTests) {
  const MatcherTestCase test_cases[] = {
      // All inner matchers must match for AllOf to match.
      // Each passing matcher should bind.
      {.matcher =
           Node5(AllOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       .root = TNode(5, TNode(1), XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node", NodeTag(1)}, {"XLeaf", LeafTag(1)}}},

      // One inner matcher failing should cause AllOf to fail.
      // No matchers should bind in this case.
      {.matcher =
           Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       .root = TNode(5, XLeaf(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher =
           Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       .root = TNode(5, TNode(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher =
           Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node")))
               .Bind("outer"),
       .root = TNode(5, XLeaf(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      {.matcher =
           Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node")))
               .Bind("outer"),
       .root = TNode(5, TNode(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, UnlessManyTests) {
  const MatcherTestCase test_cases[] = {
      // Unless should match if its inner matcher does not match
      {.matcher = Node5(Unless(PathNode1())),
       .root = TNode(5, XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {}},
      // Unless matching should not result in any Binds from its inner matcher
      {.matcher = Node5(Unless(PathNode1().Bind("inner"))),
       .root = TNode(5, XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {}},
      // Unless matching should not prevent parent/sibling matchers from binding
      {.matcher =
           Node5(Node5().Bind("sibling"), Unless(PathNode1().Bind("inner")))
               .Bind("outer"),
       .root = TNode(5, XLeaf(1)),
       .expected_result = true,
       .expected_bound_nodes = {{"sibling", NodeTag(5)},
                                {"outer", NodeTag(5)}}},

      // Unless should not match if its inner matcher does match
      {.matcher = Node5(Unless(PathNode1())),
       .root = TNode(5, TNode(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
      // Unless failing should not result in any Binds from its inner matcher
      {.matcher = Node5(Unless(PathNode1().Bind("inner"))),
       .root = TNode(5, TNode(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},

      // Unless should negate itself, but should still not result in any binds
      {.matcher = Node5(Unless(Unless(PathNode1().Bind("inner")))),
       .root = TNode(5, TNode(1)),
       .expected_result = true,
       .expected_bound_nodes = {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, AllOfAnyOfManyTests) {
  const MatcherTestCase test_cases[] = {
      // Nesting AllOf inside of AnyOf should work as expected
      {.matcher = Node5(
           AnyOf(AllOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AllOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       .root = TNode(5, TNode(1), TNode(2), XLeaf(2)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node2", NodeTag(2)}, {"Leaf2", LeafTag(2)}}},
      // Only one of two matching AllOf should match
      {.matcher = Node5(
           AnyOf(AllOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AllOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       .root = TNode(5, TNode(1), XLeaf(1), TNode(2), XLeaf(2)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node1", NodeTag(1)}, {"Leaf1", LeafTag(1)}}},
      // It should fail even when parts of both AllOf match
      {.matcher = Node5(
           AnyOf(AllOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AllOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       .root = TNode(5, TNode(1), XLeaf(2)),
       .expected_result = false,
       .expected_bound_nodes = {}},

      // Nesting AnyOf inside of AllOf should work as expected
      {.matcher = Node5(
           AllOf(AnyOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AnyOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       .root = TNode(5, TNode(1), XLeaf(2)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node1", NodeTag(1)}, {"Leaf2", LeafTag(2)}}},
      // Inner AnyOfs should still only bind once each
      {.matcher = Node5(
           AllOf(AnyOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AnyOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       .root = TNode(5, TNode(1), XLeaf(1), TNode(2), XLeaf(2)),
       .expected_result = true,
       .expected_bound_nodes = {{"Node1", NodeTag(1)}, {"Node2", NodeTag(2)}}},
      // It should still fail when only one of inner AnyOf's match
      {.matcher = Node5(
           AllOf(AnyOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AnyOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       .root = TNode(5, TNode(1)),
       .expected_result = false,
       .expected_bound_nodes = {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

}  // namespace
}  // namespace matcher
}  // namespace verible
