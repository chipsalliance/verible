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

#include "verible/verilog/analysis/checkers/module-instantiation-rules.h"

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

TEST(ModuleInstantiationTests, Parameters) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Test incorrect code
      {"module m; foo #(",
       {SymbolIdentifier, "fizz"},
       ", buzz) bar; endmodule"},
      // Note: The following cases are already syntactically rejected:
      // {"module m; foo #(.fizz(bang), buzz) bar;"},
      // {"module m; foo #(fizz, .buzz(pop)) bar;"},

      {"module m; foo #(", {TK_DecNumber, "1"}, ", 2, 3) bar; endmodule"},
      {"module m; foo #(", {TK_DecNumber, "1"}, ", \n2, \n3) bar; endmodule"},
      {
          "module m; foo #(",
          {TK_DecNumber, "3"},
          ", 1) bar; endmodule\n"
          "function f; beep #(1, 2) boop; endfunction"  // inside function does
                                                        // not trigger
      },

      // TODO(fangism): determine how to handle conditionals
      // {"module m; foo #(\n`ifdef FOO\n2,\n`endif\n4) bar; endmodule"},

      // Test correct code
      {""},
      {"\n"},
      {"module m;  endmodule"},
      {"module m; wire money; endmodule"},
      {"function f; endfunction"},
      {"class c; endclass"},
      {
          "function f; beep #(5, 2) boop; endfunction"
          // inside function does not trigger
      },
      {
          "class c; beep #(5, 2) boop; endclass"
          // inside class does not trigger
      },
      {
          "class c; function f; beep #(5, 2) boop; endfunction endclass"
          // inside method does not trigger
      },
      {"module m; foo #(1) bar; endmodule"},
      {"module m; foo bar; endmodule"},
      {"module m; foo bar (merp, la, derp); endmodule"},
      {"module m; foo bar (.he(ooo)); endmodule"},
      {"module m; foo #(.roo(ra)) bar; endmodule"},
      {"module m; foo #(.roo(ra), .bing(bong)) bar; endmodule"},
      {"module m; foo #(.roo, .bing) bar; endmodule"},
      {"module m; foo #(.roo(ra), \n.bing(bong)) bar; endmodule"},
      {"module m; foo #(.roo, \n.bing) bar; endmodule"},
      {"module m; foo #(.roo) bar; endmodule"},
      {"class c; uvm_analysis_imp_ingress_pe #(seqitem_t, scoreboard_t) "
       "ingress_pe_port; endclass"},
  };

  RunLintTestCases<VerilogAnalyzer, ModuleParameterRule>(kTestCases);
}

TEST(ModuleInstantiationTests, Ports) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module m; endmodule"},
      {"module m; foo bar (", {TK_DecNumber, "1"}, ", 2, 3); endmodule"},
      {"module m; foo bar (", {TK_DecNumber, "1"}, ", \n 2, \n  3); endmodule"},
      {"module m; foo bar (",
       {PP_ifndef, "`ifndef"},  // TODO(fangism): not sure this is best location
       " FOOR\n"
       "  1\n"
       "`else\n"
       "  2\n"
       "`endif\n"
       "); endmodule"},
      {"module m; foo bar (\n",
       {'.', "."},  // TODO(fangism): location could be better
       "foo(bar),\n"
       "`ifndef FOOR\n"
       "  1\n"
       "`else\n"
       "  2\n"
       "`endif\n"
       "); endmodule"},
      {"module m; foo bar (",
       {TK_DecNumber, "3"},
       ", 1); endmodule\n"
       "function f; beep boop (",
       {TK_DecNumber, "1"},
       ", 2); endfunction"},

      {"module m; foo bar (\n"
       "`ifndef FOOR\n"
       "  .roo(ra)\n"
       "`else\n"
       "  .merp(le)\n"
       "`endif\n"
       "); endmodule"},
      {"module m; foo bar (", {MacroIdentifier, "`ROO"}, ", `RAA ); endmodule"},
      {"module m; blah bar(", {SymbolIdentifier, "foo"}, ", fizz); endmodule"},
      {"module m; and bar(foo, fizz); endmodule"},  // primitive gate
      {"module m; foo bar(1); endmodule"},
      {"module m; foo bar(1,); endmodule"},
      {"module m; foo bar; endmodule"},
      {"module m; foo #(merp, la, derp) bar; endmodule"},
      {"module m; foo bar (.he(ooo)); endmodule"},
      {"module m; foo #(.roo(ra)) bar; endmodule"},
      {"module m; foo bar (.roo(ra), .bing(bong)); endmodule"},
      {"module m; foo bar (.roo, .bing); endmodule"},
      {"module m; foo #(.roo(ra)) \n bar; endmodule"},
      {"module m; foo bar (.roo(ra), \n.bing(bong)); endmodule"},
      {"module m; foo bar (.roo,\n.bing); endmodule"},
      {"module m; foo bar (.roo); endmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, ModulePortRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
