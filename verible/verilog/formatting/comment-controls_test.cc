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

#include "verible/verilog/formatting/comment-controls.h"

#include <initializer_list>
#include <sstream>
#include <string_view>
#include <utility>

#include "absl/strings/str_join.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/line-terminator.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace formatter {
namespace {

using ::testing::ElementsAre;
using verible::ByteOffsetSet;
using verible::ExpectedTokenInfo;
using verible::LineColumnMap;
using verible::LineNumberSet;
using verible::TokenInfoTestData;

TEST(DisableFormattingRangesTest, EmptyFile) {
  VerilogAnalyzer analyzer("", "<file>");
  EXPECT_TRUE(analyzer.Tokenize().ok());
  const auto disable_ranges = DisableFormattingRanges(
      analyzer.Data().Contents(), analyzer.Data().TokenStream());
  EXPECT_TRUE(disable_ranges.empty());
  EXPECT_THAT(disable_ranges, ElementsAre());
}

TEST(DisableFormattingRangesTest, NonEmptyNoDisabling) {
  VerilogAnalyzer analyzer("xxx yyy;", "<file>");
  EXPECT_TRUE(analyzer.Tokenize().ok());
  const auto disable_ranges = DisableFormattingRanges(
      analyzer.Data().Contents(), analyzer.Data().TokenStream());
  EXPECT_TRUE(disable_ranges.empty());
  EXPECT_THAT(disable_ranges, ElementsAre());
}

enum {
  kOff = 99  // any non-zero value, to tag the disabled ranges
};

struct DisableRangeTestData : public TokenInfoTestData {
  ByteOffsetSet expected;

  DisableRangeTestData(std::initializer_list<ExpectedTokenInfo> fragments)
      : TokenInfoTestData{fragments} {
    // convert expected_tokens into expected ranges
    const auto tokens = FindImportantTokens();
    const std::string_view base(code);
    for (const auto &t : tokens) {
      expected.Add({t.left(base), t.right(base)});
    }
  }
};

TEST(DisableFormattingRangesTest, FormatOnNoEffect) {
  // By default, nothing is disabled, formatter is on for entire file, so these
  // should have no effect.
  const char *kTestCases[] = {
      "xxx yyy;\n  // verilog_format: on\n",
      "xxx yyy;\n  /* verilog_format: on */\n",
      "xxx yyy;\n// verilog_format:  on\n//verilog_format:on\n",
      "xxx yyy;\n  // verilog_format: other\n",
      "xxx yyy;\n  // verilog_format:\n",  // no command
  };
  for (const auto *code : kTestCases) {
    VerilogAnalyzer analyzer(code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());
    EXPECT_TRUE(disable_ranges.empty());
  }
}

TEST(DisableFormattingRangesTest, FormatOffDisableToEndEOLComment) {
  const DisableRangeTestData kTestCases[] = {
      {"xxx yyy;\n  // verilog_format: off\n"},  // range to EOF is empty
      {"xxx yyy;\n  // verilog_format: off"},
      {"xxx yyy;\n  // verilog_format: off\n", {kOff, "\n"}},
      {"xxx yyy;\n  // verilog_format: off     \n", {kOff, "\n"}},
      {"xxx yyy;\n  // verilog_format: off\n", {kOff, "\n    "}},
      {"xxx yyy;\n  //verilog_format: off\n", {kOff, "\n"}},
      {"xxx yyy;\n  //verilog_format:off\n", {kOff, "\n"}},
      {"xxx yyy;\n  // verilog_format:off\n", {kOff, "\n"}},
      {"xxx yyy;\n  //  verilog_format:   off   // reason why\n", {kOff, "\n"}},
      {"xxx yyy;\n  // verilog_format: off\n",
       {kOff, "\t// verilog_format: off again\n"}},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());
    EXPECT_EQ(disable_ranges, test.expected);
  }
}

