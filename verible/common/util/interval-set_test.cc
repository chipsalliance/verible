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

#include "verible/common/util/interval-set.h"

#include <initializer_list>
#include <iterator>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/interval.h"
#include "verible/common/util/logging.h"

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
    for (const auto &interval : intervals) {
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

using pair_t = std::map<int, int>::value_type;

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
  // clang-format off
  const interval_set_type copy(iset);  // NOLINT(performance-unnecessary-copy-initialization)
  // clang-format on
  EXPECT_THAT(iset, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
  EXPECT_THAT(copy, ElementsAre(pair_t{2, 5}, pair_t{10, 11}));
}

TEST(IntervalSetTest, CopyAssign) {
  UnsafeIntervalSet iset{{2, 5}, {10, 11}};
  // clang-format off
  const interval_set_type copy = iset;  // NOLINT(performance-unnecessary-copy-initialization)
  // clang-format on
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
  for (const auto &test : kTestCases) {
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
  for (const auto &test : kTestCases) {
    UnsafeIntervalSet copy(init);
    copy.Add(test.value);
    EXPECT_EQ(copy, test.expected);
  }
}

TEST(IntervalSetTest, AddInvalidInterval) {
  UnsafeIntervalSet iset{};
  EXPECT_DEATH((iset.Add({2, 1})), "");
}

using DifferenceIntervalTestData = AddIntervalTestData;

TEST(IntervalSetTest, DifferenceInvalidInterval) {
  UnsafeIntervalSet iset{};
  EXPECT_DEATH((iset.Difference({2, 1})), "");
}

TEST(IntervalSetTest, DifferenceEmptyIntervalFromEmptySet) {
  UnsafeIntervalSet init{};
  for (int i = 5; i < 45; ++i) {
    init.Difference({i, i});
    EXPECT_TRUE(init.empty());
  }
}

TEST(IntervalSetTest, DifferenceEmptyIntervalFromNonEmptySet) {
  const UnsafeIntervalSet init{{10, 20}, {30, 40}};
  UnsafeIntervalSet copy(init);
  for (int i = 5; i < 45; ++i) {
    copy.Difference({i, i});
    EXPECT_EQ(copy, init);
  }
}

using DifferenceSingleValueTestData = AddSingleValueTestData;

TEST(IntervalSetTest, DifferenceSingleValue) {
  const UnsafeIntervalSet init{{10, 20}, {30, 40}};
  const DifferenceSingleValueTestData kTestCases[] = {
      {9, {{10, 20}, {30, 40}}},
      {10, {{11, 20}, {30, 40}}},
      {11, {{10, 11}, {12, 20}, {30, 40}}},
      {18, {{10, 18}, {19, 20}, {30, 40}}},
      {19, {{10, 19}, {30, 40}}},
      {20, {{10, 20}, {30, 40}}},
      {21, {{10, 20}, {30, 40}}},
      {29, {{10, 20}, {30, 40}}},
      {30, {{10, 20}, {31, 40}}},
      {31, {{10, 20}, {30, 31}, {32, 40}}},
      {38, {{10, 20}, {30, 38}, {39, 40}}},
      {39, {{10, 20}, {30, 39}}},
      {40, {{10, 20}, {30, 40}}},
      {41, {{10, 20}, {30, 40}}},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "remove: " << test.value << " expect: " << test.expected;
    UnsafeIntervalSet copy(init);
    copy.Difference(test.value);
    EXPECT_EQ(copy, test.expected);
  }
}

TEST(IntervalSetTest, DifferenceIntervalNonEmpty) {
  const UnsafeIntervalSet init{{10, 20}, {30, 40}};
  const DifferenceIntervalTestData kTestCases[] = {
      {{5, 9}, {{10, 20}, {30, 40}}},
      {{5, 10}, {{10, 20}, {30, 40}}},
      {{5, 11}, {{11, 20}, {30, 40}}},
      {{5, 19}, {{19, 20}, {30, 40}}},
      {{5, 20}, {{30, 40}}},
      {{5, 21}, {{30, 40}}},
      {{5, 29}, {{30, 40}}},
      {{5, 30}, {{30, 40}}},
      {{5, 31}, {{31, 40}}},
      {{5, 39}, {{39, 40}}},
      {{5, 40}, {}},
      {{5, 41}, {}},
      {{10, 10}, {{10, 20}, {30, 40}}},  // ok to remove empty interval
      {{10, 11}, {{11, 20}, {30, 40}}},
      {{10, 19}, {{19, 20}, {30, 40}}},
      {{10, 20}, {{30, 40}}},
      {{10, 21}, {{30, 40}}},
      {{10, 29}, {{30, 40}}},
      {{10, 30}, {{30, 40}}},
      {{10, 31}, {{31, 40}}},
      {{10, 39}, {{39, 40}}},
      {{10, 40}, {}},
      {{10, 41}, {}},
      {{11, 11}, {{10, 20}, {30, 40}}},
      {{11, 19}, {{10, 11}, {19, 20}, {30, 40}}},
      {{11, 20}, {{10, 11}, {30, 40}}},
      {{11, 21}, {{10, 11}, {30, 40}}},
      {{11, 29}, {{10, 11}, {30, 40}}},
      {{11, 30}, {{10, 11}, {30, 40}}},
      {{11, 31}, {{10, 11}, {31, 40}}},
      {{11, 39}, {{10, 11}, {39, 40}}},
      {{11, 40}, {{10, 11}}},
      {{11, 41}, {{10, 11}}},
      {{19, 19}, {{10, 20}, {30, 40}}},
      {{19, 20}, {{10, 19}, {30, 40}}},
      {{19, 21}, {{10, 19}, {30, 40}}},
      {{19, 29}, {{10, 19}, {30, 40}}},
      {{19, 30}, {{10, 19}, {30, 40}}},
      {{19, 31}, {{10, 19}, {31, 40}}},
      {{19, 39}, {{10, 19}, {39, 40}}},
      {{19, 40}, {{10, 19}}},
      {{19, 41}, {{10, 19}}},
      {{20, 20}, {{10, 20}, {30, 40}}},
      {{20, 21}, {{10, 20}, {30, 40}}},
      {{20, 29}, {{10, 20}, {30, 40}}},
      {{20, 30}, {{10, 20}, {30, 40}}},
      {{20, 31}, {{10, 20}, {31, 40}}},
      {{20, 39}, {{10, 20}, {39, 40}}},
      {{20, 40}, {{10, 20}}},
      {{20, 41}, {{10, 20}}},
      {{21, 21}, {{10, 20}, {30, 40}}},
      {{21, 29}, {{10, 20}, {30, 40}}},
      {{21, 30}, {{10, 20}, {30, 40}}},
      {{21, 31}, {{10, 20}, {31, 40}}},
      {{21, 39}, {{10, 20}, {39, 40}}},
      {{21, 40}, {{10, 20}}},
      {{21, 41}, {{10, 20}}},
      {{29, 29}, {{10, 20}, {30, 40}}},
      {{29, 30}, {{10, 20}, {30, 40}}},
      {{29, 31}, {{10, 20}, {31, 40}}},
      {{29, 39}, {{10, 20}, {39, 40}}},
      {{29, 40}, {{10, 20}}},
      {{29, 41}, {{10, 20}}},
      {{30, 30}, {{10, 20}, {30, 40}}},
      {{30, 31}, {{10, 20}, {31, 40}}},
      {{30, 39}, {{10, 20}, {39, 40}}},
      {{30, 40}, {{10, 20}}},
      {{30, 41}, {{10, 20}}},
      {{31, 31}, {{10, 20}, {30, 40}}},
      {{31, 39}, {{10, 20}, {30, 31}, {39, 40}}},
      {{31, 40}, {{10, 20}, {30, 31}}},
      {{39, 39}, {{10, 20}, {30, 40}}},
      {{39, 40}, {{10, 20}, {30, 39}}},
      {{39, 41}, {{10, 20}, {30, 39}}},
      {{40, 40}, {{10, 20}, {30, 40}}},
      {{40, 41}, {{10, 20}, {30, 40}}},
      {{41, 41}, {{10, 20}, {30, 40}}},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "remove: " << test.value << " expect: " << test.expected;
    UnsafeIntervalSet copy(init);
    copy.Difference(test.value);
    EXPECT_EQ(copy, test.expected);
  }
}

struct SetOperationTestData {
  // roles of a,b,c can be interchanged, e.g. op(a,b)=c vs. op(c,a)=b
  UnsafeIntervalSet a;
  UnsafeIntervalSet b;
  UnsafeIntervalSet c;
};

TEST(IntervalSetTest, SetDifferences) {
  const SetOperationTestData kTestCases[] = {
      {{}, {}, {}},                              // empty - empty = empty
      {{}, {{5, 6}}, {}},                        // empty - anything = empty
      {{}, {{5, 6}, {7, 8}}, {}},                // empty - anything = empty
      {{{1, 2}}, {}, {{1, 2}}},                  // anything - empty = (same)
      {{{1, 2}, {7, 9}}, {}, {{1, 2}, {7, 9}}},  // anything - empty = (same)
      {{{0, 100}}, {{30, 40}, {60, 70}}, {{0, 30}, {40, 60}, {70, 100}}},
      {{{0, 50}}, {{30, 40}, {60, 70}}, {{0, 30}, {40, 50}}},
      {{{50, 100}}, {{30, 40}, {60, 70}}, {{50, 60}, {70, 100}}},
      {{{10, 20}, {30, 40}}, {{5, 9}}, {{10, 20}, {30, 40}}},
      {{{1, 2}, {3, 4}, {5, 6}, {7, 8}}, {{2, 7}}, {{1, 2}, {7, 8}}},
      {{{1, 2}, {3, 4}, {5, 6}, {7, 8}}, {{4, 9}}, {{1, 2}, {3, 4}}},
      {{{1, 2}, {3, 4}, {5, 6}, {7, 8}}, {{0, 4}}, {{5, 6}, {7, 8}}},
      {{{1, 2}, {3, 4}, {5, 6}, {7, 8}}, {{1, 9}}, {}},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << test.a << " - " << test.b << " == " << test.c;
    UnsafeIntervalSet set(test.a);
    set.Difference(test.b);
    EXPECT_EQ(set, test.c);
  }
}

TEST(IntervalSetTest, SetUnions) {
  const SetOperationTestData kTestCases[] = {
      {{}, {}, {}},  // empty U empty = empty
      {{}, {{3, 7}}, {{3, 7}}},
      {{{3, 7}}, {{3, 7}}, {{3, 7}}},
      {{{4, 6}}, {{3, 7}}, {{3, 7}}},
      {{{12, 14}}, {{3, 7}}, {{3, 7}, {12, 14}}},
      {{{1, 3}}, {{3, 7}}, {{1, 7}}},
      {{{1, 2}, {3, 4}, {5, 6}}, {{2, 3}, {4, 5}, {6, 7}}, {{1, 7}}},
      {{{1, 2}, {3, 4}, {5, 6}}, {{2, 7}}, {{1, 7}}},
      {{{1, 2}, {5, 6}}, {{3, 4}, {7, 8}}, {{1, 2}, {3, 4}, {5, 6}, {7, 8}}},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << test.a << " U " << test.b << " == " << test.c;
    UnsafeIntervalSet set(test.a);
    set.Union(test.b);
    EXPECT_EQ(set, test.c);
  }
  // commutative test
  for (const auto &test : kTestCases) {
    VLOG(1) << test.b << " U " << test.a << " == " << test.c;
    UnsafeIntervalSet set(test.b);
    set.Union(test.a);
    EXPECT_EQ(set, test.c);
  }
}

using ComplementTestData = AddIntervalTestData;

TEST(IntervalSetTest, ComplementEmptyInitial) {
  const interval_type kTestCases[] = {
      {0, 0},     //
      {0, 1},     //
      {1, 10},    //
      {10, 100},  //
  };
  for (const auto &test : kTestCases) {
    interval_set_type set;
    set.Complement(test);
    EXPECT_EQ(set, interval_set_type{test});
  }
}

TEST(IntervalSetTest, ComplementGeneral) {
  const UnsafeIntervalSet initial{{10, 20}, {30, 40}};
  const ComplementTestData kTestCases[] = {
      {{5, 10}, {{5, 10}}},
      {{5, 11}, {{5, 10}}},
      {{5, 20}, {{5, 10}}},
      {{5, 21}, {{5, 10}, {20, 21}}},
      {{5, 29}, {{5, 10}, {20, 29}}},
      {{5, 30}, {{5, 10}, {20, 30}}},
      {{5, 31}, {{5, 10}, {20, 30}}},
      {{5, 39}, {{5, 10}, {20, 30}}},
      {{5, 40}, {{5, 10}, {20, 30}}},
      {{5, 41}, {{5, 10}, {20, 30}, {40, 41}}},
      {{10, 11}, {}},
      {{10, 20}, {}},
      {{10, 21}, {{20, 21}}},
      {{10, 29}, {{20, 29}}},
      {{10, 30}, {{20, 30}}},
      {{10, 31}, {{20, 30}}},
      {{10, 39}, {{20, 30}}},
      {{10, 40}, {{20, 30}}},
      {{10, 41}, {{20, 30}, {40, 41}}},
      {{20, 21}, {{20, 21}}},
      {{20, 29}, {{20, 29}}},
      {{20, 30}, {{20, 30}}},
      {{20, 31}, {{20, 30}}},
      {{20, 39}, {{20, 30}}},
      {{20, 40}, {{20, 30}}},
      {{20, 41}, {{20, 30}, {40, 41}}},
      {{21, 29}, {{21, 29}}},
      {{21, 30}, {{21, 30}}},
      {{21, 31}, {{21, 30}}},
      {{21, 39}, {{21, 30}}},
      {{21, 40}, {{21, 30}}},
      {{21, 41}, {{21, 30}, {40, 41}}},
      {{29, 29}, {}},
      {{29, 30}, {{29, 30}}},
      {{29, 31}, {{29, 30}}},
      {{29, 39}, {{29, 30}}},
      {{29, 40}, {{29, 30}}},
      {{29, 41}, {{29, 30}, {40, 41}}},
      {{30, 30}, {}},
      {{30, 31}, {}},
      {{30, 39}, {}},
      {{30, 40}, {}},
      {{30, 41}, {{40, 41}}},
      {{39, 39}, {}},
      {{39, 40}, {}},
      {{39, 41}, {{40, 41}}},
      {{40, 40}, {}},
      {{40, 41}, {{40, 41}}},
      {{40, 45}, {{40, 45}}},
  };
  for (const auto &test : kTestCases) {
    VLOG(1) << "comp " << test.value << " == " << test.expected;
    UnsafeIntervalSet set(initial);
    set.Complement(test.value);
    EXPECT_EQ(set, test.expected);
  }
}

TEST(IntervalSetTest, MonotonicTransformShiftUp) {
  const UnsafeIntervalSet initial{{10, 20}, {30, 40}};
  const interval_set_type result =
      initial.MonotonicTransform<int>([](const int x) { return x + 5; });
  const UnsafeIntervalSet expected{{15, 25}, {35, 45}};
  EXPECT_EQ(result, expected);
}

TEST(IntervalSetTest, MonotonicTransformScaleUp) {
  const UnsafeIntervalSet initial{{10, 20}, {30, 40}};
  const interval_set_type result =
      initial.MonotonicTransform<int>([](const int x) { return x * 2; });
  const UnsafeIntervalSet expected{{20, 40}, {60, 80}};
  EXPECT_EQ(result, expected);
}

TEST(IntervalSetTest, MonotonicTransformScaleDown) {
  const UnsafeIntervalSet initial{{10, 11}, {13, 14}, {30, 40}};
  const interval_set_type result =
      initial.MonotonicTransform<int>([](const int x) { return x / 2; });
  // Note that {10,11} -> {5,5} (empty) when truncated (integer division),
  // so the result doesn't include it.
  const UnsafeIntervalSet expected{{6, 7}, {15, 20}};
  EXPECT_EQ(result, expected);
}

TEST(IntervalSetTest, MonotonicTransformReflect) {
  const UnsafeIntervalSet initial{{10, 20}, {30, 50}};
  // function is inverting
  const interval_set_type result =
      initial.MonotonicTransform<int>([](const int x) { return 100 - x; });
  const UnsafeIntervalSet expected{{50, 70}, {80, 90}};
  EXPECT_EQ(result, expected);
}

TEST(IntervalSetTest, StreamOutputEmpty) {
  const UnsafeIntervalSet initial{};
  std::ostringstream stream;
  stream << initial;
  EXPECT_EQ(stream.str(), "");
}

TEST(IntervalSetTest, StreamOutputNonEmpty) {
  const UnsafeIntervalSet initial{{3, 5}, {7, 9}};
  std::ostringstream stream;
  stream << initial;
  EXPECT_EQ(stream.str(), "[3, 5), [7, 9)");
}

TEST(ParseInclusivesRangesTest, Empty) {
  const std::initializer_list<absl::string_view> kRanges{};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_TRUE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_TRUE(errstream.str().empty());
  EXPECT_TRUE(iset.empty());
}

TEST(ParseInclusivesRangesTest, ParseErrorSingle) {
  const std::initializer_list<absl::string_view> kRanges{"yyy"};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_FALSE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_FALSE(errstream.str().empty());
  EXPECT_TRUE(iset.empty());
}

TEST(ParseInclusivesRangesTest, ParseErrorRange) {
  const std::initializer_list<absl::string_view> kRanges{"1-x"};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_FALSE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_FALSE(errstream.str().empty());
  EXPECT_TRUE(iset.empty());
}

TEST(ParseInclusivesRangesTest, EmptyString) {
  // Empty string can come from splitting.
  const std::initializer_list<absl::string_view> kRanges{""};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_TRUE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_TRUE(errstream.str().empty());
  EXPECT_TRUE(iset.empty());
  const UnsafeIntervalSet expected{};
  EXPECT_EQ(iset, expected);
}

TEST(ParseInclusivesRangesTest, SingleValues) {
  const std::initializer_list<absl::string_view> kRanges{"1", "3", "4", "5"};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_TRUE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_TRUE(errstream.str().empty());
  const UnsafeIntervalSet expected{{1, 2}, {3, 6}};
  EXPECT_EQ(iset, expected);
}

TEST(ParseInclusivesRangesTest, SingleValuesAndEmpty) {
  const std::initializer_list<absl::string_view> kRanges{"1", "", "4", "5"};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_TRUE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_TRUE(errstream.str().empty());
  const UnsafeIntervalSet expected{{1, 2}, {4, 6}};
  EXPECT_EQ(iset, expected);
}

TEST(ParseInclusivesRangesTest, PairValues) {
  const std::initializer_list<absl::string_view> kRanges{"1-10", "3-11",
                                                         "41-52"};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_TRUE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_TRUE(errstream.str().empty());
  const UnsafeIntervalSet expected{{1, 12}, {41, 53}};
  EXPECT_EQ(iset, expected);
}

TEST(ParseInclusivesRangesTest, PairValuesCustomSeparator) {
  const std::initializer_list<absl::string_view> kRanges{"1:10", "3:11",
                                                         "41:52"};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_TRUE(ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(),
                                   &errstream, ':'));
  EXPECT_TRUE(errstream.str().empty());
  const UnsafeIntervalSet expected{{1, 12}, {41, 53}};
  EXPECT_EQ(iset, expected);
}

TEST(ParseInclusivesRangesTest, MixedValues) {
  const std::initializer_list<absl::string_view> kRanges{"2-10", "11-3", "41",
                                                         "42-52"};
  interval_set_type iset;
  std::ostringstream errstream;
  EXPECT_TRUE(
      ParseInclusiveRanges(&iset, kRanges.begin(), kRanges.end(), &errstream));
  EXPECT_TRUE(errstream.str().empty());
  const UnsafeIntervalSet expected{{2, 12}, {41, 53}};
  EXPECT_EQ(iset, expected);
}

TEST(FormatInclusiveRangesTest, Various) {
  const interval_set_type iset{{2, 4}, {5, 6}, {7, 9}};
  {
    std::ostringstream formatted;
    iset.FormatInclusive(formatted, false);
    EXPECT_EQ(formatted.str(), "2-3,5-5,7-8");
  }
  {
    std::ostringstream formatted;
    iset.FormatInclusive(formatted, true);
    EXPECT_EQ(formatted.str(), "2-3,5,7-8");
  }
}

TEST(UniformRandomGeneratorTest, EmptyInvalid) {
  interval_set_type iset;
  EXPECT_DEATH(iset.UniformRandomGenerator(),
               "Non-empty interval set required");
}

TEST(UniformRandomGeneratorTest, Singleton) {
  interval_set_type iset{{42, 43}};
  const auto gen = iset.UniformRandomGenerator();
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(gen(), 42);
  }
}

