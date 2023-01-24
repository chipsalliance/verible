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

#include "verilog/analysis/checkers/always_comb_rule.h"

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

TEST(AlwaysCombTest, FunctionFailures) {
  constexpr int kToken = TK_always;
  const std::initializer_list<LintTestCase> kAlwaysCombTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @* begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @(*) begin end\nendmodule"},
      {"module m;\nalways_ff begin a <= b; end\nendmodule"},
      {"module m;\nalways_comb begin a = b; end\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, AlwaysCombRule>(kAlwaysCombTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
