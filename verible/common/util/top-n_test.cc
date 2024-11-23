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

#include "verible/common/util/top-n.h"

#include <algorithm>  // for std::next_permutation
#include <array>
#include <cstddef>
#include <functional>  // for std::less

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

TEST(TopNTest, AnySizeInitiallyEmpty) {
  for (size_t i = 0; i < 3; ++i) {
    TopN<int> values(i);
    EXPECT_EQ(values.max_size(), i);
    EXPECT_EQ(values.size(), 0);
    EXPECT_TRUE(values.empty());
    EXPECT_THAT(values.Take(), ElementsAre());
  }
}

TEST(TopNTest, SizeZeroPush) {
  TopN<int> values(0);
  values.push(1);
  EXPECT_EQ(values.max_size(), 0);
  EXPECT_EQ(values.size(), 0);
  EXPECT_TRUE(values.empty());
  EXPECT_THAT(values.Take(), ElementsAre());
}

TEST(TopNTest, MaxSizeOnePushing) {
  TopN<int> values(1);
  values.push(3);
  EXPECT_EQ(values.max_size(), 1);
  EXPECT_EQ(values.size(), 1);
  EXPECT_FALSE(values.empty());
  EXPECT_THAT(values.Take(), ElementsAre(3));

  values.push(3);  // same value
  EXPECT_THAT(values.Take(), ElementsAre(3));

  values.push(2);  // lesser value
  EXPECT_THAT(values.Take(), ElementsAre(3));

  values.push(4);  // greater value (replaces)
  EXPECT_THAT(values.Take(), ElementsAre(4));
}

TEST(TopNTest, MaxSizeTwoPushingDifferentOrders) {
  // first permutation in increasing order
  std::array<int, 2> incoming{2, 3};
  do {
    // every iteration will push values in a different permutation
    TopN<int> values(2);
    for (const auto v : incoming) {
      values.push(v);
    }
    EXPECT_THAT(values.Take(), ElementsAre(3, 2));
  } while (std::next_permutation(incoming.begin(), incoming.end()));
}

TEST(TopNTest, MaxSizeThreePushingDifferentOrders) {
  // first permutation in increasing order, 5! = 120 permutations
  std::array<int, 5> incoming{1, 2, 3, 5, 8};
  do {
    // every iteration will push values in a different permutation
    TopN<int> values(3);
    for (const auto v : incoming) {
      values.push(v);
    }
    EXPECT_THAT(values.Take(), ElementsAre(8, 5, 3));
  } while (std::next_permutation(incoming.begin(), incoming.end()));
}

TEST(TopNTest, MaxSizeThreeSmallestWins) {
  // first permutation in increasing order, 5! = 120 permutations
  std::array<int, 5> incoming{1, 2, 3, 5, 8};
  do {
    // every iteration will push values in a different permutation
    TopN<int, std::less<int>> values(3);  // using smallest as best
    for (const auto v : incoming) {
      values.push(v);
    }
    EXPECT_THAT(values.Take(), ElementsAre(1, 2, 3));
  } while (std::next_permutation(incoming.begin(), incoming.end()));
}

}  // namespace
}  // namespace verible
