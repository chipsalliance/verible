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

#include "verilog/analysis/checkers/generate_label_rule.h"

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

constexpr int kToken = TK_begin;

TEST(GenerateLabelRuleTest, Various) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module m;\n"
       "endmodule\n"},
      // Test incorrect code
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) ",
       {kToken, "begin"},
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) ",
       {kToken, "begin"},
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end else ",
       {kToken, "begin"},
       "\n"
       "  always @(negedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "genvar ii;\n"
       "generate\n"
       "  for (ii = 0; ii < NumberOfBuses; ii++) ",
       {kToken, "begin"},
       "\n"
       "    my_bus #(.index(ii)) i_my_bus (.foo(foo), .bar(bar[ii]));\n"
       "end\n"
       "endgenerate\nendmodule\n"},

      // Tests correct code
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) begin : label\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) begin : label1\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end else begin : label2\n"
       "  always @(negedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "genvar ii;\n"
       "generate\n"
       "for (ii = 0; ii < NumberOfBuses; ii++) begin : label\n"
       "  my_bus #(.index(ii)) i_my_bus (.foo(foo), .bar(bar[ii]));\n"
       "end\n"
       "endgenerate\nendmodule\n"},
  };

  RunLintTestCases<VerilogAnalyzer, GenerateLabelRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