TEST(DisableFormattingRangesTest, FormatOffDisableToEndBlockComment) {
  const DisableRangeTestData kTestCases[] = {
      {"xxx yyy;\n  /* verilog_format: off */", {kOff, "\n"}},
      {"xxx yyy;\n  /* verilog_format: off */", {kOff, "  \n"}},
      {"xxx yyy;\n  /*verilog_format: off */", {kOff, "\n  "}},
      {"xxx yyy;\n  /* verilog_format:off */", {kOff, "\n  "}},
      {"xxx yyy;\n  /*verilog_format:off */", {kOff, "\n  "}},
      {"xxx yyy;\n  /*****     verilog_format:    off    ****/",
       {kOff, "\n  "}},
      {"xxx yyy;\n  /* verilog_format: off  : reason why... */",
       {kOff, "\n\t\t"}},
      {"xxx yyy;\n  /* verilog_format: off  // reason why... */",
       {kOff, "\n \t"}},
      {"  /* verilog_format: off */", {kOff, "/* verilog_format:on */"}, "\n"},
      {"  /* verilog_format: off */", {kOff, " /* verilog_format:on */"}, "\n"},
      {"  /* verilog_format: off */",
       {kOff, "  \t  /* verilog_format:on */"},
       "\n"},
      {"  /* verilog_format: off */",
       {kOff, "\n/* verilog_format:on */"},
       "\n"},
      {"  /* verilog_format: off */",
       {kOff, "\n\n/* verilog_format:on */"},
       "\n"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());
    EXPECT_EQ(disable_ranges, test.expected);
  }
}

TEST(DisableFormattingRangesTest, FormatOffVarious) {
  const DisableRangeTestData kTestCases[] = {
      {// one disabled interval, very brief (off and on again)
       "xxx yyy;\n"
       "// verilog_format: off\n",
       {kOff, "// verilog_format: on"},
       "\n"
       "ppp qqq;\n"},
      {// one disabled interval affecting one line (extra blank line)
       "xxx yyy;\n"
       "// verilog_format: off\n",
       {kOff, "\n// verilog_format: on"},
       "\n"
       "ppp qqq;\n"},
      {// one disabled interval affecting multiple lines
       "xxx yyy;\n"
       "// verilog_format: off\n",
       {kOff, "\n\n\n// verilog_format: on"},
       "\n"
       "ppp qqq;\n"},
      {// disable to end-of-file, second command is neither on/off
       "xxx yyy;\n"
       "// verilog_format: off\n",
       {kOff,
        "// verilog_format: other\n"
        "ppp qqq;\n"}},
      {// one disabled interval in the middle
       "xxx yyy;\n"
       "// verilog_format: off\n",
       {kOff, "zzz www;\n// verilog_format: on"},
       "\n"
       "ppp qqq;\n"},
      {// one disabled interval in the middle
       "xxx yyy;\n"
       "/*    verilog_format: off */",
       {kOff, "\nzzz www;\n/* verilog_format:   on */"},
       "\n"
       "ppp qqq;\n"},
      {// null interval
       "xxx yyy;\n"
       "/*    verilog_format: off */",
       {kOff, "/* verilog_format:   on */"},
       "\n"
       "ppp qqq;\n"},
      {// two disabled intervals
       "xxx yyy;\n"
       "// verilog_format: off\n",
       {kOff, "zzz www;\n// verilog_format: on"},
       "\n"
       "ppp qqq;\n"
       "// verilog_format:off\n",
       {kOff, "aa bb;\n// verilog_format:on"},
       "\n"
       "cc dd;\n"},
  };
  for (const auto &test : kTestCases) {
    VerilogAnalyzer analyzer(test.code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());
    EXPECT_EQ(disable_ranges, test.expected) << "code:\n" << test.code;
  }
}

struct DisabledBytesTestCase {
  std::string_view text;
  LineNumberSet enabled_lines;
  ByteOffsetSet expected_bytes;
};

