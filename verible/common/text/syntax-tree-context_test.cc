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

#include "verible/common/text/syntax-tree-context.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/util/iterator-range.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

// Test that AutoPop properly pushes and pops nodes on and off the stack
TEST(SyntaxTreeContextTest, PushPopTest) {
  SyntaxTreeContext context;
  const auto &const_context = context;
  EXPECT_TRUE(context.empty());
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    EXPECT_EQ(&const_context.top(), &node1);
  }
  EXPECT_TRUE(context.empty());
  SyntaxTreeNode node2(2);
  SyntaxTreeContext::AutoPop p2(&context, &node2);
  SyntaxTreeNode node3(3);
  SyntaxTreeNode node4(4);
  {
    SyntaxTreeContext::AutoPop p3(&context, &node3);
    EXPECT_EQ(&const_context.top(), &node3);
    SyntaxTreeContext::AutoPop p4(&context, &node4);
    EXPECT_EQ(&const_context.top(), &node4);
  }
  EXPECT_EQ(&const_context.top(), &node2);
}

// Test that forward/reverse iterators correctly look down/up the stack.
TEST(SyntaxTreeContextTest, IteratorsTest) {
  SyntaxTreeContext context;
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, &node2);
      {
        SyntaxTreeNode node3(3);
        SyntaxTreeContext::AutoPop p3(&context, &node3);

        EXPECT_THAT(verible::make_range(context.begin(), context.end()),
                    ElementsAre(&node1, &node2, &node3));
        EXPECT_THAT(verible::make_range(context.rbegin(), context.rend()),
                    ElementsAre(&node3, &node2, &node1));
      }
    }
  }
}

// Test that IsInside and IsInsideStartingFrom correctly reports whether
// context matches.
TEST(SyntaxTreeContextTest, IsInsideTest) {
  SyntaxTreeContext context;
  EXPECT_FALSE(context.IsInside(1));
  EXPECT_FALSE(context.IsInside(2));
  EXPECT_FALSE(context.IsInside(3));
  EXPECT_FALSE(context.IsInsideStartingFrom(1, 1));
  EXPECT_FALSE(context.IsInsideStartingFrom(1, 0));
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    EXPECT_TRUE(context.IsInside(1));
    EXPECT_FALSE(context.IsInside(2));
    EXPECT_FALSE(context.IsInside(3));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, &node2);
      EXPECT_TRUE(context.IsInside(1));
      EXPECT_TRUE(context.IsInside(2));
      EXPECT_FALSE(context.IsInside(3));
      {
        SyntaxTreeNode node3(3);
        SyntaxTreeContext::AutoPop p3(&context, &node3);
        EXPECT_TRUE(context.IsInside(1));
        EXPECT_TRUE(context.IsInside(2));
        EXPECT_TRUE(context.IsInside(3));
        // With an offset, we won't see some of these elements
        EXPECT_TRUE(context.IsInsideStartingFrom(3, 0));
        EXPECT_FALSE(context.IsInsideStartingFrom(3, 1));
        EXPECT_TRUE(context.IsInsideStartingFrom(2, 1));
        EXPECT_FALSE(context.IsInsideStartingFrom(2, 2));
        // Check stack boundary
        EXPECT_FALSE(context.IsInsideStartingFrom(2, 3));
        EXPECT_FALSE(context.IsInsideStartingFrom(2, 4));
      }
    }
  }
}

