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

#include "verilog/analysis/checkers/forbid_implicit_declarations_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/text_structure_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_treebuilder_utils.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(ForbidImplicitDeclarationsRule, FunctionFailures) {
  auto kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase>
      ForbidImplicitDeclarationsTestCases = {
          {""},
          {"module m;\nendmodule\n"},
          {"module m;\nassign ", {kToken, "a1"}, " = 1'b0;\nendmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  assign a1 = 1'b0;\n"
           "endmodule"},
          {"module m;\n"
           "  assign ",
           {kToken, "a1"},
           " = 1'b1;\n"
           "  module foo;\n"
           "  endmodule\n"
           "endmodule"},
          {"module m;\n"
           "  module foo;\n"
           "  endmodule\n"
           "  assign ",
           {kToken, "a1"},
           " = 1'b1;\n"
           "endmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  module foo;\n"
           "    assign a1 = 1'b0;\n"
           "  endmodule;\n"
           "endmodule"},

          // declaration and assignement separated by module block
          {"module m;\n"
           "  wire a1;\n"
           "  module foo;\n"
           "  endmodule;\n"
           "  assign a1 = 1'b0;\n"
           "endmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  module foo;\n"
           "    assign a1 = 1'b0;\n"
           "  endmodule\n"
           "  assign a1 = 1'b0;\n"
           "endmodule"},

          // overlapping net
          {"module m;\n"
           "  wire a1;\n"
           "  module foo;\n"
           "    wire a1;\n"
           "    assign a1 = 1'b0;\n"
           "  endmodule\n"
           "  assign a1 = 1'b0;\n"
           "endmodule"},

          // multiple declarations
          {"module m;\n"
           "  wire a0, a1;\n"
           "  assign a0 = 1'b0;\n"
           "  assign a1 = 1'b1;\n"
           "endmodule"},
          {"module m;\n"
           "  wire a0, a2;\n"
           "  assign a0 = 1'b0;\n"
           "  assign ",
           {kToken, "a1"},
           " = 1'b1;\n"
           "endmodule"},

          // multiple net assignments
          {"module m;\n"
           "  assign ",
           {kToken, "a"},
           " = b, ",
           {kToken, "c"},
           " = d;\n"
           "endmodule"},

          // concatenated
          {"module m;\n"
           "  assign {",
           {kToken, "a"},
           "} = 1'b0;\n"
           "endmodule"},
          {"module m;\n"
           "  assign {",
           {kToken, "a"},
           ",",
           {kToken, "b"},
           "} = 2'b01;\n"
           "endmodule"},
          {"module m;\n"
           "  assign {",
           {kToken, "a"},
           ",",
           {kToken, "b"},
           ",",
           {kToken, "c"},
           "} = 3'b010;\n"
           "endmodule"},
          {"module m;\n"
           "  wire b;\n"
           "  assign {",
           {kToken, "a"},
           ", b,",
           {kToken, "c"},
           "} = 3'b010;\n"
           "endmodule"},

          // out-of-scope
          {"module m;\n"
           "  module foo;\n"
           "    wire a1;\n"
           "  endmodule\n"
           "  assign ",
           {kToken, "a1"},
           " = 1'b1;\n"
           "endmodule"},
          {"module m;\n"
           "  module foo;\n"
           "    wire a1;\n"
           "    assign a1 = 1'b0;\n"
           "  endmodule\n"
           "  assign ",
           {kToken, "a1"},
           " = 1'b1;\n"
           "endmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  module foo;\n"
           "    assign a1 = 1'b0;\n"
           "  endmodule\n"
           "  assign a1 = 1'b1;\n"
           "endmodule"},

          // multi-level module blocks
          {"module m1;\n"
           "  wire x1;\n"
           "  module m2;\n"
           "    wire x2;\n"
           "    module m3;\n"
           "      wire x3;\n"
           "      module m4;\n"
           "        wire x4;\n"
           "        assign x4 = 1'b0;\n"
           "        assign x3 = 1'b0;\n"
           "        assign x2 = 1'b0;\n"
           "        assign x1 = 1'b0;\n"
           "      endmodule\n"
           "      assign ",
           {kToken, "x4"},
           " = 1'b0;\n"
           "      assign x3 = 1'b1;\n"
           "      assign x2 = 1'b0;\n"
           "      assign x1 = 1'b0;\n"
           "    endmodule\n"
           "    assign ",
           {kToken, "x4"},
           " = 1'b0;\n"
           "    assign ",
           {kToken, "x3"},
           " = 1'b0;\n"
           "    assign x2 = 1'b0;\n"
           "    assign x1 = 1'b0;\n"
           "  endmodule\n"
           "  assign ",
           {kToken, "x4"},
           " = 1'b0;\n"
           "  assign ",
           {kToken, "x3"},
           " = 1'b0;\n"
           "  assign ",
           {kToken, "x2"},
           " = 1'b0;\n"
           "  assign x1 = 1'b1;\n"
           "endmodule"},

          // generate block, TODO: multi-level
          {"module m;\ngenerate\nendgenerate\nendmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  assign a1 = 1'b1;\n"
           "  generate\n"
           "  endgenerate\n"
           "  assign a1 = 1'b0;\n"
           "endmodule"},
          {"module m;\n"
           "  generate\n"
           "    wire a1;\n"
           "    assign a1 = 1'b1;\n"
           "  endgenerate\n"
           "endmodule"},
          {"module m;\n"
           "  generate\n"
           "    assign ",
           {kToken, "a1"},
           " = 1'b1;\n"
           "  endgenerate\n"
           "endmodule"},
          {"module m;\n"
           "  generate\n"
           "    wire a1;\n"
           "    assign a1 = 1'b1;\n"
           "  endgenerate\n"
           "  assign a1 = 1'b1;\n"
           "endmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  assign a1 = 1'b1;\n"
           "  generate\n"
           "    assign a1 = 1'b1;\n"
           "  endgenerate\n"
           "  assign a1 = 1'b0;\n"
           "endmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  generate\n"
           "    wire a2\n"
           "    assign a1 = 1'b1;\n"
           "  endgenerate\n"
           "  assign ",
           {kToken, "a2"},
           " = 1'b0;\n"
           "endmodule"},
          {"module m;\n"
           "  wire a1;\n"
           "  generate\n"
           "    wire a2;\n"
           "    assign a1 = 1'b1;\n"
           "    assign a2 = a1;\n"
           "  endgenerate\n"
           "  assign a2 = 1'b0;\n"
           "  assign a1 = a2;\n"
           "endmodule"},

          // TODO: nets declared inside terminal/port connection list
          // TODO: assignments/connections inside loop and conditional generate
          // constructs
      };

  RunLintTestCases<VerilogAnalyzer, ForbidImplicitDeclarationsRule>(
      ForbidImplicitDeclarationsTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