TEST(EnabledLinesToDisabledByteRangesTest, AllCases) {
  const DisabledBytesTestCase kTestCases[] = {
      {.text = "", .enabled_lines = {}, .expected_bytes = {}},  // empty text
      {.text = "aaaa\n"
               "bbbbbb\n"
               "cccc\n",
       .enabled_lines = {},  // no disabled lines
       .expected_bytes = {}},
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines = {{1, 2}},   // enabled first line only
          .expected_bytes = {{5, 17}}  // disable all other lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines = {{2, 3}},            // enabled second line only
          .expected_bytes = {{0, 5}, {12, 17}}  // disable all other lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines = {{3, 4}},   // enabled third line only
          .expected_bytes = {{0, 12}}  // disable all other lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines = {{1, 3}},    // enabled first two lines only
          .expected_bytes = {{12, 17}}  // disable all other lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines = {{2, 4}},  // enabled last two lines only
          .expected_bytes = {{0, 5}}  // disable all other lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines = {{1, 4}},  // enabled no lines only
          .expected_bytes = {}        // disable no lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines = {{0, 5}},  // excess range
          .expected_bytes = {}        // disable no lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc",             // missing terminating '\n' (POSIX)
          .enabled_lines = {{1, 4}},  // excess range
          .expected_bytes = {}        // disable no lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc",             // missing terminating '\n' (POSIX)
          .enabled_lines = {{0, 5}},  // excess range
          .expected_bytes = {}        // disable no lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc",              // missing terminating '\n' (POSIX)
          .enabled_lines = {{4, 8}},   // excess range
          .expected_bytes = {{0, 12}}  // disable all (whole) lines
      },
      {
          .text = "aaaa\n"
                  "bbbbbb\n"
                  "cccc\n",
          .enabled_lines =
              {{4, 8}},  // range outside, interpret as disable all other lines
          .expected_bytes = {{0, 17}}  // disable all lines
      },
  };
  for (const auto &test : kTestCases) {
    LineColumnMap line_map(test.text);
    const ByteOffsetSet result(
        EnabledLinesToDisabledByteRanges(test.enabled_lines, line_map));
    EXPECT_EQ(result, test.expected_bytes)
        << "lines: " << test.enabled_lines << "\ncolumn map: "
        << absl::StrJoin(line_map.GetBeginningOfLineOffsets(), ",",
                         absl::StreamFormatter());
  }
}

struct FormatWhitespaceTestCase {
  std::string_view full_text;
  std::pair<int, int> substring_range;
  ByteOffsetSet disabled_ranges;
  std::string_view expected;
};

TEST(FormatWhitespaceWithDisabledByteRangesTest, InvalidSubstring) {
  const std::string_view foo("foo"), bar("bar");
  std::ostringstream stream;
  EXPECT_DEATH(
      FormatWhitespaceWithDisabledByteRanges(foo, bar, {}, true, stream,
                                             verible::LineTerminatorStyle::kLF),
      "IsSubRange");
}

