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

#include "verible/verilog/analysis/checkers/explicit-begin-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/token-stream-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

// Tests that space-only text passes.
TEST(ExplicitBeginRuleTest, AcceptsBlank) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {" "},
      {"\n"},
      {" \n\n"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(kTestCases);
}

// Tests that properly matched if/begin passes.
TEST(ExplicitBeginRuleTest, AcceptsBlocksWithBegin) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"if (FOO) /*block comment */ begin a <= 1;"},
      {"if (FOO) begin  a <= 1;"},
      {"if (FOO)begin : name_statement a <= 1;"},
      {"if (FOO)\n begin  a <= 1;"},
      {"if (FOO) //Comment\n begin a <= 1;"},

      {"else begin \n FOO"},
      {"else \nbegin \n FOO"},
      {"else //Comment\n begin \n FOO"},
      {"else \n //Comment\n begin \n FOO"},
      {"else if (FOO) begin a <= 1;"},
      {"else if (FOO)\n begin a <= 1;"},
      {"else if (FOO) //Comment\n begin a <= 1;"},
      {"else if (FOO)\n //Comment\n begin a <= 1;"},

      {"for(i = 0; i < N; i++) begin a <= 1;"},
      {"for(i = 0; i < N; i++)\nbegin a <= 1;"},
      {"for(i = 0; i < N; i++) // Comment\n begin a <= 1;"},
      {"for(i = 0; i < N; i++)\n // Comment\nbegin a <= 1;"},

      {"foreach(array[i]) begin a <= 1;"},
      {"foreach(array[i])\nbegin a <= 1;"},
      {"foreach(array[i]) // Comment\n begin a <= 1;"},
      {"foreach(array[i])\n // Comment\nbegin a <= 1;"},

      {"while (a < 3) begin a = a + 1;"},
      {"while(a < 3)\nbegin a = a + 1;"},
      {"while (a < 3) // Comment\n begin a = a + 1;"},
      {"while(a < 3)\n // Comment\nbegin a = a + 1;"},

      {"forever begin a <= 1;"},
      {"forever\nbegin a <= 1;"},
      {"forever // Comment\n begin a <= 1;"},
      {"forever\n // Comment\nbegin a <= 1;"},

      {"initial begin a <= 1;"},
      {"initial\nbegin a <= 1;"},
      {"initial // Comment\n begin a <= 1;"},
      {"initial\n // Comment\nbegin a <= 1;"},

      {"always_comb begin a = 1;"},
      {"always_comb\nbegin a = 1;"},
      {"always_comb // Comment\n begin a = 1;"},
      {"always_comb\n // Comment\nbegin a = 1;"},

      {"always_latch begin a <= 1;"},
      {"always_latch\nbegin a <= 1;"},
      {"always_latch // Comment\n begin a <= 1;"},
      {"always_latch\n // Comment\nbegin a <= 1;"},

      {"always_ff @( a or b) begin a <= 1;"},
      {"always_ff @ ( a or b)\nbegin a <= 1;"},
      {"always_ff @( (a) and b) // Comment\n begin a <= 1;"},
      {"always_ff @( a or ((b)))\n // Comment\nbegin a <= 1;"},

      {"always @( a or b) begin a <= 1;"},
      {"always @ ( a or b)\nbegin a <= 1;"},
      {"always @( (a) and b) // Comment\n begin a <= 1;"},
      {"always @( a or ((b)))\n // Comment\nbegin a <= 1;"},
      {"always@* begin a = 1'b1;"},
      {"always@(*) begin a = 1'b1;"},
      {"always @* begin a = 1'b1;"},
      {"always begin a = 1'b1;"},
      {"always begin #10 a = 1'b1;"},

      // Ignore constraints
      {"constraint c_array { foreach (array[i]) {array[i] == i;}}"},
      {"constraint c {if(a == 2){b == 1;}else{b == 2;}}"},

      // Ignore inline constraints
      {"task a(); std::randomize(b) with {foreach(b[i]){b[i] inside "
       "{[0:1024]};}}; endtask"},
      {"task a(); std::randomize(b) with {if(a == 2){b == 1;}else{b == 2;}}; "
       "endtask"},

      // Multiple consecutive failures
      {"if(FOO) begin for(i = 0; i < N; i++) begin a <= i;"},
      {"if(FOO) begin foreach(array[i]) begin a <= i;"},
      {"if(FOO) begin while(i < N) begin i++;"},
      {"for(i = 0; i < N; i++) begin if (FOO) begin a <= 1'b1;"},
      {"always @* begin if(FOO) begin a = 1; end else begin a = 0;"},
      {"always @(*) begin if(FOO) begin a = 1; end else begin a = 0;"},
      {"always @(posedge c) begin if(FOO) begin a <= 1; end else begin "
       "a <= 0;"},
      {"always_comb begin if(FOO) begin a = 1; end else begin a = 0;"},
      {"always_ff @(posedge c) begin if(FOO) begin a <= 1; end else begin "
       "a <= 0;"},
      {"constraint c_array { foreach (array[i]) {array[i] == i;}}if(FOO) begin "
       "a <= 1;end"},
      {"if(FOO) begin a <= 1;end constraint c {if(a == 2){b == 1;}else{b == "
       "2;}}"},
  };

  RunLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(kTestCases);
}

