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

#include "verilog/analysis/checkers/v2001_generate_begin_rule.h"

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

TEST(V2001GenerateBeginRuleTests, BeginBlocks) {
  constexpr int kToken = TK_begin;
  const std::initializer_list<LintTestCase> kTestCases = {
      // Test incorrect code
      {"module m; generate\n",
       {kToken, "begin"},
       "\n"
       "  always @(posedge clk) foo <= bar;\n"
       "end\n"
       "endgenerate\nendmodule"},
      {"module m; generate\n",
       {kToken, "begin"},
       "\n"
       "  foo bar(bq);\n"
       "end\n"
       "endgenerate\nendmodule"},
      {"module m; generate\n",
       {kToken, "begin"},
       "\n"
       "  genvar i;\n"
       "  for (i=0; i<K; i++) begin\n"
       "    foo bar(none);\n"
       "  end\n"
       "end\n"
       "endgenerate\nendmodule"},

      // Tests correct code
      {"module m; endmodule"},
      {""},
      {"module m; generate\n"
       "endgenerate\nendmodule"},
      {"module m; generate\n"
       "  wire bar;\n"
       "endgenerate\nendmodule"},
      {"module m; generate\n"
       "if (TypeIsPosedge) begin : label\n"
       "  foo bar();\n"
       "end\n"
       "endgenerate\nendmodule"},
      {"module m; generate\n"
       "for (genvar i = 0; i<N; ++i) begin : label\n"
       "  foo bar();\n"
       "end\n"
       "endgenerate\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, V2001GenerateBeginRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
