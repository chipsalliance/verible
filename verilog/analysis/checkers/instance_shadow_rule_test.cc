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

#include "verilog/analysis/checkers/instance_shadow_rule.h"

#include <initializer_list>

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

TEST(InstanceShadowingTest, FunctionPass) {
  const std::initializer_list<LintTestCase> kInstanceShadowingTestCases = {
      {""},
      {"module foo; logic a; endmodule"},
      {"module foo; logic a, b, c;  endmodule:foo;"},
      {"module foo; logic a; class boo; endclass:boo; endmodule:foo;"},
      {"module foo; logic a; begin boo; bit var1; end:boo; endmodule:foo;"},
      {"module foo; logic a; begin boo;", "if(a) begin baz\n",
       "$display(\"\t Value of i = %0d\",i);", "\nend:baz", "\nend:boo;",
       "endmodule:foo;"},
      {"module foo; logic a; begin boo;",
       "loop: for(int i=0;i<5;i++) begin baz\n",
       "$display(\"\t Value of i = %0d\",i);", "\nend:baz", "\nend:boo;",
       "endmodule:foo;"},
      {
          "module bad_string;\n",
          "genvar i;\n",
          "generate\n",
          "initial begin\n",
          "for(int i=0;i<5;i++) $display(\"\t i = %d\",i);\n",
          "end\n",
          "endgenerate\n",
          "endmodule:bad_string;\n",
      },
  };
  RunLintTestCases<VerilogAnalyzer, InstanceShadowRule>(
      kInstanceShadowingTestCases);
}

TEST(InstanceShadowingTest, FunctionFailures) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kInstanceShadowingTestCases = {
      {"module foo; logic ", {kToken, "foo"}, "; endmodule:foo;"},
      {"module foo; logic a; logic ", {kToken, "a"}, "; endmodule:foo;"},
      {"module foo; logic a; class ",
       {kToken, "foo"},
       "; endclass:foo; endmodule:foo;"},
      {"module foo; logic a; class boo; logic ",
       {kToken, "boo"},
       "; endclass:boo; endmodule:foo;"},
      {"module foo; logic a; class boo; logic ",
       {kToken, "foo"},
       "; endclass:boo; endmodule:foo;"},
      {
          "module foo;\n",
          "int i;\n",
          "initial begin\n",
          "for(int ",
          {kToken, "i"},
          "=0;i<5;i++) $display(\"i = %d\",i);\n",
          "end\n",
          "endmodule:foo;\n",
      },
  };
  RunLintTestCases<VerilogAnalyzer, InstanceShadowRule>(
      kInstanceShadowingTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
