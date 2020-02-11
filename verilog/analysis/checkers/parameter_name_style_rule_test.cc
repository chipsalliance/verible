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

#include "verilog/analysis/checkers/parameter_name_style_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that ParameterNameStyleRule correctly accepts valid names.
TEST(ParameterNameStyleRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
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

      // Make sure parameter type triggers no violation
      {"module foo; localparam type Bar_Hello_1 = 1; endmodule"},
      {"module foo #(parameter type Bar_1_Hello__); endmodule"},
      {"package foo; parameter type Hello_world; endpackage"},
  };
  RunLintTestCases<VerilogAnalyzer, ParameterNameStyleRule>(kTestCases);
}

// Tests that ParameterNameStyleRule rejects invalid names.
TEST(ParameterNameStyleRuleTest, RejectTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module foo; localparam ", {kToken, "Bar_Hello"}, " = 1; endmodule"},
      {"module foo; localparam int ", {kToken, "Bar_Hello"}, " = 1; endmodule"},
      {"module foo; parameter int ", {kToken, "__Bar"}, " = 1; endmodule"},
      {"module foo #(parameter int ",
       {kToken, "Bar_1_Hello"},
       " = 1); endmodule"},
      {"module foo #(int ", {kToken, "Bar_1_Two"}, "); endmodule"},
      {"module foo; localparam int ",
       {kToken, "bar"},
       " = 1; localparam int BarSecond = 2; "
       "endmodule"},
      {"module foo; localparam int ",
       {kToken, "bar"},
       " = 1; localparam int ",
       {kToken, "Bar_Second"},
       " = 2; "
       "endmodule"},
      {"class foo; localparam int ", {kToken, "helloWorld"}, " = 1; endclass"},
      {"class foo #(parameter int ",
       {kToken, "hello_world"},
       " = 1); endclass"},
      {"package foo; parameter ", {kToken, "hello__1"}, " = 1; endpackage"},
      {"package foo; parameter ", {kToken, "HELLO_WORLd"}, " = 1; endpackage"},
      {"package foo; parameter int ", {kToken, "_1Bar"}, " = 1; endpackage"},
      {"package foo; parameter int ",
       {kToken, "HELLO_World"},
       " = 1; parameter int ",
       {kToken, "bar"},
       " = 2; "
       "endpackage"},
      {"parameter int ", {kToken, "HelloWorld_"}, " = 1;"},
      {"parameter ", {kToken, "HelloWorld__"}, " = 1;"},
  };
  RunLintTestCases<VerilogAnalyzer, ParameterNameStyleRule>(kTestCases);
}  // namespace

}  // namespace
}  // namespace analysis
}  // namespace verilog