TEST(UniformRandomGeneratorTest, SingleRange) {
  interval_set_type iset{{42, 49}};
  const auto gen = iset.UniformRandomGenerator();
  for (int i = 0; i < 20; ++i) {
    const auto sample = gen();
    EXPECT_TRUE(iset.Contains(sample)) << "got: " << sample;
  }
}

TEST(UniformRandomGeneratorTest, MultiRanges) {
  interval_set_type iset{{42, 49}, {99, 104}, {200, 244}};
  const auto gen = iset.UniformRandomGenerator();
  for (int i = 0; i < 100; ++i) {
    const auto sample = gen();
    EXPECT_TRUE(iset.Contains(sample)) << "got: " << sample;
  }
}

// DisjointIntervalSet tests

using IntIntervalSet = DisjointIntervalSet<int>;
using VectorIntervalSet = DisjointIntervalSet<std::vector<int>::const_iterator>;

// Make sure values interior to a range point back to the entire range.
template <typename M>
static void DisjointIntervalConsistencyCheck(const M &iset) {
  // works on integers and iterators
  for (auto iter = iset.begin(); iter != iset.end(); ++iter) {
    for (auto i = iter->first; i != iter->second; ++i) {
      EXPECT_EQ(iset.find(i), iter);
    }
  }
}

