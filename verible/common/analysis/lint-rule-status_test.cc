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

#include "verible/common/analysis/lint-rule-status.h"

#include <cstring>
#include <iostream>
#include <set>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-builder-test-util.h"

namespace verible {
namespace {

// Tests initialization of LintRuleStatus.
TEST(LintRuleStatusTest, Construction) {
  std::set<LintViolation> violations;
  LintRuleStatus status(violations, "RULE_NAME", "http://example.com/svstyle");
  EXPECT_TRUE(status.violations.empty());
  EXPECT_EQ(status.lint_rule_name, "RULE_NAME");
  EXPECT_EQ(status.url, "http://example.com/svstyle");
  EXPECT_TRUE(status.isOk());
}

// Tests adding violations to LintRuleStatus.
TEST(LintRuleStatusTest, ConstructWithViolation) {
  const TokenInfo token(1, "1bad-id");
  std::set<LintViolation> violations({LintViolation(token, "invalid id")});
  LintRuleStatus status(violations, "RULE_NAME", "http://example.com/svstyle");
  EXPECT_FALSE(status.violations.empty());
  EXPECT_FALSE(status.isOk());
}

// Tests waiving violations and removing them from LintRuleStatus.
TEST(LintRuleStatusTest, WaiveViolations) {
  const TokenInfo token(1, "1bad-id");
  std::set<LintViolation> violations({LintViolation(token, "invalid id")});
  LintRuleStatus status(violations, "RULE_NAME", "http://example.com/svstyle");
  EXPECT_FALSE(status.violations.empty());
  EXPECT_FALSE(status.isOk());
  // First, waive nothing.
  status.WaiveViolations([](const LintViolation &) { return false; });
  EXPECT_FALSE(status.violations.empty());
  EXPECT_FALSE(status.isOk());
  // Second, waive everything.
  status.WaiveViolations([](const LintViolation &) { return true; });
  EXPECT_TRUE(status.violations.empty());
  EXPECT_TRUE(status.isOk());
}

// Struct for checking expected formatting of a single Lint Violation
// Note that the filename produced by formatter is provided by LintStatusTest,
// which contains this struct.
struct LintViolationTest {
  std::string reason;
  TokenInfo token;
  std::string expected_output;
  std::vector<TokenInfo> related_tokens;
};

// Struct for checking expected formatting of a LintRuleStatus
// TODO(b/136092807): leverage SynthesizedLexerTestData to produce
// expected findings.
struct LintStatusTest {
  absl::string_view rule_name;
  std::string url;
  absl::string_view path;
  absl::string_view text;
  std::vector<LintViolationTest> violations;
};

void RunLintStatusTest(const LintStatusTest &test) {
  // Dummy tree so we have something for test cases to point at
  SymbolPtr root = Node();

  LintRuleStatus status;
  status.url = test.url;
  status.lint_rule_name = test.rule_name;
  for (const auto &violation_test : test.violations) {
    status.violations.insert(LintViolation(violation_test.token,
                                           violation_test.reason, {},
                                           violation_test.related_tokens));
  }

  std::ostringstream ss;

  LintStatusFormatter formatter(test.text);
  formatter.FormatLintRuleStatus(&ss, status, test.text, test.path);
  auto result_parts = absl::StrSplit(ss.str(), '\n');
  auto part_iterator = result_parts.begin();

  for (const auto &violation_test : test.violations) {
    EXPECT_EQ(*part_iterator, violation_test.expected_output);
    part_iterator++;
  }
}

TEST(LintRuleStatusFormatterTest, SimpleOutput) {
  SymbolPtr root = Node();
  static const int dont_care_tag = 0;
  constexpr absl::string_view text(
      "This is some code\n"
      "That you are looking at right now\n"
      "It is nice code, make no mistake\n"
      "Very nice");
  LintStatusTest test = {
      "test-rule",
      "http://foobar",
      "some/path/to/somewhere.fvg",
      text,
      {{"reason1", TokenInfo(dont_care_tag, text.substr(0, 5)),
        "some/path/to/somewhere.fvg:1:1-5: reason1 http://foobar [test-rule]"},
       {"reason2", TokenInfo(dont_care_tag, text.substr(21, 4)),
        "some/path/to/somewhere.fvg:2:4-7: reason2 http://foobar "
        "[test-rule]"}}};

  RunLintStatusTest(test);
}

TEST(LintRuleStatusFormatterTest, HelperTokensReplacmentWithTokensLocation) {
  SymbolPtr root = Node();
  static const int dont_care_tag = 0;
  constexpr absl::string_view text(
      "This is some code\n"
      "That you are looking at right now\n"
      "It is nice code, make no mistake\n"
      "Very nice");
  LintStatusTest test = {
      "test-rule",
      "http://foobar",
      "some/path/to/somewhere.fvg",
      text,
      {{"reason1 @",
        TokenInfo(dont_care_tag, text.substr(0, 5)),
        "some/path/to/somewhere.fvg:1:1-5: reason1 @ http://foobar [test-rule]",
        {}},
       {"reason2",
        TokenInfo(dont_care_tag, text.substr(6, 2)),
        "some/path/to/somewhere.fvg:1:7-8: reason2 http://foobar [test-rule]",
        {TokenInfo(dont_care_tag, text.substr(0, 5))}},
       {"reason3 \\@",
        TokenInfo(dont_care_tag, text.substr(8, 2)),
        "some/path/to/somewhere.fvg:1:9-10: reason3 @ http://foobar "
        "[test-rule]",
        {TokenInfo(dont_care_tag, text.substr(0, 5))}},
       {"reason4 @",
        TokenInfo(dont_care_tag, text.substr(15, 4)),
        "some/path/to/somewhere.fvg:1:16:2:1: reason4 "
        "some/path/to/somewhere.fvg:1:1 http://foobar [test-rule]",
        {TokenInfo(dont_care_tag, text.substr(0, 5))}},
       {"@ reason5 @",
        TokenInfo(dont_care_tag, text.substr(21, 4)),
        "some/path/to/somewhere.fvg:2:4-7: some/path/to/somewhere.fvg:1:10 "
        "reason5 some/path/to/somewhere.fvg:2:4 http://foobar [test-rule]",
        {TokenInfo(dont_care_tag, text.substr(9, 4)),
         TokenInfo(dont_care_tag, text.substr(21, 4))}}}};

  RunLintStatusTest(test);
}

TEST(LintRuleStatusFormatterTest, NoOutput) {
  SymbolPtr root = Node();
  LintStatusTest test = {"cool-rule",
                         "http://example.com/svstyle",
                         "some/path/to/somewhere.fvg",
                         "This is some code\n"
                         "That you are looking at right now\n"
                         "It is nice code, make no mistake\n"
                         "Very nice",
                         {}};

  RunLintStatusTest(test);
}

void RunLintStatusesTest(const LintStatusTest &test, bool show_context) {
  // Dummy tree so we have something for test cases to point at
  SymbolPtr root = Node();

  std::vector<LintRuleStatus> statuses;
  LintRuleStatus status0;
  status0.url = test.url;
  status0.lint_rule_name = test.rule_name;

  LintRuleStatus status1;
  status1.url = test.url;
  status1.lint_rule_name = test.rule_name;

  ASSERT_EQ(test.violations.size(), 2);

  // Insert the violations in the wrong order
  status0.violations.insert(
      LintViolation(test.violations[1].token, test.violations[1].reason));

  status1.violations.insert(
      LintViolation(test.violations[0].token, test.violations[0].reason));

  statuses.push_back(status0);
  statuses.push_back(status1);

  std::ostringstream ss;

  LintStatusFormatter formatter(test.text);
  const std::vector<absl::string_view> lines;
  if (!show_context) {
    formatter.FormatLintRuleStatuses(&ss, statuses, test.text, test.path, {});
  } else {
    formatter.FormatLintRuleStatuses(&ss, statuses, test.text, test.path,
                                     absl::StrSplit(test.text, '\n'));
    std::cout << ss.str() << std::endl;
  }
  std::vector<std::string> result_parts;
  if (!show_context) {
    result_parts = absl::StrSplit(ss.str(), '\n');
  } else {
    result_parts = absl::StrSplit(ss.str(), "^\n");
  }

  auto part_iterator = result_parts.begin();

  for (const auto &violation_test : test.violations) {
    EXPECT_EQ(*part_iterator, violation_test.expected_output);
    part_iterator++;
  }
}

TEST(LintRuleStatusFormatterTest, MultipleStatusesSimpleOutput) {
  SymbolPtr root = Node();
  static const int dont_care_tag = 0;
  constexpr absl::string_view text(
      "This is some code\n"
      "That you are looking at right now\n"
      "It is nice code, make no mistake\n"
      "Very nice");
  LintStatusTest test = {
      "test-rule",
      "http://foobar",
      "some/path/to/somewhere.fvg",
      text,
      {{"reason1", TokenInfo(dont_care_tag, text.substr(0, 5)),
        "some/path/to/somewhere.fvg:1:1-5: reason1 http://foobar [test-rule]"},
       {"reason2", TokenInfo(dont_care_tag, text.substr(21, 4)),
        "some/path/to/somewhere.fvg:2:4-7: reason2 http://foobar "
        "[test-rule]"}}};

  RunLintStatusesTest(test, false);
}

TEST(LintRuleStatusFormatterTestWithContext, MultipleStatusesSimpleOutput) {
  SymbolPtr root = Node();
  static const int dont_care_tag = 0;
  constexpr absl::string_view text(
      "This is some code\n"
      "That you are looking at right now\n"
      "It is nice code, make no mistake\n"
      "Very nice");
  LintStatusTest test = {
      "test-rule",
      "http://foobar",
      "some/path/to/somewhere.fvg",
      text,
      {{"reason1", TokenInfo(dont_care_tag, text.substr(0, 5)),
        "some/path/to/somewhere.fvg:1:1-5: reason1 http://foobar "
        "[test-rule]\nThis is some code\n"},
       {"reason2", TokenInfo(dont_care_tag, text.substr(21, 4)),
        "some/path/to/somewhere.fvg:2:4-7: reason2 http://foobar "
        "[test-rule]\nThat you are looking at right now\n   "}}};
  RunLintStatusesTest(test, true);
}

TEST(LintRuleStatusFormatterTestWithContext, PointToCorrectUtf8Char) {
  SymbolPtr root = Node();
  static const int dont_care_tag = 0;
  constexpr absl::string_view text("äöüß\n");
  //                                ^ä^ü
  LintStatusTest test = {
      "rule",
      "URL",
      "some/file.sv",
      text,
      {{"reason1", TokenInfo(dont_care_tag, text.substr(0, 2)),
        "some/file.sv:1:1: reason1 URL "
        "[rule]\näöüß\n"},
       {"reason2", TokenInfo(dont_care_tag, text.substr(strlen("äö"), 2)),
        "some/file.sv:1:3: reason2 URL "
        "[rule]\näöüß\n  "}}};
  RunLintStatusesTest(test, true);
}

TEST(AutoFixTest, ValidUseCases) {
  //                                       0123456789abcdef
  static constexpr absl::string_view text("This is an image");

  // AutoFix(ReplacementEdit)
  const AutoFix singleEdit("e", {text.substr(5, 2), "isn't"});
  EXPECT_EQ(singleEdit.Apply(text), "This isn't an image");

  const AutoFix singleInsert("i", {text.substr(16, 0), "."});
  EXPECT_EQ(singleInsert.Apply(text), "This is an image.");

  // AutoFix()
  AutoFix fixesCollection;
  EXPECT_TRUE(fixesCollection.AddEdits(singleEdit.Edits()));
  EXPECT_TRUE(fixesCollection.AddEdits(singleInsert.Edits()));
  EXPECT_EQ(fixesCollection.Apply(text), "This isn't an image.");
  EXPECT_TRUE(fixesCollection.AddEdits({{text.substr(0, 0), "Hello. "}}));
  EXPECT_EQ(fixesCollection.Apply(text), "Hello. This isn't an image.");

  static const int dont_care_tag = 0;
  TokenInfo image_token(dont_care_tag, text.substr(11, 5));

  // AutoFix(ReplacementEdit),
  // ReplacementEdit(const TokenInfo&, const std::string&)
  AutoFix otherCollection("image", {image_token, "🖼"});
  EXPECT_TRUE(otherCollection.AddEdits(fixesCollection.Edits()));
  EXPECT_EQ(otherCollection.Apply(text), "Hello. This isn't an 🖼.");

  // AutoFix(std::initializer_list<ReplacementEdit>)
  const AutoFix multipleEdits("Multi-edit", {
                                                {text.substr(11, 5), "text"},
                                                {text.substr(8, 2), "a"},
                                            });
  EXPECT_EQ(multipleEdits.Apply(text), "This is a text");

  // AutoFix(const AutoFix& other)
  AutoFix copyFix(multipleEdits);
  EXPECT_EQ(copyFix.Apply(text), "This is a text");
  EXPECT_TRUE(copyFix.AddEdits(singleInsert.Edits()));
  EXPECT_EQ(copyFix.Apply(text), "This is a text.");

  // AutoFix(const AutoFix&& other)
  AutoFix moveFix(std::move(copyFix));
  EXPECT_EQ(moveFix.Apply(text), "This is a text.");

  // AutoFix(const std::string&, ReplacementEdit)
  const AutoFix singleInsertWithDescription("Add dot",
                                            {text.substr(16, 0), "."});
  EXPECT_EQ(singleInsertWithDescription.Apply(text), "This is an image.");
  EXPECT_EQ(singleInsertWithDescription.Description(), "Add dot");

  // AutoFix(const std::string&, std::initializer_list<ReplacementEdit>)
  const AutoFix multipleEditsWithDescription("Stop lying",
                                             {
                                                 {text.substr(11, 5), "text"},
                                                 {text.substr(8, 2), "a"},
                                             });
  EXPECT_EQ(multipleEditsWithDescription.Apply(text), "This is a text");
  EXPECT_EQ(multipleEditsWithDescription.Description(), "Stop lying");
}

TEST(AutoFixTest, ConflictingEdits) {
  //                                       0123456789abcdef
  static constexpr absl::string_view text("This is an image");

  AutoFix fixesCollection;
  EXPECT_TRUE(fixesCollection.AddEdits({{text.substr(8, 8), "a text"}}));
  EXPECT_FALSE(fixesCollection.AddEdits({{text.substr(11, 5), "IMAGE"}}));
  EXPECT_FALSE(fixesCollection.AddEdits({{text.substr(8, 1), "A"}}));
  EXPECT_FALSE(fixesCollection.AddEdits({{text.substr(15, 1), "ination"}}));
  EXPECT_EQ(fixesCollection.Apply(text), "This is a text");

  EXPECT_DEATH(AutoFix("overlap", {{text.substr(8, 8), "a text"},  //
                                   {text.substr(11, 5), "IMAGE"}}),
               "Edits must not overlap");

  EXPECT_DEATH(AutoFix("overlap", {{text.substr(8, 8), "a text"},  //
                                   {text.substr(8, 1), "A"}}),
               "Edits must not overlap");

  EXPECT_DEATH(AutoFix("overlap", {{text.substr(8, 8), "a text"},  //
                                   {text.substr(15, 1), "ination"}}),
               "Edits must not overlap");
}

TEST(LintViolationTest, ViolationWithAutoFix) {
  static constexpr absl::string_view text("This is an image");
  static const int dont_care_tag = 0;
  const TokenInfo an_image_token(dont_care_tag, text.substr(8, 5));

  const LintViolation no_fixes(an_image_token, "No, it's not.");
  EXPECT_TRUE(no_fixes.autofixes.empty());

  const LintViolation single_fix(
      an_image_token, "No, it's not.",
      {
          AutoFix("Replace with 'a text'", {an_image_token, "a text"}),
      });
  EXPECT_EQ(single_fix.autofixes.size(), 1);

  const LintViolation multiple_fixes(
      an_image_token, "No, it's not.",
      {
          AutoFix("Replace with 'a text'", {an_image_token, "a text"}),
          AutoFix("Add waiver comment",
                  {text.substr(0, 0), "// verilog_lint: waive no-lying\n"}),
      });
  EXPECT_EQ(multiple_fixes.autofixes.size(), 2);
}

}  // namespace
}  // namespace verible
