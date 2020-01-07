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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace formatter {
namespace {

using ::testing::ElementsAre;

TEST(SetRangeTest, Empty) {
  std::vector<bool> bits(6, false);
  const std::vector<bool> expect({false, false, false, false, false, false});
  EXPECT_EQ(bits, expect);
  SetRange(&bits, 3, 3);
  EXPECT_EQ(bits, expect);
}

TEST(SetRangeTest, EmptyGrows) {
  std::vector<bool> bits(6, false);
  SetRange(&bits, 8, 8);
  EXPECT_THAT(bits, ElementsAre(false, false, false, false, false, false, false,
                                false));
}

TEST(SetRangeTest, BadRangeBackwards) {
  std::vector<bool> bits(6, false);
  EXPECT_DEATH(SetRange(&bits, 4, 3), "");
}

TEST(SetRangeTest, BadRangeNegative) {
  std::vector<bool> bits(6, false);
  EXPECT_DEATH(SetRange(&bits, -1, 3), "");
}

TEST(SetRangeTest, SetOne) {
  std::vector<bool> bits(6, false);
  SetRange(&bits, 3, 4);
  EXPECT_THAT(bits, ElementsAre(false, false, false, true, false, false));
  SetRange(&bits, 0, 1);
  EXPECT_THAT(bits, ElementsAre(true, false, false, true, false, false));
}

TEST(SetRangeTest, SetTwo) {
  std::vector<bool> bits(6, false);
  SetRange(&bits, 3, 5);
  EXPECT_THAT(bits, ElementsAre(false, false, false, true, true, false));
  SetRange(&bits, 2, 4);
  EXPECT_THAT(bits, ElementsAre(false, false, true, true, true, false));
}

TEST(SetRangeTest, SetOneGrows) {
  std::vector<bool> bits(6, false);
  SetRange(&bits, 7, 8);
  EXPECT_THAT(
      bits, ElementsAre(false, false, false, false, false, false, false, true));
}

TEST(ContainsRangeTest, EmptyDegenerate) {
  const std::vector<bool> bits({false, false, false, false});
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(ContainsRange(bits, i, i));
  }
}

TEST(ContainsRangeTest, EmptyAllRanges) {
  const std::vector<bool> bits({false, false, false, false});
  for (int i = 0; i < 8; ++i) {
    for (int j = i + 1; j < 8; ++j) {
      EXPECT_FALSE(ContainsRange(bits, i, j));
    }
  }
}

TEST(ContainsRangeTest, BadRangeBackwards) {
  std::vector<bool> bits(6, false);
  EXPECT_DEATH(ContainsRange(bits, 4, 3), "");
}

TEST(ContainsRangeTest, BadRangeNegative) {
  std::vector<bool> bits(6, false);
  EXPECT_DEATH(ContainsRange(bits, -1, 3), "");
}

TEST(ContainsRangeTest, AllTrue) {
  const std::vector<bool> bits({true, true, true, true});
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      EXPECT_TRUE(ContainsRange(bits, i, j));
    }
  }
}

TEST(ContainsRangeTest, SomeTrue) {
  const std::vector<bool> bits({false, true, true, false});
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      EXPECT_EQ(ContainsRange(bits, i, j), i >= 1 && i < 3 && j >= 2 && j < 4);
    }
  }
}

TEST(ContainsRangeTest, BeyondEnd) {
  const std::vector<bool> bits({true, true, true, true});
  for (int i = 0; i < 5; ++i) {
    EXPECT_FALSE(ContainsRange(bits, i, 5));
  }
}

TEST(DisableFormattingRangesTest, EmptyFile) {
  VerilogAnalyzer analyzer("", "<file>");
  EXPECT_TRUE(analyzer.Tokenize().ok());
  const auto disable_ranges = DisableFormattingRanges(
      analyzer.Data().Contents(), analyzer.Data().TokenStream());
  EXPECT_THAT(disable_ranges, ElementsAre());
}

TEST(DisableFormattingRangesTest, NonEmptyNoDisabling) {
  VerilogAnalyzer analyzer("xxx yyy;", "<file>");
  EXPECT_TRUE(analyzer.Tokenize().ok());
  const auto disable_ranges = DisableFormattingRanges(
      analyzer.Data().Contents(), analyzer.Data().TokenStream());
  const std::vector<bool> expect(analyzer.Data().Contents().length(), false);
  EXPECT_EQ(disable_ranges, expect);
}

