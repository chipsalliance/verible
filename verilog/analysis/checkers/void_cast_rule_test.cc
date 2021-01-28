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

#include "verilog/analysis/checkers/void_cast_rule.h"

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_treebuilder_utils.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(VoidCastTest, FunctionFailures) {
  // {} implies that no lint errors are expected.
  // violation is the expected finding tag in this set of tests.
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"class c; function f; endfunction endclass"},
      {"class c; function f; void'(",
       {SymbolIdentifier, "uvm_hdl_read"},
       "(a)); endfunction endclass"},
      {"class c; function f; void'(",
       {SymbolIdentifier, "uvm_hdl_read"},
       "(uvm_hdl_read())); endfunction endclass"},
      {"class c; function f; void'(",
       {SymbolIdentifier, "uvm_hdl_read"},
       "(randomize())); endfunction endclass"},
      {"class c; function f; void'(",
       {SymbolIdentifier, "uvm_hdl_read"},
       "(randomize)); endfunction endclass"},
      {"class c; function f; void'(",
       {"thing"},  // TODO(fangism): leftmost leaf of expression should be here,
       {'.', "."},  // but finding points here instead.  Fix this.  Below too.
       "randomize(a)); endfunction endclass"},

      // TODO(fangism): Note that we only want to catch terminal method calls
      //                to randomize.

      {"class c; function f; void'(thing",
       {'.', "."},
       "randomize().foo(a)); endfunction endclass"},
      {"class c; function f; void'(uvm_hdl_read",
       {'.', "."},
       "randomize(a)); endfunction endclass"},
      {"class c; function f; void'(uvm_hdl_read); endfunction endclass"},
      {"class c; function f; void'(",
       {TK_randomize, "randomize"},
       "()); endfunction endclass"},
      {"class c; function f; "
       "void'(normal_function()); "
       "endfunction endclass"},
      {"class c; function f; "
       "void'(foo.bar.normal_function()); "
       "endfunction endclass"},
  };

  RunLintTestCases<VerilogAnalyzer, VoidCastRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
