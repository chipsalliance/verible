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

#include "verible/common/util/iterator-adaptors.h"

#include <initializer_list>
#include <list>
#include <set>
#include <type_traits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/iterator-range.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

TEST(MakeReverseIteratorTest, EmptyVector) {
  std::vector<int> v;
  auto rend = verible::make_reverse_iterator(v.begin());
  auto rbegin = verible::make_reverse_iterator(v.end());
  EXPECT_EQ(rbegin, rend);
  EXPECT_EQ(rend, rbegin);
  EXPECT_THAT(make_range(rbegin, rend), ElementsAre());
}

TEST(MakeReverseIteratorTest, NonEmptyVector) {
  std::vector<int> v{7, 8, 9};
  auto rend = verible::make_reverse_iterator(v.begin());
  auto rbegin = verible::make_reverse_iterator(v.end());
  EXPECT_NE(rbegin, rend);
  EXPECT_NE(rend, rbegin);
  EXPECT_THAT(make_range(rbegin, rend), ElementsAre(9, 8, 7));
}

TEST(ReversedViewTest, EmptyVector) {
  std::vector<int> v;
  EXPECT_THAT(reversed_view(v), ElementsAre());
}

TEST(ReversedViewTest, NonEmptyVector) {
  std::vector<int> v{5, 6, 7};
  EXPECT_THAT(reversed_view(v), ElementsAre(7, 6, 5));
}

TEST(ReversedViewTest, EmptyList) {
  std::list<int> v;
  EXPECT_THAT(reversed_view(v), ElementsAre());
}

TEST(ReversedViewTest, NonEmptyList) {
  std::list<int> v{1, 6, 7};
  EXPECT_THAT(reversed_view(v), ElementsAre(7, 6, 1));
}

TEST(ReversedViewTest, EmptyInitList) {
  std::initializer_list<int> v;
  EXPECT_THAT(reversed_view(v), ElementsAre());
}

TEST(ReversedViewTest, NonEmptyInitList) {
  std::initializer_list<int> v{5, 6, 8};
  EXPECT_THAT(reversed_view(v), ElementsAre(8, 6, 5));
}

TEST(ReversedViewTest, EmptySet) {
  std::set<int> v;
  EXPECT_THAT(reversed_view(v), ElementsAre());
}

TEST(ReversedViewTest, NonEmptySet) {
  std::set<int> v{3, 6, 7};
  EXPECT_THAT(reversed_view(v), ElementsAre(7, 6, 3));
}

TEST(ConvertToMutableIteratorTest, Convert) {
  std::vector<int> v{3, 6, 7};
  const std::vector<int>::const_iterator const_it = v.cbegin() + 1;
  auto it = ConvertToMutableIterator(const_it, v.begin());
  static_assert(std::is_same_v<decltype(it), std::vector<int>::iterator>);
  EXPECT_EQ(*it, 6);
  *it = 42;
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(v[1], 42);
}

}  // namespace
}  // namespace verible
