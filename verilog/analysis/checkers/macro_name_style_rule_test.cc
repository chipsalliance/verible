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

#include "verilog/analysis/checkers/macro_name_style_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/token_stream_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that the expected number of MacroNameStyleRule violations are found.
TEST(MacroNameStyleRuleTest, BasicTests) {
  constexpr int kToken = PP_Identifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"function automatic foo(); endfunction"},
      {"`define FOO 1"},
      {"`define FOO 1\n"},
      {"`define ___ 1"},
      {"`define __UPPER__ 1"},
      {"`define FOO(a,b) 1"},
      {"`define ", {kToken, "Foo"}, "(a,b) 1"},
      {"`define ", {kToken, "FOo"}, " 1"},
      {"`define FOO_1 1"},
      {"`define _123 1"},
      {"`define ", {kToken, "lower"}, " 1"},
      {"`define ", {kToken, "lower_underscore"}, " 1"},
      {"package foo;\n`define FOO 1\nendpackage"},
      {"package foo;\n`define ", {kToken, "Foo_"}, " 1\nendpackage"},
      // special case uvm_* macros
      {"`define uvm_foo_bar(arg) arg\n"},
      {"`define UVM_FOO_BAR(arg) arg\n"},
      {"`define ", {kToken, "uvm_FOO_BAR"}, "(arg) arg\n"},
      {"`define ", {kToken, "uvm_FooBar"}, "(arg) arg\n"},
      {"`define ", {kToken, "UVM_FooBar"}, "(arg) arg\n"},
      {
          // checks macro define inside macro define
          "`define FOO \\\n"
          "`define ",
          {kToken, "bar"},
          "\n",
      },
      {
          // checks macro define inside macro define inside macro define
          "`define FOO \\\n"
          "`define GOO \\\n"
          "`define ",
          {kToken, "barf"},
          "\n",
      },
  };
  RunLintTestCases<VerilogAnalyzer, MacroNameStyleRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
