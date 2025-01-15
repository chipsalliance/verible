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

#include "verible/common/analysis/lint-waiver.h"

#include <cstddef>
#include <set>
#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/text-structure-test-utils.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/iterator-range.h"

#undef EXPECT_OK
#undef EXPECT_NOK
#define EXPECT_OK(expr) EXPECT_TRUE((expr).ok())
#define EXPECT_NOK(expr) EXPECT_FALSE((expr).ok())

namespace verible {
namespace {

// Tests that an empty LintWaiver waives nothing.
TEST(LintWaiverTest, NoWaivers) {
  LintWaiver lint_waiver;
  EXPECT_TRUE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("foo", 0));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("foo", 1));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("bar", 1));
}

// Tests that only one line is waived for one rule.
TEST(LintWaiverTest, WaiveOneLineOneRule) {
  LintWaiver lint_waiver;
  auto rule_name = "xyz-rule";
  lint_waiver.WaiveOneLine(rule_name, 14);
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 13));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 14));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 15));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("other-rule", 14));
}

// Tests that re-waiving the same line has no additional effect.
TEST(LintWaiverTest, ReWaiveOneLineOneRule) {
  LintWaiver lint_waiver;
  auto rule_name = "xyz-rule";
  lint_waiver.WaiveOneLine(rule_name, 14);
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 13));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 14));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 14));  // yes, repeat
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 15));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("other-rule", 14));
}

// Tests that only two lines are waived properly.
TEST(LintWaiverTest, WaiveTwoLinesOneRule) {
  LintWaiver lint_waiver;
  auto rule_name = "aaa-rule";
  lint_waiver.WaiveOneLine(rule_name, 14);
  lint_waiver.WaiveOneLine(rule_name, 10);
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 9));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 10));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 11));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 13));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 14));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 15));
}

// Tests that only one range is waived for one rule.
TEST(LintWaiverTest, WaiveRangeOneRule) {
  LintWaiver lint_waiver;
  auto rule_name = "www-rule";
  lint_waiver.WaiveLineRange(rule_name, 5, 9);
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 4));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 5));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 8));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 9));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("other-rule", 7));
}

// Tests that two disjoint ranges are waived for one rule.
TEST(LintWaiverTest, WaiveTwoDisjointRangesOneRule) {
  LintWaiver lint_waiver;
  auto rule_name = "zzz-rule";
  lint_waiver.WaiveLineRange(rule_name, 5, 7);
  lint_waiver.WaiveLineRange(rule_name, 9, 11);
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 4));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 5));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 6));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 7));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 8));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 9));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 10));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 11));
}

// Tests that fused ranges are waived for one rule.
TEST(LintWaiverTest, WaiveFusedRangesOneRule) {
  LintWaiver lint_waiver;
  auto rule_name = "yy-rule";
  lint_waiver.WaiveLineRange(rule_name, 5, 9);
  lint_waiver.WaiveLineRange(rule_name, 7, 11);  // overlaps
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 4));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 5));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 6));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 7));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 8));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 9));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine(rule_name, 10));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine(rule_name, 11));
}

// Token type enumerations.
// For convenience, using plain int avoids static_cast-ing everywhere.
constexpr int kSpace = 0;
constexpr int kComment = 1;
constexpr int kOther = 2;
constexpr int kNewline = 3;
constexpr char kLinterName[] = "mylinter";
constexpr char kWaiveLineCommand[] = "waive";
constexpr char kWaiveStartCommand[] = "waive-begin";
constexpr char kWaiveStopCommand[] = "waive-end";

// Helper class for testing an example of a waiver.
class LintWaiverBuilderTest : public testing::Test, public LintWaiverBuilder {
 public:
  LintWaiverBuilderTest()
      : LintWaiverBuilder(
            [](const TokenInfo &token) {
              return token.token_enum() == kComment;
            },
            [](const TokenInfo &token) {
              return token.token_enum() == kSpace ||
                     token.token_enum() == kNewline;
            },
            kLinterName, kWaiveLineCommand, kWaiveStartCommand,
            kWaiveStopCommand) {}

