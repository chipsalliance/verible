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

#include "verilog/analysis/checkers/uvm_macro_semicolon_rule.h"

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

TEST(UvmMacroSemicolonRule, BaseTests) {
  constexpr int kToken = MacroCallId;
  const std::initializer_list<LintTestCase> kTestCases = {
    // Wrong UVM macro calls
    {"class c extends uvm_object;\n",
      {kToken, "`uvm_object_utils_begin"}, "(c);\n" , 
      "`uvm_object_utils_end\n",
     "endclass"},
    {"function f;",
       {kToken, "`uvm_error"}, "(\"error_id\",\"message\");",
     "endfunction\n"},
    {{kToken,"`uvm_analysis_imp_decl"}, "(_RX);"},
    {{kToken,"`uvm_declare_p_sequencer"}, "(SEQUENCER);"},



    // Accepted UVM macro calls
    {"function f();"
       "`uvm_error(\"error_id\",\"message\")\n"
     "endfunction"},  

    {"module top;"
       "initial begin\n"
         "`uvm_config_db#(virtual top_if)::set"
         "(\"uvm_test_top\", \" \",  \"if\", if);"
        "end\n"
     "endmodule"},
    {"`uvm_analysis_imp_decl(_RX)\n"}
  };

  RunLintTestCases<VerilogAnalyzer, UvmMacroSemicolonRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog