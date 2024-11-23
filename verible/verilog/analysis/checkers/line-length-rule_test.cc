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

#include "verible/verilog/analysis/checkers/line-length-rule.h"

#include <initializer_list>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/text-structure-linter-test-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(LineLengthRuleTest, Configuration) {
  LineLengthRule rule;
  absl::Status status;
  EXPECT_TRUE((status = rule.Configure("")).ok()) << status.message();
  EXPECT_TRUE((status = rule.Configure("length:50")).ok()) << status.message();

  EXPECT_FALSE((status = rule.Configure("foo:42")).ok());
  EXPECT_TRUE(absl::StrContains(status.message(), "supported parameter"));

  EXPECT_FALSE((status = rule.Configure("length:hello")).ok());
  EXPECT_TRUE(absl::StrContains(status.message(), "Cannot parse integer"));

  EXPECT_FALSE((status = rule.Configure("length:-1")).ok());
  EXPECT_TRUE(absl::StrContains(status.message(), "out of range"));
}

// Tests that space-only text passes.
TEST(LineLengthRuleTest, AcceptsText) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {" "},
      {"\n"},
      {" \n\n"},
      {"module foo;\nendmodule\n"},
      {
          "aaaaaaaaaa"
          "bbbbbbbbbb"
          "cccccccccc"
          "dddddddddd"
          "eeeeeeeeee"
          "ffffffffff"
          "gggggggggg"
          "hhhhhhhhhh"
          "iiiiiiiiii"
          "jjjjjjjjjj\n"  // 100 characters
      },
      {
          "// aaaaaaa"
          "bbbbbbbbbb"
          "cccccccccc"
          "dddddddddd"
          "eeeeeeeeee"
          "ffffffffff"
          "gggggggggg"
          "hhhhhhhhhh"
          "iiiiiiiiii"
          "jjjjjjjjjj\n"  // 100 characters
      },
      {
          "// aaaaaaa"
          "bbbbbbbbbb"
          "cccccccccc"
          "dddddddddd"
          "eeeeeeeeee"
          "ffffffffff"
          "gggggggggg"
          "hhhhhhhhhh"
          "iiiiiiiiii"
          "jjjjjjjjjj"
          "k\n"  // 101 characters, in a single comment, forgiven
      },
      {
          "    // aaa"  // indented comment
          "bbbbbbbbbb"
          "cccccccccc"
          "dddddddddd"
          "eeeeeeeeee"
          "ffffffffff"
          "gggggggggg"
          "hhhhhhhhhh"
          "iiiiiiiiii"
          "jjjjjjjjjj"
          "k\n"  // 101 characters, in a single indented comment, forgiven
      },
  };
  RunLintTestCases<VerilogAnalyzer, LineLengthRule>(kTestCases);
}

// Tests that exceptional cases for long lines are allowed.
TEST(LineLengthRuleTest, AcceptsTextExceptions) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"`ifdef "
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"},
      {"    `ifdef "  // ignore leading spaces
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"},
      {"`ifndef "
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"},
      {"`endif  //"
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
       "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n"},
      {"`include \""
       "AAAAAAAA/AAAAAAAAAAAAAAAAAAA/AAAAAAAAAAAAAAAAAAAAA/"
       "AAAAAAAAA/AAAAAAA/AAAAAAAAAAAAAAAA/AAAAAAAAAAAA.svh\"\n"},
      {"parameter foo = \""
       "AAAAAAAA/AAAAAAAAAAAAAAAAAAA/AAAAAAAAAAAAAAAAAAAAA/"
       "AAAAAAAAA/AAAAAAAAA/AAAAAAAA\";  // ri lint_check_waive RULE_NAME\n"},
      {"parameter bar = \""
       "AAAAAAAA/AAAAAAAAAAAAAAAAAAA/AAAAAAAAAAAAAAAAAAAAA/"
       "AAAAAAAAA/AAAAAAAAA/AAAAAA\";  /* ri lint_check_waive RULE_NAME */\n"},
      {"parameter foo = \""
       "AAAAAAAA/AAAAAAAAAAAAAAAAAAA/AAAAAAAAAAAAAAAAAAAAA/"
       "AAAAAAAAA/AAAAAAAAA/AAAAAAAA\";  // verilog_lint: blah blah\n"},
      {"one_long_token_gooooooooooooooooo00000000000ooooooooooooooooooooo"
       "ooooooooooooooooooooooooooooogle_com\n"},
      {
          "// http://www.foooooooooooooooooooooooooooooooooooooooooooooooooo"
          "ooooooooooooooooooooooooooooogle.com\n"  // one token inside EOL
                                                    // comment
      },
      {"//        gooooooooooooooooo00000000000oooooooooooooooooooooooooo"
       "ooooooooooooooooooooooooooooogle_com\n"},
  };
  // Make sure that these lines would normally be flagged by this rule.
  for (const auto &test : kTestCases) {
    VLOG(1) << "TEST: " << test.code;
    EXPECT_TRUE(test.code.length() > LineLengthRule::kDefaultLineLength)
        << "code:\n"
        << test.code;
  }
  RunLintTestCases<VerilogAnalyzer, LineLengthRule>(kTestCases);
}

// Tests that length violations are caught.
TEST(LineLengthRuleTest, RejectsText) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"aaaaaaaaaa"
       "bbbbbbbbbb"
       "cccccccccc"
       "dddddddddd"
       "eeeeeeeeee"
       "ffff fffff"  // intentional space, so this is more than one token
       "gggggggggg"
       "hhhhhhhhhh"
       "iiiiiiiiii"
       "jjjjjjjjjj",  // 100 chars
       {TK_OTHER, "k"},
       "\n"},  // 101 characters
      {"aaaaaaaaaa"
       "bbbbbbbbbb"
       "cccccccccc"
       "dddddddddd"
       "eeeeeeeeee"
       "ffffffffff"
       "gggggggggg"
       "hhhhhhhhhh"
       "iiiiiiiiii"  // 90 chars
       " // not a ",
       {TK_OTHER, "waiver comment"},
       "\n"},
  };
  RunLintTestCases<VerilogAnalyzer, LineLengthRule>(kTestCases);
}

TEST(LineLengthRuleTest, RejectsTextConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"aaaaaaaaaabbbbbbbbbbcccc cccccdddddddddd", {TK_OTHER, "X"}, "\n"},
      {"aaaaaaaaaabbbbbbbbbbcccc cccccdddddddddd\n"},  // shorter line ok.
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, LineLengthRule>(kTestCases,
                                                              "length:40");
}

#if 0
TEST(LineLengthRuleTest, Encrypted) {
  const LintTestCase kTestCases[] = {
      {// middle line is too long, but not tokenized by the lexer
       // until b/134180314 is addressed.
       R"(`pragma protect begin_protected
`pragma protect key_keyowner = "Cyberdyne Technologies", key_keyname = "cdn_rsa_key", key_method = "rsa"
`pragma protect end_protected
)",
       {/* for now, don't care about violations on encrypted lines */}},
  };
  RunLintTestCases<VerilogAnalyzer, LineLengthRule>(kTestCases);
}
#endif

}  // namespace
}  // namespace analysis
}  // namespace verilog