  // Convenient sequence adapter to iterator range.
  void ProcessLine(const TokenSequence &tokens, size_t line_number) {
    LintWaiverBuilder::ProcessLine(make_range(tokens.begin(), tokens.end()),
                                   line_number);
  }
};

// Tests that initial state contains no line waivers.
TEST_F(LintWaiverBuilderTest, PostConstruction) {
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_TRUE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("some-rule", 0));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("some-rule", 1));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("another-rule", 1));
}

// Tests that an empty line waives nothing.
TEST_F(LintWaiverBuilderTest, EmptyLine) {
  const TokenSequence tokens{};
  ProcessLine(tokens, 0);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_TRUE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("some-rule", 0));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("some-rule", 1));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("another-rule", 1));
}

// Tests that a comment-only line waives the next line.
TEST_F(LintWaiverBuilderTest, OneCommentOnly) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment,
                 "// mylinter waive x-rule")},  // token locations do not matter
      {TokenInfo(kOther, "hello")}};
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("x-rule", 2));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("x-rule", 3));
}

// Tests that a next-line on the last line does nothing.
TEST_F(LintWaiverBuilderTest, LastLineWaiveNextLine) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "hello")},
      {TokenInfo(kComment, "// mylinter waive z-rule")}};
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("z-rule", 2));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("z-rule", 3));
  // Does nothing with next line until next line is actually encountered.
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("z-rule", 4));
  // At this point example looks like waiver comment is last line.
  // As a small extension, we can verify that next line would be waived.
  ProcessLine(lines[0], 4);
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("z-rule", 4));
}

// Tests that a comment-only with no waive command does nothing.
TEST_F(LintWaiverBuilderTest, OneCommentOnlyMissingWaiveCommand) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment,
                 "// mylinter x-rule")},  // missing 'waive' does nothing
      {TokenInfo(kOther, "hello")}};
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_TRUE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("x-rule", 2));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("x-rule", 3));
}

// Tests that a comment-only with the wrong waive command does nothing.
TEST_F(LintWaiverBuilderTest, OneCommentOnlyWrongWaiveCommand) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment,
                 "// mylinter wave x-rule")},  // only 'waive' does something
      {TokenInfo(kOther, "hello")}};
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_TRUE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("x-rule", 2));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("x-rule", 3));
}

// Tests that a comment-only line with extra text waives the next line.
TEST_F(LintWaiverBuilderTest, OneCommentOnlyExtraTextIgnored) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment, "// mylinter waive x-rule  // yay, waiver!")},
      {TokenInfo(kOther, "hello")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("x-rule", 0));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("x-rule", 1));
}

// Tests that a comment-only line waives the next line.
TEST_F(LintWaiverBuilderTest, OneCommentOnlyOddSpacing) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment, "//mylinter      waive     y-rule    ")},
      {TokenInfo(kOther, "hello")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("y-rule", 0));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("y-rule", 1));
}

// Tests that a comment-only line with leading space waives the next line.
TEST_F(LintWaiverBuilderTest, OneCommentOnlyLeadingSpace) {
  const TokenSequence lines[] = {
      {TokenInfo(kSpace, "    "),  // leading space
       TokenInfo(
           kComment,
           "// mylinter waive xx-rule")},  // token locations do not matter
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 0));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("xx-rule", 1));
}

// Tests that a block-style comment-only line space waives the next line.
TEST_F(LintWaiverBuilderTest, OneCommentOnlyBlockStyle) {
  const TokenSequence lines[] = {
      {
          TokenInfo(kSpace, "    "),  // leading space
          TokenInfo(kComment, "/* mylinter waive xx-rule */"),
          TokenInfo(kSpace, "   ")  // trailing space
      },
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 0));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("xx-rule", 1));
}

// Tests that a comment-only line with leading space waives the next line.
TEST_F(LintWaiverBuilderTest, CommentWaiverCanceledByBlankLine) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment, "// mylinter waive xx-rule")},
      {},  // blank line
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  ProcessLine(lines[2], 2);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_TRUE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 0));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 1));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 2));
}