// Tests that unmatched block/begin fails is detected.
TEST(ExplicitBeginRuleTest, RejectBlocksWithoutBegin) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {{TK_if, "if"}, " (FOO)\n BAR"},
      {{TK_if, "if"}, " (FOO) //Comment\n BAR"},

      {{TK_else, "else"}, " \n FOO"},
      {{TK_else, "else"}, " \n \n FOO"},
      {{TK_else, "else"}, " //Comment\n  FOO"},
      {{TK_else, "else"}, " \n //Comment\n FOO"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO)\n BAR"},
      {"else ", {TK_if, "if"}, " (FOO) //Comment\n BAR"},
      {"else ", {TK_if, "if"}, " (FOO)\n //Comment\n BAR"},

      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_for, "for"}, "(i = 0; i < N; i++)\n a <= 1'b1;"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) // Comment \n a <= 1'b1;"},
      {{TK_for, "for"}, "(i = 0; i < N; i++)\n // Comment\n a <= 1'b1;"},

      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i])\n a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) // Comment \n a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i])\n // Comment\n a <= 1'b1;"},

      {{TK_while, "while"}, "(i < N) a <= 1'b1;"},
      {{TK_while, "while"}, " (i < N)\n a <= 1'b1;"},
      {{TK_while, "while"}, "(i < N) // Comment \n a <= 1'b1;"},
      {{TK_while, "while"}, " (i < N)\n // Comment\n a <= 1'b1;"},

      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_forever, "forever"}, "\n a <= 1'b1;"},
      {{TK_forever, "forever"}, " // Comment \n a <= 1'b1;"},
      {{TK_forever, "forever"}, "\n // Comment\n a <= 1'b1;"},

      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_initial, "initial"}, "\n a = 1'b1;"},
      {{TK_initial, "initial"}, " // Comment \n a = 1'b1;"},
      {{TK_initial, "initial"}, "\n // Comment\n a = 1'b1;"},

      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, "\n a = 1'b1;"},
      {{TK_always_comb, "always_comb"}, " // Comment \n a = 1'b1;"},
      {{TK_always_comb, "always_comb"}, "\n // Comment\n a = 1'b1;"},

      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, "\n a = 1'b1;"},
      {{TK_always_latch, "always_latch"}, " // Comment \n a = 1'b1;"},
      {{TK_always_latch, "always_latch"}, "\n // Comment\n a = 1'b1;"},

      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, "@(a or b)\n a <= 1'b1;"},
      {{TK_always_ff, "always_ff"},
       " @(posedge a or negedge b) // Comment \n a <= 1'b1;"},
      {{TK_always_ff, "always_ff"}, "@(a || b)\n // Comment\n a <= 1'b1;"},

      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
      {{TK_always, "always"}, "@(a or b)\n a = 1'b1;"},
      {{TK_always, "always"},
       " @(posedge a or negedge b) // Comment \n a <= 1'b1;"},
      {{TK_always, "always"}, "@(a || b)\n // Comment\n  <= 1'b1;"},
      {{TK_always, "always"}, "@* a = 1'b1;"},
      {{TK_always, "always"}, "@(*) a = 1'b1;"},
      {{TK_always, "always"}, " @* a = 1'b1;"},
      {{TK_always, "always"}, " a = 1'b1;"},
      {{TK_always, "always"}, " #10 a = 1'b1;"},

      // Multiple consecutive failures
      {{TK_if, "if"}, "(FOO) ", {TK_for, "for"}, "(i = 0; i < N; i++) a <= i;"},
      {{TK_if, "if"}, "(FOO) ", {TK_foreach, "foreach"}, "(array[i]) a <= i;"},
      {{TK_if, "if"}, "(FOO) ", {TK_while, "while"}, "(i < N) i++;"},
      {{TK_for, "for"},
       "(i = 0; i < N; i++)\n",
       {TK_if, "if"},
       " (FOO) a <= 1'b1;"},
      {{TK_always, "always"},
       " @* ",
       {TK_if, "if"},
       "(FOO) a = 1;",
       {TK_else, "else"},
       " a = 0;"},
      {{TK_always, "always"},
       " @(*) ",
       {TK_if, "if"},
       "(FOO) a = 1;",
       {TK_else, "else"},
       " a = 0;"},
      {{TK_always, "always"},
       " @(posedge c) ",
       {TK_if, "if"},
       "(FOO) a <= 1;",
       {TK_else, "else"},
       " a <= 0;"},
      {{TK_always_comb, "always_comb"},
       " ",
       {TK_if, "if"},
       "(FOO) a = 1;",
       {TK_else, "else"},
       " a = 0;"},
      {{TK_always_ff, "always_ff"},
       " @(posedge c) ",
       {TK_if, "if"},
       "(FOO) a <= 1;",
       {TK_else, "else"},
       " a <= 0;"},
      {"constraint c_array { foreach (array[i]) array[i] == i;}",
       {TK_if, "if"},
       "(FOO) a <= 1;"},
      {{TK_if, "if"},
       "(FOO) a <= 1; constraint c {if(a == 2) b == 1;else b == 2;}"},
  };

  RunLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(kTestCases);
}

