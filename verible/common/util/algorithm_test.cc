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

#include "verible/common/util/algorithm.h"

#include <cstddef>
#include <iterator>  // for std::back_inserter
#include <list>
#include <string>
#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/iterator-range.h"

namespace verible {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

// Heterogeneous compare function for testing.
// Returns < 0 if i is considered "less than" c,
// > 0 if i is considered "greater than" c,
// otherwise 0 if they are considered equal.
int CharCompare(int i, char c) {
  return i - (c - 'a' + 1);  // Let 'a' correspond to 1, etc.
}

TEST(SetSymmetricDifferenceSplitTest, EmptyInputs) {
  std::vector<int> seq1, diff1;
  std::string_view seq2;
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_TRUE(diff1.empty());
  EXPECT_TRUE(diff2.empty());
}

TEST(SetSymmetricDifferenceSplitTest, EmptyInputsPreallocatedOutput) {
  std::vector<int> seq1, diff1(3);
  std::string_view seq2;
  std::string diff2(3, 'x');
  auto p = set_symmetric_difference_split(
      seq1.begin(), seq1.end(), seq2.begin(), seq2.end(), diff1.begin(),
      diff2.begin(), &CharCompare);
  EXPECT_TRUE(p.first == diff1.begin());
  EXPECT_TRUE(p.second == diff2.begin());
}

TEST(SetSymmetricDifferenceSplitTest, FirstSequenceEmpty) {
  std::vector<int> seq1, diff1;
  std::string_view seq2("ace");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_TRUE(diff1.empty());
  EXPECT_EQ(diff2, seq2);
}

TEST(SetSymmetricDifferenceSplitTest, FirstSequenceEmptyPreallocatedOutput) {
  std::vector<int> seq1, diff1(3);
  std::string_view seq2("ace");
  std::string diff2(4, 'x');
  auto p = set_symmetric_difference_split(
      seq1.begin(), seq1.end(), seq2.begin(), seq2.end(), diff1.begin(),
      diff2.begin(), &CharCompare);
  EXPECT_TRUE(p.first == diff1.begin());
  EXPECT_TRUE(p.second == diff2.begin() + 3);
  EXPECT_THAT(make_range(diff2.begin(), p.second), ElementsAreArray(seq2));
}

TEST(SetSymmetricDifferenceSplitTest, SecondSequenceEmpty) {
  std::vector<int> seq1({2, 4, 6}), diff1;
  std::string_view seq2;
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_THAT(diff1, ElementsAreArray(seq1));
  EXPECT_TRUE(diff2.empty());
}

TEST(SetSymmetricDifferenceSplitTest, SecondSequenceEmptyPreallocatedOutput) {
  std::vector<int> seq1({2, 4, 6}), diff1(3);
  std::string_view seq2;
  std::string diff2(3, 'x');
  auto p = set_symmetric_difference_split(
      seq1.begin(), seq1.end(), seq2.begin(), seq2.end(), diff1.begin(),
      diff2.begin(), &CharCompare);
  EXPECT_TRUE(p.first == diff1.begin() + 3);
  EXPECT_TRUE(p.second == diff2.begin());
  EXPECT_THAT(make_range(diff1.begin(), p.first), ElementsAreArray(seq1));
}

TEST(SetSymmetricDifferenceSplitTest, CompleteMatch) {
  std::vector<int> seq1({2, 4, 6}), diff1;
  std::string_view seq2("bdf");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_TRUE(diff1.empty());
  EXPECT_TRUE(diff2.empty());
}

TEST(SetSymmetricDifferenceSplitTest, CompleteMatchPreallocatedOutput) {
  std::vector<int> seq1({2, 4, 6}), diff1(3);
  std::string_view seq2("bdf");
  std::string diff2(3, 'x');
  auto p = set_symmetric_difference_split(
      seq1.begin(), seq1.end(), seq2.begin(), seq2.end(), diff1.begin(),
      diff2.begin(), &CharCompare);
  EXPECT_TRUE(p.first == diff1.begin());
  EXPECT_TRUE(p.second == diff2.begin());
}

TEST(SetSymmetricDifferenceSplitTest, CompleteMismatchInterleaved) {
  std::vector<int> seq1({3, 5, 7}), diff1;
  std::string_view seq2("bdf");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_THAT(diff1, ElementsAreArray(seq1));
  EXPECT_EQ(diff2, seq2);
}

TEST(SetSymmetricDifferenceSplitTest, CompleteMismatchNonoverlapping1) {
  std::vector<int> seq1({7, 8, 9}), diff1;
  std::string_view seq2("bdf");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_THAT(diff1, ElementsAreArray(seq1));
  EXPECT_EQ(diff2, seq2);
}

TEST(SetSymmetricDifferenceSplitTest, CompleteMismatchNonoverlapping2) {
  std::vector<int> seq1({1, 2, 4}), diff1;
  std::string_view seq2("xyz");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_THAT(diff1, ElementsAreArray(seq1));
  EXPECT_EQ(diff2, seq2);
}

TEST(SetSymmetricDifferenceSplitTest, PartialMatch1) {
  std::vector<int> seq1({2, 3, 6}), diff1;
  std::string_view seq2("bdf");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_THAT(diff1, ElementsAre(3));
  EXPECT_THAT(diff2, ElementsAre('d'));
}

TEST(SetSymmetricDifferenceSplitTest, PartialMatch1PreallocatedOutput) {
  std::vector<int> seq1({2, 3, 6}), diff1(3);
  std::string_view seq2("bdf");
  std::string diff2(3, 'x');
  auto p = set_symmetric_difference_split(
      seq1.begin(), seq1.end(), seq2.begin(), seq2.end(), diff1.begin(),
      diff2.begin(), &CharCompare);
  EXPECT_TRUE(p.first == diff1.begin() + 1);
  EXPECT_TRUE(p.second == diff2.begin() + 1);
  EXPECT_THAT(make_range(diff1.begin(), p.first), ElementsAre(3));
  EXPECT_THAT(make_range(diff2.begin(), p.second), ElementsAre('d'));
}

TEST(SetSymmetricDifferenceSplitTest, PartialMatch2) {
  std::vector<int> seq1({1, 4, 5}), diff1;
  std::string_view seq2("bdf");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_THAT(diff1, ElementsAre(1, 5));
  EXPECT_THAT(diff2, ElementsAre('b', 'f'));
}

TEST(SetSymmetricDifferenceSplitTest, CompleteSubset) {
  std::vector<int> seq1({2, 4, 6}), diff1;
  std::string_view seq2("bcdf");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_TRUE(diff1.empty());
  EXPECT_THAT(diff2, ElementsAre('c'));
}

TEST(SetSymmetricDifferenceSplitTest, CompleteSubset2) {
  std::vector<int> seq1({2, 4, 5, 6}), diff1;
  std::string_view seq2("bdf");
  std::string diff2;
  set_symmetric_difference_split(seq1.begin(), seq1.end(), seq2.begin(),
                                 seq2.end(), std::back_inserter(diff1),
                                 std::back_inserter(diff2), &CharCompare);
  EXPECT_THAT(diff1, ElementsAre(5));
  EXPECT_TRUE(diff2.empty());
}

TEST(FindAllTest, EmptyRangeTruePredicate) {
  const std::vector<int> seq;  // empty
  std::vector<std::vector<int>::const_iterator> bounds;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds),
           [](int) { return true; });
  EXPECT_TRUE(bounds.empty());
}

