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

#include "verilog/analysis/checkers/suggest_parentheses_rule.h"

#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(SuggestParenthesesTest, Various) {
  constexpr int kTag = 293;  // don't care

  const std::initializer_list<LintTestCase> kSuggestParenthesesTestCases = {
      // Non rule violation cases.
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a : b;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? (condition_b? a : b) : c;",
       "\nendmodule"},
      {"module m;\n",
       "assign foo = condition_a? (condition_b? a : b) : (condition_c? c : d);",
       "\nendmodule"},
      {"module m;\n",
       "assign foo = condition_a? (condition_b? (condition_c? a : b) : c) : d;",
       "\nendmodule"},
      {"module m;\n",
       "assign foo = condition_a? (condition_b? a : b) : condition_c? c : d;",
       "\nendmodule"},
      {"module m;\n", "parameter foo = condition_a? a : b;", "\nendmodule"},
      {"module m;\n",
       "always @(posedge clk) begin\n left <= condition_a? a : b; \nend",
       "\nendmodule"},
      {"function f;\n g = h(condition_a? a : b); \nendfunction"},
      {"module m;\n", "always @(posedge clk)\n", "case (condition_a? a : b)\n",
       "default :;\n", "endcase\n", "\nendmodule"},
      // Rule Violation cases.
      {"module m;\n assign foo = condition_a? ",
       {kTag, "condition_b ? a : b"},
       " : c;\nendmodule"},
      {"module m;\n",
       "parameter foo = condition_a? ",
       {kTag, "condition_b ? a : b"},
       " : c;",
       "\nendmodule\n"},
      {"module m;\n",
       "always @(posedge clk) begin\n left <= condition_a? ",
       {kTag, "condition_b ? a : b"},
       " : c; \nend",
       "\nendmodule"},
      {"function f;\n g = h(condition_a? ",
       {kTag, "condition_b ? a : b"},
       " : c); \nendfunction"},
      {"module m;\n",
       "always @(posedge clk)\n",
       "case (condition_a? ",
       {kTag, "condition_b ? a : b"},
       " : c)\n",
       "default :;\n",
       "endcase\n",
       "\nendmodule"},
      // Other expression types for true case expression.
      {"module m;\n", "assign foo = condition_a? a + b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a - b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a * b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a / b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a << b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a >> b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a & b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a && b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a | b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a || b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a == b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a > b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a < b : c;", "\nendmodule"},
      {"module m;\n", "assign foo = condition_a? a ^ b : c;", "\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, SuggestParenthesesRule>(
      kSuggestParenthesesTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
