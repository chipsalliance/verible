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

#include "verilog/analysis/checkers/proper_parameter_declaration_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

// Tests that ProperParameterDeclarationRule does not report a violation when
// not necessary.
TEST(ProperParameterDeclarationRuleTest, BasicTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"package foo; endpackage"},
      {"module foo; endmodule"},
      {"class foo; endclass"},
  };
  RunLintTestCases<VerilogAnalyzer, ProperParameterDeclarationRule>(kTestCases);
}

// Tests rejection of package parameters and allow package localparams
TEST(ProperParameterDeclarationRuleTest, RejectPackageParameters) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"parameter int Foo = 1;"},
      {"package foo; ",
       {TK_parameter, "parameter"},
       " int Bar = 1; endpackage"},
      {"package foo; ",
       {TK_parameter, "parameter"},
       " int Bar = 1; ",
       {TK_parameter, "parameter"},
       " int Bar2 = 2; "
       "endpackage"},
      {"module foo #(parameter int Bar = 1); endmodule"},
      {"module foo #(int Bar = 1); endmodule"},
      {"class foo #(parameter int Bar = 1); endclass"},
      {"module foo #(parameter type Foo); endmodule"},
      {"module foo; ", {TK_parameter, "parameter"}, " int Bar = 1; endmodule"},
      {"class foo; ", {TK_parameter, "parameter"}, " int Bar = 1; endclass"},
      {"package foo; class bar; endclass ",
       {TK_parameter, "parameter"},
       " int HelloWorld = 1; "
       "endpackage"},
      {"package foo; class bar; ",
       {TK_parameter, "parameter"},
       " int HelloWorld = 1; endclass "
       "endpackage"},
      {"module foo #(parameter int Bar = 1); ",
       {TK_parameter, "parameter"},
       " int HelloWorld = 1; "
       "endmodule"},
      {"module foo #(parameter type Foo, parameter int Bar = 1); "
       "endmodule"},
      {"module foo #(parameter type Bar); ",
       {TK_parameter, "parameter"},
       " type Bar2; endmodule"},
      {"module foo #(parameter type Bar);"
       "module innerFoo #("
       "parameter type innerBar,",
       "localparam int j = 2)();",
       {TK_parameter, "parameter"},
       " int i = 1;"
       "localparam int j = 2;"
       "endmodule "
       "endmodule"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ProperParameterDeclarationRule>(
      kTestCases,
      "package_allow_parameter:false;package_allow_localparam:true");
}

// Tests that the expected number of localparam usage violations are found.
TEST(ProperParameterDeclarationRuleTest, LocalParamTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module foo; localparam int Bar = 1; endmodule"},
      {"class foo; localparam int Bar = 1; endclass"},
      {"module foo; localparam int Bar = 1; localparam int Bar2 = 2; "
       "endmodule"},
      {"module foo #(localparam int Bar = 1); endmodule"},
      {"module foo #(localparam type Bar); endmodule"},
      {"class foo #(localparam int Bar = 1); endclass"},
      {{TK_localparam, "localparam"},
       " int Bar = 1;"},  // localparam defined outside a module or package
      {"package foo; localparam int Bar = 1; endpackage"},
      {"package foo; class bar; endclass localparam int HelloWorld = 1; "
       "endpackage"},
      {"package foo; class bar; localparam int HelloWorld = 1; endclass "
       "endpackage"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, ProperParameterDeclarationRule>(
      kTestCases,
      "package_allow_parameter:false;package_allow_localparam:true");
}

// Tests that the expected number of localparam and parameter usage violations
// are found when both are used together.
TEST(ProperParameterDeclarationRuleTest, CombinationParametersTest) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"parameter int Foo = 1; ",
       {TK_localparam, "localparam"},
       " int Bar = 1;"},
      {"package foo; ",
       {TK_parameter, "parameter"},
       " int Bar = 1; ",
       "localparam int Bar2 = 2; "
       "endpackage"},
      {"module foo #(parameter int Bar = 1); localparam int Bar2 = 2; "
       "endmodule"},
      {"module foo #(parameter type Bar); localparam type Bar2; endmodule"},
      {"module foo #(localparam int Bar = 1); ",
       {TK_parameter, "parameter"},
       " int Bar2 = 2; "
       "endmodule"},
      {"module foo; ",
       {TK_parameter, "parameter"},
       " int Bar = 1; localparam int Bar2 = 2; endmodule"},
      {"class foo; ",
       {TK_parameter, "parameter"},
       " int Bar = 1; localparam int Bar2 = 2; endclass"},
      {"package foo; class bar; localparam int Bar2 = 2; endclass ",
       {TK_parameter, "parameter"},
       " int HelloWorld = 1; "
       "endpackage"},
      {"package foo; ",
       "localparam",
       " int Bar2 = 2; class bar; endclass ",
       {TK_parameter, "parameter"},
       " int HelloWorld = 1; "
       "endpackage"},
      {"parameter int Foo = 1; module bar; localparam int Bar2 = 2; "
       "endmodule"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, ProperParameterDeclarationRule>(
      kTestCases,
      "package_allow_parameter:false;package_allow_localparam:true");
}

