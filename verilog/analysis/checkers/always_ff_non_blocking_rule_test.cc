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

#include "verilog/analysis/checkers/always_ff_non_blocking_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunApplyFixCases;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(AlwaysFFNonBlockingRule, LegacyFailures) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kAlwaysFFNonBlockingTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) ", {kToken, "a"}, " = b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin ",
       {kToken, "a"},
       " = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin if(sel == 0) ",
       {kToken, "a"},
       "= b; "
       "else ",
       {kToken, "a"},
       "= 1'b0; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n`ifdef RST\n",
       {kToken, "f"},
       "= g;\n`else\n",
       {kToken, "f"},
       "= 1'b1;\n`endif\nend\nendmodule"},
      {"module m;\nalways_ff @(posedge c) a <= b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin a <= b; end\nendmodule"},
      {"module m;\nalways_comb a = b;\nendmodule"},
      {"module m;\nalways_comb begin a = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n  for (int i=0 ; i<3 ; i=i+1) "
       "begin\n"
       "    a <= b;\nend\nend\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n  for (int i=0 ; i<3 ; i=i+1) "
       "begin\n"
       "    ",
       {kToken, "a"},
       "= b;\n"
       "end\nend\nendmodule"},
      // Legacy behavior: Do NOT waive on locals
      {"module m;\nalways_ff @(posedge c) begin static type(b) a; ",
       {kToken, "a"},
       " = b; end\nendmodule"},
      // Legacy behavior: Miss modifying operators
      {"module m;\nalways_ff @(posedge c) begin if(sel == 0) a++; else a &= b; "
       "end\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(
      kAlwaysFFNonBlockingTestCases);
}

TEST(AlwaysFFNonBlockingRule, LocalWaiving) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kAlwaysFFNonBlockingTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) ", {kToken, "a"}, " = b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin ",
       {kToken, "a"},
       " = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin automatic logic a = b; "
       "end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin static logic a; a = b; "
       "end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin static type(b) a; a = b; "
       "end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin if(sel == 0) ",
       {kToken, "a"},
       "++; "
       "else ",
       {kToken, "a"},
       "&= b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n`ifdef RST\n",
       {kToken, "f"},
       "= g;\n`else\n",
       {kToken, "f"},
       "= 1'b1;\n`endif\nend\nendmodule"},
      {"module m;\nalways_ff @(posedge c) a <= b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin a <= b; end\nendmodule"},
      {"module m;\nalways_comb a = b;\nendmodule"},
      {"module m;\nalways_comb begin a = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n  for (int i=0 ; i<3 ; i=i+1) "
       "begin\n"
       "    a <= b;\nend\nend\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n  for (int i=0 ; i<3 ; i=i+1) "
       "begin\n"
       "    ",
       {kToken, "a"},
       "= b;\n"
       "end\nend\nendmodule"},
      // Waive, 'a' is a local declaration
      {"module m;\nalways_ff @(posedge c) begin static type(b) a; a = b; "
       "end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin static type(b) a; a++; "
       "end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin static type(b) a; ++a; "
       "end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin static type(b) a; a &= b; "
       "end\nendmodule"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(
      kAlwaysFFNonBlockingTestCases,
      "catch_modifying_assignments:on;waive_for_locals:on");
}

TEST(AlwaysFFNonBlockingRule, AutoFixDefault) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      // Check we're not waiving local variables
      {"module m;\nalways_ff begin reg k; k = 1; end\nendmodule",
       "module m;\nalways_ff begin reg k; k <= 1; end\nendmodule"},
      // Check we're ignoring modifying assignments
      {"module m;\nalways_ff begin k &= 1; k = 1; end\nendmodule",
       "module m;\nalways_ff begin k &= 1; k <= 1; end\nendmodule"},
  };

  RunApplyFixCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(kTestCases, "");
}

TEST(AlwaysFFNonBlockingRule, AutoFixCatchModifyingAssignments) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      {"module m;\nalways_ff begin k &= 1; end\nendmodule",
       "module m;\nalways_ff begin k <= k & 1; end\nendmodule"},
      {"module m;\nalways_ff begin k &= a; end\nendmodule",
       "module m;\nalways_ff begin k <= k & a; end\nendmodule"},
      {"module m;\nalways_ff begin k |= (2 + 1); end\nendmodule",
       "module m;\nalways_ff begin k <= k | (2 + 1); end\nendmodule"},
      {"module m;\nalways_ff begin k |= 2 + (1); end\nendmodule",
       "module m;\nalways_ff begin k <= k | (2 + (1)); end\nendmodule"},
      {"module m;\nalways_ff begin k |= (2) + (1); end\nendmodule",
       "module m;\nalways_ff begin k <= k | ((2) + (1)); end\nendmodule"},
      {"module m;\nalways_ff begin k *= 2 + 1; end\nendmodule",
       "module m;\nalways_ff begin k <= k * (2 + 1); end\nendmodule"},
      {"module m;\nalways_ff begin a++; end\nendmodule",
       "module m;\nalways_ff begin a <= a + 1; end\nendmodule"},
      {"module m;\nalways_ff begin ++a; end\nendmodule",
       "module m;\nalways_ff begin a <= a + 1; end\nendmodule"},
  };

  RunApplyFixCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(
      kTestCases, "catch_modifying_assignments:on");
}

TEST(AlwaysFFNonBlockingRule, AutoFixDontBreakCode) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      // We can't fix 'k &= 1', because it would affect
      // 'p <= k'
      {"module m;\nalways_ff begin\n"
       "k &= 1;\n"
       "p <= k;\n"
       "a++;"
       "end\nendmodule",
       "module m;\nalways_ff begin\n"
       "k &= 1;\n"
       "p <= k;\n"
       "a <= a + 1;"
       "end\nendmodule"},
      // Correctly fix despite there is a reference to 'k' in the rhs
      {"module m;\nalways_ff begin k = k + 1; end\nendmodule",
       "module m;\nalways_ff begin k <= k + 1; end\nendmodule"},
      // Dont autofix inside expressions
      {"module m;\nalways_ff begin k <= p++; p++; end\nendmodule",
       "module m;\nalways_ff begin k <= p++; p <= p + 1; end\nendmodule"}};

  RunApplyFixCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(
      kTestCases, "catch_modifying_assignments:on");
}

TEST(AlwaysFFNonBlockingRule, AutoFixWaiveLocals) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      // Check that we're correctly waiving the 'p = 0' as it is a local
      // variable
      {"module m;\nalways_ff begin reg p; p = 0; k = 1; end\nendmodule",
       "module m;\nalways_ff begin reg p; p = 0; k <= 1; end\nendmodule"},
  };

  RunApplyFixCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(
      kTestCases, "waive_for_locals:on");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
