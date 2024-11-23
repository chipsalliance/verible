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

#include "verible/verilog/analysis/checkers/always-comb-rule.h"

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

TEST(AlwaysCombTest, FunctionFailures) {
  constexpr int kToken = TK_always;
  const std::initializer_list<LintTestCase> kAlwaysCombTestCases = {
      {""},
      {"module m;\nendmodule\n"},
      {"module m;\ninitial begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @* begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @(*) begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @( *) begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @(* ) begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @( * ) begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @(/*t*/*) begin end\nendmodule"},
      {"module m;\n", {kToken, "always"}, " @(*/*t*/) begin end\nendmodule"},
      {"module m;\n",
       {kToken, "always"},
       " @(/*t*/*/*t*/) begin end\nendmodule"},
      {"module m;\nalways_ff begin a <= b; end\nendmodule"},
      {"module m;\nalways_comb begin a = b; end\nendmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, AlwaysCombRule>(kAlwaysCombTestCases);
}

TEST(AlwaysCombTest, AutoFixAlwaysComb) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      {"module m;\nalways @* begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways @(*) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways @( *) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways @(* ) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways @( * ) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways @(/*t*/*) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways @(*/*t*/) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways @(/*t*/*/*t*/) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
      {"module m;\nalways \n@(/*t*/*/*t*/)\n begin end\nendmodule",
       "module m;\nalways_comb\n begin end\nendmodule"},
      {"module m;\nalways @(\n*\n) begin end\nendmodule",
       "module m;\nalways_comb begin end\nendmodule"},
  };

  RunApplyFixCases<VerilogAnalyzer, AlwaysCombRule>(kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
