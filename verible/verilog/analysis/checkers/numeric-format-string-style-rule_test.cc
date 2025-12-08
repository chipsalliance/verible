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

#include "verible/verilog/analysis/checkers/numeric-format-string-style-rule.h"

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
using verible::RunLintTestCases;

// Tests that the numeric formatting string is style-compilant
TEST(NumericFormatStringStyleRuleTest, BasicTests) {
  constexpr int kToken = TK_StringLiteral;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},

      // Hexadecimal value
      {"module test; initial $display(\"",
       {kToken, "%0x"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"0x%0x\", hex); endmodule"},
      {"module test; initial $display(\"'h%0h\", hex); endmodule"},
      {"module test; initial $display(\"'h%h\", hex); endmodule"},
      {"module test; initial $display(\"0h%h\", hex); endmodule"},
      {"module test; initial $display(\"Value: 0x%0x\", hex); endmodule"},
      {"module test; initial $display(\"Value: 'x%0x\", hex); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "0X%0x"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "'X%0x"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "%0x"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "%h"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "%x"},
       "\", hex); endmodule"},

      {"module test; initial $display(\"0x%0h\", hex); endmodule"},
      {"module test; initial $display(\"'h%0x\", hex); endmodule"},
      {"module test; initial $display(\"'h%0X\", hex); endmodule"},
      {"module test; initial $display(\"'x%0h\", hex); endmodule"},
      {"module test; initial $display(\"'x%0H\", hex); endmodule"},

      {"module test; initial $display(\"",
       {kToken, "'H%0x"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"V: ",
       {kToken, "0X%0H"},
       "\", hex); endmodule"},

      // Binary value
      {"module test; initial $display(\"",
       {kToken, "%0b"},
       "\", bin); endmodule"},
      {"module test; initial $display(\"0b%0b\", bin); endmodule"},
      {"module test; initial $display(\"'b%0b\", bin); endmodule"},  // also
                                                                     // acceptable
      {"module test; initial $display(\"Value: 0b%0b\", bin); endmodule"},
      {"module test; initial $display(\"Value: 'b%0b\", bin); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "0B%0b"},
       "\", bin); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "'B%0b"},
       "\", bin); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "%0b"},
       "\", bin); endmodule"},

      // Decimal value (no prefix allowed)
      {"module test; initial $display(\"%0d\", dec); endmodule"},
      {"module test; initial $display(\"",
       {kToken, "0d%0d"},
       "\", dec); endmodule"},
      {"module test; initial $display(\"",
       {kToken, "'d%0d"},
       "\", dec); endmodule"},
      {"module test; initial $display(\"Value: %0d\", dec); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "0d%0d"},
       "\", dec); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "'d%0d"},
       "\", dec); endmodule"},

      // Invalid prefix
      {"module test; initial $display(\"Value: ",
       {kToken, "0b%0x"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "'b%0x"},
       "\", hex); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "0x%0b"},
       "\", bin); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "'h%0b"},
       "\", bin); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "0x%0d"},
       "\", dec); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "'h%0d"},
       "\", dec); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "0b%0d"},
       "\", dec); endmodule"},
      {"module test; initial $display(\"Value: ",
       {kToken, "'b%0d"},
       "\", dec); endmodule"},

      // Multiple violations
      {"module test;\n",
       "  initial $display(\"Value: 0x%0x ",
       {kToken, "%0x"},
       " ",
       {kToken, "%0x"},
       "\", hex1, hex2, hex3);",
       "endmodule"},

      {"module test;\n",
       "  initial $display(\"Value: ",
       {kToken, "%0x"},
       " 'h%0h ",
       {kToken, "%0x"},
       "\", hex1, hex2, hex3);",
       "endmodule"},

      {"module test;\n",
       "  initial $display(\"Value: ",
       {kToken, "%0x"},
       " ",
       {kToken, "%0x"},
       " ",
       {kToken, "%0x"},
       "\", hex1, hex2, hex3);",
       "endmodule"},

      {"module test;\n",
       "  initial $display(\"Value: 0b%0b ",
       {kToken, "%0b"},
       " ",
       {kToken, "%0b"},
       "\", bin1, bin2, bin3);",
       "endmodule"},

      {"module test;\n",
       "  initial $display(\"Value: ",
       {kToken, "%0b"},
       " 'b%0b ",
       {kToken, "%0b"},
       "\", bin1, bin2, bin3);",
       "endmodule"},

      {"module test;\n",
       "  initial $display(\"Value: ",
       {kToken, "%0b"},
       " ",
       {kToken, "%0b"},
       " ",
       {kToken, "%0b"},
       "\",  bin1, bin2, bin3);",
       "endmodule"},

      {"module test;\n",
       "  initial $display(\"Value: ",
       {kToken, "0d%0d"},
       {kToken, "'D%0d"},
       " %0d\", dec1, dec2, dec3);",
       "endmodule"},

      {"module test;\n",
       "  initial $display(\"Value: %0d ",
       {kToken, "0D%0d"},
       " ",
       {kToken, "'d%0d"},
       "\", dec1, dec2, dec3);",
       "endmodule"},

      {"module test;"
       "  initial $display(\"Value: %0d %0d %0d\", dec1, dec2, dec3);"
       "endmodule"},

      {"module test;"
       "  initial $display(\"0x%0x, %d\", hex, dec);"
       "endmodule"},

      {"module test;"
       "  initial $display(\"0x%0x, 'b%0b\", hex, bin);"
       "endmodule"},

      {"module test;"
       "  initial $display(\"Value: 0x%0x (bin: 'b%b, dec: %d)\", hex, bin, "
       "dec);"
       "endmodule"},

      {"module test;"
       "  initial $display(\"Value: 0x%0x (bin: ",
       {kToken, "%b"},
       ", dec: %d)\", hex, bin, dec);"
       "endmodule"},

      {"module test;"
       "  parameter string fmt = \"Interrupts: %d\";"
       "endmodule"},

      {"module test;"
       "  parameter string fmt = \"Interrupts: ",
       {kToken, "'d%d"},
       "\";"
       "endmodule"},

      {"module test;"
       "  parameter string fmt = \"Interrupts: %d (flags: 0x%0x)\";"
       "endmodule"},

      {"module test;"
       "  parameter string fmt = \"Interrupts: ",
       {kToken, "'d%d"},
       " (flags: 0x%0x)\";"
       "endmodule"},

      {"module test;"
       "  string s;"
       "  initial $sformat(s, \"misc: 0x%0x\", some_hex_value);"
       "endmodule"},

      {"module test;"
       "  string s;"
       "  initial $sformat(s, \"misc: ",
       {kToken, "%0x"},
       "\", some_hex_value);"
       "endmodule"},

      {"module test;"
       "  string s;"
       "  initial $sformat(s, \"misc: 0b%0b\", some_hex_value);"
       "endmodule"},

      {"module test;"
       "  string s;"
       "  initial $sformat(s, \"misc: ",
       {kToken, "%0b"},
       "\", some_hex_value);"
       "endmodule"},

      {"module test;"
       "  string s;"
       "  initial $sformat(s, \"counter: %d\", some_dec_value);"
       "endmodule"},

      {"module test;"
       "  string s;"
       "  initial $sformat(s, \"counter: ",
       {kToken, "'d%0d"},
       "\", some_dec_value);"
       "endmodule"},

      // macro definition body
      {"`define DBG_FMT \"0x%x\""},
      {"`define DBG_FMT \"", {kToken, "%x"}, "\""},

      // macro call
      {"`dbg(\"0b%b\", bin"},
      {"`dbg(\"", {kToken, "%b"}, "\", bin"},

      // macro call in macro definition
      {"`define dump(value) `dbg(\"Hex: 0x%h\", value)"},
      {"`define dump(value) `dbg(\"Hex: ", {kToken, "0X%h"}, "\", value)"},
  };
  RunLintTestCases<VerilogAnalyzer, NumericFormatStringStyleRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
