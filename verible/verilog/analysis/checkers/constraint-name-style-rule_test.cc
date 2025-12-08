// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/constraint-name-style-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

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

TEST(ConstraintNameStyleRuleTest, VariousPrefixTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"class foo; rand logic a; constraint c_foo { a == 16; } endclass"},
      {"class foo; rand logic a; constraint c_a { a == 16; } endclass"},
      {"class foo; rand logic a; constraint c_foo_bar { a == 16; } endclass"},
      {"class foo; rand logic a; constraint ",
       {kToken, "c_"},
       " { a == 16; } endclass"},
      {"class foo; rand logic a; constraint ",
       {kToken, "no_suffix"},
       " { a == 16; } endclass"},
      {"class foo; rand logic a; constraint ",
       {kToken, "suffix_ok_but_we_want_prefix_c"},
       " { a == 16; } endclass"},
  };
  // Lower snake case, starts with `c_`
  RunConfiguredLintTestCases<VerilogAnalyzer, ConstraintNameStyleRule>(
      kTestCases, "pattern:c+(_[a-z0-9]+)+");
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