TEST(FindAllTest, EmptyRangeFalsePredicate) {
  const std::vector<int> seq;  // empty
  std::vector<std::vector<int>::const_iterator> bounds;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds),
           [](int) { return false; });
  EXPECT_TRUE(bounds.empty());
}

TEST(FindAllTest, PartitionEveryElement) {
  const std::vector<int> seq({6, 5, 4, 3, 2});
  std::vector<std::vector<int>::const_iterator> bounds;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds),
           [](int) { return true; });
  const auto b = seq.begin();
  EXPECT_THAT(bounds, ElementsAre(b, b + 1, b + 2, b + 3, b + 4));
}

TEST(FindAllTest, PartitionNoElements) {
  const std::vector<int> seq({6, 5, 4, 3, 2});
  std::vector<std::vector<int>::const_iterator> bounds;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds),
           [](int) { return false; });
  EXPECT_TRUE(bounds.empty());
}

TEST(FindAllTest, PartitionAtEveryZero) {
  const std::vector<int> seq({0, 6, 5, 0, 4, 3, 2, 0});
  std::vector<std::vector<int>::const_iterator> bounds;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds),
           [](int i) { return i == 0; });
  const auto b = seq.begin();
  EXPECT_THAT(bounds, ElementsAre(b, b + 3, b + 7));
}

TEST(FindAllTest, PartitionAfterEveryZero) {
  const std::vector<int> seq({0, 6, 5, 0, 4, 3, 2, 0});
  std::vector<std::vector<int>::const_iterator> bounds;
  // This demonstrates using a stateful predicate.
  int prev = -1;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds), [&](int i) {
    // Split *after* each zero.
    const bool p = prev == 0;
    prev = i;
    return p;
  });
  const auto b = seq.begin();
  EXPECT_THAT(bounds, ElementsAre(b + 1, b + 4));
  // Last 0 is not evaluated because it is right before the end().
  EXPECT_EQ(prev, 0);  // the last value seen
}

TEST(FindAllTest, PartitionAfterEveryZero2) {
  const std::vector<int> seq({6, 0, 5, 0, 4, 3, 2, 0, 1});
  std::vector<std::vector<int>::const_iterator> bounds;
  // This demonstrates using a stateful predicate.
  int prev = -1;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds), [&](int i) {
    // Split *after* each zero.
    const bool p = prev == 0;
    prev = i;
    return p;
  });
  const auto b = seq.begin();
  EXPECT_THAT(bounds, ElementsAre(b + 2, b + 4, b + 8));
  EXPECT_EQ(prev, 1);  // the last value seen
}

