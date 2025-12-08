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

#include "verible/common/strings/display-utils.h"

#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace verible {
namespace {

struct TruncateTestCase {
  std::string_view input;
  int max_chars;
  std::string_view expected;
};

TEST(AutoTruncateTest, Various) {
  constexpr TruncateTestCase kTestCases[] = {
      {"abcde", 9, "abcde"},
      {"abcdef", 9, "abcdef"},
      {"abcdefg", 9, "abcdefg"},
      {"abcdefgh", 9, "abcdefgh"},
      {"abcdefghi", 9, "abcdefghi"},
      {"abcdefghij", 9, "abc...hij"},
      {"abcdefghijk", 9, "abc...ijk"},
      {"abcdefghijk", 10, "abcd...ijk"},  // more head than tail
      {"123!(@*#&)!#$!@#(*xyz", 9, "123...xyz"},
      {"123!(@*#&)!#$!@#(*xyz", 10, "123!...xyz"},
      {"123!(@*#&)!#$!@#(*xyz", 11, "123!...*xyz"},
      {"123!(@*#&)!#$!@#(*xyz", 12, "123!(...*xyz"},
  };
  for (const auto &test : kTestCases) {
    std::ostringstream stream;
    stream << AutoTruncate{test.input, test.max_chars};
    EXPECT_EQ(stream.str(), test.expected);
  }
}

TEST(VisualizeWhitespaceTest, Various) {
  constexpr std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"", ""},          {"abc", "abc"},
      {"ABC", "ABC"},    {"123", "123"},
      {"   ", "..."},    {"\n\n\n", "\\\n\\\n\\\n"},
      {"\t\t\t", "###"}, {"abc \n\t123", "abc.\\\n#123"},
  };
  for (const auto &test : kTestCases) {
    std::ostringstream stream;
    stream << VisualizeWhitespace{test.first};
    EXPECT_EQ(stream.str(), test.second);
  }
}

TEST(VisualizeWhitespaceTest, OtherSubstitutions) {
  constexpr std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"", ""},          {"abc", "abc"},
      {"ABC", "ABC"},    {"123", "123"},
      {"   ", "---"},    {"\n\n\n", "NNN"},
      {"\t\t\t", "TTT"}, {"abc \n\t123", "abc-NT123"},
  };
  for (const auto &test : kTestCases) {
    std::ostringstream stream;
    stream << VisualizeWhitespace{test.first, '-', "N", "T"};
    EXPECT_EQ(stream.str(), test.second);
  }
}

TEST(EscapeStringTest, Various) {
  constexpr std::pair<std::string_view, std::string_view> kTestCases[] = {
      {"", ""},
      {"abc", "abc"},
      {"ABC", "ABC"},
      {"123", "123"},
      {"a\nb\r\nc", R"(a\nb\r\nc)"},
      {"  a\nb  \r\nc  ", R"(  a\nb  \r\nc  )"},
      {"    \x01\x02\x03\x04\x05\x06\x07", R"(    \x01\x02\x03\x04\x05\x06\a)"},
      {"\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f", R"(\b\t\n\v\f\r\x0e\x0f)"},
      {"\x10\x11\x12\x13\x14\x15\x16\x17",
       R"(\x10\x11\x12\x13\x14\x15\x16\x17)"},
      {"\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
       R"(\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f)"},
      {R"( ' " \ \\ )", R"( \' \" \\ \\\\ )"},
      {" ~", " ~"},
      {"\x7f\x80\xfe\xff", R"(\x7f\x80\xfe\xff)"},
  };
  for (const auto &test : kTestCases) {
    std::ostringstream stream;
    stream << EscapeString{test.first};
    EXPECT_EQ(stream.str(), test.second);
  }
}

using IntVector = std::vector<int>;

// Normally a definition like the following would appear in a header
// to be shared.
// TODO(fangism): Use auto return type with C++17 as minimum standard.
static SequenceStreamFormatter<IntVector> AngleBracketFormatter(
    const IntVector &t) {
  return SequenceFormatter(t, " | ", "< ", " >");
}

TEST(SequenceFormatterTest, AngleBracketVectorNotation) {
  const IntVector v{5, 6, 7, 8};
  std::ostringstream stream;
  stream << AngleBracketFormatter(v);
  EXPECT_EQ(stream.str(), "< 5 | 6 | 7 | 8 >");
}

}  // namespace
}  // namespace verible
