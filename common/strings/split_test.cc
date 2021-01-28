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

#include "common/strings/split.h"

#include "common/strings/range.h"
#include "common/util/range.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

static void AcceptFunctionChar(std::function<absl::string_view(char)> func) {}
static void AcceptFunctionStringView(
    std::function<absl::string_view(absl::string_view)> func) {}

// This tests that StringSpliterator can be passed to a std::function.
TEST(StringSpliteratorTest, CompileTimeAsFunction) {
  StringSpliterator splitter("a,b,c");
  AcceptFunctionChar(splitter);
  AcceptFunctionStringView(splitter);
}

TEST(StringSpliteratorTest, EmptyOriginal) {
  constexpr absl::string_view empty("");
  StringSpliterator splitter(empty);
  EXPECT_TRUE(splitter);
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), empty));
  const auto token = splitter(",");
  EXPECT_EQ(token, "");
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), empty));
  EXPECT_TRUE(BoundsEqual(token, empty));
  EXPECT_FALSE(splitter);  // reached end
  EXPECT_EQ(splitter(","), "");
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), empty));
}

TEST(StringSpliteratorTest, MyGodItsFullOfStars) {
  constexpr absl::string_view stars("***");
  const auto gen = MakeStringSpliterator(stars, '*');  // char delimiter
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(0, 0)));
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(1, 0)));
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(2, 0)));
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(3, 0)));
}

TEST(StringSpliteratorTest, StringDelimiter) {
  constexpr absl::string_view stars("xxx");
  const auto gen = MakeStringSpliterator(stars, "x");  // string delimiter
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(0, 0)));
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(1, 0)));
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(2, 0)));
  EXPECT_TRUE(BoundsEqual(gen(), stars.substr(3, 0)));
}

TEST(StringSpliteratorTest, StarsAndStripes) {
  constexpr absl::string_view space("==*===*=*====");
  StringSpliterator splitter(space);
  EXPECT_TRUE(splitter);
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), space));

  absl::string_view token = splitter('*');
  EXPECT_TRUE(splitter);
  EXPECT_TRUE(BoundsEqual(token, space.substr(0, 2)))
      << " got \"" << token << '"';
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), space.substr(3)));

  token = splitter('*');
  EXPECT_TRUE(splitter);
  EXPECT_TRUE(BoundsEqual(token, space.substr(3, 3)))
      << " got \"" << token << '"';
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), space.substr(7)));

  token = splitter('*');
  EXPECT_TRUE(splitter);
  EXPECT_TRUE(BoundsEqual(token, space.substr(7, 1)))
      << " got \"" << token << '"';
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), space.substr(9)));

  token = splitter('*');
  EXPECT_FALSE(splitter);  // this was the last token
  EXPECT_TRUE(BoundsEqual(token, space.substr(9, 4)))
      << " got \"" << token << '"';
  EXPECT_TRUE(BoundsEqual(splitter.Remainder(), space.substr(space.length())));
}

TEST(StringSpliteratorTest, InSpaceNoOneCanHearYouScream) {
  constexpr absl::string_view space("  *   * *    ");
  const auto gen = MakeStringSpliterator(space, '*');  // char delimiter
  // expect to match the spaces between the stars
  EXPECT_TRUE(BoundsEqual(gen(), space.substr(0, 2)));
  EXPECT_TRUE(BoundsEqual(gen(), space.substr(3, 3)));
  EXPECT_TRUE(BoundsEqual(gen(), space.substr(7, 1)));
  EXPECT_TRUE(BoundsEqual(gen(), space.substr(9, 4)));
}

TEST(StringSpliteratorTest, CommaBabyCommaOverBaby) {
  constexpr absl::string_view csv_row("abcd,,efg,hi");
  const auto gen = MakeStringSpliterator(csv_row, ",");  // string delimiter
  // expect to match the spaces between the stars
  EXPECT_TRUE(BoundsEqual(gen(), csv_row.substr(0, 4)));
  EXPECT_TRUE(BoundsEqual(gen(), csv_row.substr(5, 0)));
  EXPECT_TRUE(BoundsEqual(gen(), csv_row.substr(6, 3)));
  EXPECT_TRUE(BoundsEqual(gen(), csv_row.substr(10, 2)));
}

using IntPair = std::pair<int, int>;

// For testing purposes, directly compare the substring indices,
// which is a stronger check than string contents comparison.
static std::vector<IntPair> SplitLinesToOffsets(absl::string_view text) {
  std::vector<IntPair> offsets;
  for (const auto& line : SplitLines(text)) {
    offsets.push_back(SubstringOffsets(line, text));
  }
  return offsets;
}

TEST(SplitLinesTest, Empty) {
  constexpr absl::string_view text("");
  const auto lines = SplitLines(text);
  EXPECT_TRUE(lines.empty());
}

TEST(SplitLinesTest, OneSpace) {
  constexpr absl::string_view text(" ");
  EXPECT_THAT(SplitLines(text), ElementsAre(" "));
  EXPECT_THAT(SplitLinesToOffsets(text), ElementsAre(IntPair(0, 1)));
}

TEST(SplitLinesTest, OneBlankLine) {
  constexpr absl::string_view text("\n");
  EXPECT_THAT(SplitLines(text), ElementsAre(""));
  EXPECT_THAT(SplitLinesToOffsets(text), ElementsAre(IntPair(0, 0)));
}

TEST(SplitLinesTest, BlankLines) {
  constexpr absl::string_view text("\n\n\n");
  EXPECT_THAT(SplitLines(text), ElementsAre("", "", ""));
  EXPECT_THAT(SplitLinesToOffsets(text),
              ElementsAre(IntPair(0, 0), IntPair(1, 1), IntPair(2, 2)));
}

TEST(SplitLinesTest, NonBlankLines) {
  constexpr absl::string_view text("a\nbc\ndef\n");
  EXPECT_THAT(SplitLines(text), ElementsAre("a", "bc", "def"));
  EXPECT_THAT(SplitLinesToOffsets(text),
              ElementsAre(IntPair(0, 1), IntPair(2, 4), IntPair(5, 8)));
}

TEST(SplitLinesTest, NonBlankLinesUnterminated) {
  constexpr absl::string_view text("abc\nde\nf");  // no \n at the end
  EXPECT_THAT(SplitLines(text), ElementsAre("abc", "de", "f"));
  EXPECT_THAT(SplitLinesToOffsets(text),
              ElementsAre(IntPair(0, 3), IntPair(4, 6), IntPair(7, 8)));
}

}  // namespace
}  // namespace verible
