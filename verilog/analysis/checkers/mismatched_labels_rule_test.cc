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

#include "verilog/analysis/checkers/mismatched_labels_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(MismatchedLabelsRuleTest, MismatchedLabelsTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      // always block
      // no labels
      {"module foo(input clk);\n"
       "  always_ff @(posedge clk)\n"
       "    begin\n"
       "    end\n"
       "endmodule"},
      // both labels are correct
      {"module foo(input clk);\n"
       "  always_ff @(posedge clk)\n"
       "    begin : foo\n"
       "    end : foo\n"
       "endmodule"},
      // end label mismatch
      {"module foo(input clk);\n"
       "  always_ff @(posedge clk)\n"
       "    begin : foo\n"
       "    end : ",
       {SymbolIdentifier, "bar"},
       "\n"
       "endmodule"},
      // end label missing
      {"module foo(input clk);\n"
       "  always_ff @(posedge clk)\n"
       "    begin : foo\n"
       "    end\n"
       "endmodule"},
      // begin label missing
      {"module foo(input clk);\n"
       "  always_ff @(posedge clk)\n"
       "    ",
       {TK_begin, "begin"},
       "\n"
       "    end : foo\n"
       "endmodule"},

      // initial block
      // no labels
      {"module foo;\n"
       "  initial\n"
       "    begin\n"
       "    end\n"
       "endmodule"},
      // both labels are correct
      {"module foo;\n"
       "  initial\n"
       "    begin : foo\n"
       "    end : foo\n"
       "endmodule"},
      // end label mismatch
      {"module foo;\n"
       "  initial\n"
       "    begin : foo\n"
       "    end : ",
       {SymbolIdentifier, "bar"},
       "\n"
       "endmodule"},
      // end label missing
      {"module foo;\n"
       "  initial\n"
       "    begin : foo\n"
       "    end\n"
       "endmodule"},
      // begin label missing
      {"module foo;\n"
       "  initial\n"
       "    ",
       {TK_begin, "begin"},
       "\n"
       "    end : foo\n"
       "endmodule"},

      // for block
      // no labels
      {"module foo;\n"
       "  initial for(int i=0; i<5; ++i)\n"
       "    begin\n"
       "    end\n"
       "endmodule"},
      // both labels are correct
      {"module foo;\n"
       "  initial for(int i=0; i<5; ++i)\n"
       "    begin : foo\n"
       "    end : foo\n"
       "endmodule"},
      // end label mismatch
      {"module foo;\n"
       "  initial for(int i=0; i<5; ++i)\n"
       "    begin : foo\n"
       "    end : ",
       {SymbolIdentifier, "bar"},
       "\n"
       "endmodule"},
      // end label missing
      {"module foo;\n"
       "  initial for(int i=0; i<5; ++i)\n"
       "    begin : foo\n"
       "    end\n"
       "endmodule"},
      // begin label missing
      {"module foo;\n"
       "  initial for(int i=0; i<5; ++i)\n"
       "    ",
       {TK_begin, "begin"},
       "\n"
       "    end : foo\n"
       "endmodule"},

      // nested blocks
      // no labels
      {"module foo;\n"
       "  initial begin\n"
       "    for(int i=0; i<5; ++i)\n"
       "    begin\n"
       "    end\n"
       "  end\n"
       "endmodule"},
      // all labels are correct
      {"module foo;\n"
       "  initial begin : first_label\n"
       "    for(int i=0; i<5; ++i)\n"
       "    begin : second_label\n"
       "    end : second_label\n"
       "  end : first_label\n"
       "endmodule"},
      // end label mismatch
      {"module foo;\n"
       "  initial begin : first_label\n"
       "    for(int i=0; i<5; ++i)\n"
       "    begin : second_label\n"
       "    end : ",
       {SymbolIdentifier, "inv_second_label"},
       "\n"
       "  end : first_label\n"
       "endmodule"},
      {"module foo;\n"
       "  initial begin : first_label\n"
       "    for(int i=0; i<5; ++i)\n"
       "    begin : second_label\n"
       "    end : second_label\n"
       "  end : ",
       {SymbolIdentifier, "inv_first_label"},
       "\n"
       "endmodule"},
      {"module foo;\n"
       "  initial begin : first_label\n"
       "    for(int i=0; i<5; ++i)\n"
       "    begin : second_label\n"
       "    end : ",
       {SymbolIdentifier, "inv_second_label"},
       "\n"
       "  end : ",
       {SymbolIdentifier, "inv_first_label"},
       "\n"
       "endmodule"},
      {"module foo;\n"
       "  initial begin : first_label\n"
       "    for(int i=0; i<5; ++i)\n"
       "    begin : second_label\n"
       "    end : ",
       {SymbolIdentifier, "first_label"},
       "\n"
       "  end : ",
       {SymbolIdentifier, "second_label"},
       "\n"
       "endmodule"},
      // end label missing
      {"module foo;\n"
       "  initial begin : first_label\n"
       "    for(int i=0; i<5; ++i)\n"
       "    begin : second_label\n"
       "    end\n"
       "  end\n"
       "endmodule"},
      // begin label missing
  };
  RunLintTestCases<VerilogAnalyzer, MismatchedLabelsRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
