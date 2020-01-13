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

#include "common/util/interval_set.h"

#include <initializer_list>
#include <sstream>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

using interval_type = Interval<int>;
using interval_set_type = IntervalSet<int>;

// For constructing golden test values, without relying on ::Add().
class UnsafeIntervalSet : public interval_set_type {
 public:
  // intervals must be non-overlapping, but can be any order.
  UnsafeIntervalSet(std::initializer_list<interval_type> intervals) {
    for (const auto& interval : intervals) {
      AddUnsafe(interval);
    }
    // Ensure class invariants.
    this->CheckIntegrity();
  }
};

// UnsafeIntervalTests excercise failure of IntervalSet::CheckIntegrity.

TEST(UnsafeIntervalTest, NullInterval) {
  EXPECT_DEATH((UnsafeIntervalSet{{3, 3}}), "");
}

TEST(UnsafeIntervalTest, NullIntervalSecond) {
  EXPECT_DEATH((UnsafeIntervalSet{{0, 1}, {3, 3}}), "");
}

TEST(UnsafeIntervalTest, OverlappingInputs) {
  EXPECT_DEATH((UnsafeIntervalSet{{0, 3}, {1, 2}}), "");
}

TEST(UnsafeIntervalTest, BackwardsInterval) {
  EXPECT_DEATH((UnsafeIntervalSet{{3, 2}}), "");
}

TEST(UnsafeIntervalTest, BackwardsIntervalSecond) {
  EXPECT_DEATH((UnsafeIntervalSet{{0, 1}, {3, 2}}), "");
}

TEST(UnsafeIntervalTest, AbuttingInputs) {
  EXPECT_DEATH((UnsafeIntervalSet{{0, 3}, {3, 5}}), "");
}

TEST(IntervalSetTest, DefaultConstruction) {
  const interval_set_type iset;
  EXPECT_TRUE(iset.empty());
  EXPECT_EQ(iset.size(), 0);
  EXPECT_FALSE(iset.Contains(0));
  EXPECT_FALSE(iset.Contains({0, 0}));
  EXPECT_FALSE(iset.Contains({0, 1}));
}

TEST(IntervalSetTest, EqualityBothEmpty) {
  const interval_set_type iset1, iset2;
  EXPECT_EQ(iset1, iset2);
  EXPECT_EQ(iset2, iset1);
}

TEST(IntervalSetTest, EqualityOneEmpty) {
  const interval_set_type iset1;
  const interval_set_type iset2{{4, 5}};
  EXPECT_NE(iset1, iset2);
  EXPECT_NE(iset2, iset1);
}

TEST(IntervalSetTest, EqualitySame) {
  const interval_set_type iset1{{4, 5}};
  const interval_set_type iset2{{4, 5}};
  EXPECT_EQ(iset1, iset2);
  EXPECT_EQ(iset2, iset1);
}

TEST(IntervalSetTest, EqualityDifferentNonOverlap) {
  const interval_set_type iset1{{4, 5}};
  const interval_set_type iset2{{3, 4}};
  EXPECT_NE(iset1, iset2);
  EXPECT_NE(iset2, iset1);
}

TEST(IntervalSetTest, EqualityDifferentAsymmetricOverlapLeft) {
  const interval_set_type iset1{{4, 5}};
  const interval_set_type iset2{{3, 5}};
  EXPECT_NE(iset1, iset2);
  EXPECT_NE(iset2, iset1);
}

TEST(IntervalSetTest, EqualityDifferentAsymmetricOverlapRight) {
  const interval_set_type iset1{{4, 5}};
  const interval_set_type iset2{{4, 6}};
  EXPECT_NE(iset1, iset2);
  EXPECT_NE(iset2, iset1);
}

typedef std::map<int, int>::value_type pair_t;

