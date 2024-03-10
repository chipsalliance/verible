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

#include "common/strings/range.h"

#include <cstddef>
#include <string_view>
#include <utility>

#include "common/util/range.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(MakeStringViewRangeTest, Empty) {
  std::string_view text("");
  auto copy_view = make_string_view_range(text.begin(), text.end());
  EXPECT_TRUE(BoundsEqual(copy_view, text));
}

TEST(MakeStringViewRangeTest, NonEmpty) {
  std::string_view text("I'm not empty!!!!");
  auto copy_view = make_string_view_range(text.begin(), text.end());
  EXPECT_TRUE(BoundsEqual(copy_view, text));
}

TEST(MakeStringViewRangeTest, BadRange) {
  std::string_view text("backwards");
  EXPECT_DEATH(make_string_view_range(text.end(), text.begin()), "Malformed");
}

using IntPair = std::pair<int, int>;

TEST(ByteOffsetRangeTest, EmptyInEmpty) {
  const std::string_view superstring("");
  const auto substring = superstring;
  EXPECT_EQ(SubstringOffsets(substring, superstring), IntPair(0, 0));
}

TEST(ByteOffsetRangeTest, RangeInvariant) {
  const std::string_view superstring("xxxxxxxx");
  for (size_t i = 0; i < superstring.length(); ++i) {
    for (size_t j = i; j < superstring.length(); ++j) {
      const auto substring = superstring.substr(i, j - i);
      EXPECT_EQ(SubstringOffsets(substring, superstring), IntPair(i, j))
          << i << ", " << j;
    }
  }
}

// Tests that swapping substring with superstring fails.
TEST(ByteOffsetRangeTest, InsideOut) {
  const std::string_view superstring("yyyyyyy");
  for (size_t i = 0; i < superstring.length(); ++i) {
    for (size_t j = i; j < superstring.length(); ++j) {
      const auto substring = superstring.substr(i, j - i);
      // clang-tidy is really good in recognizing that this is suspicious,
      // deduced from the naming of the parameters.
      // clang-format off
      EXPECT_DEATH(SubstringOffsets(superstring, substring), "") /* NOLINT(readability-suspicious-call-argument) */
        << i << ", " << j;
      // clang-format on
    }
  }
}

TEST(ByteOffsetRangeTest, PartialOverlap) {
  const std::string_view superstring("zzzz");
  for (size_t i = 0; i < superstring.length(); ++i) {
    for (size_t j = 1; j < superstring.length(); ++j) {
      const auto left = superstring.substr(0, i);
      const auto right = superstring.substr(j);
      EXPECT_DEATH(SubstringOffsets(left, right), "") << i << ", " << j;
    }
  }
}

}  // namespace
}  // namespace  verible