TEST(FindAllTest, StrSplitAtEqual) {
  const std::string_view seq("aa=b=cc");
  std::vector<std::string_view::const_iterator> bounds;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds),
           [](char i) { return i == '='; });
  const auto b = seq.begin();
  EXPECT_THAT(bounds, ElementsAre(b + 2, b + 4));
}

TEST(FindAllTest, StrSplitAfterEqual) {
  const std::string_view seq("aa=b=cd");
  std::vector<std::string_view::const_iterator> bounds;
  char prev = 0;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds), [&](char i) {
    // Split *after* each '='.
    const bool p = (prev == '=');
    prev = i;
    return p;
  });
  const auto b = seq.begin();
  EXPECT_THAT(bounds, ElementsAre(b + 3, b + 5));
  EXPECT_EQ(prev, 'd');  // the last value seen
}

// generator that returns infinite sequence: {[false]*(N-1), true}*
class EveryN {
 public:
  explicit EveryN(int n, int init = 0) : i_(init), n_(n) {}

  template <class T>
  bool operator()(const T &) {
    const bool ret = (i_ == n_);
    if (ret) i_ = 0;
    ++i_;
    return ret;
  }

 private:
  int i_;
  const int n_;
};

TEST(FindAllTest, StrSplitEveryN) {
  const std::string_view seq("xxxxyyyyaaaabbb");
  std::vector<std::string_view::const_iterator> bounds;
  find_all(seq.begin(), seq.end(), std::back_inserter(bounds), EveryN(4));
  const auto b = seq.begin();
  EXPECT_THAT(bounds, ElementsAre(b + 4, b + 8, b + 12));
}

TEST(LexicographicalLessTest, Containers) {
  LexicographicalLess comp;
  using List = std::list<int>;
  using Vector = std::vector<int>;
  EXPECT_FALSE(comp(List{}, List{}));
  EXPECT_FALSE(comp(Vector{}, Vector{}));
  EXPECT_FALSE(comp(Vector{}, List{}));
  EXPECT_FALSE(comp(List{}, Vector{}));
  EXPECT_TRUE(comp(Vector{}, List{1}));
  EXPECT_FALSE(comp(Vector{0}, List{}));
  EXPECT_FALSE(comp(Vector{0}, List{0}));
  EXPECT_FALSE(comp(Vector{4}, List{4}));
  EXPECT_TRUE(comp(List{1}, Vector{2}));
  EXPECT_TRUE(comp(List{1}, Vector{1, -1}));
  EXPECT_TRUE(comp(List{1}, List{1, -1}));
  EXPECT_FALSE(comp(List{1, 0}, List{1, -1}));
  EXPECT_TRUE(comp(List{1, -2}, List{1, -1}));
}

// like substr()
template <class T>
iterator_range<typename T::const_iterator> make_subrange(const T &t, size_t b,
                                                         size_t e) {
  return make_range(t.begin() + b, t.begin() + e);
}

TEST(LexicographicalLessTest, Ranges) {
  LexicographicalLess comp;
  const std::vector<int> v{1, 0, 2, 1, 3};
  EXPECT_FALSE(comp(make_subrange(v, 0, 0), make_subrange(v, 0, 0)));
  EXPECT_FALSE(comp(make_subrange(v, 1, 1), make_subrange(v, 1, 1)));
  EXPECT_FALSE(comp(v, v));
  EXPECT_FALSE(comp(v, make_subrange(v, 0, 1)));
  EXPECT_TRUE(comp(make_subrange(v, 0, 1), v));
  EXPECT_FALSE(comp(v, make_subrange(v, 0, 5)));  // whole vector
  EXPECT_FALSE(comp(make_subrange(v, 0, 5), v));  // whole vector
  EXPECT_FALSE(comp(v, make_subrange(v, 1, 2)));
  EXPECT_TRUE(comp(v, make_subrange(v, 2, 3)));
  EXPECT_FALSE(comp(v, make_subrange(v, 3, 4)));
  EXPECT_TRUE(comp(v, make_subrange(v, 3, 5)));
  EXPECT_FALSE(comp(make_subrange(v, 0, 1), make_subrange(v, 0, 1)));
  EXPECT_FALSE(comp(make_subrange(v, 0, 5), make_subrange(v, 0, 5)));  // whole
  EXPECT_TRUE(comp(make_subrange(v, 1, 5), make_subrange(v, 0, 5)));
  EXPECT_FALSE(comp(make_subrange(v, 0, 5), make_subrange(v, 1, 5)));
  EXPECT_TRUE(comp(make_subrange(v, 0, 5), make_subrange(v, 2, 5)));
  EXPECT_FALSE(comp(make_subrange(v, 2, 5), make_subrange(v, 0, 5)));
  EXPECT_FALSE(comp(make_subrange(v, 0, 1), make_subrange(v, 3, 4)));
  EXPECT_FALSE(comp(make_subrange(v, 3, 4), make_subrange(v, 0, 1)));
}

}  // namespace
}  // namespace verible
