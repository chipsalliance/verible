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
      {.input = "//", .expect = ""},
      {.input = "//\t", .expect = "\t"},
      {.input = "//  ", .expect = "  "},
      {.input = "/////", .expect = ""},
      {.input = "/// ", .expect = " "},
      {.input = "//foo", .expect = "foo"},
      {.input = "//foo\nabc", .expect = "foo\nabc"},
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
      {.input = "/**/", .expect = ""},        // smallest comment
      {.input = "/*******/", .expect = ""},   // "My god, it's full of stars!"
      {.input = "/*  */", .expect = "  "},    // spaces only
      {.input = "/*fgh*/", .expect = "fgh"},  // text
      {.input = "/*fgh\nijk*/", .expect = "fgh\nijk"},  // text
      {.input = "/* zzz */", .expect = " zzz "},        // keeps spaces
      {.input = "/**jkl****/", .expect = "jkl"},
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
      {.input = "//", .expect = ""},
      {.input = "//\t", .expect = ""},
      {.input = "//  ", .expect = ""},
      {.input = "/////", .expect = ""},
      {.input = "/// ", .expect = ""},
      {.input = "//foo", .expect = "foo"},
      {.input = "//foo\nabc", .expect = "foo\nabc"},
      {.input = "//  bar", .expect = "bar"},
      {.input = "//  bar  ", .expect = "bar"},
      {.input = "//  foo bar  ", .expect = "foo bar"},
      {.input = "//\t\tbar", .expect = "bar"},
      {.input = "/**/", .expect = ""},
      {.input = "/***/", .expect = ""},
      {.input = "/* */", .expect = ""},
      {.input = "/*\t*/", .expect = ""},
      {.input = "/*\n*/", .expect = ""},
      {.input = "/**qqq**/", .expect = "qqq"},
      {.input = "/**  qqq  **/", .expect = "qqq"},
      {.input = "/**\n\tqqqq\n\t**/", .expect = "qqqq"},
      {.input = "/**  qqq bbb.  **/", .expect = "qqq bbb."},
      {.input = "/**\n\tqqqq\n\t**/", .expect = "qqqq"},
      {.input = "/****qqq bbb.******/", .expect = "qqq bbb."},
      {.input = "/****\n** qqq\n** bbb\n******/", .expect = "** qqq\n** bbb"},
  };
  for (const auto &data : test_cases) {
    EXPECT_EQ(StripCommentAndSpacePadding(data.input), data.expect)
        << "input: \"" << data.input << "\"";
    EXPECT_TRUE(IsSubRange(StripComment(data.input), data.input));
  }
}

}  // namespace
}  // namespace verible
