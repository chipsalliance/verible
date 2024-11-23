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

#include "verible/verilog/analysis/checkers/forbid-consecutive-null-statements-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunApplyFixCases;
using verible::RunLintTestCases;

TEST(ForbidConsecutiveNullStatementsRuleTest, FunctionFailures) {
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

TEST(ForbidConsecutiveNullStatementsRuleTest, ApplyAutoFix) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
    {"module m;\ninitial begin ;; end\nendmodule",
     "module m;\ninitial begin ; end\nendmodule"},
    {"module m;\ninitial begin ;  ; end\nendmodule",
     "module m;\ninitial begin ;   end\nendmodule"},
    {"module m;\ninitial begin ;  /*  */; end\nendmodule",
     "module m;\ninitial begin ;  /*  */ end\nendmodule"},
#if 0
      // TODO: apply multi-violation fixes in linter_test_utils.
      { "module m;\ninitial begin; ; ; ; end\nendmodule",
        "module m;\ninitial begin ; end\nendmodule" }
#endif
  };
  RunApplyFixCases<VerilogAnalyzer, ForbidConsecutiveNullStatementsRule>(
      kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
