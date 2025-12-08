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

#include "verible/verilog/analysis/checkers/signal-name-style-rule.h"

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

TEST(SignalNameStyleRuleTest, DefaultTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      // Tests for module ports
      {"module foo(input logic b_a_r); endmodule"},
      {"module foo(input wire hello_world1); endmodule"},
      {"module foo(wire ", {kToken, "HelloWorld"}, "); endmodule"},
      {"module foo(input logic [3:0] ", {kToken, "Foo_bar"}, "); endmodule"},
      {"module foo(input logic b_a_r [3:0]); endmodule"},
      {"module foo(input logic [3:0] ",
       {kToken, "Bar"},
       ", input logic ",
       {kToken, "Bar2"},
       " [4]); endmodule"},
      {"module foo(input logic hello_world, input bar); endmodule"},
      {"module foo(input logic hello_world, input ",
       {kToken, "b_A_r"},
       "); endmodule"},
      {"module foo(input logic ",
       {kToken, "HelloWorld"},
       ", output ",
       {kToken, "Bar"},
       "); endmodule"},
      {"module foo(input logic ",
       {kToken, "hello_World"},
       ", wire b_a_r = 1); endmodule"},
      {"module foo(input hello_world, output b_a_r, input wire bar2); "
       "endmodule"},
      {"module foo(input hello_world, output b_a_r, input wire ",
       {kToken, "Bad"},
       "); "
       "endmodule"},
      // Tests for nets
      {"module foo; wire single_net; endmodule"},
      {"module foo; wire first_net, second_net; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "SingleNet"},
       "; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "FirstNet"},
       ";"
       "wire second_net; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "FirstNet"},
       ";"
       "wire ",
       {kToken, "SecondNet"},
       "; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "FirstNet"},
       ", ",
       {kToken, "SecondNet"},
       "; endmodule"},
      {"module foo; wire first_net, ", {kToken, "SecondNet"}, "; endmodule"},
      {"module foo; wire ", {kToken, "FirstNet"}, ", second_net; endmodule"},
      // Tests for data
      {"module foo; logic okay_name; endmodule"},
      {"module foo; logic ", {kToken, "NotOkayName"}, "; endmodule"},
      {"module foo; logic m_ultiple, o_kay, n_ames; endmodule"},
      {"module foo; "
       "logic the_middle, ",
       {kToken, "NameIs"},
       ", not_okay; endmodule"},
      {"module foo; logic a = 1, b = 0, c = 1; endmodule"},
      {"module foo; logic a = 1, ", {kToken, "B"}, "= 0, c = 1; endmodule"},
      {"module top; foo baz(0); endmodule"},
      {"module top; foo ba_x(0), ba_y(1), ba_z(0); endmodule"},
      {"module top; foo ", {kToken, "Baz"}, "(0); endmodule"},
      {"module top; foo ba_x(0), ", {kToken, "BaY"}, "(1); endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, SignalNameStyleRule>(kTestCases);
}

TEST(SignalNameStyleRuleTest, UpperSnakeCaseTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      // Tests for module ports
      {"module foo(input logic B_A_R); endmodule"},
      {"module foo(input wire HELLO_WORLD1); endmodule"},
      {"module foo(wire ", {kToken, "HelloWorld"}, "); endmodule"},
      {"module foo(input logic [3:0] ", {kToken, "Foo_bar"}, "); endmodule"},
      {"module foo(input logic B_A_R [3:0]); endmodule"},
      {"module foo(input logic [3:0] ",
       {kToken, "Bar"},
       ", input logic ",
       {kToken, "Bar2"},
       " [4]); endmodule"},
      {"module foo(input logic HELLO_WORLD, input BAR); endmodule"},
      {"module foo(input logic HELLO_WORLD, input ",
       {kToken, "b_A_r"},
       "); endmodule"},
      {"module foo(input logic ",
       {kToken, "HelloWorld"},
       ", output ",
       {kToken, "Bar"},
       "); endmodule"},
      {"module foo(input logic ",
       {kToken, "hello_World"},
       ", wire B_A_R = 1); endmodule"},
      {"module foo(input HELLO_WORLD, output B_A_R, input wire BAR2); "
       "endmodule"},
      {"module foo(input HELLO_WORLD, output B_A_R, input wire ",
       {kToken, "Bad"},
       "); "
       "endmodule"},
      // Tests for nets
      {"module foo; wire SINGLE_NET; endmodule"},
      {"module foo; wire FIRST_NET, SECOND_NET; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "SingleNet"},
       "; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "FirstNet"},
       ";"
       "wire SECOND_NET; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "FirstNet"},
       ";"
       "wire ",
       {kToken, "SecondNet"},
       "; endmodule"},
      {"module foo; "
       "wire ",
       {kToken, "FirstNet"},
       ", ",
       {kToken, "SecondNet"},
       "; endmodule"},
      {"module foo; wire FIRST_NET, ", {kToken, "SecondNet"}, "; endmodule"},
      {"module foo; wire ", {kToken, "FirstNet"}, ", SECOND_NET; endmodule"},
      // Tests for data
      {"module foo; logic OKAY_NAME; endmodule"},
      {"module foo; logic ", {kToken, "NotOkayName"}, "; endmodule"},
      {"module foo; logic M_ULTIPLE, O_KAY, N_AMES; endmodule"},
      {"module foo; "
       "logic THE_MIDDLE, ",
       {kToken, "NameIs"},
       ", NOT_OKAY; endmodule"},
      {"module foo; logic A = 1, B = 0, C = 1; endmodule"},
      {"module foo; logic A = 1, ", {kToken, "b"}, "= 0, C = 1; endmodule"},
      {"module top; foo BAZ(0); endmodule"},
      {"module top; foo BA_X(0), BA_Y(1), BA_Z(0); endmodule"},
      {"module top; foo ", {kToken, "Baz"}, "(0); endmodule"},
      {"module top; foo BA_X(0), ", {kToken, "BaY"}, "(1); endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, SignalNameStyleRule>(
      kTestCases, "style_regex:[A-Z_0-9]+");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
