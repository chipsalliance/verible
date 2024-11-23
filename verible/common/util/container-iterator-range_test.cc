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

#include "verible/common/util/container-iterator-range.h"

#include <list>
#include <set>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

TEST(ContainerIteratorRangeTest, EmptyVector) {
  std::vector<int> v;
  auto range = make_container_range(v.begin(), v.end());
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
}

TEST(ContainerIteratorRangeTest, EmptyVectorRangeComparison) {
  std::vector<int> v;
  const auto range = make_container_range(v.begin(), v.end());
  const auto range2 = make_container_range(v.begin(), v.end());
  EXPECT_EQ(range, range2);
  EXPECT_EQ(range2, range);

  auto crange = make_container_range(v.cbegin(), v.cend());
  EXPECT_EQ(crange, crange);
  // Testing mixed-iterator-constness
  EXPECT_EQ(range, crange);
  EXPECT_EQ(crange, range);
}

TEST(ContainerIteratorRangeTest, EmptyList) {
  std::list<int> v;
  auto range = make_container_range(v.begin(), v.end());
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
}

TEST(ContainerIteratorRangeTest, EmptySet) {
  std::set<int> v;
  auto range = make_container_range(v.begin(), v.end());
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
}

TEST(ContainerIteratorRangeTest, VectorExtendBack) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.begin());  // empty
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_back();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 2);
  range.extend_back();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 2);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 3);
}

TEST(ContainerIteratorRangeTest, VectorExtendFront) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.end(), v.end());  // empty
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_front();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(range.front(), 13);
  EXPECT_EQ(range.back(), 13);
  range.extend_front();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 2);
  EXPECT_EQ(range.front(), 11);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, WholeVectorBeginEnd) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 6);
  EXPECT_THAT(range, ElementsAreArray(v));
  EXPECT_EQ(range[0], 2);
  EXPECT_EQ(range[5], 13);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, WholeVectorDequeOperations) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.pop_front();
  EXPECT_EQ(range.front(), 3);
  EXPECT_EQ(range.size(), 5);
  range.pop_back();
  EXPECT_EQ(range.back(), 11);
  EXPECT_EQ(range.size(), 4);
  range.extend_front();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.size(), 5);
  range.extend_back();
  EXPECT_EQ(range.back(), 13);
  EXPECT_EQ(range.size(), 6);
}

TEST(ContainerIteratorRangeTest, EqualNonemptyVectorRangeComparisons) {
  std::vector<int> v = {3, 5, 11};
  auto range = make_container_range(v.begin(), v.end());
  auto crange = make_container_range(v.cbegin(), v.cend());
  EXPECT_EQ(range, range);
  EXPECT_EQ(range, crange);
  EXPECT_EQ(crange, range);
  EXPECT_EQ(crange, crange);
}

TEST(ContainerIteratorRangeTest, UnequalVectorRangeComparisons) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  auto range2(range);
  auto range3(range);
  range2.pop_front();
  range3.pop_back();
  EXPECT_NE(range, range2);
  EXPECT_NE(range, range3);
  EXPECT_NE(range2, range);
  EXPECT_NE(range2, range3);
  EXPECT_NE(range3, range);
  EXPECT_NE(range3, range2);
}

TEST(ContainerIteratorRangeTest, VectorClearToBegin) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.clear_to_begin();
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_back();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 2);
  range.extend_back();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 3);
}

TEST(ContainerIteratorRangeTest, VectorClearToEnd) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.clear_to_end();
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_front();
  EXPECT_EQ(range.front(), 13);
  EXPECT_EQ(range.back(), 13);
  range.extend_front();
  EXPECT_EQ(range.front(), 11);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, VectorSetBegin) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  const auto iter = v.begin() + 2;
  range.set_begin(iter);
  EXPECT_EQ(range.begin(), iter);
}

TEST(ContainerIteratorRangeTest, VectorSetEnd) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  const auto iter = v.begin() + 3;
  range.set_end(iter);
  EXPECT_EQ(range.end(), iter);
}

TEST(ContainerIteratorRangeTest, ListExtendBack) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.begin());  // empty
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_back();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 2);
  range.extend_back();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 2);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 3);
}

TEST(ContainerIteratorRangeTest, ListExtendFront) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.end(), v.end());  // empty
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_front();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(range.front(), 13);
  EXPECT_EQ(range.back(), 13);
  range.extend_front();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 2);
  EXPECT_EQ(range.front(), 11);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, WholeListBeginEnd) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 6);
  EXPECT_THAT(range, ElementsAreArray(v));
  // no random-access iterator
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, WholeListDequeOperations) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.pop_front();
  EXPECT_EQ(range.front(), 3);
  EXPECT_EQ(range.size(), 5);
  range.pop_back();
  EXPECT_EQ(range.back(), 11);
  EXPECT_EQ(range.size(), 4);
  range.extend_front();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.size(), 5);
  range.extend_back();
  EXPECT_EQ(range.back(), 13);
  EXPECT_EQ(range.size(), 6);
}

