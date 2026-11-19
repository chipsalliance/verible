// Copyright 2017-2026 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/enum-constant-name-style-rule.h"

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
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(EnumConstantNameStyleRuleTest, ValidConstants) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module foo; logic a; assign a = 1'b0; endmodule"},
      {"typedef enum { kOne, kTwo } my_name_e;"},
      {"typedef enum logic [1:0] { kFir, kOak, kPine } tree_e;"},
      {"typedef enum { kRed = 3, kGreen = 5 } state_e;"},
      {"typedef enum { kIdle, kBusy } status_e;"},
      {"typedef enum { kState1, kState_1, kDMA_0 } mode_e;"},
      {"typedef enum { kOp[4] } op_e;"},
      {"typedef enum { kReg[0:3] } reg_e;"},
      {"enum { kFirst, kSecond } a_var;"},
      {"enum logic [1:0] { kRead, kWrite } access;"},
  };
  RunLintTestCases<VerilogAnalyzer, EnumConstantNameStyleRule>(kTestCases);
}

TEST(EnumConstantNameStyleRuleTest, InvalidConstantsInTypedefEnum) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef enum { ",
       {kToken, "One"},
       ", ",
       {kToken, "Two"},
       " } my_name_e;"},
      {"typedef enum logic [1:0] { ",
       {kToken, "Fir"},
       ", ",
       {kToken, "Oak"},
       " } tree_e;"},
      {"typedef enum { ",
       {kToken, "BadOne"},
       " = 1, ",
       {kToken, "BadTwo"},
       " = 2 } bad_val_e;"},
      {"typedef enum { ", {kToken, "Op"}, "[4] } op_e;"},
      {"typedef enum { ",
       {kToken, "STATE_IDLE"},
       ", ",
       {kToken, "STATE_BUSY"},
       " } state_e;"},
      {"typedef enum { ", {kToken, "k_bad_underscore"}, " } bad_e;"},
      {"typedef enum { ", {kToken, "klower"}, " } bad_e;"},
      {"typedef enum { ", {kToken, "bad_snake"}, " } bad_e;"},
  };
  RunLintTestCases<VerilogAnalyzer, EnumConstantNameStyleRule>(kTestCases);
}

TEST(EnumConstantNameStyleRuleTest, InvalidConstantsInAnonymousEnum) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"enum { ", {kToken, "One"}, ", ", {kToken, "Two"}, " } my_var;"},
      {"enum logic [1:0] { ",
       {kToken, "read"},
       ", ",
       {kToken, "write"},
       " } op;"},
      {"enum { ", {kToken, "STATE_IDLE"}, " } target;\n"},
  };
  RunLintTestCases<VerilogAnalyzer, EnumConstantNameStyleRule>(kTestCases);
}

TEST(EnumConstantNameStyleRuleTest, CustomConfiguration) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef enum { STATE_IDLE, STATE_BUSY } state_e;"},
      {"enum { ALL_CAPS_VAL } my_var;"},
      {"typedef enum { ", {kToken, "kCamelCase"}, " } my_e;"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, EnumConstantNameStyleRule>(
      kTestCases, "style_regex:[A-Z_]+");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
