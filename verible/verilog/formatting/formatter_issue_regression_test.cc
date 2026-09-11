// Copyright 2017-2026 The Verible Authors.
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

// Test cases in this file should be *insensitive* to wrapping penalties.
// Penalty-sensitive tests belong in formatter-tuning_test.cc.

#include <sstream>
#include <string_view>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter-test-utils.h"
#include "verible/verilog/formatting/formatter.h"

namespace verilog {
namespace formatter {
namespace {

using testing::HasSubstr;

// Regression for https://github.com/chipsalliance/verible/issues/2544:
// Wrapping a $bits(...)'(...) cast may leave `MACRO at EOL, reclassifying
// MacroIdentifier as MacroIdItem. FormatEquivalent must accept that, and
// formatting must still pass verification.
TEST(FormatterEndToEndTest, MacroBeforeCloseParenFormatEquivalent) {
  static constexpr std::string_view kInput =
      "module m;\n"
      "  assign result_value = $bits(result_value)'( "
      "compare_bytes(input_data[DATA_WIDTH_INT-1:0], "
      "input_datak[STROBE_WIDTH_INT-1:0], `TOKEN_BYTE) );\n"
      "endmodule\n";
  FormatStyle style;
  std::ostringstream stream;
  const auto status = FormatVerilog(kInput, "<filename>", style, stream);
  EXPECT_OK(status) << status.message();
  EXPECT_THAT(stream.str(), testing::HasSubstr("`TOKEN_BYTE"));
}

// Regression for https://github.com/chipsalliance/verible/issues/2542:
// Continuation EOL comments after a wrapped assign must keep a stable column
// across re-format (convergence).
TEST(FormatterEndToEndTest, ContinuationCommentAfterWrappedAssignConverges) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Comments originally column-aligned after a wrapped assign
       "module m;\n"
       "  assign status_ur = !(status_sc || status_ca ||\n"
       "    status_crs);      // Completions with a Reserved Completion\n"
       "                      // Status value are treated as UR\n"
       "endmodule\n",
       "module m;\n"
       "  assign status_ur =\n"
       "      !(status_sc || status_ca || status_crs);  // Completions with a "
       "Reserved Completion\n"
       "                                                // Status value are "
       "treated as UR\n"
       "endmodule\n"},
      {// Previously mis-aligned continuation is not treated as a continuation
       // (column delta > 1) and must still converge
       "module m;\n"
       "  assign status_ur = !(status_sc || status_ca ||\n"
       "    status_crs);      // Completions with a Reserved Completion\n"
       "                                                                       "
       "// Status value are treated as UR\n"
       "endmodule\n",
       "module m;\n"
       "  assign status_ur =\n"
       "      !(status_sc || status_ca || status_crs);  // Completions with a "
       "Reserved Completion\n"
       "  // Status value are treated as UR\n"
       "endmodule\n"},
  };
  FormatStyle style;  // default column_limit (100)
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Regression for https://github.com/chipsalliance/verible/issues/2540:
// Trailing EOL comment after `end` before `else if` must not change whether
// the else-if assignment stays on one line across re-format (convergence).
TEST(FormatterEndToEndTest, EndElseIfWithEOLCommentConverges) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Comment on its own line between end and else if
       "module m;\n"
       "  always_comb begin\n"
       "    case (state)\n"
       "      STATE_A: begin\n"
       "        if (cond_aaaa) next_state_value = STATE_B;\n"
       "        else if (cond_bbbb) begin\n"
       "          next_state_value = STATE_B;\n"
       "        end\n"
       "        // xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
       "        else if (cond_cccc) next_state_value = STATE_C;\n"
       "      end\n"
       "    endcase\n"
       "  end\n"
       "endmodule\n",
       "module m;\n"
       "  always_comb begin\n"
       "    case (state)\n"
       "      STATE_A: begin\n"
       "        if (cond_aaaa) next_state_value = STATE_B;\n"
       "        else if (cond_bbbb) begin\n"
       "          next_state_value = STATE_B;\n"
       "        end  // xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
       "        else if (cond_cccc) next_state_value = STATE_C;\n"
       "      end\n"
       "    endcase\n"
       "  end\n"
       "endmodule\n"},
      {// Same construct with comment already on the end line
       "module m;\n"
       "  always_comb begin\n"
       "    case (state)\n"
       "      STATE_A: begin\n"
       "        if (cond_aaaa) next_state_value = STATE_B;\n"
       "        else if (cond_bbbb) begin\n"
       "          next_state_value = STATE_B;\n"
       "        end  // xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
       "        else if (cond_cccc) next_state_value = STATE_C;\n"
       "      end\n"
       "    endcase\n"
       "  end\n"
       "endmodule\n",
       "module m;\n"
       "  always_comb begin\n"
       "    case (state)\n"
       "      STATE_A: begin\n"
       "        if (cond_aaaa) next_state_value = STATE_B;\n"
       "        else if (cond_bbbb) begin\n"
       "          next_state_value = STATE_B;\n"
       "        end  // xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
       "        else if (cond_cccc) next_state_value = STATE_C;\n"
       "      end\n"
       "    endcase\n"
       "  end\n"
       "endmodule\n"},
  };
  FormatStyle style;  // default column_limit (100)
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Regression for https://github.com/chipsalliance/verible/issues/2008
// (also https://github.com/chipsalliance/verible/issues/2474 and
// https://github.com/chipsalliance/verible/issues/2063):
// Non-ANSI "input wire signed" used to abort in the tree-unwrapper because
// the CST visited "signed" before "wire", which is the reverse of source
// order.
TEST(FormatterEndToEndTest, NonAnsiWireSignedModulePortDoesNotAbort) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Original issue #2008 sample
       "module uut( sig1 );\n"
       "\n"
       "input wire signed [15:0] sig1;\n"
       "\n"
       "endmodule\n",
       "module uut (\n"
       "    sig1\n"
       ");\n"
       "\n"
       "  input wire signed [15:0] sig1;\n"
       "\n"
       "endmodule\n"},
      {// Issue #2474 sample
       "module myModule (\n"
       "    myinput\n"
       ");\n"
       "input wire signed [7:0] myInput;\n"
       "endmodule\n",
       "module myModule (\n"
       "    myinput\n"
       ");\n"
       "  input wire signed [7:0] myInput;\n"
       "endmodule\n"},
      {// Issue #2063 sample: signed wire with no packed dimensions
       "module top(a);\n"
       "    input wire signed a;\n"
       "endmodule\n",
       "module top (\n"
       "    a\n"
       ");\n"
       "  input wire signed a;\n"
       "endmodule\n"},
      {// Same production with logic instead of wire
       "module uut(sig1);\n"
       "input logic signed [15:0] sig1;\n"
       "endmodule\n",
       "module uut (\n"
       "    sig1\n"
       ");\n"
       "  input logic signed [15:0] sig1;\n"
       "endmodule\n"},
      {// output / inout net types
       "module uut(sig1, sig2);\n"
       "output wire signed [15:0] sig1;\n"
       "inout wire signed [7:0] sig2;\n"
       "endmodule\n",
       "module uut (\n"
       "    sig1,\n"
       "    sig2\n"
       ");\n"
       "  output wire signed [15:0] sig1;\n"
       "  inout wire signed [7:0] sig2;\n"
       "endmodule\n"},
      {// unsigned is the same production
       "module uut(sig1);\n"
       "input wire unsigned [15:0] sig1;\n"
       "endmodule\n",
       "module uut (\n"
       "    sig1\n"
       ");\n"
       "  input wire unsigned [15:0] sig1;\n"
       "endmodule\n"},
      {// ANSI form already worked; keep as a regression
       "module uut(input wire signed [15:0] sig1);\n"
       "endmodule\n",
       "module uut (\n"
       "    input wire signed [15:0] sig1\n"
       ");\n"
       "endmodule\n"},
      {// Non-ANSI without signed still works
       "module uut(sig1);\n"
       "input wire [15:0] sig1;\n"
       "endmodule\n",
       "module uut (\n"
       "    sig1\n"
       ");\n"
       "  input wire [15:0] sig1;\n"
       "endmodule\n"},
      {// Non-ANSI signed without net type still works
       "module uut(sig1);\n"
       "input signed [15:0] sig1;\n"
       "endmodule\n",
       "module uut (\n"
       "    sig1\n"
       ");\n"
       "  input signed [15:0] sig1;\n"
       "endmodule\n"},
  };
  FormatStyle style;  // default column_limit (100)
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}
}  // namespace
}  // namespace formatter
}  // namespace verilog
