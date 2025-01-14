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

#include "verible/verilog/formatting/formatter.h"

// prevent header re-ordering

#include <sstream>
#include <string_view>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/strings/position.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/formatting/format-style.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace formatter {
namespace {

struct FormatterTestCase {
  std::string_view input;
  std::string_view expected;
};

static const verible::LineNumberSet kEnableAllLines;

// Tests in this file are intended to be sensitive to wrapping penalty tuning.
// These test cases should be kept short, small enough to be directed
// at particular desirable characteristics.
// TODO(b/145558510): these tests must maintain unique-best solutions

static constexpr FormatterTestCase kTestCases[] = {
    //----------- 40 column marker --------->|
    {// TODO(b/148972363): might want to attract "= sss(" more
     "module m;"
     "assign wwwwww[77:66]"
     "= sss(qqqq[33:22],"
     "vv[44:1]);"
     "endmodule",
     "module m;\n"
     "  assign wwwwww[77:66] = sss(\n"
     "      qqqq[33:22], vv[44:1]\n"
     "  );\n"
     "endmodule\n"},
    {"module m;\n"
     "localparam int foo = xxxxxxxxxx + yyyyyyyyyyyyyy + zzzzzzzzzzz;\n"
     "endmodule\n",
     "module m;\n"
     "  localparam int foo = xxxxxxxxxx +\n"
     "      yyyyyyyyyyyyyy + zzzzzzzzzzz;\n"
     "endmodule\n"},
};

// These formatter tests involve line wrapping and hence line-wrap penalty
// tuning.  Keep these short and minimal where possible.
TEST(FormatterEndToEndTest, PenaltySensitiveLineWrapping) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream, debug_stream;
    ExecutionControl control;
    control.stream = &debug_stream;
    const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                      stream, kEnableAllLines, control);
    EXPECT_OK(status);
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
    EXPECT_TRUE(debug_stream.str().empty());
  }
}

// Sometimes it's hard to reduce a real test case to a 40 column version,
// so this set of tests uses 100-column.  Use raw string literals here.
static constexpr FormatterTestCase k100ColTestCases[] = {
    {
        R"sv(
module m;
localparam int DDDDDDDDDDD = pppppppppppppppppp + LLLLLLLLLLLLLL
+ ((EEEEEEEEEEEE && FFFFFFFFFFFFFF > 0) ? hhhhhhhhhhhhhhhhhhhhhhhhhhhhhh : 0);
endmodule
)sv",
        // make sure the line does not break before a '+'
        R"sv(
module m;
  localparam int DDDDDDDDDDD = pppppppppppppppppp + LLLLLLLLLLLLLL +
      ((EEEEEEEEEEEE && FFFFFFFFFFFFFF > 0) ? hhhhhhhhhhhhhhhhhhhhhhhhhhhhhh : 0);
endmodule
)sv"},
    {
        R"sv(
module m;
assign bbbbbbbbbbbbbbbbb =
      iiiiiiiiiiiiiiiiiiiii ?
      xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx :
      yyyyyyyyyyyyyyyyyyyyyy;
endmodule
)sv",
        // make sure break happens after '?' and ':'
        R"sv(
module m;
  assign bbbbbbbbbbbbbbbbb = iiiiiiiiiiiiiiiiiiiii ?
      xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx : yyyyyyyyyyyyyyyyyyyyyy;
endmodule
)sv"},
    {
        R"sv(
module m;
  if (x) begin
    assign {ooooooooooooooooooo, ssssssssss} =
    bbbbbbbbbbbbbbbbb >= cccccccccccccccccccccccc
        ? ddddd - (qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq) :
       eeeee - (rrrrrrrrrrrrrrrrrrfffjjjjjjjjjjjjjjjjjgggkkkkkkkkkkkkkkkkkkkkkkkk);
 end
endmodule
)sv",
        // make sure break happens after '?' and ':'
        R"sv(
module m;
  if (x) begin
    assign {ooooooooooooooooooo, ssssssssss} = bbbbbbbbbbbbbbbbb >= cccccccccccccccccccccccc ?
        ddddd - (qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq) :
        eeeee - (rrrrrrrrrrrrrrrrrrfffjjjjjjjjjjjjjjjjjgggkkkkkkkkkkkkkkkkkkkkkkkk);
  end
endmodule
)sv"},
};

TEST(FormatterEndToEndTest, PenaltySensitiveLineWrapping100Col) {
  FormatStyle style;
  style.column_limit = 100;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto &test_case : k100ColTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream, debug_stream;
    ExecutionControl control;
    control.stream = &debug_stream;
    const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                      stream, kEnableAllLines, control);
    EXPECT_OK(status);
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
    EXPECT_TRUE(debug_stream.str().empty());
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
