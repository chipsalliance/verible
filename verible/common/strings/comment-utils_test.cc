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

#include "verible/common/strings/comment-utils.h"

#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

struct TestData {
  std::string_view input;
  const char *expect;
};

// Test that non-comments are left unmodified.
TEST(StripCommentTest, NotComment) {
  constexpr std::string_view test_cases[] = {
      "",
      "/",  // too short to be a comment
      "foo",
      "not a comment",
      " // not a comment, due to leading space",
      " /* not a comment, due to leading space */",
      "*/",
      "/*",   // un-terminated comment
      "/**",  // un-terminated comment
      "*/",   // not a comment
      "**/",  // not a comment
      "/*/",
  };
  for (const auto &data : test_cases) {
    EXPECT_EQ(StripComment(data), data);
    EXPECT_TRUE(IsSubRange(StripComment(data), data));
  }
}

// Test that endline-style comments are trimmed.
TEST(StripCommentTest, EndlineComment) {
  constexpr TestData test_cases[] = {
      {"//", ""},
      {"//\t", "\t"},
      {"//  ", "  "},
      {"/////", ""},
      {"/// ", " "},
      {"//foo", "foo"},
      {"//foo\nabc", "foo\nabc"},
  };
  for (const auto &data : test_cases) {
    EXPECT_EQ(StripComment(data.input), data.expect)
        << "input: \"" << data.input << "\"";
    EXPECT_TRUE(IsSubRange(StripComment(data.input), data.input));
  }
}

// Test that block-style comments are trimmed
TEST(StripCommentTest, BlockComment) {
  constexpr TestData test_cases[] = {
      {"/**/", ""},                  // smallest comment
      {"/*******/", ""},             // "My god, it's full of stars!"
      {"/*  */", "  "},              // spaces only
      {"/*fgh*/", "fgh"},            // text
      {"/*fgh\nijk*/", "fgh\nijk"},  // text
      {"/* zzz */", " zzz "},        // keeps spaces
      {"/**jkl****/", "jkl"},
  };
  for (const auto &data : test_cases) {
    EXPECT_EQ(StripComment(data.input), data.expect)
        << "input: \"" << data.input << "\"";
    EXPECT_TRUE(IsSubRange(StripComment(data.input), data.input));
  }
}

// Test that leading/trailing spaces inside comments are removed.
TEST(StripCommentAndSpacePaddingTest, StripsSpaces) {
  constexpr TestData test_cases[] = {
      {"//", ""},
      {"//\t", ""},
      {"//  ", ""},
      {"/////", ""},
      {"/// ", ""},
      {"//foo", "foo"},
      {"//foo\nabc", "foo\nabc"},
      {"//  bar", "bar"},
      {"//  bar  ", "bar"},
      {"//  foo bar  ", "foo bar"},
      {"//\t\tbar", "bar"},
      {"/**/", ""},
      {"/***/", ""},
      {"/* */", ""},
      {"/*\t*/", ""},
      {"/*\n*/", ""},
      {"/**qqq**/", "qqq"},
      {"/**  qqq  **/", "qqq"},
      {"/**\n\tqqqq\n\t**/", "qqqq"},
      {"/**  qqq bbb.  **/", "qqq bbb."},
      {"/**\n\tqqqq\n\t**/", "qqqq"},
      {"/****qqq bbb.******/", "qqq bbb."},
      {"/****\n** qqq\n** bbb\n******/", "** qqq\n** bbb"},
  };
  for (const auto &data : test_cases) {
    EXPECT_EQ(StripCommentAndSpacePadding(data.input), data.expect)
        << "input: \"" << data.input << "\"";
    EXPECT_TRUE(IsSubRange(StripComment(data.input), data.input));
  }
}

}  // namespace
}  // namespace verible