TEST(IntervalSetTest, ConstructionWithInitializerOneInterval) {
  const interval_set_type iset{{2, 4}};
  EXPECT_FALSE(iset.empty());
  EXPECT_EQ(iset.size(), 1);
  EXPECT_EQ(iset, iset);

  EXPECT_THAT(iset, ElementsAre(pair_t{2, 4}));

  EXPECT_FALSE(iset.Contains(0));
  EXPECT_FALSE(iset.Contains(1));
  EXPECT_TRUE(iset.Contains(2));
  EXPECT_TRUE(iset.Contains(3));
  EXPECT_FALSE(iset.Contains(4));

  EXPECT_FALSE(iset.Contains({0, 1}));
  EXPECT_FALSE(iset.Contains({1, 2}));
  EXPECT_TRUE(iset.Contains({2, 3}));
  EXPECT_TRUE(iset.Contains({3, 4}));
  EXPECT_FALSE(iset.Contains({4, 5}));

  EXPECT_FALSE(iset.Contains({0, 2}));
  EXPECT_FALSE(iset.Contains({1, 3}));
  EXPECT_TRUE(iset.Contains({2, 4}));
  EXPECT_FALSE(iset.Contains({3, 5}));

  EXPECT_FALSE(iset.Contains({0, 3}));
  EXPECT_FALSE(iset.Contains({1, 4}));
  EXPECT_FALSE(iset.Contains({2, 5}));
}

// Reminder: constructor tests are actually testing Add(interval).

