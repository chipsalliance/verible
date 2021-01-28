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

#include "verilog/analysis/checkers/always_comb_blocking_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_treebuilder_utils.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(AlwaysCombBlockingRule, FunctionFailures) {
  auto kToken = TK_LE;
  const std::initializer_list<LintTestCase> kAlwaysCombBlockingTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\nalways_comb a ", {kToken, "<="}, " b;\nendmodule"},
      {"module m;\nalways_comb a ", {kToken, "<="}, " b <= c;\nendmodule"},
      {"module m;\nalways_comb begin a ", {kToken, "<="}, " b; end\nendmodule"},
      {"module m;\nalways_comb begin if (sel == 0) a ",
       {kToken, "<="},
       " b; "
       "else a ",
       {kToken, "<="},
       " 1'b0; end\nendmodule"},
      {"module m;\nalways_comb begin\n`ifdef RST\nf ",
       {kToken, "<="},
       " g;\n`else\nf ",
       {kToken, "<="},
       " 1'b1;\n`endif\nend\nendmodule"},
      {"module m;\nalways_comb a = b;\nendmodule"},
      {"module m;\nalways_comb begin a = b; end\nendmodule"},
      {"module m;\nalways_ff a <= b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin a <= b; end\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, AlwaysCombBlockingRule>(
      kAlwaysCombBlockingTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