TEST(DisjointIntervalSetTest, DefaultCtor) {
  const IntIntervalSet iset;
  EXPECT_TRUE(iset.empty());
}

TEST(DisjointIntervalSetTest, FindEmpty) {
  const IntIntervalSet iset;
  EXPECT_EQ(iset.find(3), iset.end());
}

TEST(DisjointIntervalSetTest, EmplaceOne) {
  IntIntervalSet iset;
  const auto p = iset.emplace(3, 4);
  EXPECT_TRUE(p.second);
  EXPECT_EQ(p.first->first, 3);
  EXPECT_EQ(p.first->second, 4);
  EXPECT_FALSE(iset.empty());
  DisjointIntervalConsistencyCheck(iset);
}

template <typename M>
static typename M::const_iterator VerifyEmplace(M *iset,
                                                typename M::mapped_type min,
                                                typename M::mapped_type max) {
  const auto p = iset->emplace(min, max);
  EXPECT_TRUE(p.second);
  EXPECT_EQ(p.first->first, min);
  EXPECT_EQ(p.first->second, max);
  return p.first;
}

TEST(DisjointIntervalSetTest, EmplaceIterators) {
  VectorIntervalSet iset;
  std::vector<int> vec{{1, 4, 1, 5, 9, 2, 6}};
  VerifyEmplace(&iset, vec.begin() + 3, vec.begin() + 5);
  DisjointIntervalConsistencyCheck(iset);
}