// Tests that a comment carry waivers to the next non-comment line.
TEST_F(LintWaiverBuilderTest, CommentWaiverCarriedToNextLine) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment, "// mylinter waive xx-rule")},
      {TokenInfo(kComment, "//")},  // comment line, carry waiver to next line
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  ProcessLine(lines[2], 2);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 0));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 1));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("xx-rule", 2));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 3));
}

// Tests that a comment carry waivers to the next non-comment line (with
// spaces).
TEST_F(LintWaiverBuilderTest, CommentWaiverCarriedToNextLineLeadingSpaces) {
  const TokenSequence lines[] = {
      {TokenInfo(kSpace, "\t"),
       TokenInfo(kComment, "// mylinter waive xx-rule")},
      {TokenInfo(kSpace, "\t"),
       TokenInfo(kComment, "//")},  // comment line, carry waiver to next line
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world")}};
  ProcessLine(lines[0], 3);
  ProcessLine(lines[1], 4);
  ProcessLine(lines[2], 5);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 3));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 4));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("xx-rule", 5));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("xx-rule", 6));
}

// Tests that a comment carry waivers to the next non-comment line.
TEST_F(LintWaiverBuilderTest, MultipleNextLineWaiversAccumulate) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment, "// mylinter waive aa-rule")},
      {TokenInfo(kComment, "// mylinter waive bb-rule")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  ProcessLine(lines[2], 2);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("aa-rule", 0));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("aa-rule", 1));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("aa-rule", 2));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("aa-rule", 3));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 0));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 1));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("bb-rule", 2));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 3));
}

// Tests that same-line waiver works.
TEST_F(LintWaiverBuilderTest, ThisLineWaiver) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive bb-rule")}};
  ProcessLine(lines[0], 8);
  ProcessLine(lines[1], 9);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 8));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("bb-rule", 9));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 10));
}

// Tests that a next-line and same-line waivers accumulate.
TEST_F(LintWaiverBuilderTest, NextLineAndThisLineWaiversCombine) {
  const TokenSequence lines[] = {
      {TokenInfo(kComment, "// mylinter waive aa-rule")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive bb-rule")}};
  ProcessLine(lines[0], 0);
  ProcessLine(lines[1], 1);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("aa-rule", 0));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("aa-rule", 1));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("aa-rule", 2));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 0));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("bb-rule", 1));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 2));
}

// Tests that multiple same-line waivers work.
TEST_F(LintWaiverBuilderTest, MultipleThisLineWaiver) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "/* mylinter waive bb-rule */"),
       TokenInfo(kComment, "/* mylinter waive cc-rule */")}};
  ProcessLine(lines[0], 8);
  ProcessLine(lines[1], 9);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 8));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("bb-rule", 9));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("bb-rule", 10));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 8));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 9));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 10));
}

// Tests that waived line range works with end-line comments.
TEST_F(LintWaiverBuilderTest, SingleRangeWaiver) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive-begin cc-rule")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive-end cc-rule")},
  };
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  ProcessLine(lines[2], 4);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 2));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 3));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 4));
}

// Tests that waived line range mis-matched end has no effect.
TEST_F(LintWaiverBuilderTest, EndRangeWaiverNoEffect) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive-end xx-rule")},
  };
  ProcessLine(lines[0], 12);
  ProcessLine(lines[1], 15);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_TRUE(waiver.Empty());
}

// Tests that waived line range works with end-line comments on their own line.
TEST_F(LintWaiverBuilderTest, SingleRangeWaiverDirectivesOnOwnLine) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kComment, "// mylinter waive-begin cc-rule")},
      {TokenInfo(kComment, "// mylinter waive-end cc-rule")},
  };
  ProcessLine(lines[0], 4);
  ProcessLine(lines[1], 5);
  ProcessLine(lines[2], 6);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 4));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 5));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 6));
}

