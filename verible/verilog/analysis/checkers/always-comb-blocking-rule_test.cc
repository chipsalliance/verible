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

#include "verible/verilog/analysis/checkers/always-comb-blocking-rule.h"

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
using verible::RunApplyFixCases;
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

TEST(AlwaysCombBlockingTest, AutoFixAlwaysCombBlocking) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      {"module m;\nalways_comb a <= b;\nendmodule",
       "module m;\nalways_comb a = b;\nendmodule"},
      {"module m;\nalways_comb begin a <= b; end\nendmodule",
       "module m;\nalways_comb begin a = b; end\nendmodule"},
      {"module m;\nalways_comb begin if (sel == 0) a <= b; else a = 1'b0; "
       "end\nendmodule",
       "module m;\nalways_comb begin if (sel == 0) a = b; else a = 1'b0; "
       "end\nendmodule"},
      {"module m;\nalways_comb begin\n`ifdef RST\nf <= g;\n`else\nf = "
       "1'b1;\n`endif\nend\nendmodule",
       "module m;\nalways_comb begin\n`ifdef RST\nf = g;\n`else\nf = "
       "1'b1;\n`endif\nend\nendmodule"},
  };

  RunApplyFixCases<VerilogAnalyzer, AlwaysCombBlockingRule>(kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
