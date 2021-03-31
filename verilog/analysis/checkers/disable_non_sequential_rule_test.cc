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

#include "verilog/analysis/checkers/disable_non_sequential_rule.h"

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
  RunLintTestCases<VerilogAnalyzer, DisableForkNoLabelsRule>(
      kDisableStatementTestCases);
}

TEST(DisableStatementTest, FunctionFailures) {
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
       "begin : fo\n",
       {kToken, "disable"},
       " foo;\n",
       "end\n",
       "end\nendmodule"},
      {"module m;\n",
       "final begin:foo;\n",
       "begin : fo\n",
       {kToken, "disable"},
       " foo;\n",
       "end\n",
       "end\nendmodule"},
      {"module m;\n",
       "always_comb begin:foo;\n",
       "begin : fo\n",
       {kToken, "disable"},
       " foo;\n",
       "end\n",
       "end\nendmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, DisableForkNoLabelsRule>(
      kDisableStatementTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
