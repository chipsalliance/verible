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

#include "verilog/analysis/checkers/undersized_binary_literal_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(UndersizedBinaryLiteralTest, FunctionFailures) {
  constexpr int kToken = TK_BinDigits;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"localparam x = 0;"},
      {"localparam x = 1;"},
      {"localparam x = '0;"},
      {"localparam x = '1;"},
      {"localparam x = 2'b0;"},                     // exception granted for 0
      {"localparam x = 3'b", {kToken, "00"}, ";"},  // only 2 0s for 3 bits
      {"localparam x = 32'b0;"},                    // exception granted for 0
      {"localparam x = 2'b", {kToken, "1"}, ";"},
      {"localparam x = 2'b?;"},  // exception granted for ?
      {"localparam x = 2'b", {kToken, "x"}, ";"},
      {"localparam x = 2'b", {kToken, "z"}, ";"},
      {"localparam x = 2'b ", {kToken, "1"}, ";"},    // with space after base
      {"localparam x = 2'b ", {kToken, "_1_"}, ";"},  // with underscores
      {"localparam x = 1'b0;"},
      {"localparam x = 1'b1;"},
      {"localparam x = 2'h1;"},
      {"localparam x = 2'habcd;"},
      {"localparam x = 32'd20;"},
      {"localparam x = 16'o7;"},
      {"localparam x = 8'b 0001_1000;"},
      {"localparam x = 8'b ", {kToken, "001_1000"}, ";"},
      {"localparam x = 8'b ", {kToken, "0001_100"}, ";"},
      {"localparam x = 8'b ", {kToken, "????_xx1"}, ";"},
      {"localparam x = 8'b ", {kToken, "1??_xz10"}, ";"},
      {"localparam x = 0 + 2'b", {kToken, "1"}, ";"},
      {"localparam x = 3'b", {kToken, "10"}, " + 3'b", {kToken, "1"}, ";"},
      {"localparam x = 2'b ", {kToken, "x"}, " & 2'b ", {kToken, "1"}, ";"},
      {"localparam x = 5'b?????;"},
      {"localparam x = 5'b?;"},                     // exception granted for ?
      {"localparam x = 5'b", {kToken, "??"}, ";"},  // only 2 ?s for 5 bits
  };

  RunLintTestCases<VerilogAnalyzer, UndersizedBinaryLiteralRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
