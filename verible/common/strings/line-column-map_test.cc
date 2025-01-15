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

// Tests for LineColumnMap.

#include "verible/common/strings/line-column-map.h"

#include <cstring>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string_view>
#include <vector>

#include "absl/strings/str_split.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

struct LineColumnTestData {
  LineColumn line_col;
  const char *text;
};

struct LineColumnRangeTestData {
  LineColumnRange range;
  const char *text;
};

struct LineColumnQuery {
  int offset;
  LineColumn line_col;
};

struct LineColumnMapTestData {
  const char *text;
  std::vector<int> expected_offsets;
  std::vector<LineColumnQuery> queries;
};

// Raw line and column are 0-indexed.
const LineColumnMapTestData map_test_data[] = {
    // Also testing beyond the end of the file - it should return last position
    {"", {0}, {{0, {0, 0}}, {1, {0, 0}}, {100, {0, 0}}}},  // empty file
    {"_", {0}, {{0, {0, 0}}, {1, {0, 1}}}},                // no \n before EOF
    {"abc", {0}, {{0, {0, 0}}, {2, {0, 2}}, {3, {0, 3}}}},
    {"\n", {0, 1}, {{0, {0, 0}}, {1, {1, 0}}}},  // one empty line
    {"\n\n", {0, 1, 2}, {{0, {0, 0}}, {1, {1, 0}}, {2, {2, 0}}}},
    {"ab\nc", {0, 3}, {{0, {0, 0}}, {2, {0, 2}}, {3, {1, 0}}, {4, {1, 1}}}},
    {"_\n_\n", {0, 2, 4}, {{0, {0, 0}}, {1, {0, 1}}, {2, {1, 0}}, {3, {1, 1}}}},
    {"\nxx\n", {0, 1, 4}, {{0, {0, 0}}, {1, {1, 0}}, {2, {1, 1}}, {3, {1, 2}}}},
    {"hello\ndarkness\nmy old friend\n",
     {0, 6, 15, 29},
     {{0, {0, 0}}, {10, {1, 4}}, {15, {2, 0}}, {20, {2, 5}}}},
    // Multi-byte characters. Let's use strlen() to count the bytes, the
    // column should accurately point to the character.
    {"😀😀😀", {0}, {{static_cast<int>(2 * strlen("😀")), {0, 2}}}},
    {"Heizölrückstoßabdämpfung",
     {0},
     {{static_cast<int>(strlen("Heizölrückstoß")), {0, 14}}}},
};

// Test test verifies that line-column offset appear to the user correctly.
TEST(LineColumnTextTest, PrintLineColumn) {
  static constexpr LineColumnTestData text_test_data[] = {
      {{0, 0}, "1:1"},
      {{0, 1}, "1:2"},
      {{1, 0}, "2:1"},
      {{10, 8}, "11:9"},
  };
  for (const auto &test_case : text_test_data) {
    std::ostringstream oss;
    oss << test_case.line_col;
    EXPECT_EQ(oss.str(), test_case.text);
  }
}

TEST(LineColumnTextTest, PrintLineColumnRange) {
  static constexpr LineColumnRangeTestData text_test_data[] = {
      {{{0, 0}, {0, 7}}, "1:1-7:"},  // Same line, multiple columns
      {{{0, 1}, {0, 3}}, "1:2-3:"},
      {{{1, 0}, {2, 14}}, "2:1:3:14:"},  // start/end different lines
      {{{10, 8}, {11, 2}}, "11:9:12:2:"},
      {{{10, 8}, {10, 9}}, "11:9:"},  // Single character range
      {{{10, 8}, {10, 8}}, "11:9:"},  // Empty range.
  };
  for (const auto &test_case : text_test_data) {
    std::ostringstream oss;
    oss << test_case.range;
    EXPECT_EQ(oss.str(), test_case.text);
  }
}