TEST(DisjointIntervalSetTest, EmplaceNonoverlappingAbutting) {
  IntIntervalSet iset;
  const auto iter1 = VerifyEmplace(&iset, 3, 4);
  // insert new leftmost range
  const auto iter2 = VerifyEmplace(&iset, 1, 3);
  // insert new rightmost range
  const auto iter3 = VerifyEmplace(&iset, 4, 7);

  EXPECT_EQ(iset.find(0), iset.end());
  for (int i = 1; i < 3; ++i) {
    EXPECT_EQ(iset.find(i), iter2);
  }
  for (int i = 3; i < 4; ++i) {
    EXPECT_EQ(iset.find(i), iter1);
  }
  for (int i = 4; i < 7; ++i) {
    EXPECT_EQ(iset.find(i), iter3);
  }
  EXPECT_EQ(iset.find(7), iset.end());
  DisjointIntervalConsistencyCheck(iset);
}

TEST(DisjointIntervalSetTest, EmplaceNonoverlappingWithGaps) {
  IntIntervalSet iset;
  const auto iter1 = VerifyEmplace(&iset, 20, 25);
  // insert new leftmost range
  const auto iter2 = VerifyEmplace(&iset, 30, 40);
  // insert new rightmost range
  const auto iter3 = VerifyEmplace(&iset, 10, 15);

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(iset.find(i), iset.end());
  }
  for (int i = 10; i < 15; ++i) {
    EXPECT_EQ(iset.find(i), iter3);
  }
  for (int i = 15; i < 20; ++i) {
    EXPECT_EQ(iset.find(i), iset.end());
  }
  for (int i = 20; i < 25; ++i) {
    EXPECT_EQ(iset.find(i), iter1);
  }
  for (int i = 25; i < 30; ++i) {
    EXPECT_EQ(iset.find(i), iset.end());
  }
  for (int i = 30; i < 40; ++i) {
    EXPECT_EQ(iset.find(i), iter2);
  }
  DisjointIntervalConsistencyCheck(iset);
}

