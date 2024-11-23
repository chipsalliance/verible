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

#include "verible/verilog/analysis/checkers/uvm-macro-semicolon-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunApplyFixCases;
using verible::RunLintTestCases;

TEST(UvmMacroSemicolonRuleTest, BaseTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"function void f();\nendfunction\n"},
      {"task t();\nendtask\n"},
      {"module m;\nendmodule\n"},
      {"class c;\nendclass\n"},
      {"package m_pkg;\nendpackage\n"}};

  RunLintTestCases<VerilogAnalyzer, UvmMacroSemicolonRule>(kTestCases);
}

TEST(UvmMacroSemicolonRuleTest, NoFalsePositivesTest) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module m;\nint k = `UVM_DEFAULT_TIMEOUT; endmodule\n"},
      {"module m;\nbit [63:0] k = `UVM_REG_ADDR_WIDTH'(0); endmodule\n"},
  };

  RunLintTestCases<VerilogAnalyzer, UvmMacroSemicolonRule>(kTestCases);
}

TEST(UvmMacroSemicolonRuleTest, AcceptedUvmMacroCallTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Function/Task scope
      {"function void f();"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "endfunction"},
      {"function void f();"
       "`uvm_info_begin(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_message_add_tag(\"my_color\", \"red\")\n"
       "`uvm_message_add_int(my_int, UVM_DEC)\n"
       "`uvm_info_end\n"
       "endfunction\n"},
      {"function void f();"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_warning(\"msg_id\",\"message\")\n"
       "`uvm_error(\"msg_id\",\"message\")\n"
       "`uvm_fatal(\"msg_id\",\"message\")\n"
       "endfunction\n"},
      {"task t();"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "endtask\n"},
      {"task t();"
       "`uvm_info_begin(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_message_add_tag(\"my_color\", \"red\")\n"
       "`uvm_message_add_int(my_int, UVM_DEC)\n"
       "`uvm_info_end\n"
       "endtask\n"},
      {"task t();"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_warning(\"msg_id\",\"message\")\n"
       "`uvm_error(\"msg_id\",\"message\")\n"
       "`uvm_fatal(\"msg_id\",\"message\")\n"
       "endtask\n"},

      // Class scope
      {"class c extends uvm_object;"
       "`uvm_object_utils(c)\n"
       "endclass\n"},
      {"class s extends uvm_sequence;"
       "`uvm_declare_p_sequencer(SEQUENCER)\n"
       "endclass\n"},
      {"class c extends uvm_object;"
       "`uvm_object_utils_begin(c)\n"
       "`uvm_object_utils_end\n"
       "endclass\n"},
      {"class c extends uvm_object;"
       "function void f();"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_warning(\"msg_id\",\"message\")\n"
       "`uvm_error(\"msg_id\",\"message\")\n"
       "`uvm_fatal(\"msg_id\",\"message\")\n"
       "endfunction\n"
       "endclass\n"},

      // Module scope
      {"module top;"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_warning(\"msg_id\",\"message\")\n"
       "`uvm_error(\"msg_id\",\"message\")\n"
       "`uvm_fatal(\"msg_id\",\"message\")\n"
       "endmodule\n"},
      {"module top;"
       "initial begin\n"
       "`uvm_config_db#(virtual top_if)::set(\"uvm_test_top\", \" \",  \"if\", "
       "if);"
       "end\n"
       "endmodule\n"},
      {"`uvm_analysis_imp_decl(_RX)\n"},

      // Block scope
      {"function void f();"
       "if(1)\n"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "if(1)\n"
       "`uvm_warning(\"msg_id\",\"message\")\n"
       "if(1)\n"
       "`uvm_error(\"msg_id\",\"message\")\n"
       "if(1)\n"
       "`uvm_fatal(\"msg_id\",\"message\")\n"
       "endfunction\n"},
      {"function void f();"
       "if(1) begin\n"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_warning(\"msg_id\",\"message\")\n"
       "`uvm_error(\"msg_id\",\"message\")\n"
       "`uvm_fatal(\"msg_id\",\"message\")\n"
       "end\n"
       "endfunction\n"},
      {"task t();"
       "for(int i=0;i<10;i++)"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "for(int i=0;i<10;i++)"
       "`uvm_warning(\"msg_id\",\"message\", UVM_LOW)\n"
       "for(int i=0;i<10;i++)"
       "`uvm_error(\"msg_id\",\"message\", UVM_LOW)\n"
       "for(int i=0;i<10;i++)"
       "`uvm_fatal(\"msg_id\",\"message\", UVM_LOW)\n"
       "endtask\n"},
      {"task t();"
       "for(int i=0;i<10;i++) begin"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)\n"
       "`uvm_warning(\"msg_id\",\"message\")\n"
       "`uvm_error(\"msg_id\",\"message\")\n"
       "`uvm_fatal(\"msg_id\",\"message\")\n"
       "end\n"
       "endtask\n"}};

  RunLintTestCases<VerilogAnalyzer, UvmMacroSemicolonRule>(kTestCases);
}

TEST(UvmMacroSemicolonRule, WrongUvmMacroTest) {
  constexpr int kToken = ';';

  const std::initializer_list<LintTestCase> kTestCases = {
      // File/Package scope
      {"`uvm_analysis_imp_decl(_RX)",
       {kToken, ";"},
       "`uvm_analysis_imp_decl(_TX)",
       {kToken, ";"}},

      {"`uvm_info_begin(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_message_add_tag(\"my_color\", \"red\")",
       {kToken, ";"},
       "`uvm_message_add_int(my_int, UVM_DEC)",
       {kToken, ";"},
       "`uvm_info_end",
       {kToken, ";"}},

      // Function/Task scope
      {"function void f();"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "endfunction\n"},
      {"function void f();",
       "`uvm_info_begin(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_message_add_tag(\"my_color\", \"red\")",
       {kToken, ";"},
       "`uvm_message_add_int(my_int, UVM_DEC)",
       {kToken, ";"},
       "`uvm_info_end",
       {kToken, ";"},
       "endfunction\n"},

      {"function void f();",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_warning(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_error(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_fatal(\"msg_id\",\"message\")",
       {kToken, ";"},
       "endfunction\n"},
      {"task t();"
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "endtask\n"},

      {"task t();",
       "`uvm_info_begin(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_message_add_tag(\"my_color\", \"red\")",
       {kToken, ";"},
       "`uvm_message_add_int(my_int, UVM_DEC)",
       {kToken, ";"},
       "`uvm_info_end",
       {kToken, ";"},
       "endtask\n"},

      {"task t();",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_warning(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_error(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_fatal(\"msg_id\",\"message\")",
       {kToken, ";"},
       "endtask\n"},

      // Class scope
      {"class c extends uvm_object;"
       "`uvm_object_utils(c)",
       {kToken, ";"},
       "endclass\n"},
      {"class s extends uvm_sequence;"
       "`uvm_declare_p_sequencer(SEQUENCER)",
       {kToken, ";"},
       "endclass\n"},
      {"class c extends uvm_object;",
       "`uvm_object_utils_begin(c)",
       {kToken, ";"},
       "\n",
       "`uvm_object_utils_end\n",
       "endclass\n"},
      {"class c extends uvm_object;",
       "function void f();",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_warning(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_error(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_fatal(\"msg_id\",\"message\")",
       {kToken, ";"},
       "endfunction\n",
       "endclass\n"},

      // Module scope
      {"module top;",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_warning(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_error(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_fatal(\"msg_id\",\"message\")",
       {kToken, ";"},
       "endmodule\n"},

      // Block scope
      {"function void f();",
       "if(1)\n",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "if(1)\n",
       "`uvm_warning(\"msg_id\",\"message\")",
       {kToken, ";"},
       "if(1)\n",
       "`uvm_error(\"msg_id\",\"message\")",
       {kToken, ";"},
       "if(1)\n",
       "`uvm_fatal(\"msg_id\",\"message\")",
       {kToken, ";"},
       "endfunction"},
      {"function void f();",
       "if(1) begin\n",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_warning(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_error(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_fatal(\"msg_id\",\"message\")",
       {kToken, ";"},
       "end\n",
       "endfunction\n"},
      {"task t();",
       "for(int i=0;i<10;i++)",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "for(int i=0;i<10;i++)",
       "`uvm_warning(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "for(int i=0;i<10;i++)",
       "`uvm_error(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "for(int i=0;i<10;i++)",
       "`uvm_fatal(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "endtask\n"},

      {"task t();",
       "for(int i=0;i<10;i++) begin",
       "`uvm_info(\"msg_id\",\"message\", UVM_LOW)",
       {kToken, ";"},
       "`uvm_warning(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_error(\"msg_id\",\"message\")",
       {kToken, ";"},
       "`uvm_fatal(\"msg_id\",\"message\")",
       {kToken, ";"},
       "end\n",
       "endtask\n"}};
  RunLintTestCases<VerilogAnalyzer, UvmMacroSemicolonRule>(kTestCases);
}

TEST(UvmMacroSemicolonRuleTest, ApplyAutoFix) {
  const std::initializer_list<verible::AutoFixInOut> kTestCases = {
      {"`uvm_foo(abc);\n", "`uvm_foo(abc)\n"},
  };
  RunApplyFixCases<VerilogAnalyzer, UvmMacroSemicolonRule>(kTestCases, "");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