// Test offset lookup values by line number.
TEST(LineColumnMapTest, OffsetAtLine) {
  LineColumnMap line_map("hello\n\nworld\n");
  EXPECT_EQ(line_map.OffsetAtLine(0), 0);
  EXPECT_EQ(line_map.OffsetAtLine(1), 6);
  EXPECT_EQ(line_map.OffsetAtLine(2), 7);
  EXPECT_EQ(line_map.OffsetAtLine(3), 13);  // There is no line[3].
}

// This test verifies the offsets where columns are reset to 0,
// which happens after every newline.
TEST(LineColumnMapTest, Offsets) {
  for (const auto &test_case : map_test_data) {
    const LineColumnMap line_map(test_case.text);
    EXPECT_EQ(line_map.GetBeginningOfLineOffsets(), test_case.expected_offsets)
        << "Text: \"" << test_case.text << "\"";
  }
}

// This tests that computing offsets from pre-split lines is consistent
// with the constructor that takes the whole text string.
TEST(LineColumnMapTest, OffsetsFromLines) {
  for (const auto &test_case : map_test_data) {
    const LineColumnMap line_map(test_case.text);
    std::vector<std::string_view> lines = absl::StrSplit(test_case.text, '\n');
    const LineColumnMap alt_line_map(lines);
    EXPECT_EQ(line_map.GetBeginningOfLineOffsets(),
              alt_line_map.GetBeginningOfLineOffsets())
        << "Text: \"" << test_case.text << "\"";
  }
}

TEST(LineColumnMapTest, EndOffsetNoLines) {
  const std::vector<std::string_view> lines;
  const LineColumnMap map(lines);
  EXPECT_EQ(map.LastLineOffset(), 0);
}

struct EndOffsetTestCase {
  std::string_view text;
  int expected_offset;
};

TEST(LineColumnMapTest, EndOffsetVarious) {
  const EndOffsetTestCase kTestCases[] = {
      {"", 0},             // empty text
      {"aaaa", 0},         // missing EOL
      {"aaaa\nbbb", 5},    // missing EOL
      {"\n", 1},           //
      {"aaaa\n", 5},       //
      {"aaaa\nbbb\n", 9},  //
      {"\n\n", 2},
  };
  for (const auto &test : kTestCases) {
    const LineColumnMap map(test.text);
    EXPECT_EQ(map.LastLineOffset(), test.expected_offset) << "text:\n"
                                                          << test.text;
  }
}

// This test verifies the translation from byte-offset to line-column.
TEST(LineColumnMapTest, Lookup) {
  for (const auto &test_case : map_test_data) {
    const LineColumnMap line_map(test_case.text);
    for (const auto &q : test_case.queries) {
      EXPECT_EQ(q.line_col,
                line_map.GetLineColAtOffset(test_case.text, q.offset))
          << "Text: \"" << test_case.text << "\"\n"
          << "Failed testing offset " << q.offset;
    }
  }
}

TEST(LineColumnTest, LineColumnComparison) {
  constexpr LineColumn before_line{.line = 41, .column = 1};
  constexpr LineColumn before_col{.line = 42, .column = 1};
  constexpr LineColumn center{.line = 42, .column = 8};
  constexpr LineColumn after_col{.line = 42, .column = 8};

  EXPECT_LT(before_line, center);
  EXPECT_LT(before_col, center);
  EXPECT_EQ(center, center);
  EXPECT_GE(center, center);
  EXPECT_GE(after_col, center);
}

TEST(LineColumnTest, LineColumnRangeComparison) {
  constexpr LineColumnRange range{{.line = 42, .column = 17},
                                  {.line = 42, .column = 22}};

  constexpr LineColumn before{.line = 42, .column = 16};
  constexpr LineColumn inside_start{.line = 42, .column = 17};
  constexpr LineColumn inside_end{.line = 42, .column = 21};
  constexpr LineColumn outside_after_end{.line = 42, .column = 22};

  EXPECT_FALSE(range.PositionInRange(before));
  EXPECT_TRUE(range.PositionInRange(inside_start));
  EXPECT_TRUE(range.PositionInRange(inside_end));
  EXPECT_FALSE(range.PositionInRange(outside_after_end));
}
}  // namespace
}  // namespace verible