TEST(DisjointIntervalSetTest, EmplaceBackwardsRange) {
  IntIntervalSet iset;
  EXPECT_DEATH(iset.emplace(4, 3), "min_key <= max_key");
}

TEST(DisjointIntervalSetTest, MustEmplaceSuccess) {
  IntIntervalSet iset;
  constexpr std::pair<int, int> kTestValues[] = {
      {3, 4}, {1, 3}, {4, 7}, {-10, -5}, {10, 15},
  };
  for (const auto &t : kTestValues) {
    const auto iter = iset.must_emplace(t.first, t.second);
    // Ensure that inserted value is the expected key min and max.
    EXPECT_EQ(iter->first, t.first);
    EXPECT_EQ(iter->second, t.second);
  }
  DisjointIntervalConsistencyCheck(iset);
}

TEST(DisjointIntervalSetTest, MustEmplaceOverlapLeft) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  EXPECT_DEATH(iset.must_emplace(20, 31), "Failed to emplace");
}

TEST(DisjointIntervalSetTest, MustEmplaceOverlapRight) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  EXPECT_DEATH(iset.must_emplace(39, 45), "Failed to emplace");
}

TEST(DisjointIntervalSetTest, MustEmplaceOverlapInterior) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  EXPECT_DEATH(iset.must_emplace(31, 39), "Failed to emplace");
}

