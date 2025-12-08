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

#include "verible/verilog/analysis/checkers/forbidden-macro-rule.h"

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

// Tests for expected findings of forbidden macros.
TEST(ForbiddenMacroTest, Tests) {
  constexpr int kToken = MacroCallId;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},                // empty text
      {"`uvm_warning\n"},  // is not a macro *call*, not triggered
      {{kToken, "`uvm_warning"}, "()\n"},                     // without ;
      {"`uvm_error()\n"},                                     // not forbidden
      {{kToken, "`uvm_warning"}, "(\"id\", \"message\")\n"},  // with call args
      {{kToken, "`uvm_warning"}, "();"},                      // with ;
      {"`uvm_info();"},                                       // not forbidden
      {{kToken, "`uvm_warning"}, "(\"id\", \"message\");"},
      {
          "class c; function f; `uvm_error()\nendfunction endclass"
          // not forbidden
      },
      {"class c; function f; ",
       {kToken, "`uvm_warning"},
       "()\nendfunction endclass"},
      {"class c; function f; ",
       {kToken, "`uvm_warning"},
       "(\"id\", \"msg\")\nendfunction endclass"},
      {
          "class c; function f; uvm_warning();\nendfunction endclass"
          // not a macro
      },
      {
          "class c; function f; $uvm_warning();\nendfunction endclass"
          // not a macro
      },
      {"class c; function f; begin\n",
       {kToken, "`uvm_warning"},
       "()\nend\nendfunction endclass"},
      {"class c; function f; if (foo) begin\n",
       {kToken, "`uvm_warning"},
       "()\nend\nendfunction endclass"},
      {
          "class c; function f; ",
          {kToken, "`uvm_warning"},
          "()\n$do_something();\n",
          {kToken, "`uvm_warning"},
          "()\nendfunction endclass"
          // multiple violations
      },
  };

  RunLintTestCases<VerilogAnalyzer, ForbiddenMacroRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
