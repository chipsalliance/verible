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

#include "verilog/CST/verilog_matchers.h"

#include "gtest/gtest.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/matcher/matcher_test_utils.h"
#include "verilog/CST/verilog_treebuilder_utils.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

using verible::matcher::RawMatcherTestCase;

// Tests for SystemTFIdentifierLeaf matching
TEST(VerilogMatchers, SystemTFIdentifierLeafTests) {
  const RawMatcherTestCase tests[] = {
      {SystemTFIdentifierLeaf(), EmbedInClassMethod("$psprintf(\"foo\");"), 1},
      {SystemTFIdentifierLeaf(), EmbedInClassMethod("psprintf(\"foo\");"), 0},
      {SystemTFIdentifierLeaf(), EmbedInClass(""), 0},
      {SystemTFIdentifierLeaf(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for MacroCallIdLeaf matching
TEST(VerilogMatchers, MacroCallIdLeafTests) {
  const RawMatcherTestCase tests[] = {
      {MacroCallIdLeaf(), "", 0},
      {MacroCallIdLeaf(), EmbedInClass(""), 0},
      {MacroCallIdLeaf(), EmbedInClassMethod("`uvm_foo"), 0},  // not a call
      {MacroCallIdLeaf(), "`uvm_foo(\"foo\");", 1},
      {MacroCallIdLeaf(), "`uvm_foo(\"foo\")\n", 1},
      {MacroCallIdLeaf(), EmbedInClassMethod("`uvm_foo(\"foo\");"), 1},
      {MacroCallIdLeaf(), EmbedInClassMethod("`uvm_foo(\"foo\")\n"), 1},
      {MacroCallIdLeaf(), EmbedInClassMethod("uvm_foo(\"foo\");"), 0},
      {MacroCallIdLeaf(), EmbedInClassMethod("$uvm_foo(\"foo\");"), 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for SymbolIdentifierLeaf matching
TEST(VerilogMatchers, SymbolIdentifierLeaf) {
  const RawMatcherTestCase tests[] = {
      {SymbolIdentifierLeaf(), "", 0},
      {SymbolIdentifierLeaf(), EmbedInClass(""), 1}, // +1 by the class name
      {SymbolIdentifierLeaf(),
        EmbedInClassMethod("reg foo;"), 3}, // count +2 by class & method names
      {SymbolIdentifierLeaf(), EmbedInClassMethod("uvm_foo(\"foo\");"), 3},
      {SymbolIdentifierLeaf(), "parameter foo = 32'hDEADBEEF;", 1},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NodekVoidcast matching
TEST(VerilogMatchers, VoidCastNodeTests) {
  const RawMatcherTestCase tests[] = {
      {NodekVoidcast(), EmbedInClassMethod("void'(bad());"), 1},
      {NodekVoidcast(), EmbedInClassMethod("rar(bad());"), 0},
      {NodekVoidcast(), EmbedInClass(""), 0},
      {NodekVoidcast(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NodekExpression matching
TEST(VerilogMatchers, ExpressionNodeTests) {
  const RawMatcherTestCase tests[] = {
      {NodekExpression(), EmbedInClassMethod("x = 1;"), 1},
      {NodekExpression(), EmbedInClassMethod("foo();"), 0},
      {NodekExpression(), EmbedInClass(""), 0},
      {NodekExpression(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for ExpressionHasFunctionCall matching
TEST(VerilogMatchers, ExpressionHasFunctionCallTests) {
  const RawMatcherTestCase tests[] = {
      {ExpressionHasFunctionCall(), EmbedInClassMethod("foo();"), 1},
      {ExpressionHasFunctionCall(), EmbedInClassMethod("x = foo();"), 1},
      {ExpressionHasFunctionCall(), EmbedInClassMethod("foo(bar);"), 1},
      {ExpressionHasFunctionCall(), EmbedInClassMethod("foo(bar, baz);"), 1},
      {ExpressionHasFunctionCall(), EmbedInClassMethod("x = foo;"), 0},
      {ExpressionHasFunctionCall(), EmbedInClass(""), 0},
      {ExpressionHasFunctionCall(), "", 0},
      // Qualified Id function call:
      {ExpressionHasFunctionCall(), EmbedInClassMethod("bar::foo();"), 1},
      {ExpressionHasFunctionCall(), EmbedInClassMethod("x = bar::foo();"), 1},
      // This is a method call, different from a function call:
      {ExpressionHasFunctionCall(), EmbedInClassMethod("bar.foo();"), 0},
      {ExpressionHasFunctionCall(), EmbedInClassMethod("x = bar.foo();"), 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for ExpressionHasRandomizeCallExtension matching
TEST(VerilogMatchers, ExpressionHasRandomizeCallExtensionTests) {
  const RawMatcherTestCase tests[] = {
      {ExpressionHasRandomizeCallExtension(),
       EmbedInClassMethod("foo().randomize();"), 1},
      {ExpressionHasRandomizeCallExtension(), EmbedInClassMethod("foo;"), 0},
      {ExpressionHasRandomizeCallExtension(),
       EmbedInClassMethod("randomize();"), 0},
      {ExpressionHasRandomizeCallExtension(), EmbedInClass(""), 0},
      {ExpressionHasRandomizeCallExtension(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for ExpressionHasRandomizeFunction matching
TEST(VerilogMatchers, ExpressionHasRandomizeFunctionTests) {
  const RawMatcherTestCase tests[] = {
      {ExpressionHasRandomizeFunction(), EmbedInClassMethod("randomize();"), 1},
      {ExpressionHasRandomizeFunction(), EmbedInClassMethod("foo.randomize();"),
       0},
      {ExpressionHasRandomizeFunction(), EmbedInClass(""), 0},
      {ExpressionHasRandomizeFunction(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for FunctionCallHasId matching
TEST(VerilogMatchers, FunctionCallHasIdTests) {
  const RawMatcherTestCase tests[] = {
      {FunctionCallHasId(), EmbedInClassMethod("foo();"), 1},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NumberHasConstantWidth matching
TEST(VerilogMatchers, NumberHasConstantWidthTests) {
  const RawMatcherTestCase tests[] = {
      {NumberHasConstantWidth(), "", 0},
      {NumberHasConstantWidth(), "localparam x = 1'bx;", 1},
      {NumberHasConstantWidth(), "localparam x = 2'b 01;", 1},
      {NumberHasConstantWidth(), "localparam x = 8'b0000_1111;", 1},
      {NumberHasConstantWidth(), "localparam x = `WIDTH'bx;", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NumberHasBasedLiteral matching
TEST(VerilogMatchers, NumberHasBasedLiteralTests) {
  const RawMatcherTestCase tests[] = {
      {NumberHasBasedLiteral(), "", 0},
      {NumberHasBasedLiteral(), "localparam x = 1'0;", 0},
      {NumberHasBasedLiteral(), "localparam x = 1'b0;", 1},
      {NumberHasBasedLiteral(), "localparam x = 1'B1;", 1},
      {NumberHasBasedLiteral(), "localparam x = 4'b 10_10;", 1},
      {NumberHasBasedLiteral(), "localparam x = 8'b0000_1111;", 1},
      {NumberHasBasedLiteral(), "localparam x = `WIDTH'bx;", 1},
      {NumberHasBasedLiteral(), "localparam x = 2'sb0;", 1},
      {NumberHasBasedLiteral(), "localparam x = 2'SB1;", 1},
      {NumberHasBasedLiteral(), "localparam x = 32'd 2000;", 1},
      {NumberHasBasedLiteral(), "localparam x = 32'D 4095;", 1},
      {NumberHasBasedLiteral(), "localparam x = 32'o 66666;", 1},
      {NumberHasBasedLiteral(), "localparam x = 32'O 777_777;", 1},
      {NumberHasBasedLiteral(), "localparam x = 32'h 0000_4321;", 1},
      {NumberHasBasedLiteral(), "localparam x = 64'H aaaa_bbbb_cccc_dddd;", 1},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NumberIsBinary matching
TEST(VerilogMatchers, NumberIsBinaryTests) {
  const RawMatcherTestCase tests[] = {
      {NumberIsBinary(), "", 0},
      {NumberIsBinary(), "localparam x = 1'0;", 0},
      {NumberIsBinary(), "localparam x = 1'b0;", 1},
      {NumberIsBinary(), "localparam x = 1'B1;", 1},
      {NumberIsBinary(), "localparam x = 4'b 10_10;", 1},
      {NumberIsBinary(), "localparam x = 8'b0000_1111;", 1},
      {NumberIsBinary(), "localparam x = `WIDTH'bx;", 1},
      {NumberIsBinary(), "localparam x = `WIDTH'b`DIGITS;", 1},
      {NumberIsBinary(), "localparam x = 2'sb0;", 1},
      {NumberIsBinary(), "localparam x = 2'SB1;", 1},
      {NumberIsBinary(), "localparam x = 32'd 2000;", 0},
      {NumberIsBinary(), "localparam x = 32'D 4095;", 0},
      {NumberIsBinary(), "localparam x = 32'o 66666;", 0},
      {NumberIsBinary(), "localparam x = 32'O 777_777;", 0},
      {NumberIsBinary(), "localparam x = 32'h 0000_4321;", 0},
      {NumberIsBinary(), "localparam x = 64'H aaaa_bbbb_cccc_dddd;", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NumberHasBInaryDigits matching
TEST(VerilogMatchers, NumberHasBinaryDigitsTests) {
  const RawMatcherTestCase tests[] = {
      {NumberHasBinaryDigits(), "", 0},
      {NumberHasBinaryDigits(), "localparam x = 1'0;", 0},
      {NumberHasBinaryDigits(), "localparam x = 1'b0;", 1},
      {NumberHasBinaryDigits(), "localparam x = 1'B1;", 1},
      {NumberHasBinaryDigits(), "localparam x = 4'b 10_10;", 1},
      {NumberHasBinaryDigits(), "localparam x = 8'b0000_1111;", 1},
      {NumberHasBinaryDigits(), "localparam x = `WIDTH'bx;", 1},
      {NumberHasBinaryDigits(), "localparam x = `WIDTH'b`DIGITS;", 0},
      {NumberHasBinaryDigits(), "localparam x = 2'sb0;", 1},
      {NumberHasBinaryDigits(), "localparam x = 2'SB1;", 1},
      {NumberHasBinaryDigits(), "localparam x = 32'd 2000;", 0},
      {NumberHasBinaryDigits(), "localparam x = 32'D 4095;", 0},
      {NumberHasBinaryDigits(), "localparam x = 32'o 66666;", 0},
      {NumberHasBinaryDigits(), "localparam x = 32'O 777_777;", 0},
      {NumberHasBinaryDigits(), "localparam x = 32'h 0000_4321;", 0},
      {NumberHasBinaryDigits(), "localparam x = 64'H aaaa_bbbb_cccc_dddd;", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NodekActualParameterList matching
TEST(VerilogMatchers, ActualParameterListNodeTests) {
  const RawMatcherTestCase tests[] = {
      {NodekActualParameterList(), EmbedInModule("foo #(1, 2) bar;"), 1},
      {NodekActualParameterList(),
       EmbedInModule("foo #(.foo(1), .bar(5)) bar;"), 1},
      {NodekActualParameterList(), EmbedInModule("foo bar;"), 0},
      {NodekActualParameterList(), EmbedInModule(""), 0},
      {NodekActualParameterList(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for ActualParameterListHasPositionalParameterList matching
TEST(VerilogMatchers, ActualParameterListHasPositionalParameterListTests) {
  const RawMatcherTestCase tests[] = {
      {ActualParameterListHasPositionalParameterList(),
       EmbedInModule("foo #(1, 2) bar;"), 1},
      {ActualParameterListHasPositionalParameterList(),
       EmbedInModule("foo #(.foo(1), .bar(5)) bar;"), 0},
      {ActualParameterListHasPositionalParameterList(), EmbedInModule(""), 0},
      {ActualParameterListHasPositionalParameterList(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NodekGateInstance matching
TEST(VerilogMatchers, GateInstanceNodeTests) {
  const RawMatcherTestCase tests[] = {
      {NodekGateInstance(), EmbedInModule("foo bar(1, 2);"), 1},
      {NodekGateInstance(), EmbedInModule("foo bar;"), 0},
      {NodekGateInstance(), EmbedInModule("and a0(a, b, x1);"), 0},
      {NodekGateInstance(), EmbedInModule(""), 0},
      {NodekGateInstance(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for GateInstanceHasPortList matching
TEST(VerilogMatchers, GateInstanceHasPortListTests) {
  const RawMatcherTestCase tests[] = {
      {GateInstanceHasPortList(), EmbedInModule("foo bar(1, 2);"), 1},
      {GateInstanceHasPortList(), EmbedInModule("foo bar;"), 0},
      {GateInstanceHasPortList(), EmbedInModule(""), 0},
      {GateInstanceHasPortList(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NodekGenerateBlock matching
TEST(VerilogMatchers, GenerateBlockNodeTests) {
  const RawMatcherTestCase tests[] = {
      {NodekGenerateBlock(),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin : gen_posedge\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       1},
      {NodekGenerateBlock(),
       EmbedInModule("generate\n"
                     "endgenerate"),
       0},
      {NodekGenerateBlock(), EmbedInModule(""), 0},
      {NodekGenerateBlock(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for GenerateRegionNode matching
TEST(VerilogMatchers, GenerateRegionNodeTests) {
  const RawMatcherTestCase tests[] = {
      {NodekGenerateRegion(),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin : foobar\n"
                     "  foo bar;\n"
                     "end\n"
                     "endgenerate"),
       1},
      {NodekGenerateRegion(),
       EmbedInModule("generate\n"
                     "endgenerate"),
       1},
      {NodekGenerateRegion(), EmbedInModule("wire rats_nest;"), 0},
      {NodekGenerateRegion(), EmbedInModule(""), 0},
      {NodekGenerateRegion(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for HasBeginLabel matching
TEST(VerilogMatchers, HasBeginLabelTests) {
  const RawMatcherTestCase tests[] = {
      {HasBeginLabel(),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin : gen_posedge\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       1},
      {HasBeginLabel(),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       0},
      {HasBeginLabel(), EmbedInModule(""), 0},
      {HasBeginLabel(), "", 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for HasGenerateBlock matching
TEST(VerilogMatchers, HasGenerateBlockTests) {
  const RawMatcherTestCase tests[] = {
      {HasGenerateBlock(), "", 0},
      {HasGenerateBlock(), EmbedInModule(""), 0},
      {HasGenerateBlock(),
       EmbedInModule("generate\n"
                     "begin\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       1},
      {HasGenerateBlock(),
       EmbedInModule("generate\n"
                     "begin\n"
                     "  genvar j;\n"
                     "end\n"
                     "endgenerate"),
       1},
      {HasGenerateBlock(),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin : gen_posedge\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       0},
      {HasGenerateBlock(),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for integration between NodekGenerateBlock and HasBeginLabel
TEST(VerilogMatchers, GenerateBlockHasBeginLabelTests) {
  const RawMatcherTestCase tests[] = {
      {NodekGenerateBlock(verible::matcher::Unless(HasBeginLabel())),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       1},
      {NodekGenerateBlock(verible::matcher::Unless(HasBeginLabel())),
       EmbedInModule("generate\n"
                     "if (TypeIsPosedge) begin : gen_posedge\n"
                     "  always @(posedge clk) foo <= bar;\n"
                     "end\n"
                     "endgenerate"),
       0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for NodekAlwaysStatement matching
TEST(VerilogMatchers, AlwaysStatementNodeTests) {
  const RawMatcherTestCase tests[] = {
      {NodekAlwaysStatement(), EmbedInModule(""), 0},
      {NodekAlwaysStatement(), EmbedInModule("initial begin a <= 0; end"), 0},
      {NodekAlwaysStatement(), EmbedInModule("always_ff begin a <= b; end"), 1},
      {NodekAlwaysStatement(), EmbedInModule("always_comb begin a = b; end"),
       1},
      {NodekAlwaysStatement(), EmbedInModule("always @* begin a = b; end"), 1},
      {NodekAlwaysStatement(), EmbedInModule("always @(*) begin a = b; end"),
       1},
      {NodekAlwaysStatement(),
       EmbedInModule("always @(posedge foo) begin a <= b; end"), 1},
      {NodekAlwaysStatement(),
       EmbedInModule("always_ff begin a <= b; end\n"
                     "always_comb begin a = b; end"),
       2},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for AlwaysKeyword matching
TEST(VerilogMatchers, AlwaysKeywordTests) {
  const RawMatcherTestCase tests[] = {
      {AlwaysKeyword(), EmbedInModule(""), 0},
      {AlwaysKeyword(), EmbedInModule("initial begin a <= 0; end"), 0},
      {AlwaysKeyword(), EmbedInModule("always_ff begin a <= b; end"), 0},
      {AlwaysKeyword(), EmbedInModule("always_comb begin a = b; end"), 0},
      {AlwaysKeyword(), EmbedInModule("always @* begin a = b; end"), 1},
      {AlwaysKeyword(), EmbedInModule("always @(*) begin a = b; end"), 1},
      {AlwaysKeyword(),
       EmbedInModule("always @(posedge foo) begin a <= b; end"), 1},
      {AlwaysKeyword(),
       EmbedInModule("always_ff begin a <= b; end\n"
                     "always_comb begin a = b; end"),
       0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for AlwaysCombKeyword matching
TEST(VerilogMatchers, AlwaysCombKeywordTests) {
  const RawMatcherTestCase tests[] = {
      {AlwaysCombKeyword(), EmbedInModule(""), 0},
      {AlwaysCombKeyword(), EmbedInModule("initial begin a <= 0; end"), 0},
      {AlwaysCombKeyword(), EmbedInModule("always_ff begin a <= b; end"), 0},
      {AlwaysCombKeyword(), EmbedInModule("always_comb begin a = b; end"), 1},
      {AlwaysCombKeyword(), EmbedInModule("always @* begin a = b; end"), 0},
      {AlwaysCombKeyword(), EmbedInModule("always @(*) begin a = b; end"), 0},
      {AlwaysCombKeyword(),
       EmbedInModule("always @(posedge foo) begin a <= b; end"), 0},
      {AlwaysCombKeyword(),
       EmbedInModule("always_ff begin a <= b; end\n"
                     "always_comb begin a = b; end"),
       1},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for AlwaysFFKeyword matching
TEST(VerilogMatchers, AlwaysFFKeywordTests) {
  const RawMatcherTestCase tests[] = {
      {AlwaysFFKeyword(), EmbedInModule(""), 0},
      {AlwaysFFKeyword(), EmbedInModule("initial begin a <= 0; end"), 0},
      {AlwaysFFKeyword(), EmbedInModule("always_ff begin a <= b; end"), 1},
      {AlwaysFFKeyword(), EmbedInModule("always_comb begin a = b; end"), 0},
      {AlwaysFFKeyword(), EmbedInModule("always @* begin a = b; end"), 0},
      {AlwaysFFKeyword(), EmbedInModule("always @(*) begin a = b; end"), 0},
      {AlwaysFFKeyword(),
       EmbedInModule("always @(posedge foo) begin a <= b; end"), 0},
      {AlwaysFFKeyword(),
       EmbedInModule("always_ff begin a <= b; end\n"
                     "always_comb begin a = b; end"),
       1},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for AlwaysStatementHasEventControlStar matching
TEST(VerilogMatchers, AlwaysStatementHasEventControlStarTests) {
  const RawMatcherTestCase tests[] = {
      {AlwaysStatementHasEventControlStar(), EmbedInModule(""), 0},
      {AlwaysStatementHasEventControlStar(),
       EmbedInModule("initial begin a <= 0; end"), 0},
      {AlwaysStatementHasEventControlStar(),
       EmbedInModule("always_ff begin a <= b; end"), 0},
      {AlwaysStatementHasEventControlStar(),
       EmbedInModule("always_comb begin a = b; end"), 0},
      {AlwaysStatementHasEventControlStar(),
       EmbedInModule("always @* begin a = b; end"), 1},
      {AlwaysStatementHasEventControlStar(),
       EmbedInModule("always @(*) begin a = b; end"), 1},
      {AlwaysStatementHasEventControlStar(),
       EmbedInModule("always @(posedge foo) begin a <= b; end"), 0},
      {AlwaysStatementHasEventControlStar(),
       EmbedInModule("always_ff begin a <= b; end\n"
                     "always_comb begin a = b; end"),
       0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests that matcher finds left-hand-sides of assignments.
TEST(VerilogMatchers, PathkLPValueTests) {
  const RawMatcherTestCase tests[] = {
      {PathkLPValue(), EmbedInClassMethod(""), 0},
      {PathkLPValue(), EmbedInClassMethod("foo();"), 0},
      {PathkLPValue(), EmbedInClassMethod("x = 1;"), 1},
      {PathkLPValue(), EmbedInClassMethod("x = a + b;"), 1},
      {PathkLPValue(), EmbedInModule("initial begin\nfoo();\nend\n"), 0},
      {PathkLPValue(), EmbedInModule("initial begin\nx = 1;\nend\n"), 1},
      {PathkLPValue(), EmbedInClassMethod("x[1] = 1;"), 1},
      {PathkLPValue(), EmbedInClassMethod("x.y = 2;"), 1},
      {PathkLPValue(), EmbedInClassMethod("x[0].y = 3;"), 1},
      {PathkLPValue(), EmbedInClassMethod("x.y[2] = 4;"), 1},
      {PathkLPValue(), EmbedInClassMethod("x[0].y[0] = 5;"), 1},
      {PathkLPValue(), EmbedInClassMethod("if (0) x = 1;"), 1},
      {PathkLPValue(), EmbedInClassMethod("forever x = y;"), 1},
      {PathkLPValue(), EmbedInClassMethod("x = 1;\ny = two();"), 2},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests that matcher finds assignments values that are function calls.
TEST(VerilogMatchers, RValueIsFunctionCallTest) {
  const RawMatcherTestCase tests[] = {
      {RValueIsFunctionCall(), EmbedInModule(""), 0},
      {RValueIsFunctionCall(), EmbedInClassMethod(""), 0},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = 1;"), 0},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = y / z;"), 0},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = y / z();"), 0},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = foo();"), 1},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = bar::foo();"), 1},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = bar::foo(a);"), 1},
      {RValueIsFunctionCall(), EmbedInClassMethod("x.y = bar::foo(a);"), 1},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = bar.foo();"), 0},
      {RValueIsFunctionCall(), EmbedInClassMethod("z = pkg::bar::foo();"), 1},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = bar::foo() -12;"), 0},
      {RValueIsFunctionCall(), EmbedInClassMethod("x = a + bar::foo();"), 0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests that qualified function calls are found.
TEST(VerilogMatchers, FunctionCallIsQualifiedTest) {
  const RawMatcherTestCase tests[] = {
      {FunctionCallIsQualified(), EmbedInModule(""), 0},
      {FunctionCallIsQualified(), EmbedInClassMethod(""), 0},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = 1;"), 0},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = y / z;"), 0},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = y / z();"), 0},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = foo();"), 0},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = bar::foo();"), 1},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = bar::foo(a);"), 1},
      {FunctionCallIsQualified(), EmbedInClassMethod("x.y = bar::foo(a);"), 1},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = bar.foo();"), 0},
      {FunctionCallIsQualified(), EmbedInClassMethod("z = pkg::bar::foo();"),
       1},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = bar::foo() -12;"), 1},
      {FunctionCallIsQualified(), EmbedInClassMethod("x = a + bar::foo();"), 1},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests that assignments to qualified function calls are found.
TEST(VerilogMatchers, RValueFunctionCallIsQualifiedTest) {
  const RawMatcherTestCase tests[] = {
      {RValueIsFunctionCall(FunctionCallIsQualified()), EmbedInModule(""), 0},
      {RValueIsFunctionCall(FunctionCallIsQualified()), EmbedInClassMethod(""),
       0},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = 1;"), 0},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = y / z;"), 0},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = y / z();"), 0},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = foo();"), 0},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = bar::foo();"), 1},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("bar::foo();"), 0},  // no assignment
      // TODO(fangism): The following test case wrongly matches, but only
      // because the matcher matches two different function calls: qqq is the
      // outermost match, but the recursive inner match finds a qualified
      // function call deeper in the syntax tree.  Solution is to use a direct
      // inner matcher that does not search recursively.
      // {RValueIsFunctionCall(FunctionCallIsQualified()),
      //  EmbedInClassMethod("qqq(bar::foo(), 1, 2);"), 0},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("y = z();\nbar::foo();"),
       0},  // no matching assignment
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = bar::foo(a);"), 1},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x.y = bar::foo(a);"), 1},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = bar.foo();"), 0},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("z = pkg::bar::foo();"), 1},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("z = pkg::bar::foo(\"a\", b, cc);"), 1},
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = bar::foo() -12;"), 0},  // outermost rvalue is -
      {RValueIsFunctionCall(FunctionCallIsQualified()),
       EmbedInClassMethod("x = a + bar::foo();"), 0},  // outermost rvalue is +
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests that function call arguments are matched.
TEST(VerilogMatchers, FunctionCallArgumentsTest) {
  const RawMatcherTestCase tests[] = {
      {FunctionCallArguments(), EmbedInModule(""), 0},
      {FunctionCallArguments(), EmbedInClassMethod(""), 0},
      {FunctionCallArguments(), EmbedInClassMethod("a = 1 + 2;"), 0},
      {FunctionCallArguments(), EmbedInClassMethod("foobar();"), 0},
      {FunctionCallArguments(), EmbedInClassMethod("foobar(1);"), 1},
      {FunctionCallArguments(), EmbedInClassMethod("foobar(a, b, c);"), 1},
      {FunctionCallArguments(), EmbedInClassMethod("`foobar();"),
       0},  // macro call arguments are different
      {FunctionCallArguments(), EmbedInClassMethod("`foobar(1);"), 0},
      {FunctionCallArguments(), EmbedInClassMethod("`foobar(a, b, c);"), 0},
      {FunctionCallArguments(), EmbedInClassMethod("foo(bar());"), 1},
      {FunctionCallArguments(), EmbedInClassMethod("x = foobar();"), 0},
      {FunctionCallArguments(), EmbedInClassMethod("f.g.h = foobar(g);"), 1},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests that ranged dimensions are matched.
TEST(VerilogMatchers, DeclarationDimensionsHasRanges) {
  const RawMatcherTestCase tests[] = {
      {DeclarationDimensionsHasRanges(), "", 0},
      {DeclarationDimensionsHasRanges(), EmbedInModule(""), 0},
      {DeclarationDimensionsHasRanges(), "wire w;", 0},
      {DeclarationDimensionsHasRanges(), "wire [1:0] w;", 1},
      {DeclarationDimensionsHasRanges(), "wire w [1:0];", 1},
      {DeclarationDimensionsHasRanges(), "wire [1:2] w [1:0];", 2},
      {DeclarationDimensionsHasRanges(), EmbedInModule("wire w;"), 0},
      {DeclarationDimensionsHasRanges(), EmbedInModule("wire [1:0] w;"), 1},
      {DeclarationDimensionsHasRanges(), EmbedInModule("wire w [1:0];"), 1},
      {DeclarationDimensionsHasRanges(), EmbedInModule("wire [1:2] w [1:0];"),
       2},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

// Tests for HasDefaultCase matching.
TEST(VerilogMatchers, HasDefaultCaseTests) {
  const RawMatcherTestCase tests[] = {
      {HasDefaultCase(), "", 0},
      {HasDefaultCase(),
       R"(
       function automatic int foo (input in);
         case (in)
           default: return 0;
         endcase
       endfunction
       )",
       1},
      {HasDefaultCase(),
       R"(
       function automatic int foo (input in);
         case (in)
           1: return 0;
         endcase
       endfunction
       )",
       0},
  };
  for (const auto& test : tests)
    verible::matcher::RunRawMatcherTestCase<VerilogAnalyzer>(test);
}

}  // namespace
}  // namespace verilog