// Tests that waived line range works on a longer range.
TEST_F(LintWaiverBuilderTest, SingleRangeWaiverLonger) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive-begin cc-rule")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive-end cc-rule")},
  };
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  ProcessLine(lines[2], 8);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 2));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 3));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 7));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 8));
}

// Tests that waived line range works with duplicate range-opens.
TEST_F(LintWaiverBuilderTest, SingleRangeWaiverDoubleOpenDoubleClose) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive-begin cc-rule")},
      {TokenInfo(kOther, "hello"), TokenInfo(kOther, "world"),
       TokenInfo(kComment, "// mylinter waive-end cc-rule")},
  };
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  ProcessLine(lines[1], 4);  // duplicate waive-begin (ignored)
  ProcessLine(lines[2], 5);
  ProcessLine(lines[2], 6);  // duplicate waive-end (harmless)
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 2));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 3));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 4));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 5));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 6));
}

// Tests that multiple overlapping waived line ranges work.
TEST_F(LintWaiverBuilderTest, MultiRangeWaiver) {
  const TokenSequence lines[] = {
      {TokenInfo(kOther, "blah blah")},
      {TokenInfo(kComment, "// mylinter waive-begin cc-rule")},
      {TokenInfo(kComment, "// mylinter waive-begin dd-rule")},
      {TokenInfo(kComment, "// mylinter waive-end cc-rule")},
      {TokenInfo(kComment, "// mylinter waive-end dd-rule")},
  };
  ProcessLine(lines[0], 2);
  ProcessLine(lines[1], 3);
  ProcessLine(lines[2], 5);
  ProcessLine(lines[3], 7);
  ProcessLine(lines[4], 9);
  const LintWaiver &waiver = GetLintWaiver();
  EXPECT_FALSE(waiver.Empty());
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 2));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 3));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("cc-rule", 6));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("cc-rule", 7));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("dd-rule", 4));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("dd-rule", 5));
  EXPECT_TRUE(waiver.RuleIsWaivedOnLine("dd-rule", 8));
  EXPECT_FALSE(waiver.RuleIsWaivedOnLine("dd-rule", 9));
}

static const TokenInfo EOL(kNewline, "\n");

// Tests that empty lexical token structure constructs an empty LintWaiver.
TEST_F(LintWaiverBuilderTest, FromTextStructureEmptyFile) {
  const TextStructureTokenized text_structure({});  // empty
  ProcessTokenRangesByLine(text_structure.Data());
  const auto &lint_waiver = GetLintWaiver();
  EXPECT_TRUE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("abc-rule", 0));
}

// Tests that lexical token structure yields an empty LintWaiver.
TEST_F(LintWaiverBuilderTest, FromTextStructureNoWaivers) {
  const TextStructureTokenized text_structure(
      {{TokenInfo(kOther, "hello"), TokenInfo(kOther, ","),
        TokenInfo(kSpace, " "), TokenInfo(kOther, "world"), EOL},
       {EOL},
       {TokenInfo(kOther, "hello"), TokenInfo(kOther, ","),
        TokenInfo(kSpace, " "), TokenInfo(kOther, "world"), EOL}});
  ProcessTokenRangesByLine(text_structure.Data());
  const auto &lint_waiver = GetLintWaiver();
  EXPECT_TRUE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("abc-rule", 0));
}

// Tests that lexical token structure can waive the line after a comment.
TEST_F(LintWaiverBuilderTest, FromTextStructureOneWaiverNextLine) {
  const TextStructureTokenized text_structure(
      {{TokenInfo(kComment, "// mylinter waive abc-rule"), EOL},
       {TokenInfo(kOther, "hello"), EOL}});
  ProcessTokenRangesByLine(text_structure.Data());
  const auto &lint_waiver = GetLintWaiver();
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine("abc-rule", 1));
}

