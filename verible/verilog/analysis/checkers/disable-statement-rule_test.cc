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

#include "verible/verilog/analysis/checkers/disable-statement-rule.h"

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

TEST(DisableStatementTest, FunctionPass) {
  const std::initializer_list<LintTestCase> kDisableStatementTestCases = {
      {""},
      {"module m;\ninitial begin;\n", "fork\n", "begin\n#6;\nend\n",
       "begin\n#3;\nend\n", "join_any\n", "disable fork;\n", "end\nendmodule"},
      {"module m;\ninitial begin\n", "fork\n", "begin : foo\n",
       "disable foo;\n", "end\n", "join_any\n", "end\nendmodule"},
      {"module m;\ninitial begin\n", "fork\n", "begin : foo\n",
       "begin : foo_2\n", "disable foo_2;\n", "end\n", "end\n", "join_any\n",
       "end\nendmodule"},
      {"module m;\ninitial begin\n", "fork\n", "begin : foo\n",
       "begin : foo_2\n", "disable foo;\n", "end\n", "end\n", "join_any\n",
       "end\nendmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DisableStatementNoLabelsRule>(
      kDisableStatementTestCases);
}

TEST(DisableStatementTest, ForkDisableStatementsFail) {
  constexpr int kToken = TK_disable;
  const std::initializer_list<LintTestCase> kDisableStatementTestCases = {
      {"module m;\ninitial begin\n",
       "fork\n",
       "begin\n#6;\nend\n",
       "begin\n#3;\nend\n",
       "join_any\n",
       {kToken, "disable"},
       " fork_invalid;\n",
       "end\nendmodule"},
      {"module m;\ninitial begin\n",
       "fork:fork_label\n",
       "begin\n#6;\nend\n",
       "begin\n#3;\nend\n",
       "join_any\n",
       {kToken, "disable"},
       " fork_label;\n",
       "end\nendmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DisableStatementNoLabelsRule>(
      kDisableStatementTestCases);
}

TEST(DisableStatementTest, NonSequentialDisableStatementsFail) {
  constexpr int kToken = TK_disable;
  const std::initializer_list<LintTestCase> kDisableStatementTestCases = {
      {"module m;\n",
       "initial begin;\n",
       "fork\n",
       "begin : foo\n",
       "end\n",
       {kToken, "disable"},
       " foo;\n",
       "join_any\n",
       "end\nendmodule"},
      {"module m;\n",
       "initial begin:foo\n",
       "end\n",
       "initial begin:boo\n",
       {kToken, "disable"},
       " foo;\n",
       "end\nendmodule"},
      {"module m;\n",
       "initial begin:foo;\n",
       "begin : bar\n",
       {kToken, "disable"},
       " foo;\n",
       "end\n",
       "end\nendmodule"},
      {"module m;\n",
       "final begin:foo;\n",
       "begin : bar\n",
       {kToken, "disable"},
       " foo;\n",
       "end\n",
       "end\nendmodule"},
      {"module m;\n",
       "always_comb begin:foo;\n",
       "begin : bar\n",
       {kToken, "disable"},
       " foo;\n",
       "end\n",
       "end\nendmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DisableStatementNoLabelsRule>(
      kDisableStatementTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
