// Copyright 2017-2019 The Verible Authors.
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

#include "common/text/syntax_tree_context.h"

#include "gtest/gtest.h"
#include "common/text/concrete_syntax_tree.h"

namespace verible {
namespace {

// Test that AutoPop properly pushes and pops nodes on and off the stack
TEST(SyntaxTreeContextTest, PushPopTest) {
  SyntaxTreeContext context;
  EXPECT_TRUE(context.empty());
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, node1);
    EXPECT_EQ(&context.top(), &node1);
  }
  EXPECT_TRUE(context.empty());
  SyntaxTreeNode node2(2);
  SyntaxTreeContext::AutoPop p2(&context, node2);
  SyntaxTreeNode node3(3);
  SyntaxTreeNode node4(4);
  {
    SyntaxTreeContext::AutoPop p3(&context, node3);
    EXPECT_EQ(&context.top(), &node3);
    SyntaxTreeContext::AutoPop p4(&context, node4);
    EXPECT_EQ(&context.top(), &node4);
  }
  EXPECT_EQ(&context.top(), &node2);
}

// Test that IsInside correctly reports whether context matches.
TEST(SyntaxTreeContextTest, IsInsideTest) {
  SyntaxTreeContext context;
  EXPECT_FALSE(context.IsInside(1));
  EXPECT_FALSE(context.IsInside(2));
  EXPECT_FALSE(context.IsInside(3));
  {
    SyntaxTreeNode node1(1);
    SyntaxTreeContext::AutoPop p1(&context, node1);
    EXPECT_TRUE(context.IsInside(1));
    EXPECT_FALSE(context.IsInside(2));
    EXPECT_FALSE(context.IsInside(3));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, node2);
      EXPECT_TRUE(context.IsInside(1));
      EXPECT_TRUE(context.IsInside(2));
      EXPECT_FALSE(context.IsInside(3));
      {
        SyntaxTreeNode node3(3);
        SyntaxTreeContext::AutoPop p3(&context, node3);
        EXPECT_TRUE(context.IsInside(1));
        EXPECT_TRUE(context.IsInside(2));
        EXPECT_TRUE(context.IsInside(3));
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
    SyntaxTreeContext::AutoPop p1(&context, node1);
    EXPECT_TRUE(context.IsInsideFirst({1}, {0, 2, 3}));
    EXPECT_FALSE(context.IsInsideFirst({0}, {1, 2, 3}));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, node2);
      EXPECT_TRUE(context.IsInsideFirst({2}, {0, 1, 3}));
      EXPECT_TRUE(context.IsInsideFirst({1}, {0}));
      EXPECT_FALSE(context.IsInsideFirst({1}, {2}));
      EXPECT_TRUE(context.IsInsideFirst({1, 2}, {0}));
      EXPECT_TRUE(context.IsInsideFirst({1, 3}, {0}));
      {
        SyntaxTreeNode node3(3);
        SyntaxTreeContext::AutoPop p3(&context, node3);
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
    SyntaxTreeContext::AutoPop p1(&context, node1);
    EXPECT_FALSE(context.DirectParentIs(0));
    EXPECT_TRUE(context.DirectParentIs(1));
    EXPECT_FALSE(context.DirectParentIs(2));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, node2);
      EXPECT_FALSE(context.DirectParentIs(0));
      EXPECT_FALSE(context.DirectParentIs(1));
      EXPECT_TRUE(context.DirectParentIs(2));
      {
        SyntaxTreeNode node3(5);
        SyntaxTreeContext::AutoPop p3(&context, node3);
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
    SyntaxTreeContext::AutoPop p1(&context, node1);
    EXPECT_FALSE(context.DirectParentIsOneOf({0, 3, 6}));
    EXPECT_TRUE(context.DirectParentIsOneOf({1, 4, 7}));
    EXPECT_FALSE(context.DirectParentIsOneOf({2, 5, 8}));
    {
      SyntaxTreeNode node2(5);
      SyntaxTreeContext::AutoPop p2(&context, node2);
      EXPECT_FALSE(context.DirectParentIsOneOf({0, 3, 6}));
      EXPECT_FALSE(context.DirectParentIsOneOf({1, 4, 7}));
      EXPECT_TRUE(context.DirectParentIsOneOf({2, 5, 8}));
      {
        SyntaxTreeNode node3(9);
        SyntaxTreeContext::AutoPop p3(&context, node3);
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
    SyntaxTreeContext::AutoPop p1(&context, node1);
    EXPECT_TRUE(context.DirectParentsAre<int>({}));  // degenerate case
    EXPECT_FALSE(context.DirectParentsAre({0}));
    EXPECT_TRUE(context.DirectParentsAre({1}));
    EXPECT_FALSE(context.DirectParentsAre({1, 0}));
    EXPECT_FALSE(context.DirectParentsAre({0, 1}));
    EXPECT_FALSE(context.DirectParentsAre({1, 1}));
    {
      SyntaxTreeNode node2(2);
      SyntaxTreeContext::AutoPop p2(&context, node2);
      EXPECT_FALSE(context.DirectParentsAre({1}));
      EXPECT_TRUE(context.DirectParentsAre({2}));
      EXPECT_FALSE(context.DirectParentsAre({1, 2}));
      EXPECT_TRUE(context.DirectParentsAre({2, 1}));
      EXPECT_FALSE(context.DirectParentsAre({1, 1}));
      EXPECT_FALSE(context.DirectParentsAre({2, 2}));
      EXPECT_FALSE(context.DirectParentsAre({2, 1, 0}));
      {
        SyntaxTreeNode node3(5);
        SyntaxTreeContext::AutoPop p3(&context, node3);
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

}  // namespace
}  // namespace verible
