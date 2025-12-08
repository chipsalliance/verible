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

#include "verible/verilog/analysis/checkers/generate-label-prefix-rule.h"

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
using verible::RunLintTestCases;

TEST(GenerateLabelPrefixRuleTest, Various) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module m;\n"
       "endmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) ",
       "begin",
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) "
       "begin : g_label"
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) "
       "begin : gen_label"
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) "
       "begin : gen_label"
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end : gen_label_also\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "begin\n"
       "  initial begin : g_ini\n"
       "    if (1) begin : gen_if\n"
       "    end : gen_endif\n"
       "  end : g_endini\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "begin\n",
       "  initial begin : not_wrong\n"
       "    if (1) begin : gen_if\n"
       "    end : gen_endif\n"
       "  end : g_endini\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "begin\n",
       "  initial begin : not_wrong\n"
       "    if (1) begin : gen_if\n"
       "    end : also_not_wrong\n"
       "  end : g_endini\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module mod_a;\n"
       "genvar i;\n"
       "for (i=0; i<5; i=i+1) begin : gen_a\n"
       "end\n"
       "endmodule\n"},
      {"module mod_a;\n"
       "genvar i;\n"
       "for (i=0; i<5; i=i+1) begin : g_a\n"
       "end\n"
       "endmodule\n"},
      {"module mod_a;\n"
       "genvar i;\n"
       "for (i=0; i<5; i=i+1) begin : g_a\n"
       "end : g_b\n"
       "endmodule\n"},
      {"module mod_a;\n"
       "genvar i;\n"
       "for (i=0; i<5; i=i+1) begin\n"
       "end : g_b\n"
       "endmodule\n"},
      {"module mod_b;\n"
       "parameter x = 0;\n"
       "if (x == 0) begin : gen_i\n"
       "end\n"
       "endmodule\n"},
      {"module mod_b;\n"
       "parameter x = 0;\n"
       "if (x == 0) begin : gen_i\n"
       "end : g_end\n"
       "endmodule\n"},
      {"module mod_b;\n"
       "parameter x = 0;\n"
       "if (x == 0) begin : gen_i\n"
       "end : g_end\n"
       "else begin : ",
       {SymbolIdentifier, "jen_i"},
       "\n"
       "end : ",
       {SymbolIdentifier, "j_end"},
       "\n"
       "endmodule\n"},
      {"module mod_b;\n"
       "parameter x = 0;\n"
       "if (x == 0) begin\n"
       "end : g_end\n"
       "endmodule\n"},
      {"module mod_b;\n"
       "parameter x = 0;\n"
       "case (x)\n"
       "  0, 1, 1:\n"
       "     begin : does_not_apply\n"
       "     end\n"
       "endcase\n"
       "endmodule\n"},
      {"module m;\n"
       "initial begin : OkNotAGenerateLabel\n"
       "end : OkNotAGenerateLabel\n"
       "endmodule\n"},
      // Test incorrect code
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) "
       "begin : ",
       {SymbolIdentifier, "k_label"},
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) "
       "begin",
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end : ",
       {SymbolIdentifier, "genwithoutunderscore"},
       "\nendgenerate\nendmodule\n"},
      {"module m;\n"
       "generate\n"
       "if (TypeIsPosedge) ",
       "begin : ",
       {SymbolIdentifier, "k_label"},
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end : ",
       {SymbolIdentifier, "genwithoutunderscore"},
       "\nendgenerate\nendmodule\n"},
      {"module m;\n"
       "if (x) begin : ",
       {SymbolIdentifier, "bad_label"},
       "\n"
       "end : ",
       {SymbolIdentifier, "bad_label"},
       "\n"
       "endmodule\n"},
      // Incorrect code with more blocks
      {"module m;\n"
       "generate\n"
       "begin : ",
       {SymbolIdentifier, "wrong"},
       "\n"
       "  initial begin : g_ini\n"
       "    if (1) begin : gen_if\n"
       "    end : gen_endif\n"
       "  end : g_endini\n"
       "end\n"
       "endgenerate\nendmodule\n"},
      // Incorrect code without the generate statements
      {"module mod_a;\n"
       "genvar i;\n"
       "for (i=0; i<5; i=i+1) begin : ",
       {SymbolIdentifier, "missing_prefix"},
       "\n"
       "end\n"
       "endmodule\n"},
      {"module mod_a;\n"
       "genvar i;\n"
       "for (i=0; i<5; i=i+1) begin : ",
       {SymbolIdentifier, "missing_prefix"},
       "\n"
       "end : ",
       {SymbolIdentifier, "missing_prefix"},
       "\n"
       "endmodule\n"},
      {"module mod_a;\n"
       "genvar i;\n"
       "for (i=0; i<5; i=i+1) begin : gen_ok\n"
       "end : ",
       {SymbolIdentifier, "missing_prefix"},
       "\n"
       "endmodule\n"},
  };

  RunLintTestCases<VerilogAnalyzer, GenerateLabelPrefixRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