TEST(DisableFormattingRangesTest, FormatOnNoEffect) {
  // By default, nothing is disabled, formatter is on for entire file, so these
  // should have no effect.
  const char* kTestCases[] = {
      "xxx yyy;\n  // verilog_format: on\n",
      "xxx yyy;\n  /* verilog_format: on */\n",
      "xxx yyy;\n// verilog_format:  on\n//verilog_format:on\n",
  };
  for (const auto* code : kTestCases) {
    VerilogAnalyzer analyzer(code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());
    const std::vector<bool> expect(analyzer.Data().Contents().length(), false);
    EXPECT_EQ(disable_ranges, expect);
  }
}

TEST(DisableFormattingRangesTest, FormatOffDisableToEndEOLComment) {
  const char* kTestCases[] = {
      // all effective-comments start at byte-offset 11
      "xxx yyy;\n  // verilog_format: off\n",
      "xxx yyy;\n  //verilog_format: off\n",
      "xxx yyy;\n  //verilog_format:off\n",
      "xxx yyy;\n  // verilog_format:off\n",
      "xxx yyy;\n  //  verilog_format:   off     // reason why\n",
      "xxx yyy;\n  // verilog_format: off\n// verilog_format: off again\n",
  };
  for (const auto* code : kTestCases) {
    VerilogAnalyzer analyzer(code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());

    std::vector<bool> expect(analyzer.Data().Contents().length(), false);
    // from start of comment to end of file
    SetRange(&expect, 11, analyzer.Data().Contents().length());
    EXPECT_EQ(disable_ranges, expect);
  }
}

TEST(DisableFormattingRangesTest, FormatOffDisableToEndBlockComment) {
  const char* kTestCases[] = {
      // all effective-comments start at byte-offset 11
      "xxx yyy;\n  /* verilog_format: off */\n",
      "xxx yyy;\n  /*verilog_format: off */\n",
      "xxx yyy;\n  /* verilog_format:off */\n",
      "xxx yyy;\n  /*verilog_format:off */\n",
      "xxx yyy;\n  /*****     verilog_format:    off    ****/\n",
      "xxx yyy;\n  /* verilog_format: off  : reason why... */\n",
      "xxx yyy;\n  /* verilog_format: off  // reason why... */\n",
  };
  for (const auto* code : kTestCases) {
    VerilogAnalyzer analyzer(code, "<file>");
    EXPECT_TRUE(analyzer.Tokenize().ok());
    const auto disable_ranges = DisableFormattingRanges(
        analyzer.Data().Contents(), analyzer.Data().TokenStream());

    std::vector<bool> expect(analyzer.Data().Contents().length(), false);
    // from start of comment to end of file
    SetRange(&expect, 11, analyzer.Data().Contents().length());
    EXPECT_EQ(disable_ranges, expect);
  }
}

TEST(DisableFormattingRangesTest, FormatOffSingleRange) {
  const char kCode[] = {
      "xxx yyy;\n"                // 9 bytes (start)
      "// verilog_format: off\n"  // +23 bytes = 32
      "zzz www;\n"                // +9 bytes = 41
      "// verilog_format: on\n"   // +22 bytes = 63 (end)
      "ppp qqq;\n"                // +9 bytes = 72
  };
  VerilogAnalyzer analyzer(kCode, "<file>");
  EXPECT_TRUE(analyzer.Tokenize().ok());
  const auto disable_ranges = DisableFormattingRanges(
      analyzer.Data().Contents(), analyzer.Data().TokenStream());

  std::vector<bool> expect(analyzer.Data().Contents().length(), false);
  // from start of first comment to end of second comment
  SetRange(&expect, 9, 62);  // 63 -1, excluding training \n character
  EXPECT_EQ(disable_ranges, expect);
}

TEST(DisableFormattingRangesTest, FormatOffMultipleRanges) {
  const char kCode[] = {
      "xxx yyy;\n"                // 9 bytes (start)
      "// verilog_format: off\n"  // +23 bytes = 32
      "zzz www;\n"                // +9 bytes = 41
      "// verilog_format: on\n"   // +22 bytes = 63 (end)
      "ppp qqq;\n"                // +9 bytes = 72 (start)
      "// verilog_format:off\n"   // +22 bytes = 94
      "aa bb;\n"                  // +7 bytes = 101
      "// verilog_format:on\n"    // +21 bytes = 122 (end)
      "cc dd;\n"                  // +7 bytes = 129
  };
  VerilogAnalyzer analyzer(kCode, "<file>");
  EXPECT_TRUE(analyzer.Tokenize().ok());
  const auto disable_ranges = DisableFormattingRanges(
      analyzer.Data().Contents(), analyzer.Data().TokenStream());

  std::vector<bool> expect(analyzer.Data().Contents().length(), false);
  // from start of first comment to end of second comment
  SetRange(&expect, 9, 62);    // 63 -1, excluding training \n character
  SetRange(&expect, 72, 121);  // 122 -1, excluding training \n character
  EXPECT_EQ(disable_ranges, expect);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
