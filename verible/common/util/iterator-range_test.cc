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

#include "verible/common/util/iterator-range.h"

#include <list>
#include <set>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

TEST(IteratorRangeTest, EmptyVector) {
  std::vector<int> v;
  {
    auto range = make_range(v.begin(), v.end());
    EXPECT_EQ(range.begin(), range.end());
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    EXPECT_EQ(range.begin(), range.end());
  }
}

TEST(IteratorRangeTest, EmptyVectorRangeComparison) {
  std::vector<int> v;
  {
    const auto range = make_range(v.begin(), v.end());
    const auto range2 = make_range(v.begin(), v.end());
    EXPECT_TRUE(BoundsEqual(range, range2));
    EXPECT_TRUE(BoundsEqual(range2, range));

    auto crange = make_range(v.cbegin(), v.cend());
    EXPECT_TRUE(BoundsEqual(crange, crange));
    // Testing mixed-iterator-constness
    EXPECT_TRUE(BoundsEqual(range, crange));
    EXPECT_TRUE(BoundsEqual(crange, range));
  }
  {
    const auto range = iterator_range(v.begin(), v.end());
    const auto range2 = iterator_range(v.begin(), v.end());
    EXPECT_TRUE(BoundsEqual(range, range2));
    EXPECT_TRUE(BoundsEqual(range2, range));

    auto crange = iterator_range(v.cbegin(), v.cend());
    EXPECT_TRUE(BoundsEqual(crange, crange));
    // Testing mixed-iterator-constness
    EXPECT_TRUE(BoundsEqual(range, crange));
    EXPECT_TRUE(BoundsEqual(crange, range));
  }
}

TEST(IteratorRangeTest, EmptyList) {
  std::list<int> v;
  {
    auto range = make_range(v.begin(), v.end());
    EXPECT_EQ(range.begin(), range.end());
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    EXPECT_EQ(range.begin(), range.end());
  }
}

TEST(IteratorRangeTest, EmptySet) {
  std::set<int> v;
  {
    auto range = make_range(v.begin(), v.end());
    EXPECT_EQ(range.begin(), range.end());
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    EXPECT_EQ(range.begin(), range.end());
  }
}

TEST(IteratorRangeTest, WholeVectorBeginEnd) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  {
    auto range = make_range(v.begin(), v.end());
    EXPECT_THAT(range, ElementsAreArray(v));
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    EXPECT_THAT(range, ElementsAreArray(v));
  }
}

TEST(IteratorRangeTest, EqualNonemptyVectorRangeComparisons) {
  std::vector<int> v = {3, 5, 11};
  {
    auto range = make_range(v.begin(), v.end());
    auto crange = make_range(v.cbegin(), v.cend());
    EXPECT_TRUE(BoundsEqual(range, crange));
    EXPECT_TRUE(BoundsEqual(crange, range));
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    auto crange = iterator_range(v.cbegin(), v.cend());
    EXPECT_TRUE(BoundsEqual(range, crange));
    EXPECT_TRUE(BoundsEqual(crange, range));
  }
}

TEST(IteratorRangeTest, UnequalVectorRangeComparisons) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  {
    auto range = make_range(v.begin(), v.end());
    auto range2 = make_range(range.begin() + 1, range.end());
    auto range3 = make_range(range.begin(), range.end() - 1);
    EXPECT_FALSE(BoundsEqual(range, range2));
    EXPECT_FALSE(BoundsEqual(range, range3));
    EXPECT_FALSE(BoundsEqual(range2, range));
    EXPECT_FALSE(BoundsEqual(range2, range3));
    EXPECT_FALSE(BoundsEqual(range3, range));
    EXPECT_FALSE(BoundsEqual(range3, range2));
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    auto range2 = iterator_range(range.begin() + 1, range.end());
    auto range3 = iterator_range(range.begin(), range.end() - 1);
    EXPECT_FALSE(BoundsEqual(range, range2));
    EXPECT_FALSE(BoundsEqual(range, range3));
    EXPECT_FALSE(BoundsEqual(range2, range));
    EXPECT_FALSE(BoundsEqual(range2, range3));
    EXPECT_FALSE(BoundsEqual(range3, range));
    EXPECT_FALSE(BoundsEqual(range3, range2));
  }
}

TEST(IteratorRangeTest, WholeListBeginEnd) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  {
    auto range = make_range(v.begin(), v.end());
    EXPECT_THAT(range, ElementsAreArray(v));
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    EXPECT_THAT(range, ElementsAreArray(v));
  }
}

TEST(IteratorRangeTest, WholeSetBeginEnd) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  {
    auto range = make_range(v.begin(), v.end());
    EXPECT_THAT(range, ElementsAreArray(v));
  }
  {
    auto range = iterator_range(v.begin(), v.end());
    EXPECT_THAT(range, ElementsAreArray(v));
  }
}

TEST(IteratorRangeTest, WholeVectorMakePair) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  {
    auto range = make_range(std::make_pair(v.begin(), v.end()));
    EXPECT_THAT(range, ElementsAreArray(v));
  }
  {
    auto range = iterator_range(std::pair(v.begin(), v.end()));
    EXPECT_THAT(range, ElementsAreArray(v));
  }
}

TEST(IteratorRangeTest, PartArray) {
  int v[] = {2, 3, 5, 7, 11, 13};
  {
    iterator_range<int *> range(&v[1], &v[4]);  // 3, 5, 7
    EXPECT_THAT(range, ElementsAre(3, 5, 7));
  }
  {
    iterator_range range(&v[1], &v[4]);  // 3, 5, 7
    EXPECT_THAT(range, ElementsAre(3, 5, 7));
  }
}

TEST(IteratorRangeTest, ArrayMakeRange) {
  int v[] = {2, 3, 5, 7, 11, 13};
  {
    auto range = make_range(&v[1], &v[4]);
    EXPECT_THAT(range, ElementsAre(3, 5, 7));
  }
  {
    auto range = iterator_range(&v[1], &v[4]);
    EXPECT_THAT(range, ElementsAre(3, 5, 7));
  }
}

}  // namespace
}  // namespace verible
