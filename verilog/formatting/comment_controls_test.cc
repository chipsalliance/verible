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

#include "verilog/formatting/comment_controls.h"

#include <initializer_list>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/str_join.h"
#include "common/text/line_column_map.h"
#include "common/text/token_info_test_util.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace formatter {
namespace {

using ::testing::ElementsAre;
using verible::LineColumnMap;

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

using verible::ExpectedTokenInfo;
using verible::TokenInfoTestData;

enum {
  kOff = 99  // any non-zero value, to tag the disabled ranges
};

struct DisableRangeTestData : public TokenInfoTestData {
  ByteOffsetSet expected;

  DisableRangeTestData(std::initializer_list<ExpectedTokenInfo> fragments)
      : TokenInfoTestData{fragments} {
    // convert expected_tokens into expected ranges
    const auto tokens = FindImportantTokens();
    const absl::string_view base(code);
    for (const auto& t : tokens) {
      expected.Add({t.left(base), t.right(base)});
    }
  }
};

TEST(DisableFormattingRangesTest, FormatOnNoEffect) {
  // By default, nothing is disabled, formatter is on for entire file, so these
  // should have no effect.
  const char* kTestCases[] = {
      "xxx yyy;\n  // verilog_format: on\n",
      "xxx yyy;\n  /* verilog_format: on */\n",
      "xxx yyy;\n// verilog_format:  on\n//verilog_format:on\n",
      "xxx yyy;\n  // verilog_format: other\n",
      "xxx yyy;\n  // verilog_format:\n",  // no command
  };
  for (const auto* code : kTestCases) {
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
  for (const auto& test : kTestCases) {
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
  for (const auto& test : kTestCases) {
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
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());
    EXPECT_EQ(disable_ranges, test.expected) << "code:\n" << test.code;
  }
}

struct DisabledBytesTestCase {
  absl::string_view text;
  LineNumberSet enabled_lines;
  ByteOffsetSet expected_bytes;
};

TEST(EnabledLinesToDisabledByteRangesTest, AllCases) {
  const DisabledBytesTestCase kTestCases[] = {
      {"", {}, {}},  // empty text
      {"aaaa\n"
       "bbbbbb\n"
       "cccc\n",
       {},  // no disabled lines
       {}},
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{1, 2}},  // enabled first line only
          {{5, 17}}  // disable all other lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{2, 3}},           // enabled second line only
          {{0, 5}, {12, 17}}  // disable all other lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{3, 4}},  // enabled third line only
          {{0, 12}}  // disable all other lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{1, 3}},   // enabled first two lines only
          {{12, 17}}  // disable all other lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{2, 4}},  // enabled last two lines only
          {{0, 5}}   // disable all other lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{1, 4}},  // enabled no lines only
          {}         // disable no lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{0, 5}},  // excess range
          {}         // disable no lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc",    // missing terminating '\n' (POSIX)
          {{1, 4}},  // excess range
          {}         // disable no lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc",    // missing terminating '\n' (POSIX)
          {{0, 5}},  // excess range
          {}         // disable no lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc",    // missing terminating '\n' (POSIX)
          {{4, 8}},  // excess range
          {{0, 12}}  // disable all (whole) lines
      },
      {
          "aaaa\n"
          "bbbbbb\n"
          "cccc\n",
          {{4, 8}},  // range outside, interpret as disable all other lines
          {{0, 17}}  // disable all lines
      },
  };
  for (const auto& test : kTestCases) {
    LineColumnMap line_map(test.text);
    const ByteOffsetSet result(
        EnabledLinesToDisabledByteRanges(test.enabled_lines, line_map));
    EXPECT_EQ(result, test.expected_bytes)
        << "lines: " << test.enabled_lines << "\ncolumn map: "
        << absl::StrJoin(line_map.GetBeginningOfLineOffsets(), ",",
                         absl::StreamFormatter());
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
