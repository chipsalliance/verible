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

#include "verible/verilog/analysis/checkers/explicit-function-task-parameter-type-rule.h"

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

// All tokens referenced by rule findings are of the same type.
constexpr int kToken = SymbolIdentifier;

// Tests that no explicit-function-task-parameter-type rule violations are
// found.
TEST(ExplicitFunctionTaskParameterTypeRuleTest, BasicFunctionAcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"function foo (int bar); endfunction"},
      {"function foo (int bar, int bar_2); endfunction"},
      {"function foo (bit [10:0] bar); endfunction"},
      {"function automatic foo (int bar); endfunction"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionTaskParameterTypeRule>(
      kTestCases);
}

// Tests that no explicit-function-task-parameter-type rule violations are
// found.
TEST(ExplicitFunctionTaskParameterTypeRuleTest, BasicTaskAcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"task foo (int bar); endtask"},
      {"task foo (int bar, int bar_2); endtask"},
      {"task foo (bit [10:0] bar); endtask"},
      {"task automatic foo (int bar); endtask"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionTaskParameterTypeRule>(
      kTestCases);
}

// Tests that the expected number of explicit-function-task-parameter-type rule
// violations are found for functions.
TEST(ExplicitFunctionTaskParameterTypeRuleTest, BasicFunctionViolationTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"function foo (",
       {kToken, "bar"},
       ", ",
       {kToken, "bar_2"},
       "); endfunction"},
      {"function foo (int bar, ", {kToken, "bar_2"}, "); endfunction"},
      {"function foo (input ", {kToken, "bar"}, "); endfunction"},
      {"function foo (bit foo, inout ", {kToken, "bar"}, "); endfunction"},
      {"function foo (input ",
       {kToken, "foo"},
       ", inout ",
       {kToken, "bar"},
       "); endfunction"},
      {"function foo (int foo, ref ", {kToken, "bar"}, "); endfunction"},
      {"function static foo(", {kToken, "bar"}, "); endfunction"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionTaskParameterTypeRule>(
      kTestCases);
}

// Tests that the expected number of explicit-function-task-parameter-type rule
// violations are found for tasks.
TEST(ExplicitFunctionTaskParameterTypeRuleTest, BasicTaskViolationTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"task foo (", {kToken, "bar"}, ", ", {kToken, "bar_2"}, "); endtask"},
      {"task foo (int bar, ", {kToken, "bar_2"}, "); endtask"},
      {"task foo (input ", {kToken, "bar"}, "); endtask"},
      {"task foo (bit foo, inout ", {kToken, "bar"}, "); endtask"},
      {"task foo (input ",
       {kToken, "foo"},
       ", inout ",
       {kToken, "bar"},
       "); endtask"},
      {"task foo (int foo, ref ", {kToken, "bar"}, "); endtask"},
      {"task static foo(", {kToken, "bar"}, "); endtask"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionTaskParameterTypeRule>(
      kTestCases);
}

// Tests that the expected number of explicit-function-task-parameter-type rule
// violations are found.
TEST(ExplicitFunctionTaskParameterTypeRuleTest, FunctionAsMember) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"class foo; function bar(int bar); endfunction endclass"},
      {"class foo; function bar(", {kToken, "bar"}, "); endfunction endclass"},
      {"class foo; function bar(",
       {kToken, "bar"},
       "); endfunction\n"
       "function bar_2(",
       {kToken, "bar_2"},
       "); "
       "endfunction endclass"},
      {"class foo; function bar(",
       {kToken, "foo"},
       ", ",
       {kToken, "bar2"},
       "); endfunction endclass"},
      {"module foo; function static bar(int bar); endfunction endmodule"},
      {"module foo; function automatic bar(",
       {kToken, "bar"},
       "); endfunction endmodule"},
      {"package foo; function automatic bar(int bar); endfunction "
       "endpackage"},
      {"package foo; function static bar(",
       {kToken, "bar"},
       "); endfunction endpackage"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionTaskParameterTypeRule>(
      kTestCases);
}

// Tests that the expected number of explicit-function-task-parameter-type rule
// violations are found.
TEST(ExplicitFunctionTaskParameterTypeRuleTest, TaskAsMember) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"class foo; task bar(int bar); endtask endclass"},
      {"class foo; task bar(", {kToken, "bar"}, "); endtask endclass"},
      {"class foo; task bar(",
       {kToken, "bar"},
       "); endtask task bar_2(",
       {kToken, "bar_2"},
       "); "
       "endtask endclass"},
      {"class foo; task bar(",
       {kToken, "foo"},
       ", ",
       {kToken, "bar2"},
       "); endtask endclass"},
      {"module foo; task static bar(int bar); endtask endmodule"},
      {"module foo; task automatic bar(",
       {kToken, "bar"},
       "); endtask endmodule"},
      {"package foo; task automatic bar(int bar); endtask "
       "endpackage"},
      {"package foo; task static bar(",
       {kToken, "bar"},
       "); endtask endpackage"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionTaskParameterTypeRule>(
      kTestCases);
}

TEST(ExplicitFunctionTaskParameterTypeRuleTest, ViolationLocation) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"function foo (int bar, ", {kToken, "bar_2"}, "); endfunction"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionTaskParameterTypeRule>(
      kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
