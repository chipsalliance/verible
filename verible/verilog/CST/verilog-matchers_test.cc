// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/CST/verilog-matchers.h"

#include "gtest/gtest.h"
#include "verible/common/analysis/matcher/core-matchers.h"
#include "verible/common/analysis/matcher/matcher-builders.h"
#include "verible/common/analysis/matcher/matcher-test-utils.h"
#include "verible/verilog/CST/verilog-treebuilder-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace {

using verible::matcher::RawMatcherTestCase;

// Tests for SystemTFIdentifierLeaf matching
TEST(VerilogMatchers, SystemTFIdentifierLeafTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = SystemTFIdentifierLeaf(),
       .code = EmbedInClassMethod("$psprintf(\"foo\");"),
       .num_matches = 1},
      {.matcher = SystemTFIdentifierLeaf(),
       .code = EmbedInClassMethod("psprintf(\"foo\");"),
       .num_matches = 0},
      {.matcher = SystemTFIdentifierLeaf(),
       .code = EmbedInClass(""),
       .num_matches = 0},
      {.matcher = SystemTFIdentifierLeaf(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for MacroCallIdLeaf matching
TEST(VerilogMatchers, MacroCallIdLeafTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = MacroCallIdLeaf(), .code = "", .num_matches = 0},
      {.matcher = MacroCallIdLeaf(),
       .code = EmbedInClass(""),
       .num_matches = 0},
      {.matcher = MacroCallIdLeaf(),
       .code = EmbedInClassMethod("`uvm_foo"),
       .num_matches = 0},  // not a call
      {.matcher = MacroCallIdLeaf(),
       .code = "`uvm_foo(\"foo\");",
       .num_matches = 1},
      {.matcher = MacroCallIdLeaf(),
       .code = "`uvm_foo(\"foo\")\n",
       .num_matches = 1},
      {.matcher = MacroCallIdLeaf(),
       .code = EmbedInClassMethod("`uvm_foo(\"foo\");"),
       .num_matches = 1},
      {.matcher = MacroCallIdLeaf(),
       .code = EmbedInClassMethod("`uvm_foo(\"foo\")\n"),
       .num_matches = 1},
      {.matcher = MacroCallIdLeaf(),
       .code = EmbedInClassMethod("uvm_foo(\"foo\");"),
       .num_matches = 0},
      {.matcher = MacroCallIdLeaf(),
       .code = EmbedInClassMethod("$uvm_foo(\"foo\");"),
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for SymbolIdentifierLeaf matching
TEST(VerilogMatchers, SymbolIdentifierLeaf) {
  const RawMatcherTestCase tests[] = {
      {.matcher = SymbolIdentifierLeaf(), .code = "", .num_matches = 0},
      {.matcher = SymbolIdentifierLeaf(),
       .code = EmbedInClass(""),
       .num_matches = 1},  // +1 by the class name
      {.matcher = SymbolIdentifierLeaf(),
       .code = EmbedInClassMethod("reg foo;"),
       .num_matches = 3},  // count +2 by class & method names
      {.matcher = SymbolIdentifierLeaf(),
       .code = EmbedInClassMethod("uvm_foo(\"foo\");"),
       .num_matches = 3},
      {.matcher = SymbolIdentifierLeaf(),
       .code = "parameter foo = 32'hDEADBEEF;",
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NodekVoidcast matching
TEST(VerilogMatchers, VoidCastNodeTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekVoidcast(),
       .code = EmbedInClassMethod("void'(bad());"),
       .num_matches = 1},
      {.matcher = NodekVoidcast(),
       .code = EmbedInClassMethod("rar(bad());"),
       .num_matches = 0},
      {.matcher = NodekVoidcast(), .code = EmbedInClass(""), .num_matches = 0},
      {.matcher = NodekVoidcast(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NodekExpression matching
TEST(VerilogMatchers, ExpressionNodeTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekExpression(),
       .code = EmbedInClassMethod("x = 1;"),
       .num_matches = 1},
      {.matcher = NodekExpression(),
       .code = EmbedInClassMethod("foo();"),
       .num_matches = 0},
      {.matcher = NodekExpression(),
       .code = EmbedInClass(""),
       .num_matches = 0},
      {.matcher = NodekExpression(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for ExpressionHasFunctionCall matching
TEST(VerilogMatchers, ExpressionHasFunctionCallTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClassMethod("foo();"),
       .num_matches = 1},
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClassMethod("x = foo();"),
       .num_matches = 1},
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClassMethod("foo(bar);"),
       .num_matches = 1},
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClassMethod("foo(bar, baz);"),
       .num_matches = 1},
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClassMethod("x = foo;"),
       .num_matches = 0},
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClass(""),
       .num_matches = 0},
      {.matcher = ExpressionHasFunctionCall(), .code = "", .num_matches = 0},
      // Qualified Id function call:
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClassMethod("bar::foo();"),
       .num_matches = 1},
      {.matcher = ExpressionHasFunctionCall(),
       .code = EmbedInClassMethod("x = bar::foo();"),
       .num_matches = 1},
      // This is a method call, different from a function call:
      {.matcher = ExpressionHasFunctionCallNode(
           FunctionCallHasHierarchyExtension(), FunctionCallHasParenGroup()),
       .code = EmbedInClassMethod("bar.foo();"),
       .num_matches = 1},
      {.matcher = ExpressionHasFunctionCallNode(
           FunctionCallHasHierarchyExtension(), FunctionCallHasParenGroup()),
       .code = EmbedInClassMethod("x = bar.foo();"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

TEST(VerilogMatchers, NonCallExpressionHasRandomizeCallExtensionTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NonCallHasRandomizeCallExtension(),
       .code = EmbedInClassMethod("foo.randomize();"),
       .num_matches = 1},
      {.matcher = NonCallHasRandomizeCallExtension(),
       .code = EmbedInClassMethod("foo;"),
       .num_matches = 0},
      {.matcher = NonCallHasRandomizeCallExtension(),
       .code = EmbedInClassMethod("randomize();"),
       .num_matches = 0},
      {.matcher = NonCallHasRandomizeCallExtension(),
       .code = EmbedInClass(""),
       .num_matches = 0},
      {.matcher = NonCallHasRandomizeCallExtension(),
       .code = "",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}
TEST(VerilogMatchers, CallExpressionHasRandomizeCallExtensionTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = CallHasRandomizeCallExtension(),
       .code = EmbedInClassMethod("foo().randomize();"),
       .num_matches = 1},
      {.matcher = CallHasRandomizeCallExtension(),
       .code = EmbedInClassMethod("foo;"),
       .num_matches = 0},
      {.matcher = CallHasRandomizeCallExtension(),
       .code = EmbedInClassMethod("randomize();"),
       .num_matches = 0},
      {.matcher = CallHasRandomizeCallExtension(),
       .code = EmbedInClass(""),
       .num_matches = 0},
      {.matcher = CallHasRandomizeCallExtension(),
       .code = "",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for ExpressionHasRandomizeFunction matching
TEST(VerilogMatchers, ExpressionHasRandomizeFunctionTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = ExpressionHasRandomizeFunction(),
       .code = EmbedInClassMethod("randomize();"),
       .num_matches = 1},
      {.matcher = ExpressionHasRandomizeFunction(),
       .code = EmbedInClassMethod("foo.randomize();"),
       .num_matches = 0},
      {.matcher = ExpressionHasRandomizeFunction(),
       .code = EmbedInClass(""),
       .num_matches = 0},
      {.matcher = ExpressionHasRandomizeFunction(),
       .code = "",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for FunctionCallHasId matching
TEST(VerilogMatchers, FunctionCallHasIdTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = FunctionCallHasId(),
       .code = EmbedInClassMethod("foo();"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NumberHasConstantWidth matching
TEST(VerilogMatchers, NumberHasConstantWidthTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NumberHasConstantWidth(), .code = "", .num_matches = 0},
      {.matcher = NumberHasConstantWidth(),
       .code = "localparam x = 1'bx;",
       .num_matches = 1},
      {.matcher = NumberHasConstantWidth(),
       .code = "localparam x = 2'b 01;",
       .num_matches = 1},
      {.matcher = NumberHasConstantWidth(),
       .code = "localparam x = 8'b0000_1111;",
       .num_matches = 1},
      {.matcher = NumberHasConstantWidth(),
       .code = "localparam x = `WIDTH'bx;",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NumberHasBasedLiteral matching
TEST(VerilogMatchers, NumberHasBasedLiteralTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NumberHasBasedLiteral(), .code = "", .num_matches = 0},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 1'0;",
       .num_matches = 0},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 1'b0;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 1'B1;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 4'b 10_10;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 8'b0000_1111;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = `WIDTH'bx;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 2'sb0;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 2'SB1;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 32'd 2000;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 32'D 4095;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 32'o 66666;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 32'O 777_777;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 32'h 0000_4321;",
       .num_matches = 1},
      {.matcher = NumberHasBasedLiteral(),
       .code = "localparam x = 64'H aaaa_bbbb_cccc_dddd;",
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NumberIsBinary matching
TEST(VerilogMatchers, NumberIsBinaryTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NumberIsBinary(), .code = "", .num_matches = 0},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 1'0;",
       .num_matches = 0},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 1'b0;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 1'B1;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 4'b 10_10;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 8'b0000_1111;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = `WIDTH'bx;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = `WIDTH'b`DIGITS;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 2'sb0;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 2'SB1;",
       .num_matches = 1},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 32'd 2000;",
       .num_matches = 0},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 32'D 4095;",
       .num_matches = 0},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 32'o 66666;",
       .num_matches = 0},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 32'O 777_777;",
       .num_matches = 0},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 32'h 0000_4321;",
       .num_matches = 0},
      {.matcher = NumberIsBinary(),
       .code = "localparam x = 64'H aaaa_bbbb_cccc_dddd;",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NumberHasBInaryDigits matching
TEST(VerilogMatchers, NumberHasBinaryDigitsTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NumberHasBinaryDigits(), .code = "", .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 1'0;",
       .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 1'b0;",
       .num_matches = 1},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 1'B1;",
       .num_matches = 1},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 4'b 10_10;",
       .num_matches = 1},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 8'b0000_1111;",
       .num_matches = 1},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = `WIDTH'bx;",
       .num_matches = 1},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = `WIDTH'b`DIGITS;",
       .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 2'sb0;",
       .num_matches = 1},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 2'SB1;",
       .num_matches = 1},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 32'd 2000;",
       .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 32'D 4095;",
       .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 32'o 66666;",
       .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 32'O 777_777;",
       .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 32'h 0000_4321;",
       .num_matches = 0},
      {.matcher = NumberHasBinaryDigits(),
       .code = "localparam x = 64'H aaaa_bbbb_cccc_dddd;",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NodekActualParameterList matching
TEST(VerilogMatchers, ActualParameterListNodeTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekActualParameterList(),
       .code = EmbedInModule("foo #(1, 2) bar;"),
       .num_matches = 1},
      {.matcher = NodekActualParameterList(),
       .code = EmbedInModule("foo #(.foo(1), .bar(5)) bar;"),
       .num_matches = 1},
      {.matcher = NodekActualParameterList(),
       .code = EmbedInModule("foo bar;"),
       .num_matches = 0},
      {.matcher = NodekActualParameterList(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = NodekActualParameterList(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for ActualParameterListHasPositionalParameterList matching
TEST(VerilogMatchers, ActualParameterListHasPositionalParameterListTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = ActualParameterListHasPositionalParameterList(),
       .code = EmbedInModule("foo #(1, 2) bar;"),
       .num_matches = 1},
      {.matcher = ActualParameterListHasPositionalParameterList(),
       .code = EmbedInModule("foo #(.foo(1), .bar(5)) bar;"),
       .num_matches = 0},
      {.matcher = ActualParameterListHasPositionalParameterList(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = ActualParameterListHasPositionalParameterList(),
       .code = "",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NodekGateInstance matching
TEST(VerilogMatchers, GateInstanceNodeTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekGateInstance(),
       .code = EmbedInModule("foo bar(1, 2);"),
       .num_matches = 1},
      {.matcher = NodekGateInstance(),
       .code = EmbedInModule("foo bar;"),
       .num_matches = 0},
      {.matcher = NodekGateInstance(),
       .code = EmbedInModule("and a0(a, b, x1);"),
       .num_matches = 0},
      {.matcher = NodekGateInstance(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = NodekGateInstance(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for GateInstanceHasPortList matching
TEST(VerilogMatchers, GateInstanceHasPortListTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = GateInstanceHasPortList(),
       .code = EmbedInModule("foo bar(1, 2);"),
       .num_matches = 1},
      {.matcher = GateInstanceHasPortList(),
       .code = EmbedInModule("foo bar;"),
       .num_matches = 0},
      {.matcher = GateInstanceHasPortList(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = GateInstanceHasPortList(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NodekGenerateBlock matching
TEST(VerilogMatchers, GenerateBlockNodeTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekGenerateBlock(),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin : gen_posedge\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 1},
      {.matcher = NodekGenerateBlock(),
       .code = EmbedInModule("generate\n"
                             "endgenerate"),
       .num_matches = 0},
      {.matcher = NodekGenerateBlock(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = NodekGenerateBlock(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for GenerateRegionNode matching
TEST(VerilogMatchers, GenerateRegionNodeTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekGenerateRegion(),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin : foobar\n"
                             "  foo bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 1},
      {.matcher = NodekGenerateRegion(),
       .code = EmbedInModule("generate\n"
                             "endgenerate"),
       .num_matches = 1},
      {.matcher = NodekGenerateRegion(),
       .code = EmbedInModule("wire rats_nest;"),
       .num_matches = 0},
      {.matcher = NodekGenerateRegion(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = NodekGenerateRegion(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for HasBeginLabel matching
TEST(VerilogMatchers, HasBeginLabelTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = HasBeginLabel(),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin : gen_posedge\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 1},
      {.matcher = HasBeginLabel(),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 0},
      {.matcher = HasBeginLabel(), .code = EmbedInModule(""), .num_matches = 0},
      {.matcher = HasBeginLabel(), .code = "", .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for HasGenerateBlock matching
TEST(VerilogMatchers, HasGenerateBlockTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = HasGenerateBlock(), .code = "", .num_matches = 0},
      {.matcher = HasGenerateBlock(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = HasGenerateBlock(),
       .code = EmbedInModule("generate\n"
                             "begin\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 1},
      {.matcher = HasGenerateBlock(),
       .code = EmbedInModule("generate\n"
                             "begin\n"
                             "  genvar j;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 1},
      {.matcher = HasGenerateBlock(),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin : gen_posedge\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 0},
      {.matcher = HasGenerateBlock(),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for integration between NodekGenerateBlock and HasBeginLabel
TEST(VerilogMatchers, GenerateBlockHasBeginLabelTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekGenerateBlock(verible::matcher::Unless(HasBeginLabel())),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 1},
      {.matcher = NodekGenerateBlock(verible::matcher::Unless(HasBeginLabel())),
       .code = EmbedInModule("generate\n"
                             "if (TypeIsPosedge) begin : gen_posedge\n"
                             "  always @(posedge clk) foo <= bar;\n"
                             "end\n"
                             "endgenerate"),
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for NodekAlwaysStatement matching
TEST(VerilogMatchers, AlwaysStatementNodeTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule("initial begin a <= 0; end"),
       .num_matches = 0},
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule("always_ff begin a <= b; end"),
       .num_matches = 1},
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule("always_comb begin a = b; end"),
       .num_matches = 1},
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule("always @* begin a = b; end"),
       .num_matches = 1},
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule("always @(*) begin a = b; end"),
       .num_matches = 1},
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule("always @(posedge foo) begin a <= b; end"),
       .num_matches = 1},
      {.matcher = NodekAlwaysStatement(),
       .code = EmbedInModule("always_ff begin a <= b; end\n"
                             "always_comb begin a = b; end"),
       .num_matches = 2},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for AlwaysKeyword matching
TEST(VerilogMatchers, AlwaysKeywordTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = AlwaysKeyword(), .code = EmbedInModule(""), .num_matches = 0},
      {.matcher = AlwaysKeyword(),
       .code = EmbedInModule("initial begin a <= 0; end"),
       .num_matches = 0},
      {.matcher = AlwaysKeyword(),
       .code = EmbedInModule("always_ff begin a <= b; end"),
       .num_matches = 0},
      {.matcher = AlwaysKeyword(),
       .code = EmbedInModule("always_comb begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysKeyword(),
       .code = EmbedInModule("always @* begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysKeyword(),
       .code = EmbedInModule("always @(*) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysKeyword(),
       .code = EmbedInModule("always @(posedge foo) begin a <= b; end"),
       .num_matches = 1},
      {.matcher = AlwaysKeyword(),
       .code = EmbedInModule("always_ff begin a <= b; end\n"
                             "always_comb begin a = b; end"),
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for AlwaysCombKeyword matching
TEST(VerilogMatchers, AlwaysCombKeywordTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule("initial begin a <= 0; end"),
       .num_matches = 0},
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule("always_ff begin a <= b; end"),
       .num_matches = 0},
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule("always_comb begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule("always @* begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule("always @(*) begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule("always @(posedge foo) begin a <= b; end"),
       .num_matches = 0},
      {.matcher = AlwaysCombKeyword(),
       .code = EmbedInModule("always_ff begin a <= b; end\n"
                             "always_comb begin a = b; end"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for AlwaysFFKeyword matching
TEST(VerilogMatchers, AlwaysFFKeywordTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule("initial begin a <= 0; end"),
       .num_matches = 0},
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule("always_ff begin a <= b; end"),
       .num_matches = 1},
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule("always_comb begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule("always @* begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule("always @(*) begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule("always @(posedge foo) begin a <= b; end"),
       .num_matches = 0},
      {.matcher = AlwaysFFKeyword(),
       .code = EmbedInModule("always_ff begin a <= b; end\n"
                             "always_comb begin a = b; end"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for AlwaysStatementHasEventControlStar matching
TEST(VerilogMatchers, AlwaysStatementHasEventControlStarTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = AlwaysStatementHasEventControlStar(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = AlwaysStatementHasEventControlStar(),
       .code = EmbedInModule("initial begin a <= 0; end"),
       .num_matches = 0},
      {.matcher = AlwaysStatementHasEventControlStar(),
       .code = EmbedInModule("always_ff begin a <= b; end"),
       .num_matches = 0},
      {.matcher = AlwaysStatementHasEventControlStar(),
       .code = EmbedInModule("always_comb begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysStatementHasEventControlStar(),
       .code = EmbedInModule("always @* begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasEventControlStar(),
       .code = EmbedInModule("always @(posedge foo) begin a <= b; end"),
       .num_matches = 0},
      {.matcher = AlwaysStatementHasEventControlStar(),
       .code = EmbedInModule("always_ff begin a <= b; end\n"
                             "always_comb begin a = b; end"),
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

TEST(VerilogMatchers, AlwaysStatementHasEventControlStarAndParentheses) {
  const RawMatcherTestCase tests[] = {
      {.matcher = AlwaysStatementHasEventControlStarAndParentheses(),
       .code = EmbedInModule("always @* begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysStatementHasEventControlStarAndParentheses(),
       .code = EmbedInModule("always @(*) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasEventControlStarAndParentheses(),
       .code = EmbedInModule("always @( *) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasEventControlStarAndParentheses(),
       .code = EmbedInModule("always @(* ) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasEventControlStarAndParentheses(),
       .code = EmbedInModule("always @( * ) begin a = b; end"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for AlwaysStatementHasParentheses matching
TEST(VerilogMatchers, AlwaysStatementHasParentheses) {
  const RawMatcherTestCase tests[] = {
      {.matcher = AlwaysStatementHasParentheses(),
       .code = EmbedInModule("always @* begin a = b; end"),
       .num_matches = 0},
      {.matcher = AlwaysStatementHasParentheses(),
       .code = EmbedInModule("always @(*) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasParentheses(),
       .code = EmbedInModule("always @( *) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasParentheses(),
       .code = EmbedInModule("always @(* ) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasParentheses(),
       .code = EmbedInModule("always @( * ) begin a = b; end"),
       .num_matches = 1},
      {.matcher = AlwaysStatementHasParentheses(),
       .code = EmbedInModule("always @(posedge foo) begin a <= b; end"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests that matcher finds left-hand-sides of assignments.
TEST(VerilogMatchers, PathkLPValueTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod(""),
       .num_matches = 0},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("foo();"),
       .num_matches = 0},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x = 1;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x = a + b;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInModule("initial begin\nfoo();\nend\n"),
       .num_matches = 0},
      {.matcher = PathkLPValue(),
       .code = EmbedInModule("initial begin\nx = 1;\nend\n"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x[1] = 1;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x.y = 2;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x[0].y = 3;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x.y[2] = 4;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x[0].y[0] = 5;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("if (0) x = 1;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("forever x = y;"),
       .num_matches = 1},
      {.matcher = PathkLPValue(),
       .code = EmbedInClassMethod("x = 1;\ny = two();"),
       .num_matches = 2},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests that matcher finds assignments values that are function calls.
TEST(VerilogMatchers, RValueIsFunctionCallTest) {
  const RawMatcherTestCase tests[] = {
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod(""),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = 1;"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = y / z;"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = y / z();"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = foo();"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = bar::foo();"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = bar::foo(a);"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x.y = bar::foo(a);"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = bar.foo();"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("z = pkg::bar::foo();"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = bar::foo() -12;"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(),
       .code = EmbedInClassMethod("x = a + bar::foo();"),
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests that qualified function calls are found.
TEST(VerilogMatchers, FunctionCallIsQualifiedTest) {
  const RawMatcherTestCase tests[] = {
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod(""),
       .num_matches = 0},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = 1;"),
       .num_matches = 0},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = y / z;"),
       .num_matches = 0},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = y / z();"),
       .num_matches = 0},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = foo();"),
       .num_matches = 0},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = bar::foo();"),
       .num_matches = 1},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = bar::foo(a);"),
       .num_matches = 1},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x.y = bar::foo(a);"),
       .num_matches = 1},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = bar.foo();"),
       .num_matches = 0},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("z = pkg::bar::foo();"),
       .num_matches = 1},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = bar::foo() -12;"),
       .num_matches = 1},
      {.matcher = FunctionCallIsQualified(),
       .code = EmbedInClassMethod("x = a + bar::foo();"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests that assignments to qualified function calls are found.
TEST(VerilogMatchers, RValueFunctionCallIsQualifiedTest) {
  const RawMatcherTestCase tests[] = {
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod(""),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = 1;"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = y / z;"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = y / z();"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = foo();"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = bar::foo();"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("bar::foo();"),
       .num_matches = 0},  // no assignment
      // TODO(fangism): The following test case wrongly matches, but only
      // because the matcher matches two different function calls: qqq is the
      // outermost match, but the recursive inner match finds a qualified
      // function call deeper in the syntax tree.  Solution is to use a direct
      // inner matcher that does not search recursively.
      // {RValueIsFunctionCall(FunctionCallIsQualified()),
      //  EmbedInClassMethod("qqq(bar::foo(), 1, 2);"), 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("y = z();\nbar::foo();"),
       .num_matches = 0},  // no matching assignment
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = bar::foo(a);"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x.y = bar::foo(a);"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = bar.foo();"),
       .num_matches = 0},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("z = pkg::bar::foo();"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("z = pkg::bar::foo(\"a\", b, cc);"),
       .num_matches = 1},
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = bar::foo() -12;"),
       .num_matches = 0},  // outermost rvalue is -
      {.matcher = RValueIsFunctionCall(FunctionCallIsQualified()),
       .code = EmbedInClassMethod("x = a + bar::foo();"),
       .num_matches = 0},  // outermost rvalue is +
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests that function call arguments are matched.
TEST(VerilogMatchers, FunctionCallArgumentsTest) {
  const RawMatcherTestCase tests[] = {
      {.matcher = FunctionCallArguments(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod(""),
       .num_matches = 0},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("a = 1 + 2;"),
       .num_matches = 0},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("foobar();"),
       .num_matches = 0},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("foobar(1);"),
       .num_matches = 1},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("foobar(a, b, c);"),
       .num_matches = 1},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("`foobar();"),
       .num_matches = 0},  // macro call arguments are different
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("`foobar(1);"),
       .num_matches = 0},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("`foobar(a, b, c);"),
       .num_matches = 0},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("foo(bar());"),
       .num_matches = 1},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("x = foobar();"),
       .num_matches = 0},
      {.matcher = FunctionCallArguments(),
       .code = EmbedInClassMethod("f.g.h = foobar(g);"),
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests that ranged dimensions are matched.
TEST(VerilogMatchers, DeclarationDimensionsHasRanges) {
  const RawMatcherTestCase tests[] = {
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = "",
       .num_matches = 0},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = EmbedInModule(""),
       .num_matches = 0},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = "wire w;",
       .num_matches = 0},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = "wire [1:0] w;",
       .num_matches = 1},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = "wire w [1:0];",
       .num_matches = 1},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = "wire [1:2] w [1:0];",
       .num_matches = 2},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = EmbedInModule("wire w;"),
       .num_matches = 0},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = EmbedInModule("wire [1:0] w;"),
       .num_matches = 1},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = EmbedInModule("wire w [1:0];"),
       .num_matches = 1},
      {.matcher = DeclarationDimensionsHasRanges(),
       .code = EmbedInModule("wire [1:2] w [1:0];"),
       .num_matches = 2},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for HasDefaultCase matching.
TEST(VerilogMatchers, HasDefaultCaseTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = HasDefaultCase(), .code = "", .num_matches = 0},
      {.matcher = HasDefaultCase(),
       .code = R"(
       function automatic int foo (input in);
         case (in)
           default: return 0;
         endcase
       endfunction
       )",
       .num_matches = 1},
      {.matcher = HasDefaultCase(),
       .code = R"(
       function automatic int foo (input in);
         case (in)
           1: return 0;
         endcase
       endfunction
       )",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for HasUniqueQualifier matching.
TEST(VerilogMatchers, HasUniqueQualifierTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = HasUniqueQualifier(), .code = "", .num_matches = 0},
      {.matcher = HasUniqueQualifier(),
       .code = R"(
       function automatic int foo (input in);
         case (in)
           default: return 0;
         endcase
       endfunction
       )",
       .num_matches = 0},
      {.matcher = HasUniqueQualifier(),
       .code = R"(
       function automatic int foo (input in);
         unique case (in)
           1: return 0;
         endcase
       endfunction
       )",
       .num_matches = 1},
      {.matcher = HasUniqueQualifier(),
       .code = R"(
       function automatic int foo (input in);
         unique if (in) begin
           return 0;
         end
         else if(!in) begin
           return 1;
         end
       endfunction
       )",
       .num_matches = 1},
      {.matcher = HasUniqueQualifier(),
       .code = R"(
       function automatic int foo (input in);
         unique0 case (in)
           1: return 0;
         endcase
       endfunction
       )",
       .num_matches = 0},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

// Tests for HasUnique0Qualifier matching.
TEST(VerilogMatchers, HasUnique0QualifierTests) {
  const RawMatcherTestCase tests[] = {
      {.matcher = HasUnique0Qualifier(), .code = "", .num_matches = 0},
      {.matcher = HasUnique0Qualifier(),
       .code = R"(
       function automatic int foo (input in);
         case (in)
           default: return 0;
         endcase
       endfunction
       )",
       .num_matches = 0},
      {.matcher = HasUnique0Qualifier(),
       .code = R"(
       function automatic int foo (input in);
         unique0 case (in)
           1: return 0;
         endcase
       endfunction
       )",
       .num_matches = 1},
      {.matcher = HasUnique0Qualifier(),
       .code = R"(
       function automatic int foo (input in);
         unique case (in)
           1: return 0;
         endcase
       endfunction
       )",
       .num_matches = 0},
      {.matcher = HasUnique0Qualifier(),
       .code = R"(
       function automatic int foo (input in);
         unique0 if (in) begin
           return 0;
         end
         else if(!in) begin
           return 1;
         end
       endfunction
       )",
       .num_matches = 1},
  };
  for (const auto &test : tests) {
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
  }
}

}  // namespace
}  // namespace verilog