// Tests that lexical token structure can waive the line with a comment.
TEST_F(LintWaiverBuilderTest, FromTextStructureOneWaiverThisLine) {
  const TextStructureTokenized text_structure(
      {{TokenInfo(kOther, "text"), EOL},       // line[0]
       {TokenInfo(kOther, "more-text"), EOL},  // line[1]
       {TokenInfo(kOther, "hello"),            // line[2]
        TokenInfo(kComment, "// mylinter waive qq-rule"), EOL},
       {TokenInfo(kOther, "bye"), EOL}});
  ProcessTokenRangesByLine(text_structure.Data());
  const auto &lint_waiver = GetLintWaiver();
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 2));
}

// Tests that lexical token structure can waive a range of lines.
TEST_F(LintWaiverBuilderTest, FromTextStructureOneWaiverRange) {
  const TextStructureTokenized text_structure({
      {TokenInfo(kOther, "text"), EOL},                               // line[0]
      {TokenInfo(kComment, "// mylinter waive-begin qq-rule"), EOL},  // line[1]
      {TokenInfo(kOther, "more-text"), EOL},                          // line[2]
      {TokenInfo(kComment, "// mylinter waive-end qq-rule"), EOL},    // line[3]
      {TokenInfo(kOther, "bye"), EOL}                                 // line[4]
  });
  ProcessTokenRangesByLine(text_structure.Data());
  const auto &lint_waiver = GetLintWaiver();
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 0));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 1));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 2));
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 3));
}

// Tests that lexical token structure can waive an open range of lines.
TEST_F(LintWaiverBuilderTest, FromTextStructureOneWaiverRangeOpened) {
  const TextStructureTokenized text_structure({
      {TokenInfo(kOther, "text"), EOL},                               // line[0]
      {TokenInfo(kComment, "// mylinter waive-begin qq-rule"), EOL},  // line[1]
      {TokenInfo(kOther, "more-text"), EOL},                          // line[2]
      {TokenInfo(kOther, "bye"), EOL}                                 // line[3]
  });
  ProcessTokenRangesByLine(text_structure.Data());
  const auto &lint_waiver = GetLintWaiver();
  EXPECT_FALSE(lint_waiver.Empty());
  EXPECT_FALSE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 0));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 1));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 2));
  EXPECT_TRUE(lint_waiver.RuleIsWaivedOnLine("qq-rule", 3));
}

TEST_F(LintWaiverBuilderTest, ApplyExternalWaiversInvalidCases) {
  std::set<std::string_view> active_rules;
  const std::string_view user_file = "filename";
  const std::string_view cfg_file = "waive_file.config";

  // Completely invalid config
  const std::string_view cfg_inv = "inv config";
  EXPECT_NOK(ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_inv));

  const std::string_view cfg_inv_2 = "--line=1";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_inv_2));

  // Valid command, invalid parameters
  const std::string_view cfg_inv_params = "waive --something";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_inv_params));

  // Non-registered rule name
  const std::string_view cfg_inv_rule = "waive --rule=abc --line=1";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_inv_rule));

  // register rule
  const std::string_view abc_rule = "abc";
  active_rules.insert(abc_rule);

  // Valid rule, missing params
  const std::string_view cfg_no_param = "waive --rule=abc";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_no_param));

  // Valid rule, invalid line number
  const std::string_view cfg_inv_lineno = "waive --rule=abc --line=0";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_inv_lineno));

  // Valid rule, invalid line range
  const std::string_view cfg_inv_range = "waive --rule=abc --line=1:0";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_inv_range));
  // Valid rule, invalid regex
  const std::string_view cfg_inv_regex = "waive --rule=abc --regex=\"(\"";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_inv_regex));

  // Valid rule, both regex and lines specified
  const std::string_view cfg_conflict =
      "waive --rule=abc --regex=\".*\" --line=1";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_conflict));

  // Missing rulename
  const std::string_view cfg_no_rule = "waive --line=1";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_no_rule));

  // Check that even though some rules are invalid, the consecutive ones
  // are still parsed and applied
  const std::string_view cfg_mixed =
      "waive --line=1\ndasdasda\nwaive --rule=abc --line=10";
  EXPECT_NOK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_mixed));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 8));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 9));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 10));
}