TEST(IntervalSetTest, ConstructionWithInitializerDisjoint) {
  const interval_set_type iset{{2, 4}, {5, 7}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{2, 4}, pair_t{5, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerDisjointReverse) {
  const interval_set_type iset{{5, 7}, {2, 4}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{2, 4}, pair_t{5, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerRedundantIdentical) {
  const interval_set_type iset{{3, 7}, {3, 7}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerAbutting) {
  const interval_set_type iset{{3, 5}, {5, 7}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerAbuttingReverse) {
  const interval_set_type iset{{5, 7}, {3, 5}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerEngulfed) {
  const interval_set_type iset{{3, 7}, {4, 6}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerEngulfedReverse) {
  const interval_set_type iset{{4, 6}, {3, 7}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerSameMin) {
  const interval_set_type iset{{3, 6}, {3, 7}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerSameMinReverse) {
  const interval_set_type iset{{3, 7}, {3, 6}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerSameMax) {
  const interval_set_type iset{{3, 7}, {4, 7}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 7}));
}

TEST(IntervalSetTest, ConstructionWithInitializerSameMaxReverse) {
  const interval_set_type iset{{4, 8}, {3, 8}};
  EXPECT_EQ(iset, iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{3, 8}));
}

TEST(IntervalSetTest, Swap) {
  UnsafeIntervalSet iset1{{3, 8}};
  UnsafeIntervalSet iset2{{2, 5}, {10, 11}};
  EXPECT_NE(iset1, iset2);
  EXPECT_NE(iset2, iset1);
  swap(iset1, iset2);
  EXPECT_THAT(iset2, ElementsAre(pair_t{3, 8}));
  EXPECT_THAT(iset1, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
}

TEST(IntervalSetTest, Assign) {
  UnsafeIntervalSet iset1{{3, 8}};
  UnsafeIntervalSet iset2{{2, 5}, {10, 11}};
  iset1 = iset2;
  EXPECT_EQ(iset1, iset2);
  EXPECT_EQ(iset2, iset1);
  EXPECT_THAT(iset1, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
  EXPECT_THAT(iset2, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
}

TEST(IntervalSetTest, CopyConstruct) {
  UnsafeIntervalSet iset{{2, 5}, {10, 11}};
  const interval_set_type copy(iset);
  EXPECT_THAT(iset, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
  EXPECT_THAT(copy, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
}

TEST(IntervalSetTest, CopyAssign) {
  UnsafeIntervalSet iset{{2, 5}, {10, 11}};
  const interval_set_type copy = iset;
  EXPECT_THAT(iset, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
  EXPECT_THAT(copy, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
}

TEST(IntervalSetTest, MoveConstruct) {
  interval_set_type iset{{2, 5}, {10, 11}};
  const interval_set_type copy(std::move(iset));
  EXPECT_THAT(copy, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
}

TEST(IntervalSetTest, MoveAssign) {
  interval_set_type iset{{2, 5}, {10, 11}};
  interval_set_type copy;
  copy = std::move(iset);
  EXPECT_THAT(copy, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
}

TEST(IntervalSetTest, ClearEmpty) {
  interval_set_type iset;
  EXPECT_TRUE(iset.empty());
  iset.clear();
  EXPECT_TRUE(iset.empty());
}

TEST(IntervalSetTest, ClearNonEmpty) {
  interval_set_type iset{{4, 5}, {6, 7}};
  EXPECT_FALSE(iset.empty());
  iset.clear();
  EXPECT_TRUE(iset.empty());
}

TEST(IntervalSetTest, LowerBound) {
  const UnsafeIntervalSet iset{{3, 5}, {7, 9}};
  const auto front_iter = iset.begin();
  const auto back_iter = std::next(front_iter);
  const auto end = iset.end();
  EXPECT_EQ(iset.LowerBound(2), front_iter);
  EXPECT_EQ(iset.LowerBound(3), front_iter);
  EXPECT_EQ(iset.LowerBound(4), front_iter);
  EXPECT_EQ(iset.LowerBound(5), back_iter);
  EXPECT_EQ(iset.LowerBound(6), back_iter);
  EXPECT_EQ(iset.LowerBound(7), back_iter);
  EXPECT_EQ(iset.LowerBound(8), back_iter);
  EXPECT_EQ(iset.LowerBound(9), end);
  EXPECT_EQ(iset.LowerBound(10), end);
}

TEST(IntervalSetTest, UpperBound) {
  const UnsafeIntervalSet iset{{3, 5}, {7, 9}};
  const auto front_iter = iset.begin();
  const auto back_iter = std::next(front_iter);
  const auto end = iset.end();
  EXPECT_EQ(iset.UpperBound(2), front_iter);
  EXPECT_EQ(iset.UpperBound(3), back_iter);
  EXPECT_EQ(iset.UpperBound(4), back_iter);
  EXPECT_EQ(iset.UpperBound(5), back_iter);
  EXPECT_EQ(iset.UpperBound(6), back_iter);
  EXPECT_EQ(iset.UpperBound(7), end);
  EXPECT_EQ(iset.UpperBound(8), end);
  EXPECT_EQ(iset.UpperBound(9), end);
  EXPECT_EQ(iset.UpperBound(10), end);
}

TEST(IntervalSetTest, FindValue) {
  const UnsafeIntervalSet iset{{3, 5}, {7, 9}};
  const auto front_iter = iset.begin();
  const auto back_iter = std::next(front_iter);
  const auto end = iset.end();
  EXPECT_EQ(iset.Find(2), end);
  EXPECT_EQ(iset.Find(3), front_iter);
  EXPECT_EQ(iset.Find(4), front_iter);
  EXPECT_EQ(iset.Find(5), end);
  EXPECT_EQ(iset.Find(6), end);
  EXPECT_EQ(iset.Find(7), back_iter);
  EXPECT_EQ(iset.Find(8), back_iter);
  EXPECT_EQ(iset.Find(9), end);
  EXPECT_EQ(iset.Find(10), end);
}

TEST(IntervalSetTest, FindInterval) {
  const UnsafeIntervalSet iset{{3, 5}, {7, 9}};
  const auto front_iter = iset.begin();
  const auto back_iter = std::next(front_iter);
  const auto end = iset.end();

  // empty intervals
  for (int i = 2; i < 10; ++i) {
    EXPECT_EQ(iset.Find({i, i}), end);
  }

  // end points outside of the set's span
  for (int i = 2; i < 10; ++i) {
    EXPECT_EQ(iset.Find({2, i}), end);
    EXPECT_EQ(iset.Find({i, 10}), end);
  }
  for (int i = 5; i < 10; ++i) {
    EXPECT_EQ(iset.Find({5, i}), end);
  }
  for (int i = 6; i < 10; ++i) {
    EXPECT_EQ(iset.Find({6, i}), end);
  }
  for (int i = 2; i < 6; ++i) {
    EXPECT_EQ(iset.Find({i, 6}), end);
  }
  for (int i = 2; i < 7; ++i) {
    EXPECT_EQ(iset.Find({i, 7}), end);
  }

  EXPECT_EQ(iset.Find({3, 4}), front_iter);
  EXPECT_EQ(iset.Find({4, 5}), front_iter);
  EXPECT_EQ(iset.Find({3, 5}), front_iter);
  EXPECT_EQ(iset.Find({7, 8}), back_iter);
  EXPECT_EQ(iset.Find({8, 9}), back_iter);
  EXPECT_EQ(iset.Find({7, 9}), back_iter);

  // interval spans the [5,7) gap
  for (int i = 2; i < 7; ++i) {
    for (int j = 6; j < 10; ++j) {
      if (i <= j) {
        EXPECT_EQ(iset.Find({i, j}), end) << "{i,j}=" << i << ',' << j;
      }
    }
  }
}

TEST(IntervalSetTest, FindInvalid) {
  const UnsafeIntervalSet iset{};
  EXPECT_DEATH((iset.Find({2, 1})), "");
}

struct AddSingleValueTestData {
  int value;
  UnsafeIntervalSet expected;
};

TEST(IntervalSetTest, AddSingleValue) {
  const UnsafeIntervalSet init{{10, 20}, {30, 40}};
  const AddSingleValueTestData kTestCases[] = {
      {5, {{5, 6}, {10, 20}, {30, 40}}},
      {9, {{9, 20}, {30, 40}}},
      {10, {{10, 20}, {30, 40}}},
      {19, {{10, 20}, {30, 40}}},
      {20, {{10, 21}, {30, 40}}},
      {22, {{10, 20}, {22, 23}, {30, 40}}},
      {28, {{10, 20}, {28, 29}, {30, 40}}},
      {29, {{10, 20}, {29, 40}}},
      {30, {{10, 20}, {30, 40}}},
      {39, {{10, 20}, {30, 40}}},
      {40, {{10, 20}, {30, 41}}},
      {41, {{10, 20}, {30, 40}, {41, 42}}},
  };
  for (const auto& test : kTestCases) {
    UnsafeIntervalSet copy(init);
    copy.Add(test.value);
    EXPECT_EQ(copy, test.expected);
  }
}

struct AddIntervalTestData {
  interval_type value;
  UnsafeIntervalSet expected;
};

TEST(IntervalSetTest, AddEmptyIntervalToEmptySet) {
  UnsafeIntervalSet init{};
  for (int i = 5; i < 45; ++i) {
    init.Add({i, i});
    EXPECT_TRUE(init.empty());
  }
}

TEST(IntervalSetTest, AddEmptyIntervalToNonEmptySet) {
  const UnsafeIntervalSet init{{10, 20}, {30, 40}};
  UnsafeIntervalSet copy(init);
  for (int i = 5; i < 45; ++i) {
    copy.Add({i, i});
    EXPECT_EQ(copy, init);
  }
}

TEST(IntervalSetTest, AddIntervalNonEmpty) {
  const UnsafeIntervalSet init{{10, 20}, {30, 40}};
  const AddIntervalTestData kTestCases[] = {
      {{5, 9}, {{5, 9}, {10, 20}, {30, 40}}},
      {{5, 10}, {{5, 20}, {30, 40}}},
      {{5, 20}, {{5, 20}, {30, 40}}},
      {{5, 21}, {{5, 21}, {30, 40}}},
      {{5, 29}, {{5, 29}, {30, 40}}},
      {{5, 30}, {{5, 40}}},
      {{5, 40}, {{5, 40}}},
      {{5, 41}, {{5, 41}}},
      {{10, 19}, {{10, 20}, {30, 40}}},
      {{10, 20}, {{10, 20}, {30, 40}}},
      {{10, 21}, {{10, 21}, {30, 40}}},
      {{10, 29}, {{10, 29}, {30, 40}}},
      {{10, 30}, {{10, 40}}},
      {{10, 40}, {{10, 40}}},
      {{10, 41}, {{10, 41}}},
      {{20, 21}, {{10, 21}, {30, 40}}},
      {{20, 29}, {{10, 29}, {30, 40}}},
      {{20, 30}, {{10, 40}}},  // seals the gap, abutting both ends
      {{20, 40}, {{10, 40}}},
      {{20, 41}, {{10, 41}}},
      {{21, 29}, {{10, 20}, {21, 29}, {30, 40}}},
      {{21, 30}, {{10, 20}, {21, 40}}},
      {{21, 40}, {{10, 20}, {21, 40}}},
      {{21, 41}, {{10, 20}, {21, 41}}},
      {{29, 30}, {{10, 20}, {29, 40}}},
      {{29, 40}, {{10, 20}, {29, 40}}},
      {{29, 41}, {{10, 20}, {29, 41}}},
      {{30, 40}, {{10, 20}, {30, 40}}},
      {{30, 41}, {{10, 20}, {30, 41}}},
      {{40, 41}, {{10, 20}, {30, 41}}},
      {{41, 42}, {{10, 20}, {30, 40}, {41, 42}}},
  };
  for (const auto& test : kTestCases) {
    UnsafeIntervalSet copy(init);
    copy.Add(test.value);
    EXPECT_EQ(copy, test.expected);
  }
}

TEST(IntervalSetTest, AddInvalidInterval) {
  UnsafeIntervalSet iset{};
  EXPECT_DEATH((iset.Add({2, 1})), "");
}

// Add empty intervals

// ExpectDEath on malformed UnsafeIntervalSet

}  // namespace
}  // namespace verible
