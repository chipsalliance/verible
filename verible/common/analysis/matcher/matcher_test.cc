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

#include "verible/common/analysis/matcher/matcher.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/inner-match-handlers.h"
#include "verible/common/analysis/matcher/matcher-builders.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/casts.h"

namespace verible {
namespace matcher {
namespace {

TEST(MatcherTest, SimpleNonNestedMatchers) {
  auto matcher =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 5>, InnerMatchAll);
  BoundSymbolManager bound_symbol_manager;

  const auto should_match = TNode(5);
  EXPECT_TRUE(matcher.Matches(*should_match, &bound_symbol_manager));

  const auto no_match = TNode(6);
  EXPECT_FALSE(matcher.Matches(*no_match, &bound_symbol_manager));
}

TEST(MatcherTest, SimpleNestedMatchersSuccess) {
  auto inner_matcher =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 5>, InnerMatchAll);
  auto outer_matcher =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 5>, InnerMatchAll);
  outer_matcher.AddMatchers(inner_matcher);

  BoundSymbolManager bound_symbol_manager;

  const auto should_match = TNode(5);
  EXPECT_TRUE(outer_matcher.Matches(*should_match, &bound_symbol_manager));

  const auto no_match = TNode(6);
  EXPECT_FALSE(outer_matcher.Matches(*no_match, &bound_symbol_manager));
}

TEST(MatcherTest, SimpleNestedMatchersFailure) {
  auto inner_matcher =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 6>, InnerMatchAll);
  auto outer_matcher =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 5>, InnerMatchAll);
  outer_matcher.AddMatchers(inner_matcher);

  BoundSymbolManager bound_symbol_manager;

  const auto no_match1 = TNode(5);
  EXPECT_FALSE(outer_matcher.Matches(*no_match1, &bound_symbol_manager));

  const auto no_match2 = TNode(6);
  EXPECT_FALSE(outer_matcher.Matches(*no_match2, &bound_symbol_manager));
}

TEST(MatcherTest, MatchAnyNested) {
  auto inner_matcher1 =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 5>, InnerMatchAll);
  auto inner_matcher2 =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 6>, InnerMatchAll);

  auto outer_matcher =
      Matcher(EqualTagPredicate<SymbolKind::kNode, int, 5>, InnerMatchAny);
  outer_matcher.AddMatchers(inner_matcher1, inner_matcher2);

  BoundSymbolManager bound_symbol_manager;

  const auto should_match = TNode(5);
  EXPECT_TRUE(outer_matcher.Matches(*should_match, &bound_symbol_manager));

  const auto no_match = TNode(6);
  EXPECT_FALSE(outer_matcher.Matches(*no_match, &bound_symbol_manager));
}

TEST(MatcherTest, BindMatcherFlat) {
  auto matcher = BindableMatcher(EqualTagPredicate<SymbolKind::kNode, int, 5>,
                                 InnerMatchAll)
                     .Bind("f");

  BoundSymbolManager bound_symbol_manager;

  const auto should_match = TNode(5);
  EXPECT_TRUE(matcher.Matches(*should_match, &bound_symbol_manager));
  EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("f"));
  const auto *node_ptr =
      down_cast<const SyntaxTreeNode *>(bound_symbol_manager.FindSymbol("f"));
  ASSERT_NE(node_ptr, nullptr);
  EXPECT_TRUE(node_ptr->MatchesTag(5));
  EXPECT_EQ(bound_symbol_manager.Size(), 1);

  bound_symbol_manager.Clear();
  const auto no_match = TNode(6);
  EXPECT_FALSE(matcher.Matches(*no_match, &bound_symbol_manager));
  EXPECT_FALSE(bound_symbol_manager.ContainsSymbol("f"));
  EXPECT_EQ(bound_symbol_manager.Size(), 0);
}

TEST(MatcherTest, BindMatcherNested) {
  auto outer_matcher =
      BindableMatcher(EqualTagPredicate<SymbolKind::kNode, int, 5>,
                      InnerMatchAll)
          .Bind("f");
  auto inner_matcher =
      BindableMatcher(EqualTagPredicate<SymbolKind::kNode, int, 5>,
                      InnerMatchAll)
          .Bind("g");
  outer_matcher.AddMatchers(inner_matcher);

  BoundSymbolManager bound_symbol_manager;

  auto should_match = TNode(5);
  EXPECT_TRUE(outer_matcher.Matches(*should_match, &bound_symbol_manager));
  EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("f"));
  EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("g"));
  const auto *outer_match =
      down_cast<const SyntaxTreeNode *>(bound_symbol_manager.FindSymbol("f"));
  auto inner_match =
      down_cast<const SyntaxTreeNode *>(bound_symbol_manager.FindSymbol("g"));
  ASSERT_NE(outer_match, nullptr);
  ASSERT_NE(inner_match, nullptr);
  EXPECT_TRUE(inner_match->MatchesTag(5));
  EXPECT_TRUE(outer_match->MatchesTag(5));
  EXPECT_EQ(bound_symbol_manager.Size(), 2);

  bound_symbol_manager.Clear();
  auto no_match = TNode(6);
  EXPECT_FALSE(outer_matcher.Matches(*no_match, &bound_symbol_manager));
  EXPECT_FALSE(bound_symbol_manager.ContainsSymbol("f"));
  EXPECT_FALSE(bound_symbol_manager.ContainsSymbol("g"));
  EXPECT_EQ(bound_symbol_manager.Size(), 0);
}

// Returns first child of symbol as a one element array.
// If symbol is a leaf or doesn't have children, returns empty array.
static std::vector<const Symbol *> GetFirstChild(const Symbol &symbol) {
  if (symbol.Kind() == SymbolKind::kNode) {
    const auto &node = down_cast<const SyntaxTreeNode &>(symbol);
    if (node.empty()) return {};
    return {node[0].get()};
  }

  return {};
}

static bool HasExactlyOneChild(const Symbol &symbol) {
  return GetFirstChild(symbol).size() == 1;
}

TEST(MatcherTest, SimpleTransformerTest) {
  auto outer_matcher =
      BindableMatcher(EqualTagPredicate<SymbolKind::kNode, int, 5>,
                      InnerMatchAll)
          .Bind("f");
  auto inner_matcher =
      BindableMatcher(HasExactlyOneChild, InnerMatchAll, GetFirstChild)
          .Bind("g");
  outer_matcher.AddMatchers(inner_matcher);

  BoundSymbolManager bound_symbol_manager;

  auto should_match = TNode(5, XLeaf(123));

  EXPECT_TRUE(outer_matcher.Matches(*should_match, &bound_symbol_manager));
  EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("f"));
  EXPECT_TRUE(bound_symbol_manager.ContainsSymbol("g"));

  const auto *outer_match =
      down_cast<const SyntaxTreeNode *>(bound_symbol_manager.FindSymbol("f"));
  const auto *inner_match =
      down_cast<const SyntaxTreeLeaf *>(bound_symbol_manager.FindSymbol("g"));
  ASSERT_NE(outer_match, nullptr);
  ASSERT_NE(inner_match, nullptr);
  EXPECT_EQ(outer_match->Tag(), NodeTag(5));
  EXPECT_EQ(inner_match->Tag(), LeafTag(123));
}

}  // namespace
}  // namespace matcher
}  // namespace verible