// package parameters allowed, package localparam's are rejected
TEST(ProperParameterDeclarationRuleTest, AllowPackageParameters) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"parameter int Foo = 1;"},
      {"package foo; parameter int Bar = 1; endpackage"},
      {"package foo; ",
       {TK_localparam, "localparam"},
       " int Bar = 1; endpackage"},
      {"package foo; parameter int Bar = 1; "
       "parameter int Bar2 = 2; "
       "endpackage"},
      {"module foo #(parameter int Bar = 1); endmodule"},
      {"module foo #(int Bar = 1); endmodule"},
      {"class foo #(parameter int Bar = 1); endclass"},
      {"module foo #(parameter type Foo); endmodule"},
      {"module foo; ", {TK_parameter, "parameter"}, " int Bar = 1; endmodule"},
      {"class foo; ", {TK_parameter, "parameter"}, " int Bar = 1; endclass"},
      {"package foo; class bar; endclass ",
       "parameter int HelloWorld = 1; "
       "endpackage"},
      {"package foo; class bar; ",
       {TK_parameter, "parameter"},
       " int HelloWorld = 1; endclass "
       "endpackage"},
      {"module foo #(parameter int Bar = 1); ",
       {TK_parameter, "parameter"},
       " int HelloWorld = 1; "
       "endmodule"},
      {"module foo #(parameter type Foo, parameter int Bar = 1); "
       "endmodule"},
      {"module foo #(parameter type Bar); ",
       {TK_parameter, "parameter"},
       " type Bar2; endmodule"},
      {"module foo #(parameter type Bar);"
       "module innerFoo #("
       "parameter type innerBar,",
       "localparam int j = 2)();",
       {TK_parameter, "parameter"},
       " int i = 1;"
       "localparam int j = 2;"
       "endmodule "
       "endmodule"},
      {"module foo; localparam int Bar = 1; endmodule"},
      {"class foo; localparam int Bar = 1; endclass"},
      {"module foo; localparam int Bar = 1; localparam int Bar2 = 2; "
       "endmodule"},
      {"module foo #(localparam int Bar = 1); endmodule"},
      {"module foo #(localparam type Bar); endmodule"},
      {"class foo #(localparam int Bar = 1); endclass"},
      {{TK_localparam, "localparam"}, " int Bar = 1;"},
      {"package foo; ",
       {TK_localparam, "localparam"},
       " int Bar = 1; endpackage"},
      {"package foo; class bar; endclass ",
       {TK_localparam, "localparam"},
       " int HelloWorld = 1; "
       "endpackage"},
      {"package foo; class bar; localparam int HelloWorld = 1; endclass "
       "endpackage"},
  };

  RunLintTestCases<VerilogAnalyzer, ProperParameterDeclarationRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
