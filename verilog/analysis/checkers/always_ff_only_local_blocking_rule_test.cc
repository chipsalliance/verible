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

#include "verilog/analysis/checkers/always_ff_only_local_blocking_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_treebuilder_utils.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(AlwaysFFOnlyLocalBlockingRule, FunctionFailures) {
  int constexpr  kToken = SymbolIdentifier;
  std::initializer_list<LintTestCase> const  kAlwaysFFOnlyLocalBlockingTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) ", {kToken, "a"}, " = b;\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin ", {kToken, "a"}, " = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin automatic logic a = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin static logic a; a = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin static type(b) a; a = b; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin if(sel == 0) ", {kToken, "a"}, " = b; "
       "else ", {kToken, "a"}, "= 1'b0; end\nendmodule"},
      {"module m;\nalways_ff @(posedge c) begin\n`ifdef RST\n", {kToken, "f"},
       "= g;\n`else\n", {kToken, "f"},
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
       "    ", {kToken, "a"}, "= b;\n"
       "end\nend\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, AlwaysFFOnlyLocalBlockingRule>(
      kAlwaysFFOnlyLocalBlockingTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
