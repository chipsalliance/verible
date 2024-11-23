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

#include "verible/verilog/analysis/checkers/parameter-type-name-style-rule.h"

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

// Tests that ParameterTypeNameStyleRule correctly accepts valid names.
TEST(ParameterTypeNameStyleRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module foo #(parameter type foo_bar_t); endmodule"},
      {"module foo #(parameter type foo_bar_t = logic); endmodule"},
      {"module foo; localparam type foo_bar_t; endmodule"},
      {"module foo; localparam type foo_bar_t = logic; endmodule"},
      {"parameter type foo_bar_t;"},
      {"parameter type foo_bar_t = logic;"},

      // Make sure non-type parameters trigger no violation
      {"module foo; localparam Bar_Hello = 1; endmodule"},
      {"module foo; localparam int Bar_Hello = 1; endmodule"},
      {"module foo; parameter int __Bar = 1; endmodule"},
      {"module foo #(parameter int Bar_1_Hello = 1); endmodule"},
      {"module foo #(int Bar_1_Two); endmodule"},
      {"module foo; localparam int bar = 1; "
       "localparam int BarSecond = 2; endmodule"},
      {"module foo; localparam int bar = 1; "
       "localparam int Bar_Second = 2; endmodule"},
      {"class foo; localparam int helloWorld = 1; endclass"},
      {"class foo #(parameter int hello_world = 1); endclass"},
      {"package foo; parameter hello__1 = 1; endpackage"},
      {"package foo; parameter HELLO_WORLd = 1; endpackage"},
      {"package foo; parameter int _1Bar = 1; endpackage"},
      {"package foo; parameter int HELLO_World = 1; "
       "parameter int bar = 2; endpackage"},
      {"parameter int HelloWorld_ = 1;"},
      {"parameter HelloWorld__ = 1;"},
  };
  RunLintTestCases<VerilogAnalyzer, ParameterTypeNameStyleRule>(kTestCases);
}

// Tests that ParameterTypeNameStyleRule rejects invalid names.
TEST(ParameterTypeNameStyleRuleTest, RejectTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module foo #(parameter type ", {kToken, "FooBar"}, "); endmodule"},
      {"module foo #(parameter type ",
       {kToken, "FooBar"},
       " = logic); endmodule"},
      {"module foo #(parameter type ",
       {kToken, "FooBar_t"},
       " = logic); endmodule"},
      {"module foo #(parameter type ", {kToken, "foobar"}, "); endmodule"},
      {"module foo #(parameter type ", {kToken, "_t"}, "); endmodule"},

      {"module foo; localparam type ",
       {kToken, "FooBar"},
       " = logic; endmodule"},
      {"module foo; localparam type ",
       {kToken, "foo_bar"},
       " = logic; endmodule"},
      {"module foo; localparam type ",
       {kToken, "Foo_Bar_t"},
       " = logic; endmodule"},

      {"parameter type ", {kToken, "Foo_Bar_t"}, ";"},
      {"parameter type ", {kToken, "FooBar_t"}, ";"},
      {"parameter type ", {kToken, "Foobar_t"}, ";"},
      {"parameter type ", {kToken, "foobar"}, ";"},
      {"parameter type ", {kToken, "foo_bar"}, ";"},
  };
  RunLintTestCases<VerilogAnalyzer, ParameterTypeNameStyleRule>(kTestCases);
}  // namespace

}  // namespace
}  // namespace analysis
}  // namespace verilog
