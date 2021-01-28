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

#include "common/strings/line_column_map.h"

#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

struct LineColumnTestData {
  LineColumn line_col;
  const char* text;
};

struct LineColumnQuery {
  int offset;
  LineColumn line_col;
};

struct LineColumnMapTestData {
  const char* text;
  std::vector<int> expected_offsets;
  std::vector<LineColumnQuery> queries;
};

const LineColumnTestData text_test_data[] = {
    {{0, 0}, "1:1"},
    {{0, 1}, "1:2"},
    {{1, 0}, "2:1"},
    {{10, 8}, "11:9"},
};

// Raw line and column are 0-indexed.
const LineColumnMapTestData map_test_data[] = {
    {"", {0}, {{0, {0, 0}}, {1, {0, 1}}}},   // empty file
    {"_", {0}, {{0, {0, 0}}, {1, {0, 1}}}},  // no \n before EOF
    {"abc", {0}, {{0, {0, 0}}, {2, {0, 2}}, {3, {0, 3}}}},
    {"\n", {0, 1}, {{0, {0, 0}}, {1, {1, 0}}}},  // one empty line
    {"\n\n", {0, 1, 2}, {{0, {0, 0}}, {1, {1, 0}}, {2, {2, 0}}}},
    {"ab\nc", {0, 3}, {{0, {0, 0}}, {2, {0, 2}}, {3, {1, 0}}, {4, {1, 1}}}},
    {"_\n_\n", {0, 2, 4}, {{0, {0, 0}}, {1, {0, 1}}, {2, {1, 0}}, {3, {1, 1}}}},
    {"\nxx\n", {0, 1, 4}, {{0, {0, 0}}, {1, {1, 0}}, {2, {1, 1}}, {3, {1, 2}}}},
    {"hello\ndarkness\nmy old friend\n",
     {0, 6, 15, 29},
     {{0, {0, 0}}, {10, {1, 4}}, {15, {2, 0}}, {20, {2, 5}}}},
};

// Test test verifies that line-column offset appear to the user correctly.
TEST(LineColumnTextTest, Print) {
  for (const auto& test_case : text_test_data) {
    std::ostringstream oss;
    oss << test_case.line_col;
    EXPECT_EQ(oss.str(), test_case.text);
  }
}

// Test that Clear resets the map.
TEST(LineColumnMapTest, ClearEmpty) {
  LineColumnMap line_map("hello\nworld\n");
  EXPECT_FALSE(line_map.Empty());
  line_map.Clear();
  EXPECT_TRUE(line_map.Empty());
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
  for (const auto& test_case : map_test_data) {
    const LineColumnMap line_map(test_case.text);
    EXPECT_EQ(line_map.GetBeginningOfLineOffsets(), test_case.expected_offsets)
        << "Text: \"" << test_case.text << "\"";
  }
}

// This tests that computing offsets from pre-split lines is consistent
// with the constructor that takes the whole text string.
TEST(LineColumnMapTest, OffsetsFromLines) {
  for (const auto& test_case : map_test_data) {
    const LineColumnMap line_map(test_case.text);
    std::vector<absl::string_view> lines = absl::StrSplit(test_case.text, '\n');
    const LineColumnMap alt_line_map(lines);
    EXPECT_EQ(line_map.GetBeginningOfLineOffsets(),
              alt_line_map.GetBeginningOfLineOffsets())
        << "Text: \"" << test_case.text << "\"";
  }
}

TEST(LineColumnMapTest, EndOffsetNoLines) {
  const std::vector<absl::string_view> lines;
  const LineColumnMap map(lines);
  EXPECT_EQ(map.EndOffset(), 0);
}

struct EndOffsetTestCase {
  absl::string_view text;
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
  for (const auto& test : kTestCases) {
    const LineColumnMap map(test.text);
    EXPECT_EQ(map.EndOffset(), test.expected_offset) << "text:\n" << test.text;
  }
}

// This test verifies the translation from byte-offset to line-column.
TEST(LineColumnMapTest, Lookup) {
  for (const auto& test_case : map_test_data) {
    const LineColumnMap line_map(test_case.text);
    for (const auto& q : test_case.queries) {
      EXPECT_EQ(q.line_col, line_map(q.offset))
          << "Text: \"" << test_case.text << "\"\n"
          << "Failed testing offset " << q.offset;
    }
  }
}

}  // namespace
}  // namespace verible