TEST(DisjointIntervalSetTest, MustEmplaceOverlapEnveloped) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  EXPECT_DEATH(iset.must_emplace(29, 40), "Failed to emplace");
}

TEST(DisjointIntervalSetTest, MustEmplaceSpanningTwo) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  iset.must_emplace(50, 60);
  EXPECT_DEATH(iset.must_emplace(35, 55), "Failed to emplace");
}

TEST(DisjointIntervalSetTest, MustEmplaceOverlapsLower) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  iset.must_emplace(50, 60);
  EXPECT_DEATH(iset.must_emplace(35, 45), "Failed to emplace");
}

TEST(DisjointIntervalSetTest, MustEmplaceOverlapsUpper) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  iset.must_emplace(50, 60);
  EXPECT_DEATH(iset.must_emplace(45, 55), "Failed to emplace");
}

TEST(DisjointIntervalSetTest, EraseRange) {
  IntIntervalSet iset;
  iset.must_emplace(30, 40);
  iset.must_emplace(50, 60);

  auto found = iset.find(35);
  EXPECT_NE(found, iset.end());
  iset.erase(found);

  // ... should be gone now.
  found = iset.find(35);
  EXPECT_EQ(found, iset.end());

  found = iset.find(55);
  EXPECT_NE(found, iset.end());
  iset.erase(found);

  EXPECT_TRUE(iset.empty());
}