TEST_F(LintWaiverBuilderTest, ApplyExternalWaiversValidCases) {
  const std::set<std::string_view> active_rules{"abc"};
  const std::string_view user_file = "filename";
  const std::string_view cfg_file = "waive_file.config";

  const std::string_view cfg_line = "waive --rule=abc --line=1";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_line));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 0));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 1));

  const std::string_view cfg_line_inv_ord = "waive --line=3 --rule=abc";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file,
                                 cfg_line_inv_ord));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 1));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 2));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 3));

  const std::string_view cfg_quotes = "waive --rule=\"abc\" --line=5";
  EXPECT_OK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_quotes));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 3));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 4));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 5));

  const std::string_view cfg_line_range = "waive --rule=abc --line=7:9";
  EXPECT_OK(
      ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_line_range));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 5));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 6));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 7));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 8));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 9));

  const std::string_view cfg_line_range_i = "waive --rule=abc --line=11:11";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file,
                                 cfg_line_range_i));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 9));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 10));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 11));

  const std::string_view cfg_regex = "waive --rule=abc --regex=abc";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_regex));

  const std::string_view cfg_regex_complex =
      "waive --rule=abc --regex=\"abc .*\"";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file,
                                 cfg_regex_complex));
}

TEST_F(LintWaiverBuilderTest, LocationOptionNarrowsTestedFile) {
  const std::set<std::string_view> active_rules{"abc"};
  const std::string_view user_file = "some_fancy_fileName.sv";
  const std::string_view cfg_file = "waive_file.config";

  std::string_view cfg_line = R"(
    waive --rule=abc --line=100
    waive --rule=abc --line=200 --location=".*foo.*"
    waive --rule=abc --line=300 --location=".*_fancy_.*"
)";

  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_line));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 0));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 99));    // no location
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("abc", 199));  // non-match loc
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("abc", 299));   // matching loc
}

TEST_F(LintWaiverBuilderTest, RegexToLinesSimple) {
  const std::set<std::string_view> active_rules{"rule-1"};
  const std::string_view user_file = "filename";
  const std::string_view cfg_file = "waive_file.config";

  const std::string_view cfg_regex = "waive --rule=rule-1 --regex=def";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_regex));

  const std::string_view file = "abc\ndef\nghi\n";
  const LineColumnMap line_map(file);

  lint_waiver_.RegexToLines(file, line_map);

  // The rule should be waived on the second line only (0-based indexing)
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 0));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 1));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 2));
}

TEST_F(LintWaiverBuilderTest, RegexToLinesCatchAll) {
  const std::set<std::string_view> active_rules{"rule-1"};
  const std::string_view user_file = "filename";
  const std::string_view cfg_file = "waive_file.config";

  const std::string_view cfg_regex = "waive --rule=rule-1 --regex=\".*\"";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_regex));

  const std::string_view file = "abc\ndef\nghi\n\n";
  const LineColumnMap line_map(file);

  lint_waiver_.RegexToLines(file, line_map);

  // The rule should be waived on all lines
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 0));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 1));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 2));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 3));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 4));

  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 5));  // non-exist line
}

TEST_F(LintWaiverBuilderTest, RegexToLinesMultipleMatches) {
  const std::set<std::string_view> active_rules{"rule-1"};
  const std::string_view user_file = "filename";
  const std::string_view cfg_file = "waive_file.config";

  const std::string_view cfg_regex = "waive --rule=rule-1 --regex=\"[0-9]\"";
  EXPECT_OK(ApplyExternalWaivers(active_rules, user_file, cfg_file, cfg_regex));

  const std::string_view file = "abc1\ndef\ng2hi\n";
  const LineColumnMap line_map(file);

  lint_waiver_.RegexToLines(file, line_map);

  // The rule should be waived on all lines that contain any digits
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 0));
  EXPECT_FALSE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 1));
  EXPECT_TRUE(lint_waiver_.RuleIsWaivedOnLine("rule-1", 2));
}

}  // namespace
}  // namespace verible
