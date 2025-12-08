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

#include "verible/verilog/analysis/checkers/positive-meaning-parameter-name-rule.h"

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

// Tests that ParameterNameStyleRule correctly accepts valid names.
TEST(PositiveMeaningParameterNameRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      /* various parameter declarations */
      {"module foo; endmodule"},
      {"module foo (input bar); endmodule"},
      {"module foo; localparam type Bar_1 = 1; endmodule"},
      {"module foo; localparam Bar = 1; endmodule"},
      {"module foo; localparam int Bar = 1; endmodule"},
      {"module foo; parameter int HelloWorld = 1; endmodule"},
      {"module foo #(parameter int HelloWorld_1 = 1); endmodule"},
      {"module foo #(parameter type Foo); endmodule"},
      {"module foo #(int Foo = 8); endmodule"},
      {"module foo; localparam int Bar = 1; localparam int BarSecond = 2; "
       "endmodule"},
      {"class foo; localparam int Bar = 1; endclass"},
      {"class foo #(parameter int Bar = 1); endclass"},
      {"package foo; parameter Bar = 1; endpackage"},
      {"package foo; parameter int HELLO_WORLD = 1; endpackage"},
      {"package foo; parameter int Bar = 1; endpackage"},
      {"parameter int Foo = 1;"},
      {"parameter type FooBar;"},
      {"parameter Foo = 1;"},
      {"module foo; localparam type Bar_Hello_1 = 1; endmodule"},
      {"module foo #(parameter type Bar_1_Hello__); endmodule"},
      {"package foo; parameter type Hello_world; endpackage"},

      /* parameters using the enable prefix */
      {"module foo; parameter int EnableHelloWorld = 1; endmodule"},
      {"module foo (input enable_bar); endmodule"},
      {"module foo; localparam EnAbLeBar = 1; endmodule"},

      /* Parameter type name should not trigger this */
      {"module foo #(parameter type disable_foo); endmodule"},
      {"module foo #(parameter type DisableFoo); endmodule"},
      {"module foo #(parameter type diSaBle_Foo); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, PositiveMeaningParameterNameRule>(
      kTestCases);
}

// Tests that ParameterNameStyleRule rejects invalid names.
TEST(PositiveMeaningParameterNameRuleTest, RejectTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module foo; parameter int ",
       {kToken, "disable_foo"},
       " = 1; endmodule"},
      {"module foo; parameter int ", {kToken, "DisableFoo"}, " = 1; endmodule"},
      {"module foo; parameter int ",
       {kToken, "DiSaBle_Foo"},
       " = 1; endmodule"},

      {"module foo; localparam ", {kToken, "disable_bar"}, " = 1; endmodule"},
      {"module foo; localparam ", {kToken, "DisableBar"}, " = 1; endmodule"},
      {"module foo; localparam ", {kToken, "dIsaBleBar"}, " = 1; endmodule"},

      {"module foo #(parameter int ",
       {kToken, "disable_hello"},
       " = 1); endmodule"},
      {"module foo #(parameter int ",
       {kToken, "disableHello"},
       " = 1); endmodule"},
      {"module foo #(parameter int ",
       {kToken, "DiSable_hello"},
       " = 1); endmodule"},
      {"module foo #(parameter int ",
       {kToken, "disable_hello"},
       "); endmodule"},
      {"module foo #(parameter int ", {kToken, "disableHello"}, "); endmodule"},
      {"module foo #(parameter int ",
       {kToken, "DiSable_hello"},
       "); endmodule"},

      {"module foo #(parameter int enable_f = 1, ",
       {kToken, "disable_s"},
       " = 1); endmodule"},

      {"class foo #(parameter int ",
       {kToken, "disable_opt"},
       " = 1); endclass"},
      {"class foo #(parameter int ", {kToken, "disableOpt"}, " = 1); endclass"},
      {"class foo #(parameter int ",
       {kToken, "DiSABle_Opt"},
       " = 1); endclass"},
      {"class foo #(parameter int ", {kToken, "disable_opt"}, "); endclass"},
      {"class foo #(parameter int ", {kToken, "disableOpt"}, "); endclass"},
      {"class foo #(parameter int ", {kToken, "DiSABle_Opt"}, "); endclass"},

      {"package foo; parameter int ",
       {kToken, "disable_sth"},
       " = 1; endpackage"},

      {"module foo #(parameter int ",
       {kToken, "DiSable_hello"},
       "); "
       " parameter int ",
       {kToken, "disableAbC"},
       " = 1;"
       "endmodule"},

      {"module foo #(parameter int ",
       {kToken, "DiSable_hello"},
       "); "
       " parameter int enableABC = 0, ",
       {kToken, "disableXYZ"},
       " = 1;"
       "endmodule"},

      {"module foo #(parameter int ",
       {kToken, "DiSable_hello"},
       "); "
       " parameter int ",
       {kToken, "disableAbC"},
       " = 1, ",
       {kToken, "disableXYZ"},
       " = 1;"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, PositiveMeaningParameterNameRule>(
      kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