TEST(DisjointIntervalMapTest, FindInterval) {
  IntIntervalSet iset;
  iset.must_emplace(20, 25);
  for (int i = 19; i < 26; ++i) {
    for (int j = i + 1; j < 26; ++j) {
      const auto found = iset.find({i, j});
      if (i >= 20 && j <= 25) {
        ASSERT_NE(found, iset.end());
        EXPECT_EQ(found->first, 20);
        EXPECT_EQ(found->second, 25);
      } else {
        EXPECT_EQ(found, iset.end());
      }
    }
  }
  DisjointIntervalConsistencyCheck(iset);
}

TEST(DisjointIntervalMapTest, FindVectorInterval) {
  std::vector<int> v(40, 1);  // don't care about values
  VectorIntervalSet iset;
  const auto base = v.begin();
  iset.must_emplace(base + 20, base + 25);
  for (auto i = base + 19; i < base + 26; ++i) {
    for (auto j = i + 1; j < base + 26; ++j) {
      const auto found = iset.find({i, j});
      if (i >= base + 20 && j <= base + 25) {
        ASSERT_NE(found, iset.end());
        EXPECT_EQ(found->first, base + 20);
        EXPECT_EQ(found->second, base + 25);
      } else {
        EXPECT_EQ(found, iset.end());
      }
    }
  }
  DisjointIntervalConsistencyCheck(iset);
}

}  // namespace
}  // namespace verible
