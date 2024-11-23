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

#include "verible/verilog/analysis/checkers/always-ff-non-blocking-rule.h"

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
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(
      kAlwaysFFNonBlockingTestCases,
      "catch_modifying_assignments:on;waive_for_locals:on");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