TEST(FormatWhitespaceWithDisabledByteRangesTest, EmptyStrings) {
  // The only special character in these functions/tests is '\n',
  // everything else is treated the same, space or not.
  // We use nonspace characters for positional readability.
  const FormatWhitespaceTestCase kTestCases[] = {
      {.full_text = "",
       .substring_range = {0, 0},
       .disabled_ranges = {},
       .expected = ""},
      {.full_text = "\n",
       .substring_range = {0, 0},
       .disabled_ranges = {},
       .expected = ""},
      {.full_text = "\n",
       .substring_range = {0, 1},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "\n\n",
       .substring_range = {0, 1},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "\n\n",
       .substring_range = {1, 2},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "\n\n",
       .substring_range = {1, 1},
       .disabled_ranges = {},
       .expected = "\n"},  // space text is ""
      {.full_text = "\n\n",
       .substring_range = {0, 2},
       .disabled_ranges = {},
       .expected = "\n\n"},
      {.full_text = "\n\n",
       .substring_range = {0, 2},
       .disabled_ranges = {{0, 1}},
       .expected = "\n\n"},
      {.full_text = "\n\n",
       .substring_range = {0, 2},
       .disabled_ranges = {{1, 2}},
       .expected = "\n\n"},
      {.full_text = "\n\n",
       .substring_range = {0, 2},
       .disabled_ranges = {{0, 2}},
       .expected = "\n\n"},
      {.full_text = "abcd",
       .substring_range = {0, 2},
       .disabled_ranges = {},
       .expected = ""},
      {.full_text = "abcd",
       .substring_range = {1, 3},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "abcd",
       .substring_range = {1, 3},
       .disabled_ranges = {{0, 1}, {3, 4}},
       .expected = "\n"},
      {.full_text = "abcd",
       .substring_range = {1, 3},
       .disabled_ranges = {{0, 4}},
       .expected = "bc"},
      {.full_text = "abcd",
       .substring_range = {0, 2},
       .disabled_ranges = {{0, 4}},
       .expected = "ab"},
      {.full_text = "abcd",
       .substring_range = {2, 4},
       .disabled_ranges = {{0, 4}},
       .expected = "cd"},
      {.full_text = "abcd",
       .substring_range = {1, 3},
       .disabled_ranges = {{0, 2}},
       .expected = "b\n"},  // semi-disabled
      {.full_text = "abcd",
       .substring_range = {1, 3},
       .disabled_ranges = {{2, 4}},
       .expected = "c\n"},  // semi-disabled
      {.full_text = "abcd",
       .substring_range = {0, 0},
       .disabled_ranges = {{0, 4}},
       .expected = ""},
      {.full_text = "abcd",
       .substring_range = {1, 1},
       .disabled_ranges = {{0, 4}},
       .expected = ""},
      {.full_text = "abcd",
       .substring_range = {0, 0},
       .disabled_ranges = {},
       .expected = ""},
      {.full_text = "abcd",
       .substring_range = {1, 1},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {2, 5},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {2, 6},
       .disabled_ranges = {},
       .expected = "\n\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 6},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 7},
       .disabled_ranges = {},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {2, 5},
       .disabled_ranges = {{0, 9}},
       .expected = "\ncd"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {2, 6},
       .disabled_ranges = {{0, 9}},
       .expected = "\ncd\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 6},
       .disabled_ranges = {{0, 9}},
       .expected = "cd\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 7},
       .disabled_ranges = {{0, 9}},
       .expected = "cd\ne"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {{0, 9}},
       .expected = "cd\nef\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {},
       .expected = "\n\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {{3, 4}},
       .expected = "c\n\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {{4, 5}},
       .expected = "d\n\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {{5, 6}},
       .expected = "\n\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {{6, 7}},
       .expected = "\ne\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {{7, 8}},
       .expected = "\nf\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 9},
       .disabled_ranges = {{8, 9}},
       .expected = "\n\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {2, 5},
       .disabled_ranges = {{0, 3}},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {2, 6},
       .disabled_ranges = {{0, 3}},
       .expected = "\n\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 6},
       .disabled_ranges = {{0, 3}},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 6},
       .disabled_ranges = {{5, 6}},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 6},
       .disabled_ranges = {{5, 9}},
       .expected = "\n"},
      {.full_text = "ab\ncd\nef\n",
       .substring_range = {3, 6},
       .disabled_ranges = {{6, 9}},
       .expected = "\n"},
  };
  for (const auto &test : kTestCases) {
    std::ostringstream stream;
    const auto substr = test.full_text.substr(
        test.substring_range.first,
        test.substring_range.second - test.substring_range.first);
    FormatWhitespaceWithDisabledByteRanges(test.full_text, substr,
                                           test.disabled_ranges, true, stream,
                                           verible::LineTerminatorStyle::kLF);
    EXPECT_EQ(stream.str(), test.expected)
        << "text: \"" << test.full_text << "\", sub: \"" << substr
        << "\", disabled: " << test.disabled_ranges;
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