TEST(ContainerIteratorRangeTest, ListClearToBegin) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.clear_to_begin();
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_back();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 2);
  range.extend_back();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 3);
}

TEST(ContainerIteratorRangeTest, ListClearToEnd) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.clear_to_end();
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_front();
  EXPECT_EQ(range.front(), 13);
  EXPECT_EQ(range.back(), 13);
  range.extend_front();
  EXPECT_EQ(range.front(), 11);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, ListSetBegin) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  auto iter = v.begin();
  ++iter;
  range.set_begin(iter);
  EXPECT_EQ(range.begin(), iter);
}

TEST(ContainerIteratorRangeTest, ListSetEnd) {
  std::list<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  auto iter = v.begin();
  ++iter;
  range.set_end(iter);
  EXPECT_EQ(range.end(), iter);
}

TEST(ContainerIteratorRangeTest, SetExtendBack) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.begin());  // empty
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_back();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 2);
  range.extend_back();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 2);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 3);
}

TEST(ContainerIteratorRangeTest, SetExtendFront) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.end(), v.end());  // empty
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_front();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(range.front(), 13);
  EXPECT_EQ(range.back(), 13);
  range.extend_front();
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 2);
  EXPECT_EQ(range.front(), 11);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, WholeSetBeginEnd) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 6);
  EXPECT_THAT(range, ElementsAreArray(v));
  // no random-access iterator
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, WholeSetDequeOperations) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.pop_front();
  EXPECT_EQ(range.front(), 3);
  EXPECT_EQ(range.size(), 5);
  range.pop_back();
  EXPECT_EQ(range.back(), 11);
  EXPECT_EQ(range.size(), 4);
  range.extend_front();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.size(), 5);
  range.extend_back();
  EXPECT_EQ(range.back(), 13);
  EXPECT_EQ(range.size(), 6);
}

TEST(ContainerIteratorRangeTest, SetClearToBegin) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.clear_to_begin();
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_back();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 2);
  range.extend_back();
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 3);
}

TEST(ContainerIteratorRangeTest, SetClearToEnd) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  range.clear_to_end();
  EXPECT_TRUE(range.empty());
  EXPECT_EQ(range.size(), 0);
  range.extend_front();
  EXPECT_EQ(range.front(), 13);
  EXPECT_EQ(range.back(), 13);
  range.extend_front();
  EXPECT_EQ(range.front(), 11);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, SetSetBegin) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  auto iter = v.begin();
  ++iter;
  range.set_begin(iter);
  EXPECT_EQ(range.begin(), iter);
}

TEST(ContainerIteratorRangeTest, SetSetEnd) {
  std::set<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(v.begin(), v.end());
  auto iter = v.begin();
  ++iter;
  range.set_end(iter);
  EXPECT_EQ(range.end(), iter);
}

TEST(ContainerIteratorRangeTest, WholeVectorMakePair) {
  std::vector<int> v = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(std::make_pair(v.begin(), v.end()));
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 6);
  EXPECT_THAT(range, ElementsAreArray(v));
  EXPECT_EQ(range[0], 2);
  EXPECT_EQ(range[5], 13);
  EXPECT_EQ(range.front(), 2);
  EXPECT_EQ(range.back(), 13);
}

TEST(ContainerIteratorRangeTest, PartArray) {
  int v[] = {2, 3, 5, 7, 11, 13};
  container_iterator_range<int *> range(&v[1], &v[4]);  // 3, 5, 7
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 3);
  EXPECT_THAT(range, ElementsAre(3, 5, 7));
  EXPECT_EQ(range[0], 3);
  EXPECT_EQ(range[1], 5);
  EXPECT_EQ(range.front(), 3);
  EXPECT_EQ(range.back(), 7);
}

TEST(ContainerIteratorRangeTest, ArrayMakeRange) {
  int v[] = {2, 3, 5, 7, 11, 13};
  auto range = make_container_range(&v[1], &v[4]);
  EXPECT_FALSE(range.empty());
  EXPECT_EQ(range.size(), 3);
  EXPECT_THAT(range, ElementsAre(3, 5, 7));
  EXPECT_EQ(range[0], 3);
  EXPECT_EQ(range[1], 5);
  EXPECT_EQ(range.front(), 3);
  EXPECT_EQ(range.back(), 7);
}

}  // namespace
}  // namespace verible
