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
      {Node5(AnyOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       TNode(5, TNode(1), XLeaf(1)),
       true,
       {{"Node", NodeTag(1)}}},
      {Node5(AnyOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       TNode(5, TNode(1), XLeaf(1)),
       true,
       {{"XLeaf", LeafTag(1)}}},

      // Unmatched inner matchers should not bind
      {Node5(AnyOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       TNode(5, XLeaf(1)),
       true,
       {{"XLeaf", LeafTag(1)}}},
      {Node5(AnyOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       TNode(5, TNode(1)),
       true,
       {{"Node", NodeTag(1)}}},

      // Unmatched inner matchers should not prevent binds elsewhere in bode
      {Node5(Node5().Bind("first"),
             AnyOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       TNode(5, XLeaf(1)),
       true,
       {{"XLeaf", LeafTag(1)}, {"first", NodeTag(5)}}},
      {Node5(Node5().Bind("first"),
             AnyOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node")))
           .Bind("outer"),
       TNode(5, TNode(1)),
       true,
       {{"Node", NodeTag(1)}, {"first", NodeTag(5)}, {"outer", NodeTag(5)}}},

      // AnyOf should fail when all inner matchers fail
      {Node5(AnyOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       TNode(5, TNode(5), XLeaf(5)),
       false,
       {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, EachOfManyTests) {
  const MatcherTestCase test_cases[] = {
      // All passing matchers should bind
      {Node5(EachOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       TNode(5, TNode(1), XLeaf(1)),
       true,
       {{"Node", NodeTag(1)}, {"XLeaf", LeafTag(1)}}},
      // Unmatched inner matchers should not bind
      {Node5(EachOf(PathNode2().Bind("node2"), PathLeaf1().Bind("XLeaf"),
                    PathLeaf2().Bind("leaf2"), PathNode1().Bind("Node"))),
       TNode(5, TNode(1), XLeaf(1)),
       true,
       {{"Node", NodeTag(1)}, {"XLeaf", LeafTag(1)}}},
      // Inner matchers of EachOf's inner matchers that pass should bind
      {Node5(EachOf(PathNode1(Node1().Bind("InnerNode")).Bind("Node"),
                    PathLeaf1(Leaf1().Bind("InnerLeaf")).Bind("XLeaf"))),
       TNode(5, TNode(1), XLeaf(1)),
       true,
       {{"Node", NodeTag(1)},
        {"XLeaf", LeafTag(1)},
        {"InnerNode", NodeTag(1)},
        {"InnerLeaf", LeafTag(1)}}},

      // Unmatched inner matchers should not prevent binds elsewhere
      {Node5(Node5().Bind("first"),
             EachOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       TNode(5, XLeaf(1)),
       true,
       {{"XLeaf", LeafTag(1)}, {"first", NodeTag(5)}}},
      {Node5(Node5().Bind("first"),
             EachOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node")))
           .Bind("outer"),
       TNode(5, TNode(1)),
       true,
       {{"Node", NodeTag(1)}, {"first", NodeTag(5)}, {"outer", NodeTag(5)}}},

      // EachOf should fail when all inner matchers fail
      {Node5(EachOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       TNode(5, TNode(5), XLeaf(5)),
       false,
       {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, AllOfManyTests) {
  const MatcherTestCase test_cases[] = {
      // All inner matchers must match for AllOf to match.
      // Each passing matcher should bind.
      {Node5(AllOf(PathNode1().Bind("Node"), PathLeaf1().Bind("XLeaf"))),
       TNode(5, TNode(1), XLeaf(1)),
       true,
       {{"Node", NodeTag(1)}, {"XLeaf", LeafTag(1)}}},

      // One inner matcher failing should cause AllOf to fail.
      // No matchers should bind in this case.
      {Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       TNode(5, XLeaf(1)),
       false,
       {}},
      {Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node"))),
       TNode(5, TNode(1)),
       false,
       {}},
      {Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node")))
           .Bind("outer"),
       TNode(5, XLeaf(1)),
       false,
       {}},
      {Node5(AllOf(PathLeaf1().Bind("XLeaf"), PathNode1().Bind("Node")))
           .Bind("outer"),
       TNode(5, TNode(1)),
       false,
       {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, UnlessManyTests) {
  const MatcherTestCase test_cases[] = {
      // Unless should match if its inner matcher does not match
      {Node5(Unless(PathNode1())), TNode(5, XLeaf(1)), true, {}},
      // Unless matching should not result in any Binds from its inner matcher
      {Node5(Unless(PathNode1().Bind("inner"))), TNode(5, XLeaf(1)), true, {}},
      // Unless matching should not prevent parent/sibling matchers from binding
      {Node5(Node5().Bind("sibling"), Unless(PathNode1().Bind("inner")))
           .Bind("outer"),
       TNode(5, XLeaf(1)),
       true,
       {{"sibling", NodeTag(5)}, {"outer", NodeTag(5)}}},

      // Unless should not match if its inner matcher does match
      {Node5(Unless(PathNode1())), TNode(5, TNode(1)), false, {}},
      // Unless failing should not result in any Binds from its inner matcher
      {Node5(Unless(PathNode1().Bind("inner"))), TNode(5, TNode(1)), false, {}},

      // Unless should negate itself, but should still not result in any binds
      {Node5(Unless(Unless(PathNode1().Bind("inner")))),
       TNode(5, TNode(1)),
       true,
       {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

TEST(CoreMatchers, AllOfAnyOfManyTests) {
  const MatcherTestCase test_cases[] = {
      // Nesting AllOf inside of AnyOf should work as expected
      {Node5(
           AnyOf(AllOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AllOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       TNode(5, TNode(1), TNode(2), XLeaf(2)),
       true,
       {{"Node2", NodeTag(2)}, {"Leaf2", LeafTag(2)}}},
      // Only one of two matching AllOf should match
      {Node5(
           AnyOf(AllOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AllOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       TNode(5, TNode(1), XLeaf(1), TNode(2), XLeaf(2)),
       true,
       {{"Node1", NodeTag(1)}, {"Leaf1", LeafTag(1)}}},
      // It should fail even when parts of both AllOf match
      {Node5(
           AnyOf(AllOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AllOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       TNode(5, TNode(1), XLeaf(2)),
       false,
       {}},

      // Nesting AnyOf inside of AllOf should work as expected
      {Node5(
           AllOf(AnyOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AnyOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       TNode(5, TNode(1), XLeaf(2)),
       true,
       {{"Node1", NodeTag(1)}, {"Leaf2", LeafTag(2)}}},
      // Inner AnyOfs should still only bind once each
      {Node5(
           AllOf(AnyOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AnyOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       TNode(5, TNode(1), XLeaf(1), TNode(2), XLeaf(2)),
       true,
       {{"Node1", NodeTag(1)}, {"Node2", NodeTag(2)}}},
      // It should still fail when only one of inner AnyOf's match
      {Node5(
           AllOf(AnyOf(PathNode1().Bind("Node1"), PathLeaf1().Bind("Leaf1")),
                 AnyOf(PathNode2().Bind("Node2"), PathLeaf2().Bind("Leaf2")))),
       TNode(5, TNode(1)),
       false,
       {}},
  };

  for (const auto &test_case : test_cases) {
    RunMatcherTestCase(test_case);
  }
}

}  // namespace
}  // namespace matcher
}  // namespace verible
