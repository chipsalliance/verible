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

#include "verilog/analysis/checkers/constraint_name_style_rule.h"

#include <initializer_list>

#include "absl/strings/match.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(ConstraintNameStyleRuleTest, ConfigurationPass) {
  ConstraintNameStyleRule rule;
  absl::Status status;
  EXPECT_TRUE((status = rule.Configure("name_regex:[a-z]")).ok())
      << status.message();
}

TEST(ConstraintNameStyleRuleTest, ConfigurationFail) {
  ConstraintNameStyleRule rule;
  absl::Status status;
  EXPECT_FALSE((status = rule.Configure("bad_name_regex:")).ok())
      << status.message();

  EXPECT_FALSE((status = rule.Configure("name_regex:[a-z")).ok())
      << status.message();
  EXPECT_TRUE(absl::StrContains(status.message(), "Invalid regex specified"));
}

TEST(ConstraintNameStyleRuleTestConfiguredRegex,
     ValidInterfaceDeclarationNames) {
  const absl::string_view regex = "name_regex:.*_i";
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module foo; logic a; endmodule"},
      {"class foo; logic a; endclass"},
      {"class foo; rand logic a; constraint _foo_i { a < 16; } endclass"},
      {"class foo; rand logic a; constraint foo_i { a < 16; } endclass"},
      {"class foo; rand logic a; constraint bar_i { a >= 16; } endclass"},
      {"class foo; rand logic a; constraint foo_bar_i { a == 16; } endclass"},
      {"class foo; rand logic a; constraint foo2_i { a == 16; } endclass"},
      {"class foo; rand logic a; constraint foo_2_bar_i { a == 16; } endclass"},

      /* Ignore out of line definitions */
      {"constraint classname::constraint_i { a <= b; }"},
      {"constraint classname::MY_CONSTRAINT { a <= b; }"},
      {"constraint classname::MyConstraint { a <= b; }"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, ConstraintNameStyleRule>(
      kTestCases, regex);
}

TEST(ConstraintNameStyleRuleTestConfiguredRegex, RejectTests) {
  const absl::string_view regex = "name_regex:[a-zA-Z]*_i";
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"class foo; rand logic a; constraint ",
       {kToken, "foo_c"},
       " { a == 16; } endclass"},
      {"class foo; rand logic a; constraint ",
       {kToken, "foo12_i"},
       " { a == 16; } endclass"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, ConstraintNameStyleRule>(
      kTestCases, regex);
}

// Tests that ConstraintNameStyleRule correctly accepts valid names.
TEST(ConstraintNameStyleRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module foo; logic a; endmodule"},
      {"class foo; logic a; endclass"},
      {"class foo; rand logic a; constraint foo_c { a < 16; } endclass"},
      {"class foo; rand logic a; constraint bar_c { a >= 16; } endclass"},
      {"class foo; rand logic a; constraint foo_bar_c { a == 16; } endclass"},
      {"class foo; rand logic a; constraint foo2_c { a == 16; } endclass"},
      {"class foo; rand logic a; constraint foo_2_bar_c { a == 16; } endclass"},

      /* Ignore out of line definitions */
      {"constraint classname::constraint_c { a <= b; }"},
      {"constraint classname::MY_CONSTRAINT { a <= b; }"},
      {"constraint classname::MyConstraint { a <= b; }"},
  };
  RunLintTestCases<VerilogAnalyzer, ConstraintNameStyleRule>(kTestCases);
}

// Tests that ConstraintNameStyleRule rejects invalid names.
TEST(ConstraintNameStyleRuleTest, RejectTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"class foo; rand logic a; constraint ",
       {kToken, "_c"},
       " { a == 16; } endclass"},
      {"class foo; rand logic a; constraint ",
       {kToken, "no_suffix"},
       " { a == 16; } endclass"},

      {"class foo; rand logic a; constraint ",
       {kToken, "WrongName"},
       " { a == 16; } endclass"},
      {"class foo; rand logic a; constraint ",
       {kToken, "WrongName_c"},
       " { a == 16; } endclass"},

      {"class foo; rand logic a; constraint ",
       {kToken, "wrong_name_C"},
       " { a == 16; } endclass"},

      {"class foo; rand logic a; constraint ",
       {kToken, "WRONG_NAME"},
       " { a == 16; } endclass"},

      {"class foo; rand logic a; constraint ",
       {kToken, "WRONG_NAME_c"},
       " { a == 16; } endclass"},

      {"class foo; rand logic a; constraint ",
       {kToken, "WRONG_c"},
       " { a == 16; } endclass"},

      {"class foo; rand logic a, b; "
       "constraint ",
       {kToken, "FIRST_C"},
       " { a == 16; }; "
       "constraint ",
       {kToken, "SECOND_C"},
       " {b == 20; } endclass"},

      {"class foo; rand logic a; "
       "constraint ",
       {kToken, "FIRST_C"},
       " { a == 16; } endclass;"
       "class bar; rand logic b; "
       "constraint ",
       {kToken, "SECOND_C"},
       " {b == 20; } endclass"},
  };
  RunLintTestCases<VerilogAnalyzer, ConstraintNameStyleRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
