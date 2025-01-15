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

// Unit tests for RebaseStringView

#include "verible/common/strings/rebase.h"

#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

// Test that empty string token rebases correctly.
TEST(RebaseStringViewTest, EmptyStringsZeroOffset) {
  const std::string text;
  // We want another empty string, but we need to trick too smart compilers
  // to give us a different memory address.
  std::string substr = "foo";
  substr.resize(0);  // Force empty string such as 'text' but memory space
  ASSERT_NE(text.c_str(), substr.c_str()) << "Mismatch in memory assumption";

  std::string_view text_view(text);
  const std::string_view substr_view(substr);
  EXPECT_FALSE(BoundsEqual(text_view, substr_view));
  RebaseStringView(&text_view, substr);
  EXPECT_TRUE(BoundsEqual(text_view, substr_view));
}

// Test that non-empty whole-string copy rebases correctly.
TEST(RebaseStringViewTest, IdenticalCopy) {
  const std::string text = "hello";
  const std::string substr = "hello";  // different memory space
  std::string_view text_view(text);
  const std::string_view substr_view(substr);
  EXPECT_FALSE(BoundsEqual(text_view, substr_view));
  RebaseStringView(&text_view, substr);
  EXPECT_TRUE(BoundsEqual(text_view, substr_view));
}

// Test that substring mismatch between new and old is checked.
TEST(RebaseStringViewDeathTest, SubstringMismatch) {
  const std::string_view text = "hell0";
  const std::string_view substr = "hello";
  std::string_view text_view(text);
  EXPECT_DEATH(RebaseStringView(&text_view, substr),
               "only valid when the new text referenced matches the old text");
}

TEST(RebaseStringViewDeathTest, SubstringMismatch2) {
  const std::string_view text = "hello";
  const std::string_view substr = "Hello";
  std::string_view text_view(text);
  EXPECT_DEATH(RebaseStringView(&text_view, substr),
               "only valid when the new text referenced matches the old text");
}

// Test that substring in the middle of old string is rebased correctly.
TEST(RebaseStringViewTest, NewSubstringNotAtFront) {
  const std::string_view text = "hello";
  const std::string_view new_base = "xxxhelloyyy";
  const std::string_view new_view(new_base.substr(3, 5));
  std::string_view text_view(text);
  EXPECT_FALSE(BoundsEqual(text_view, new_view));
  RebaseStringView(&text_view, new_view);
  EXPECT_TRUE(BoundsEqual(text_view, new_view));
}

// Test that substring in the middle of old string is rebased correctly.
TEST(RebaseStringViewTest, UsingCharPointer) {
  const std::string_view text = "hello";
  const char *new_base = "xxxhelloyyy";
  const char *new_view_offset = new_base + 3;
  std::string_view text_view(text);
  RebaseStringView(&text_view, new_view_offset);  // assume original length
  const std::string_view new_base_view(new_base);
  EXPECT_TRUE(BoundsEqual(text_view, new_base_view.substr(3, 5)));
}

// Test integration with substr() function rebases correctly.
TEST(RebaseStringViewTest, RelativeToOldBase) {
  const std::string_view full_text = "xxxxxxhelloyyyyy";
  std::string_view substr = full_text.substr(6, 5);
  EXPECT_EQ(substr, "hello");
  const std::string_view new_base = "aahellobbb";
  const std::string_view new_view(new_base.substr(2, substr.length()));
  RebaseStringView(&substr, new_view);
  EXPECT_TRUE(BoundsEqual(substr, new_view));
}

// Test rebasing into middle of superstring.
TEST(RebaseStringViewTest, MiddleOfSuperstring) {
  const std::string_view dest_text = "xxxxxxhell0yyyyy";
  const std::string_view src_text = "ccchell0ddd";
  const int dest_offset = 6;
  std::string_view src_substr(src_text.substr(3, 5));
  EXPECT_EQ(src_substr, "hell0");
  // src_text[3] lines up with dest_text[6].
  const std::string_view dest_view(
      dest_text.substr(dest_offset, src_substr.length()));
  RebaseStringView(&src_substr, dest_view);
  EXPECT_TRUE(BoundsEqual(src_substr, dest_view));
}

// Test rebasing into prefix superstring.
TEST(RebaseStringViewTest, PrefixSuperstring) {
  const std::string_view dest_text = "xxxhell0yyyyyzzzzzzz";
  const std::string_view src_text = "ccchell0ddd";
  const int dest_offset = 3;
  std::string_view src_substr = src_text.substr(3, 5);
  EXPECT_EQ(src_substr, "hell0");
  // src_text[3] lines up with dest_text[3].
  const std::string_view dest_view(
      dest_text.substr(dest_offset, src_substr.length()));
  RebaseStringView(&src_substr, dest_view);
  EXPECT_TRUE(BoundsEqual(src_substr, dest_view));
}

}  // namespace
}  // namespace verible
