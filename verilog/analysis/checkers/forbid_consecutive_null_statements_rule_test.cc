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

#include "verilog/analysis/checkers/forbid_consecutive_null_statements_rule.h"

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

TEST(ForbidConsecutiveNullStatementsRule, FunctionFailures) {
  auto kToken = ';';
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\nalways_ff @(posedge foo) ; endmodule\n"},
      {"module m;\nalways_ff @(posedge foo) ;", {kToken, ";"}, " endmodule\n"},
      {"module m;\nalways_ff @(posedge foo) ;",
       {kToken, ";"},
       {kToken, ";"},
       " endmodule\n"},

      {"module m;\nalways_ff @(posedge foo) ; /* comment */ ",
       {kToken, ";"},
       " endmodule\n"},
      {"module m;\nalways_ff @(posedge foo) ; /* comment */ ",
       {kToken, ";"},
       " /* comment */ ",
       {kToken, ";"},
       " endmodule\n"},

      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\ninitial begin ; end\nendmodule"},
      {"module m;\ninitial begin ;", {kToken, ";"}, " end\nendmodule"},
      {"module m;\ninitial begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendmodule"},

      {"module m;\ninitial begin for (;;) ; end\nendmodule"},
      {"module m;\ninitial begin for (;;) ;", {kToken, ";"}, " end\nendmodule"},
      {"module m;\ninitial begin for (;;) ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendmodule"},

      {"module m;\ninitial begin for (; /* comment */ ;) ; end\nendmodule"},
      {"module m;\ninitial begin for (; /* comment */ ;) ; /* comment */ ",
       {kToken, ";"},
       " end\nendmodule"},
      {"module m;\ninitial begin for (; /* comment */ ;) ; /* comment */ ",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendmodule"},

      {"function foo;\nendfunction\n"},
      {"function foo;\nbegin end\nendfunction\n"},
      {"function foo;\nbegin ; end\nendfunction\n"},
      {"function foo;\nbegin ;", {kToken, ";"}, " end\nendfunction\n"},
      {"function foo;\nbegin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendfunction\n"},

      {"function foo;\nbegin for (;;) ; end\nendfunction\n"},
      {"function foo;\nbegin for (;;) ;", {kToken, ";"}, " end\nendfunction\n"},
      {"function foo;\nbegin for (;;) ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendfunction\n"},

      {"function foo;\nbegin for (;;) begin ; end end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end end\nendfunction\n"},

      {"function foo;\nbegin for (;;) begin ; end end\nendfunction ; \n"},
      {"function foo;\nbegin for (;;) begin ; end ; end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ; end ;",
       {kToken, ";"},
       " end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ; end ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendfunction\n"},

      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end ; end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       " end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendfunction\n"},

      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end ; end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       " end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendfunction\n"},

      {"function foo;\nbegin for (;;) begin for (;;) begin end end "
       "end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin for (;;) begin ; end end "
       "end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin for (;;) begin ;",
       {kToken, ";"},
       " end end end\nendfunction\n"},
      {"function foo;\nbegin for (;;) begin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end end end\nendfunction\n"},

      {"task foo;\nendtask\n"},
      {"task foo;\nbegin end\nendtask\n"},
      {"task foo;\nbegin ; end\nendtask\n"},
      {"task foo;\nbegin ;", {kToken, ";"}, " end\nendtask\n"},
      {"task foo;\nbegin ;", {kToken, ";"}, {kToken, ";"}, " end\nendtask\n"},

      {"task foo;\nbegin for (;;) ; end\nendtask\n"},
      {"task foo;\nbegin for (;;) ;", {kToken, ";"}, " end\nendtask\n"},
      {"task foo;\nbegin for (;;) ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendtask\n"},

      {"task foo;\nbegin for (;;) begin ; end end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end end\nendtask\n"},

      {"task foo;\nbegin for (;;) begin ; end end\nendtask ; \n"},
      {"task foo;\nbegin for (;;) begin ; end ; end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ; end ;",
       {kToken, ";"},
       " end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ; end ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendtask\n"},

      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end ; end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       " end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendtask\n"},

      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end ; end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       " end\nendtask\n"},
      {"task foo;\nbegin for (;;) begin ;",
       {kToken, ";"},
       {kToken, ";"},
       " end ;",
       {kToken, ";"},
       {kToken, ";"},
       " end\nendtask\n"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbidConsecutiveNullStatementsRule>(
      kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