// Test that IsInsideFirst correctly reports whether context matches.
TEST(SyntaxTreeContextTest, IsInsideFirstTest) {
  SyntaxTreeContext context;
  EXPECT_FALSE(context.IsInsideFirst({1, 2, 3}, {0}));
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    EXPECT_TRUE(context.IsInsideFirst({1}, {0, 2, 3}));
    EXPECT_FALSE(context.IsInsideFirst({0}, {1, 2, 3}));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, &node2);
      EXPECT_TRUE(context.IsInsideFirst({2}, {0, 1, 3}));
      EXPECT_TRUE(context.IsInsideFirst({1}, {0}));
      EXPECT_FALSE(context.IsInsideFirst({1}, {2}));
      EXPECT_TRUE(context.IsInsideFirst({1, 2}, {0}));
      EXPECT_TRUE(context.IsInsideFirst({1, 3}, {0}));
      {
        SyntaxTreeNode node3(3);
        SyntaxTreeContext::AutoPop p3(&context, &node3);
        EXPECT_TRUE(context.IsInsideFirst({2}, {0, 1}));
        EXPECT_TRUE(context.IsInsideFirst({3}, {0, 1, 2}));
        EXPECT_TRUE(context.IsInsideFirst({1}, {0}));
        EXPECT_FALSE(context.IsInsideFirst({1}, {2}));
        EXPECT_FALSE(context.IsInsideFirst({1}, {3}));
        EXPECT_TRUE(context.IsInsideFirst({2}, {0}));
        EXPECT_TRUE(context.IsInsideFirst({2}, {1}));
        EXPECT_FALSE(context.IsInsideFirst({2}, {3}));
        EXPECT_TRUE(context.IsInsideFirst({3}, {0}));
        EXPECT_TRUE(context.IsInsideFirst({3}, {1}));
        EXPECT_TRUE(context.IsInsideFirst({3}, {2}));
        EXPECT_TRUE(context.IsInsideFirst({1, 3}, {2}));
        EXPECT_FALSE(context.IsInsideFirst({1, 2}, {3}));
      }
    }
  }
}

TEST(SyntaxTreeContextTest, DirectParentIsTest) {
  SyntaxTreeContext context;
  // initially empty, always false
  EXPECT_FALSE(context.DirectParentIs(0));
  EXPECT_FALSE(context.DirectParentIs(1));
  EXPECT_FALSE(context.DirectParentIs(2));
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    EXPECT_FALSE(context.DirectParentIs(0));
    EXPECT_TRUE(context.DirectParentIs(1));
    EXPECT_FALSE(context.DirectParentIs(2));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, &node2);
      EXPECT_FALSE(context.DirectParentIs(0));
      EXPECT_FALSE(context.DirectParentIs(1));
      EXPECT_TRUE(context.DirectParentIs(2));
      {
        SyntaxTreeNode node3(5);
        SyntaxTreeContext::AutoPop p3(&context, &node3);
        EXPECT_FALSE(context.DirectParentIs(0));
        EXPECT_FALSE(context.DirectParentIs(1));
        EXPECT_FALSE(context.DirectParentIs(2));
      }
    }
  }
}

TEST(SyntaxTreeContextTest, DirectParentIsOneOfTest) {
  SyntaxTreeContext context;
  // initially empty, always false
  EXPECT_FALSE(context.DirectParentIsOneOf({0, 3, 6}));
  EXPECT_FALSE(context.DirectParentIsOneOf({1, 4, 7}));
  EXPECT_FALSE(context.DirectParentIsOneOf({2, 5, 9}));
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    EXPECT_FALSE(context.DirectParentIsOneOf({0, 3, 6}));
    EXPECT_TRUE(context.DirectParentIsOneOf({1, 4, 7}));
    EXPECT_FALSE(context.DirectParentIsOneOf({2, 5, 8}));
    {
      SyntaxTreeNode node2(5);
      SyntaxTreeContext::AutoPop p2(&context, &node2);
      EXPECT_FALSE(context.DirectParentIsOneOf({0, 3, 6}));
      EXPECT_FALSE(context.DirectParentIsOneOf({1, 4, 7}));
      EXPECT_TRUE(context.DirectParentIsOneOf({2, 5, 8}));
      {
        SyntaxTreeNode node3(9);
        SyntaxTreeContext::AutoPop p3(&context, &node3);
        EXPECT_FALSE(context.DirectParentIsOneOf({0, 3, 6}));
        EXPECT_FALSE(context.DirectParentIsOneOf({1, 4, 7}));
        EXPECT_FALSE(context.DirectParentIsOneOf({2, 5, 8}));
      }
    }
  }
}