// Tests that rule can be disabled for if statements
TEST(ExplicitBeginRuleTest, AcceptsIfBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"if (FOO) BAR"},
      {"else if (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "if_enable:false");
}

// Tests that rule can be disabled for else statements
TEST(ExplicitBeginRuleTest, AcceptsElseBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {"else \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "else_enable:false");
}

// Tests that rule can be disabled for for statements
TEST(ExplicitBeginRuleTest, AcceptsForBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {"for(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "for_enable:false");
}

// Tests that rule can be disabled for foreach statements
TEST(ExplicitBeginRuleTest, AcceptsForeachBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {"foreach(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "foreach_enable:false");
}

// Tests that rule can be disabled for while statements
TEST(ExplicitBeginRuleTest, AcceptsWhileBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {"while(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "while_enable:false");
}

// Tests that rule can be disabled for forever statements
TEST(ExplicitBeginRuleTest, AcceptsForeverBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {"forever a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "forever_enable:false");
}

// Tests that rule can be disabled for initial statements
TEST(ExplicitBeginRuleTest, AcceptsInitialBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {"initial a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "initial_enable:false");
}

// Tests that rule can be disabled for always_comb statements
TEST(ExplicitBeginRuleTest, AcceptsAlwaysCombBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {"always_comb a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "always_comb_enable:false");
}

// Tests that rule can be disabled for always_latch statements
TEST(ExplicitBeginRuleTest, AcceptsAlwaysLatchBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {"always_latch a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "always_latch_enable:false");
}

// Tests that rule can be disabled for always_ff statements
TEST(ExplicitBeginRuleTest, AcceptsAlwaysFFBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {"always_ff @(a or b) a <= 1'b1;\n"},
      {{TK_always, "always"}, " @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "always_ff_enable:false");
}

// Tests that rule can be disabled for always statements
TEST(ExplicitBeginRuleTest, AcceptsAlwaysBlocksWithoutBeginConfigured) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_for, "for"}, "(i = 0; i < N; i++) a <= 1'b1;"},
      {{TK_foreach, "foreach"}, "(array[i]) a <= 1'b1;"},
      {{TK_while, "while"}, "(array[i]) a <= 1'b1;"},
      {{TK_forever, "forever"}, " a <= 1'b1;\n"},
      {{TK_initial, "initial"}, " a = 1'b1;\n"},
      {{TK_always_comb, "always_comb"}, " a = 1'b1;\n"},
      {{TK_always_latch, "always_latch"}, " a = 1'b1;\n"},
      {{TK_always_ff, "always_ff"}, " @(a or b) a <= 1'b1;\n"},
      {"always @(a or b) a = 1'b1;\n"},
  };

  RunConfiguredLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(
      kTestCases, "always_enable:false");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog