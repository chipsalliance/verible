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
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_treebuilder_utils.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(AlwaysFFNonBlockingRule, FunctionFailures) {
  constexpr int kToken = '=';
  const std::initializer_list<LintTestCase> kAlwaysFFNonBlockingTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) a ", {kToken, "="}, " b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin a ",
       {kToken, "="},
       " b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c)  begin if (sel == 0) a ",
       {kToken, "="},
       " b; "
       "else a ",
       {kToken, "="},
       " 1'b0; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n`ifdef RST\nf ",
       {kToken, "="},
       " g;\n`else\nf ",
       {kToken, "="},
       " 1'b1;\n`endif\nend\nendmodule"},
      {"module m;\nalways_ff @(posedge c) a <= b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin a <= b; end\nendmodule"},
      {"module m;\nalways_comb a = b;\nendmodule"},
      {"module m;\nalways_comb begin a = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n  for (int i=0 ; i<3 ; i=i+1) "
       "begin\n"
       "    a <= b;\nend\nend\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n  for (int i=0 ; i<3 ; i=i+1) "
       "begin\n"
       "    a ",
       {kToken, "="},
       " b;\n  end\nend\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, AlwaysFFNonBlockingRule>(
      kAlwaysFFNonBlockingTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