TEST(SyntaxTreeContextTest, DirectParentsAreTest) {
  SyntaxTreeContext context;
  // initially empty, always false
  EXPECT_TRUE(context.DirectParentsAre<int>({}));  // degenerate case
  EXPECT_FALSE(context.DirectParentsAre({0}));
  EXPECT_FALSE(context.DirectParentsAre({1}));
  EXPECT_FALSE(context.DirectParentsAre({0, 1}));
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    EXPECT_TRUE(context.DirectParentsAre<int>({}));  // degenerate case
    EXPECT_FALSE(context.DirectParentsAre({0}));
    EXPECT_TRUE(context.DirectParentsAre({1}));
    EXPECT_FALSE(context.DirectParentsAre({1, 0}));
    EXPECT_FALSE(context.DirectParentsAre({0, 1}));
    EXPECT_FALSE(context.DirectParentsAre({1, 1}));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, &node2);
      EXPECT_FALSE(context.DirectParentsAre({1}));
      EXPECT_TRUE(context.DirectParentsAre({2}));
      EXPECT_FALSE(context.DirectParentsAre({1, 2}));
      EXPECT_TRUE(context.DirectParentsAre({2, 1}));
      EXPECT_FALSE(context.DirectParentsAre({1, 1}));
      EXPECT_FALSE(context.DirectParentsAre({2, 2}));
      EXPECT_FALSE(context.DirectParentsAre({2, 1, 0}));
      {
        SyntaxTreeNode node3(5);
        SyntaxTreeContext::AutoPop p3(&context, &node3);
        EXPECT_FALSE(context.DirectParentsAre({1}));
        EXPECT_FALSE(context.DirectParentsAre({2}));
        EXPECT_TRUE(context.DirectParentsAre({5}));
        EXPECT_TRUE(context.DirectParentsAre({5, 2}));
        EXPECT_TRUE(context.DirectParentsAre({5, 2, 1}));
        EXPECT_FALSE(context.DirectParentsAre({3, 2, 1}));
        EXPECT_FALSE(context.DirectParentsAre({1, 2, 5}));
        EXPECT_FALSE(context.DirectParentsAre({5, 5}));
        EXPECT_FALSE(context.DirectParentsAre({5, 1}));
        EXPECT_FALSE(context.DirectParentsAre({5, 2, 1, 0}));
      }
    }
  }
}

TEST(SyntaxTreeContextTest, NearestParentMatchingTest) {
  // define a few predicate functions
  const auto True = [](const SyntaxTreeNode &n) { return true; };
  const auto TagEq = [](int i) {
    return [i](const SyntaxTreeNode &n) { return n.Tag().tag == i; };
  };

  SyntaxTreeContext context;
  // initially empty, always nullptr regardless of predicate
  EXPECT_EQ(context.NearestParentMatching(True), nullptr);
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, &node1);
    EXPECT_EQ(context.NearestParentMatching(True), &node1);
    EXPECT_EQ(context.NearestParentMatching(TagEq(0)), nullptr);
    EXPECT_EQ(context.NearestParentWithTag(0), nullptr);
    EXPECT_EQ(context.NearestParentMatching(TagEq(1)), &node1);
    EXPECT_EQ(context.NearestParentWithTag(1), &node1);
    {
      SyntaxTreeNode node2(1);
      SyntaxTreeContext::AutoPop p2(&context, &node2);
      EXPECT_EQ(context.NearestParentMatching(True), &node2);
      EXPECT_EQ(context.NearestParentMatching(TagEq(0)), nullptr);
      EXPECT_EQ(context.NearestParentWithTag(0), nullptr);
      EXPECT_EQ(context.NearestParentMatching(TagEq(1)), &node2);
      EXPECT_EQ(context.NearestParentWithTag(1), &node2);
      {
        SyntaxTreeNode node3(3);
        SyntaxTreeContext::AutoPop p3(&context, &node3);
        EXPECT_EQ(context.NearestParentMatching(True), &node3);
        EXPECT_EQ(context.NearestParentMatching(TagEq(0)), nullptr);
        EXPECT_EQ(context.NearestParentWithTag(0), nullptr);
        EXPECT_EQ(context.NearestParentMatching(TagEq(1)), &node2);
        EXPECT_EQ(context.NearestParentWithTag(1), &node2);
        EXPECT_EQ(context.NearestParentMatching(TagEq(3)), &node3);
        EXPECT_EQ(context.NearestParentWithTag(3), &node3);
      }
    }
  }
}

}  // namespace
}  // namespace verible
