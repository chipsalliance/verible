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

#include "verible/common/util/range.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/util/iterator-range.h"

namespace verible {
namespace {

// Test that IsSubRange matches same empty string.
TEST(IsSubRangeTest, SameEmptyString) {
  const std::string_view substring;
  EXPECT_TRUE(IsSubRange(substring, substring));
}

// Test that IsSubRange matches same nonempty string.
TEST(IsSubRangeTest, SameNonEmptyString) {
  const std::string_view substring = "nonempty";
  EXPECT_TRUE(IsSubRange(substring, substring));
}

// Test that IsSubRange works on string and string_view.
TEST(IsSubRangeTest, MixedStringViewStdString) {
  const std::string superstring("nonempty");
  const std::string_view substring = std::string_view(superstring).substr(1, 3);
  // Note: std::string and std::string_view iterators are not directly
  // comparable to each other.
  EXPECT_TRUE(IsSubRange(substring, std::string_view(superstring)));
}

// Test that IsSubRange works on string iterators.
TEST(IsSubRangeTest, StringAndIterator) {
  const std::string superstring("asdasdjasd");
  const auto substring =
      make_range(superstring.begin() + 1, superstring.begin() + 3);
  EXPECT_TRUE(IsSubRange(substring, superstring));
}

// Test that IsSubRange is false on completely different string_views.
TEST(IsSubRangeTest, DifferentStringViews) {
  const std::string_view a = "twiddle-dee";
  const std::string_view b = "twiddle-dum";
  EXPECT_FALSE(IsSubRange(a, b));
  EXPECT_FALSE(IsSubRange(b, a));
}

// Test that IsSubRange detects non-overlapping string locations.
TEST(IsSubRangeTest, IdenticalSeparateStrings) {
  const std::string superstring = "a";
  const std::string substring = "a";
  EXPECT_FALSE(IsSubRange(substring, superstring));
  // clang-tidy is really good in recognizing that this is suspicious,
  // deduced from the naming of the parameters.
  // clang-format off
  EXPECT_FALSE(IsSubRange(superstring, substring));  // NOLINT(readability-suspicious-call-argument)
  // clang-format on
}

// Test that IsSubRange matches sub-string_view.
TEST(IsSubRangeTest, SubStringView) {
  const std::string_view superstring = "not-empty";
  EXPECT_TRUE(IsSubRange(superstring.substr(0, 0), superstring));  // empty
  EXPECT_TRUE(IsSubRange(superstring.substr(3, 0), superstring));  // empty
  EXPECT_TRUE(IsSubRange(superstring.substr(0), superstring));
  EXPECT_TRUE(IsSubRange(superstring, superstring.substr(0)));
  EXPECT_TRUE(IsSubRange(superstring.substr(1), superstring));
  EXPECT_TRUE(IsSubRange(superstring.substr(1, 3), superstring));
}

// Test that IsSubRange is false on superstring views (converse).
TEST(IsSubRangeTest, SuperStringView) {
  const std::string_view superstring = "also-nonempty";
  EXPECT_FALSE(IsSubRange(superstring, superstring.substr(1)));
  EXPECT_FALSE(IsSubRange(superstring, superstring.substr(1, 3)));
}

// Test that IsSubRange works on substring ranges.
TEST(IsSubRangeTest, DerivedSubStringView) {
  const std::string_view str = "qwertyuiop";
  EXPECT_FALSE(IsSubRange(str.substr(0, 0), str.substr(1, 0)));  // empty
  EXPECT_FALSE(IsSubRange(str.substr(1, 0), str.substr(0, 0)));  // empty
  EXPECT_TRUE(IsSubRange(str.substr(1, 0), str.substr(0, 1)));
  EXPECT_TRUE(IsSubRange(str.substr(1, 1), str.substr(1)));
  EXPECT_TRUE(IsSubRange(str.substr(1, 1), str.substr(1, 1)));
  EXPECT_FALSE(IsSubRange(str.substr(1, 2), str.substr(3, 2)));  // abutting
  EXPECT_FALSE(IsSubRange(str.substr(3, 2), str.substr(1, 2)));  // abutting
  EXPECT_FALSE(IsSubRange(str.substr(1, 2), str.substr(5, 2)));  // disjoint
  EXPECT_FALSE(IsSubRange(str.substr(5, 2), str.substr(1, 2)));  // disjoint
  EXPECT_FALSE(IsSubRange(str.substr(1, 4), str.substr(3, 4)));  // partial
  EXPECT_FALSE(IsSubRange(str.substr(3, 4), str.substr(1, 4)));  // partial
}

// Test that BoundsEqual matches same empty string.
TEST(BoundsEqualTest, SameEmptyString) {
  const std::string_view substring;
  EXPECT_TRUE(BoundsEqual(substring, substring));
}

// Test that BoundsEqual matches same nonempty string.
TEST(BoundsEqualTest, SameNonEmptyString) {
  const std::string_view substring = "nonempty";
  EXPECT_TRUE(BoundsEqual(substring, substring));
}

// Test that BoundsEqual is false on completely different string_views.
TEST(BoundsEqualTest, DifferentStringViews) {
  const std::string_view a = "twiddle-dee";
  const std::string_view b = "twiddle-dum";
  EXPECT_FALSE(BoundsEqual(a, b));
  EXPECT_FALSE(BoundsEqual(b, a));
}

// Test that BoundsEqual is false on non-overlapping string locations.
TEST(BoundsEqualTest, IdenticalSeparateStrings) {
  const std::string superstring = "a";
  const std::string substring = "a";
  EXPECT_FALSE(BoundsEqual(substring, superstring));
  EXPECT_FALSE(BoundsEqual(superstring, substring));
}

// Test that BoundsEqual matches sub-string_view.
TEST(BoundsEqualTest, SubStringView) {
  const std::string_view superstring = "not-empty";
  EXPECT_FALSE(BoundsEqual(superstring.substr(0, 0), superstring));  // empty
  EXPECT_FALSE(BoundsEqual(superstring.substr(3, 0), superstring));  // empty
  EXPECT_TRUE(BoundsEqual(superstring.substr(0), superstring));
  EXPECT_TRUE(BoundsEqual(superstring, superstring.substr(0)));
  EXPECT_FALSE(BoundsEqual(superstring.substr(1), superstring));
  EXPECT_FALSE(BoundsEqual(superstring.substr(1, 3), superstring));
}

// Test that BoundsEqual is false on superstring views (converse).
TEST(BoundsEqualTest, SuperStringView) {
  const std::string_view superstring = "also-nonempty";
  EXPECT_FALSE(BoundsEqual(superstring, superstring.substr(1)));
  EXPECT_FALSE(BoundsEqual(superstring, superstring.substr(1, 3)));
}

// Test that BoundsEqual works on substring ranges.
TEST(BoundsEqualTest, DerivedSubStringView) {
  const std::string_view str = "qwertyuiop";
  EXPECT_FALSE(BoundsEqual(str.substr(0, 0), str.substr(1, 0)));  // empty
  EXPECT_FALSE(BoundsEqual(str.substr(1, 0), str.substr(0, 0)));  // empty
  EXPECT_FALSE(BoundsEqual(str.substr(1, 0), str.substr(0, 1)));
  EXPECT_FALSE(BoundsEqual(str.substr(1, 1), str.substr(1)));
  EXPECT_TRUE(BoundsEqual(str.substr(2, 0), str.substr(2, 0)));  // empty
  EXPECT_TRUE(BoundsEqual(str.substr(1, 1), str.substr(1, 1)));
  EXPECT_FALSE(BoundsEqual(str.substr(1, 2), str.substr(3, 2)));  // abutting
  EXPECT_FALSE(BoundsEqual(str.substr(3, 2), str.substr(1, 2)));  // abutting
  EXPECT_FALSE(BoundsEqual(str.substr(1, 2), str.substr(5, 2)));  // disjoint
  EXPECT_FALSE(BoundsEqual(str.substr(5, 2), str.substr(1, 2)));  // disjoint
  EXPECT_FALSE(BoundsEqual(str.substr(1, 4), str.substr(3, 4)));  // partial
  EXPECT_FALSE(BoundsEqual(str.substr(3, 4), str.substr(1, 4)));  // partial
}

using IntPair = std::pair<int, int>;

TEST(SubRangeIndicesTest, Empty) {
  const std::vector<int> v;
  const auto supersequence = make_range(v.begin(), v.end());
  const auto subsequence = supersequence;
  EXPECT_EQ(SubRangeIndices(subsequence, supersequence), IntPair(0, 0));
}

TEST(SubRangeIndicesTest, RangeInvariant) {
  const std::vector<int> supersequence(5);
  for (size_t i = 0; i < supersequence.size(); ++i) {
    for (size_t j = i; j < supersequence.size(); ++j) {
      const auto subsequence =
          make_range(supersequence.begin() + i, supersequence.begin() + j);
      EXPECT_EQ(SubRangeIndices(subsequence, supersequence), IntPair(i, j))
          << i << ", " << j;
    }
  }
}

// Tests that swapping subsequence with supersequence fails.
TEST(SubRangeIndicesTest, InsideOut) {
  const std::vector<int> supersequence(5);
  for (size_t i = 0; i < supersequence.size(); ++i) {
    for (size_t j = i; j < supersequence.size(); ++j) {
      const auto subsequence =
          make_range(supersequence.begin() + i, supersequence.begin() + j);
      EXPECT_DEATH(SubRangeIndices(supersequence, subsequence), "")
          << i << ", " << j;
    }
  }
}

TEST(SubRangeIndicesTest, PartialOverlap) {
  const std::vector<int> v(4);
  for (size_t i = 0; i < v.size(); ++i) {
    for (size_t j = 1; j < v.size(); ++j) {
      const auto left = make_range(v.begin(), v.begin() + i);
      const auto right = make_range(v.begin() + j, v.end());
      EXPECT_DEATH(SubRangeIndices(left, right), "") << i << ", " << j;
    }
  }
}

// This tests that range indices are comparable and usable in EXPECT_EQ.
TEST(SubRangeIndicesTest, EqComparable) {
  const std::vector<int> v;
  const auto supersequence = make_range(v.begin(), v.end());
  const auto subsequence = supersequence;
  auto indices = [&v](const decltype(supersequence) &range) {
    return SubRangeIndices(range, v);  // v is the common base for both ranges
  };
  EXPECT_EQ(indices(subsequence), indices(supersequence));
}

}  // namespace
}  // namespace verible
