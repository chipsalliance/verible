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

#include "verilog/analysis/checkers/signal_name_style_rule.h"

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

TEST(SignalNameStyleRuleTest, ModulePortTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      // Tests for module ports
      {"module foo(input logic b_a_r); endmodule"},
      {"module foo(input wire hello_world1); endmodule"},
      {"module foo(wire ", {kToken, "HelloWorld"}, "); endmodule"},
      {"module foo(input logic ", {kToken, "_bar"}, "); endmodule"},
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

}  // namespace
}  // namespace analysis
}  // namespace verilog
