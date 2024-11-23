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

#include "verible/common/util/auto-pop-stack.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/iterator-range.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

using IntStack = AutoPopStack<int>;

// Test that AutoPop properly pushes and pops nodes on and off the stack
TEST(AutoPopStackTest, PushPopTest) {
  IntStack context;
  const auto &const_context = context;
  EXPECT_TRUE(context.empty());
  {
    IntStack::AutoPop p1(&context, 1);
    EXPECT_EQ(const_context.top(), 1);
  }
  EXPECT_TRUE(context.empty());
  IntStack::AutoPop p2(&context, 2);
  {
    IntStack::AutoPop p3(&context, 3);
    EXPECT_EQ(const_context.top(), 3);
    IntStack::AutoPop p4(&context, 4);
    EXPECT_EQ(const_context.top(), 4);
  }
  EXPECT_EQ(const_context.top(), 2);
}

// Test that forward/reverse iterators correctly look down/up the stack.
TEST(IntStackTest, IteratorsTest) {
  IntStack context;
  {
    IntStack::AutoPop p1(&context, 1);
    {
      IntStack::AutoPop p2(&context, 2);
      {
        IntStack::AutoPop p3(&context, 3);

        EXPECT_THAT(verible::make_range(context.begin(), context.end()),
                    ElementsAre(1, 2, 3));
        EXPECT_THAT(verible::make_range(context.rbegin(), context.rend()),
                    ElementsAre(3, 2, 1));
      }
    }
  }
}

}  // namespace
}  // namespace verible
