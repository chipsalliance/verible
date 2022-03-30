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

// Test cases in this file should be *insensitive* to wrapping penalties.
// Penalty-sensitive tests belong in formatter_tuning_test.cc.
// Methods for keeping tests penalty insensitive:
//   * Short lines and partitions.  Lines that fit need no wrapping.
//   * Forced line breaks using //comments (reduce decision-making)

#include "verilog/formatting/formatter.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "common/formatting/align.h"
#include "common/strings/position.h"
#include "common/text/text_structure.h"
#include "common/util/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/formatting/format_style.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace formatter {

// private, extern function in formatter.cc, directly tested here.
absl::Status VerifyFormatting(const verible::TextStructureView& text_structure,
                              absl::string_view formatted_output,
                              absl::string_view filename);

namespace {

static constexpr VerilogPreprocess::Config kDefaultPreprocess;

using absl::StatusCode;
using testing::HasSubstr;
using verible::AlignmentPolicy;
using verible::IndentationStyle;
using verible::LineNumberSet;

// Tests that clean output passes.
TEST(VerifyFormattingTest, NoError) {
  const absl::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto& text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const auto status = VerifyFormatting(text_structure, code, "<filename>");
  EXPECT_OK(status);
}

// Tests that un-lexable outputs are caught as errors.
TEST(VerifyFormattingTest, LexError) {
  const absl::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto& text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const absl::string_view bad_code("1class c;endclass\n");  // lexical error
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

// Tests that un-parseable outputs are caught as errors.
TEST(VerifyFormattingTest, ParseError) {
  const absl::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto& text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const absl::string_view bad_code("classc;endclass\n");  // syntax error
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

// Tests that lexical differences are caught as errors.
TEST(VerifyFormattingTest, LexicalDifference) {
  const absl::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kDefaultPreprocess);
  const auto& text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const absl::string_view bad_code("class c;;endclass\n");  // different tokens
  const auto status = VerifyFormatting(text_structure, bad_code, "<filename>");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), StatusCode::kDataLoss);
}

struct FormatterTestCase {
  absl::string_view input;
  absl::string_view expected;
};

static const LineNumberSet kEnableAllLines;

// Test that the expected output is produced with the formatter using a custom
// FormatStyle.
TEST(FormatterTest, FormatCustomStyleTest) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"module m;wire w;endmodule\n",
       "module m;\n"
       "          wire w;\n"
       "endmodule\n"},
  };

  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 10;  // unconventional indentation
  style.wrap_spaces = 4;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status);
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kFormatterTestCases[] = {
    {"", ""},
    {"\n", "\n"},
    {"\n\n", "\n\n"},
    {"\t//comment\n", "//comment\n"},
    {"\t/*comment*/\n", "/*comment*/\n"},
    {"\t/*multi-line\ncomment*/\n", "/*multi-line\ncomment*/\n"},
    // preprocessor test cases
    {"`include    \"path/to/file.vh\"\n", "`include \"path/to/file.vh\"\n"},
    {"`include    `\"path/to/file.vh`\"\n", "`include `\"path/to/file.vh`\"\n"},
    {"`define    FOO\n", "`define FOO\n"},
    {"`define    FOO   BAR\n", "`define FOO BAR\n"},
    {"`define    FOO\n"
     "`define  BAR\n",
     "`define FOO\n"
     "`define BAR\n"},
    {"`ifndef    FOO\n"
     "`endif // FOO\n",
     "`ifndef FOO\n"
     "`endif  // FOO\n"},
    {"`ifndef    FOO\n"
     "`define   BAR\n"
     "`endif\n",
     "`ifndef FOO\n"
     "`define BAR\n"
     "`endif\n"},
    {"`ifndef    FOO\n"
     "`define   BAR\n\n"  // one more blank line
     "`endif\n",
     "`ifndef FOO\n"
     "`define BAR\n\n"
     "`endif\n"},
    {"`define    FOO   \\\n"
     "  BAR\n",
     "`define FOO \\\n"  // TODO(b/72527558): right align '\'s to column limit
     "  BAR\n"},
    {"`define    FOOOOOOOOOOOOOOOO   \\\n"
     "  BAAAAAAAAAAAAAAAAR BAAAAAAAAAAAAAZ;\n",
     "`define FOOOOOOOOOOOOOOOO \\\n"  // macro text starts at '\'
     "  BAAAAAAAAAAAAAAAAR BAAAAAAAAAAAAAZ;\n"},
    {"`ifdef      FOO\n"
     "  `fine()\n"
     "`else\n"
     "  `error()\n"
     "`endif\n",
     "`ifdef FOO\n"
     "`fine()\n"
     "`else\n"
     "`error()\n"
     "`endif\n"},
    {"`ifdef      FOO\n"
     "  `fine()\n"
     "`else // trouble\n"
     "  `error()\n"
     "`endif\n",
     "`ifdef FOO\n"
     "`fine()\n"
     "`else  // trouble\n"
     "`error()\n"
     "`endif\n"},
    {"`ifdef      FOO\n"
     "  `fine()\n"
     "`else /* trouble */\n"
     "  `error()\n"
     "`endif\n",
     "`ifdef FOO\n"
     "`fine()\n"
     "`else  /* trouble */\n"
     "`error()\n"
     "`endif\n"},
    {"    // lonely comment\n", "// lonely comment\n"},
    {"    // first comment\n"
     "  // last comment\n",
     "// first comment\n"
     "// last comment\n"},
    {"    // starting comment\n"
     "  `define   FOO\n",
     "// starting comment\n"
     "`define FOO\n"},
    {"  `define   FOO\n"
     "   // trailing comment\n",
     "`define FOO\n"
     "// trailing comment\n"},
    {"  `define   FOO\n"
     "   // trailing comment 1\n"
     "      // trailing comment 2\n",
     "`define FOO\n"
     "// trailing comment 1\n"
     "// trailing comment 2\n"},
    {
        "  `define   FOO    \\\n"  // multiline macro definition
        " 1\n",
        "`define FOO \\\n"
        " 1\n"  // TODO(b/141517267): Reflowing macro definitions
    },
    {"`define FOO    \\\n"  // multiline macro definition
     "        b\n",
     "`define FOO \\\n"  // no need to align '\'
     "        b\n"},
    {"`define FOO    \\\n"  // multiline macro definition
     "        a +    \\\n"
     "        b\n",
     "`define FOO    \\\n"  // preserve spacing before '\'
     "        a +    \\\n"  // to stay aligned with this one
     "        b\n"},
    {"    // comment with backslash\\\n", "// comment with backslash\\\n"},
    {// macro with MacroArg tokens as arguments
     "`FOOOOOO(\nbar1...\n,\nbar2...\n,\nbar3...\n,\nbar4\n)\n",
     "`FOOOOOO(bar1..., bar2..., bar3...,\n"
     "         bar4)\n"},
    {// macro declaration exceeds line length limit
     "`F_MACRO(looooooong_type if_it_fits_I_sits)\n",
     "`F_MACRO(\n"
     "    looooooong_type if_it_fits_I_sits)\n"},
    {// macro call with not fitting arguments
     "`MACRO_FFFFFFFFFFF("
     "type_a_aaaa,type_b_bbbbb,"
     "type_c_cccccc,type_d_dddddddd,"
     "type_e_eeeeeeee,type_f_ffff)\n",
     "`MACRO_FFFFFFFFFFF(\n"
     "    type_a_aaaa, type_b_bbbbb,\n"
     "    type_c_cccccc, type_d_dddddddd,\n"
     "    type_e_eeeeeeee, type_f_ffff)\n"},
    {// nested macro call
     "`MACRO_FFFFFFFFFFF( "
     "`A(type_a_aaaa), `B(type_b_bbbbb), "
     "`C(type_c_cccccc), `D(type_d_dddddddd), "
     "`E(type_e_eeeeeeee), `F(type_f_ffff))\n",
     "`MACRO_FFFFFFFFFFF(`A(type_a_aaaa),\n"
     "                   `B(type_b_bbbbb),\n"
     "                   `C(type_c_cccccc),\n"
     "                   `D(type_d_dddddddd),\n"
     "                   `E(type_e_eeeeeeee),\n"
     "                   `F(type_f_ffff))\n"},
    {// two-level nested macro call
     "`MACRO_FFFFFFFFFFF( "
     "`A(type_a_aaaa, `B(type_b_bbbbb)), "
     "`C(type_c_cccccc, `D(type_d_dddddddd)), "
     "`E(type_e_eeeeeeee, `F(type_f_ffff)))\n",
     "`MACRO_FFFFFFFFFFF(\n"
     "    `A(type_a_aaaa, `B(type_b_bbbbb)),\n"
     "    `C(type_c_cccccc,\n"
     "       `D(type_d_dddddddd)),\n"
     "    `E(type_e_eeeeeeee,\n"
     "       `F(type_f_ffff)))\n"},
    {// three-level nested macro call
     "`MACRO_FFFFFFFFFFF(`A(type_a_aaaa,"
     "`B(type_b_bbbbb,`C(type_c_cccccc))),"
     "`D(type_d_dddddddd,`E(type_e_eeeeeeee,"
     "`F(type_f_ffff))))\n",
     "`MACRO_FFFFFFFFFFF(\n"
     "    `A(type_a_aaaa,\n"
     "       `B(type_b_bbbbb,\n"
     "          `C(type_c_cccccc))),\n"
     "    `D(type_d_dddddddd,\n"
     "       `E(type_e_eeeeeeee,\n"
     "          `F(type_f_ffff))))\n"},
    {// macro call with MacroArg tokens as arugments and with semicolon
     "`FOOOOOO(\nbar1...\n,\nbar2...\n,\nbar3...\n,\nbar4\n);\n",
     "`FOOOOOO(bar1..., bar2..., bar3...,\n"
     "         bar4);\n"},
    {// macro declaration exceeds line length limit and contains semicolon
     "`F_MACRO(looooooong_type if_it_fits_I_sits);\n",
     "`F_MACRO(\n"
     "    looooooong_type if_it_fits_I_sits);\n"},
    {// macro call with not fitting arguments and semicolon
     "`MACRO_FFFFFFFFFFF("
     "type_a_aaaa,type_b_bbbbb,"
     "type_c_cccccc,type_d_dddddddd,"
     "type_e_eeeeeeee,type_f_ffff);\n",
     "`MACRO_FFFFFFFFFFF(\n"
     "    type_a_aaaa, type_b_bbbbb,\n"
     "    type_c_cccccc, type_d_dddddddd,\n"
     "    type_e_eeeeeeee, type_f_ffff);\n"},
    {// nested macro call with semicolon
     "`MACRO_FFFFFFFFFFF( "
     "`A(type_a_aaaa), `B(type_b_bbbbb), "
     "`C(type_c_cccccc), `D(type_d_dddddddd), "
     "`E(type_e_eeeeeeee), `F(type_f_ffff));\n",
     "`MACRO_FFFFFFFFFFF(`A(type_a_aaaa),\n"
     "                   `B(type_b_bbbbb),\n"
     "                   `C(type_c_cccccc),\n"
     "                   `D(type_d_dddddddd),\n"
     "                   `E(type_e_eeeeeeee),\n"
     "                   `F(type_f_ffff));\n"},
    {// two-level nested macro call with semicolon
     "`MACRO_FFFFFFFFFFF( "
     "`A(type_a_aaaa, `B(type_b_bbbbb)), "
     "`C(type_c_cccccc, `D(type_d_dddddddd)), "
     "`E(type_e_eeeeeeee, `F(type_f_ffff)));\n",
     "`MACRO_FFFFFFFFFFF(\n"
     "    `A(type_a_aaaa, `B(type_b_bbbbb)),\n"
     "    `C(type_c_cccccc,\n"
     "       `D(type_d_dddddddd)),\n"
     "    `E(type_e_eeeeeeee,\n"
     "       `F(type_f_ffff)));\n"},
    {// three-level nested macro call with semicolon
     "`MACRO_FFFFFFFFFFF(`A(type_a_aaaa,"
     "`B(type_b_bbbbb,`C(type_c_cccccc))),"
     "`D(type_d_dddddddd,`E(type_e_eeeeeeee,"
     "`F(type_f_ffff))));\n",
     "`MACRO_FFFFFFFFFFF(\n"
     "    `A(type_a_aaaa,\n"
     "       `B(type_b_bbbbb,\n"
     "          `C(type_c_cccccc))),\n"
     "    `D(type_d_dddddddd,\n"
     "       `E(type_e_eeeeeeee,\n"
     "          `F(type_f_ffff))));\n"},
    {// macro call with no args
     "`FOOOOOO()\n", "`FOOOOOO()\n"},
    {// macro call with no args and semicolon
     "`FOOOOOO();\n", "`FOOOOOO();\n"},
    {// macro call with no args and semicolon separated by space
     "`FOOOOOO() ;\n", "`FOOOOOO();\n"},
    {// macro call with comments in argument list
     "`FOO(aa, //aa\nbb , // bb\ncc)\n",
     "`FOO(aa,  //aa\n"
     "     bb,  // bb\n"
     "     cc)\n"},
    {// macro call with comment before first argument
     "`FOO(//aa\naa, //bb\nbb , // cc\ncc)\n",
     "`FOO(  //aa\n"
     "    aa,  //bb\n"
     "    bb,  // cc\n"
     "    cc)\n"},
    {// macro call with argument including trailing EOL comment
     "`FOO(aa, bb,//cc\ndd)\n",
     "`FOO(aa, bb,  //cc\n"
     "     dd)\n"},
    {// macro call with argument including EOL comment on own line
     "`FOOOO(aa, bb,\n//cc\ndd)\n",
     "`FOOOO(aa, bb,\n"
     "       //cc\n"
     "       dd)\n"},
    {"  // leading comment\n"
     "  `define   FOO    \\\n"  // multiline macro definition
     "1\n"
     "   // trailing comment\n",
     "// leading comment\n"
     "`define FOO \\\n"
     "1\n"  // TODO(b/141517267): Reflowing macro definitions
     "// trailing comment\n"},
    {// macro call after define
     "`define   FOO   BAR\n"
     "  `FOO( bar )\n",
     "`define FOO BAR\n"
     "`FOO(bar)\n"},
    {// multiple argument macro call
     "  `FOO( bar , baz )\n", "`FOO(bar, baz)\n"},
    {// long macro call breaking
     " `ASSERT_INIT(S, (D == 4 && K inside {0, 1}) ||"
     " (D == 3 && K== 4))\n",
     "`ASSERT_INIT(\n"
     "    S, (D == 4 && K inside {0, 1}) ||\n"
     "           (D == 3 && K == 4))\n"},
    {// long macro call breaking
     " `AINIT(S, (D == 4 && K inside {0, 1}) ||"
     " (D == 3 && K== 4))\n",
     "`AINIT(S, (D == 4 && K inside {0, 1}) ||\n"
     "              (D == 3 && K == 4))\n"},
    {// long macro call breaking
     " `ASSERT_INIT(S, D == 4 && K inside {0, 1})\n",
     "`ASSERT_INIT(S,\n"
     "             D == 4 && K inside {0, 1})\n"},
    {// macro call in function
     "function void foo( );   foo=`FOO( bar , baz ) ; endfunction\n",
     "function void foo();\n"
     "  foo = `FOO(bar, baz);\n"
     "endfunction\n"},
    {// nested macro call in function
     "function void foo( );   foo=`FOO( `BAR ( baz ) ) ; endfunction\n",
     "function void foo();\n"
     "  foo = `FOO(`BAR(baz));\n"
     "endfunction\n"},
    {// macro call in class
     "class foo;    `FOO  ( bar , baz ) ; endclass\n",
     "class foo;\n"
     "  `FOO(bar, baz);\n"
     "endclass\n"},
    {// nested macro call in class
     "class foo;    `FOO  ( `BAR ( baz1 , baz2 ) ) ; endclass\n",
     "class foo;\n"
     "  `FOO(`BAR(baz1, baz2));\n"
     "endclass\n"},
    {// multi-line macro arg "aaaa..." should start on its own line,
     // even if its first line would fit under the column limit
     "`CHECK_FATAL(rd_tr,\n"
     "             aaaa     == zzz;\n"
     "             ggg      == vv::w;,\n"
     "             \"Failed to ..........\")\n",
     "`CHECK_FATAL(rd_tr,\n"
     "             aaaa     == zzz;\n"
     "             ggg      == vv::w;,\n"
     "             \"Failed to ..........\")\n"},

    // `uvm macros indenting
    {
        // simple test case
        "`uvm_object_utils_begin(aa)\n"
        "`uvm_field_int(bb, UVM_DEFAULT)\n"
        "`uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_object_utils_end\n",
        "`uvm_object_utils_begin(aa)\n"
        "  `uvm_field_int(bb, UVM_DEFAULT)\n"
        "  `uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_object_utils_end\n",
    },
    {// multiple uvm.*begin - uvm.*end ranges
     "`uvm_object_utils_begin(aa)\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_object_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_object_utils_begin(bb)\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_object_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n",
     "`uvm_object_utils_begin(aa)\n"
     "  `uvm_field_int(bb, UVM_DEFAULT)\n"
     "  `uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_object_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_object_utils_begin(bb)\n"
     "  `uvm_field_int(bb, UVM_DEFAULT)\n"
     "  `uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_object_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"},
    {
        // empty uvm.*begin - uvm.*end range
        "`uvm_component_utils_begin(aa)\n"
        "`uvm_component_utils_end\n",
        "`uvm_component_utils_begin(aa)\n"
        "`uvm_component_utils_end\n",
    },
    {
        // uvm_field_utils
        "`uvm_field_utils_begin(aa)\n"
        "`uvm_field_int(bb, UVM_DEFAULT)\n"
        "`uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_field_utils_end\n",
        "`uvm_field_utils_begin(aa)\n"
        "  `uvm_field_int(bb, UVM_DEFAULT)\n"
        "  `uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_field_utils_end\n",
    },
    {
        // uvm_component
        "`uvm_component_utils_begin(aa)\n"
        "`uvm_field_int(bb, UVM_DEFAULT)\n"
        "`uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n",
        "`uvm_component_utils_begin(aa)\n"
        "  `uvm_field_int(bb, UVM_DEFAULT)\n"
        "  `uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n",
    },
    {
        // nested uvm macros
        "`uvm_field_int(l0, UVM_DEFAULT)\n"
        "`uvm_component_utils_begin(l0)\n"
        "`uvm_field_int(l1, UVM_DEFAULT)\n"
        "`uvm_component_utils_begin(l1)\n"
        "`uvm_field_int(l2, UVM_DEFAULT)\n"
        "`uvm_component_utils_begin(l2)\n"
        "`uvm_field_int(l3, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n"
        "`uvm_field_int(l2, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n"
        "`uvm_field_int(l1, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n"
        "`uvm_field_int(l0, UVM_DEFAULT)\n",
        "`uvm_field_int(l0, UVM_DEFAULT)\n"
        "`uvm_component_utils_begin(l0)\n"
        "  `uvm_field_int(l1, UVM_DEFAULT)\n"
        "  `uvm_component_utils_begin(l1)\n"
        "    `uvm_field_int(l2, UVM_DEFAULT)\n"
        "    `uvm_component_utils_begin(l2)\n"
        "      `uvm_field_int(l3, UVM_DEFAULT)\n"
        "    `uvm_component_utils_end\n"
        "    `uvm_field_int(l2, UVM_DEFAULT)\n"
        "  `uvm_component_utils_end\n"
        "  `uvm_field_int(l1, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n"
        "`uvm_field_int(l0, UVM_DEFAULT)\n",
    },
    {
        // non-uvm macro
        "`my_macro_begin(aa)\n"
        "`my_field(b)\n"
        "`my_field(c)\n"
        "`my_macro_end\n",
        "`my_macro_begin(aa)\n"
        "`my_field(b)\n"
        "`my_field(c)\n"
        "`my_macro_end\n",
    },
    {
        // unbalanced uvm macros: missing uvm.*end macro
        "`uvm_component_utils_begin(aa)\n"
        "`uvm_field_int(bb, UVM_DEFAULT)\n"
        "`uvm_field_int(cc, UVM_DEFAULT)\n",
        "`uvm_component_utils_begin(aa)\n"
        "`uvm_field_int(bb, UVM_DEFAULT)\n"
        "`uvm_field_int(cc, UVM_DEFAULT)\n",
    },
    {
        // unbalanced uvm macros: missing uvm.*begin macro
        "`uvm_field_int(bb, UVM_DEFAULT)\n"
        "`uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n",
        "`uvm_field_int(bb, UVM_DEFAULT)\n"
        "`uvm_field_int(cc, UVM_DEFAULT)\n"
        "`uvm_component_utils_end\n",
    },
    {// unbalanced uvm macros: missing _begin macro between
     // matching uvm.*begin-uvm.*end macros
     "`uvm_component_utils_begin(aa)\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_component_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_component_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_component_utils_begin(aa)\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_component_utils_end\n",
     "`uvm_component_utils_begin(aa)\n"
     "  `uvm_field_int(bb, UVM_DEFAULT)\n"
     "  `uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_component_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_component_utils_end\n"
     "`uvm_field_int(bb, UVM_DEFAULT)\n"
     "`uvm_component_utils_begin(aa)\n"
     "  `uvm_field_int(bb, UVM_DEFAULT)\n"
     "  `uvm_field_int(cc, UVM_DEFAULT)\n"
     "`uvm_component_utils_end\n"},

    // top-level directive test cases
    {"`timescale  1ns/1ps\n",  //
     "`timescale 1ns / 1ps\n"},

    // parameter test cases
    {
        "  parameter  int   foo=0 ;",
        "parameter int foo = 0;\n",
    },
    {
        "  parameter  int   foo=bar [ 0 ] ;",  // index expression
        "parameter int foo = bar[0];\n",
    },
    {
        "  parameter  int   foo=bar [ a+b ] ;",  // binary inside index expr
        "parameter int foo = bar[a+b];\n",       // allowed to be 0 spaces
                                                 // (preserved)
    },
    {
        "  parameter  int   foo=bar [ a+ b ] ;",  // binary inside index expr
        "parameter int foo = bar[a+b];\n",        // allowed to be 0 spaces
                                                  // (symmetrized)
    },
    {
        "  parameter  int   foo=bar [ a +b ] ;",  // compact binary inside
        "parameter int foo = bar[a+b];\n",        // index expression
    },
    {
        "  parameter  int   foo=bar [ a  +b ] ;",  // compact binary inside
        "parameter int foo = bar[a+b];\n",         // index expression
    },
    {
        // with line continuations
        "  parameter  \\\nint   \\\nfoo=a+ \\\nb ;",
        "parameter\\\n    int\\\n    foo = a +\\\n    b;\n",
        // TODO(fangism): should text following a line continuation hang-indent?
    },
    // unary prefix expressions
    {
        "  parameter  int   foo=- 1 ;",
        "parameter int foo = -1;\n",
    },
    {
        "  parameter  int   foo=+ 7 ;",
        "parameter int foo = +7;\n",
    },
    {
        "  parameter  int   foo=- J ;",
        "parameter int foo = -J;\n",
    },
    {
        "  parameter  int   foo=- ( y ) ;",
        "parameter int foo = -(y);\n",
    },
    {
        "  parameter  int   foo=- ( z*y ) ;",
        "parameter int foo = -(z * y);\n",
    },
    {
        "  parameter  int   foo=-  z*- y  ;",
        "parameter int foo = -z * -y;\n",
    },
    {
        "  parameter  int   foo=( - 2 ) ;",  //
        "parameter int foo = (-2);\n",
    },
    {
        "  parameter  int   foo=$bar(-  z,- y ) ;",
        "parameter int foo = $bar(-z, -y);\n",
    },
    {"  parameter int a=b&~(c<<d);", "parameter int a = b & ~(c << d);\n"},
    {"  parameter int a=~~~~b;", "parameter int a = ~~~~b;\n"},
    {"  parameter int a = ~ ~ ~ ~ b;", "parameter int a = ~~~~b;\n"},
    {"  parameter int a   =   ~--b;", "parameter int a = ~--b;\n"},
    {"  parameter int a   =   ~ --b;", "parameter int a = ~--b;\n"},
    {"  parameter int a = ~ ++ b;", "parameter int a = ~++b;\n"},
    {"  parameter int a=--b- --c;", "parameter int a = --b - --c;\n"},
    // ^~ and ~^ are bitwise nor, but ^ ~ isn't
    {"  parameter int a=b^~(c<<d);", "parameter int a = b ^~ (c << d);\n"},
    {"  parameter int a=b~^(c<<d);", "parameter int a = b ~^ (c << d);\n"},
    {"  parameter int a=b^ ~ (c<<d);", "parameter int a = b ^ ~(c << d);\n"},
    {"  parameter int a=b ^ ~(c<<d);", "parameter int a = b ^ ~(c << d);\n"},

    {"  parameter int a=b^~{c};", "parameter int a = b ^~ {c};\n"},
    {"  parameter int a=b~^{c};", "parameter int a = b ~^ {c};\n"},
    {"  parameter int a=b^ ~ {c};", "parameter int a = b ^ ~{c};\n"},
    {"  parameter int a=b ^ ~{c};", "parameter int a = b ^ ~{c};\n"},

    {"  parameter int a={a}^{b};", "parameter int a = {a} ^ {b};\n"},
    {"  parameter int a={b}^(c);", "parameter int a = {b} ^ (c);\n"},
    {"  parameter int a=b[0]^ {c};", "parameter int a = b[0] ^ {c};\n"},
    {"  parameter int a={c}^a[b];", "parameter int a = {c} ^ a[b];\n"},
    {"  parameter int a=(c)^{a[b]};", "parameter int a = (c) ^ {a[b]};\n"},

    {"  parameter int a={^{a,^b},c};", "parameter int a = {^{a, ^b}, c};\n"},
    {"  parameter int a=(a)^(^d[e]^{c});",
     "parameter int a = (a) ^ (^d[e] ^ {c});\n"},
    {"  parameter int a=(a)^(^d[e]^f[g]);",
     "parameter int a = (a) ^ (^d[e] ^ f[g]);\n"},
    {"  parameter int a=(b^(c^(d^e)));",
     "parameter int a = (b ^ (c ^ (d ^ e)));\n"},
    {"  parameter int a={b^{c^{d^e}}};",
     "parameter int a = {b ^ {c ^ {d ^ e}}};\n"},
    {"  parameter int a={b^{c[d^e]}};",
     "parameter int a = {b ^ {c[d^e]}};\n"},  // allow 0 spaces inside "[d^e]"
    {"  parameter int a={(b^c),(d^^e)};",
     "parameter int a = {(b ^ c), (d ^ ^e)};\n"},

    {"  parameter int a={(b[x]^{c[y]})};",
     "parameter int a = {(b[x] ^ {c[y]})};\n"},
    {"  parameter int a={d^^e[f] ^ (g)};",
     "parameter int a = {d ^ ^e[f] ^ (g)};\n"},

    // ~| is unary reduction NOR, |~ and | ~ aren't
    {"  parameter int a=b| ~(c<<d);", "parameter int a = b | ~(c << d);\n"},
    {"  parameter int a=b|~(c<<d);", "parameter int a = b | ~(c << d);\n"},
    {"  parameter int a=b| ~| ( c<<d);", "parameter int a = b | ~|(c << d);\n"},
    {"  parameter int a=b| ~| ~| ( c<<d);",
     "parameter int a = b | ~|~|(c << d);\n"},
    {"  parameter int a=b| ~~~( c<<d);",
     "parameter int a = b | ~~~(c << d);\n"},
    {
        "  parameter  int   foo=- - 1 ;",  // double negative
        "parameter int foo = - -1;\n",
    },
    {
        "  parameter  int   ternary=1?2:3;",
        "parameter int ternary = 1 ? 2 : 3;\n",
    },
    {
        "  parameter  int   ternary=a?b:c;",
        "parameter int ternary = a ? b : c;\n",
    },
    {
        "  parameter  int   ternary=\"a\"?\"b\":\"c\";",
        "parameter int ternary = \"a\" ? \"b\" : \"c\";\n",
    },
    {
        "  parameter  int   t=`\"a`\"?`\"b`\":`\"c`\";",
        "parameter int t = `\"a`\" ? `\"b`\" : `\"c`\";\n",
    },
    {
        "  parameter  int   ternary=(a)?(b):(c);",
        "parameter int ternary = (a) ? (b) : (c);\n",
    },
    {
        "  parameter  int   ternary={a}?{b}:{c};",
        "parameter int ternary = {a} ? {b} : {c};\n",
    },
    {
        "  parameter  int   long_ternary=cond?long_option_t:long_option_f;",
        "parameter int long_ternary = cond ?\n"
        "    long_option_t : long_option_f;\n",
    },
    {
        "  parameter  int   break_two=cond\n"
        "? a_really_long_option_number_one:a_really_long_option_number_two;",
        "parameter int break_two = cond ?\n"
        "    a_really_long_option_number_one :\n"
        "    a_really_long_option_number_two;\n",
    },
    {
        "  assign   ternary=1?2:3;",
        "assign ternary = 1 ? 2 : 3;\n",
    },
    {
        "  assign   ternary=a?b:c;",
        "assign ternary = a ? b : c;\n",
    },
    {
        "  assign   ternary={a}?{b}:{c};",
        "assign ternary = {a} ? {b} : {c};\n",
    },
    {
        "  assign   break_two=cond\n"
        "? a_really_long_option_number_one:a_really_long_option_number_two;",
        "assign break_two = cond ?\n"
        "    a_really_long_option_number_one :\n"
        "    a_really_long_option_number_two;\n",
    },
    {
        "assign prefetch_d     =\n"
        "lookup_grant_ic0 ? (lookup_addr_aligned + ADDR) :\n"
        "                   addr_i;",
        "assign prefetch_d = lookup_grant_ic0 ?\n"
        "    (lookup_addr_aligned + ADDR) :\n"
        "    addr_i;\n",
    },
    {
        "assign prefetch_d     =\n"
        "lookup_grant_ic0 ? (lookup_addr + 1) :\n"
        "                   addr_i;",
        "assign prefetch_d = lookup_grant_ic0 ?\n"
        "    (lookup_addr + 1) : addr_i;\n",
    },
    {
        "module test;\n"
        " assign next = // EOL\n"
        "  foo ? '0 :\n"
        "  cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next =  // EOL\n"
        "      foo ? '0 : cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo // EOL\n"
        "  ? '0 :\n"
        "  cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo  // EOL\n"
        "      ? '0 : cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? // EOL\n"
        "  '0 :\n"
        "  cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ?  // EOL\n"
        "      '0 : cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? '0 // EOL\n"
        "  :\n"
        "  cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ? '0  // EOL\n"
        "      : cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? '0 : // EOL\n"
        "  cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ? '0 :  // EOL\n"
        "      cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? '0 : // EOL\n"
        "  bar ? '1 : '0;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ? '0 :  // EOL\n"
        "      bar ? '1 : '0;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? '0 : // EOL\n"
        "  bar // EOL2\n"
        " ? '1 : '0;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ? '0 :  // EOL\n"
        "      bar  // EOL2\n"
        "      ? '1 : '0;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? '0 : // EOL\n"
        "  bar ? // EOL2\n"
        "  '1 : '0;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ? '0 :  // EOL\n"
        "      bar ?  // EOL2\n"
        "      '1 : '0;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? '0 : // EOL\n"
        "  bar ? '1 // EOL2\n"
        "  : '0;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ? '0 :  // EOL\n"
        "      bar ? '1  // EOL2\n"
        "      : '0;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? '0 : // EOL\n"
        "  bar ? '1 : // EOL2\n"
        "  '0;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ? '0 :  // EOL\n"
        "      bar ? '1 :  // EOL2\n"
        "      '0;\n"
        "endmodule\n",
    },
    {
        "assign prefetch_d     =\n"
        "lookup_ic0 ? // EOL\n"
        " (lookup_addr + 1) :// BOO\n"
        "                   addr_i;",
        "assign prefetch_d = lookup_ic0 ?  // EOL\n"
        "    (lookup_addr + 1) :  // BOO\n"
        "    addr_i;\n",
    },
    {
        "module test;\n"
        " assign next = (foo) ? '0          : // clear \n"
        "           (bar) ? cnt + 1'b1  : // count \n"
        "                   cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = (foo) ? '0 :  // clear \n"
        "      (bar) ? cnt + 1'b1 :  // count \n"
        "      cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = // FOO\n"
        "  (foo) ? '0          : // clear \n"
        "           (bar) ? cnt + 1'b1  : // count \n"
        "                   cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next =  // FOO\n"
        "      (foo) ? '0 :  // clear \n"
        "      (bar) ? cnt + 1'b1 :  // count \n"
        "      cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? a_really_long_identifier : // EOL\n"
        "  cnt;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ?\n"
        "      a_really_long_identifier :  // EOL\n"
        "      cnt;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? a_really_long_identifier : // EOL\n"
        "  another_really_long_identifier;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ?\n"
        "      a_really_long_identifier :  // EOL\n"
        "      another_really_long_identifier;\n"
        "endmodule\n",
    },
    {
        "module test;\n"
        " assign next = foo ? a_really_long_identifier : "
        "another_really_long_identifier;\n"
        "endmodule\n",
        "module test;\n"
        "  assign next = foo ?\n"
        "      a_really_long_identifier :\n"
        "      another_really_long_identifier;\n"
        "endmodule\n",
    },
    {
        "assign m = check                ? {10'b0, foo} :\n"
        "           (bar && (baz == '0)) ? hello        :\n"
        "           world                ? temp1        : temp2;\n",
        "assign m = check ? {10'b0, foo} :\n"
        "    (bar && (baz == '0)) ? hello :\n"
        "    world ? temp1 : temp2;\n",
    },
    {
        "assign {a, b} = !(c == d) ? {1'b0, e} :\n"
        "                ((e == f) && g) ?\n"
        "                {1'b0, f} : (h) ?\n"
        "                {1'b0, e} - 1'b1 :\n"
        "                {1'b0, e} + 1'b1;\n",
        "assign {a, b} = !(c == d) ? {1'b0, e} :\n"
        "    ((e == f) && g) ? {1'b0, f} : (h) ?\n"
        "    {1'b0, e} - 1'b1 : {1'b0, e} + 1'b1;\n",
    },
    {
        "assign {aaaaaaaaaa, bbbbbbbbb} = {1'b0, cccccccccccccccccc[15:0]} +\n"
        "                                 {1'b0, ddddddddddddddddd[15:0]};\n",
        "assign {aaaaaaaaaa, bbbbbbbbb} =\n"
        "    {1'b0, cccccccccccccccccc[15:0]} +\n"
        "    {1'b0, ddddddddddddddddd[15:0]};\n",
    },
    {
        "covergroup a(string b);\n"
        "foobar: cross foo, bar {"
        "ignore_bins baz = binsof(qux) intersect {1, 2, 3, 4, 5, 6, 7};"
        "}\n"
        "endgroup : a\n",
        "covergroup a(string b);\n"
        "  foobar: cross foo, bar{\n"
        "    ignore_bins baz =\n"
        "        binsof (qux) intersect {\n"
        "      1, 2, 3, 4, 5, 6, 7\n"
        "    };\n"
        "  }\n"
        "endgroup : a\n",
    },
    {
        "assign {aa, bb} = {1'b0, cc} + {1'b0, dd};\n",
        "assign {aa, bb} = {1'b0, cc} +\n"
        "    {1'b0, dd};\n",
    },

    // streaming operators
    {
        "   parameter  int  b={ >>   { a } } ;",
        "parameter int b = {>>{a}};\n",
    },
    {
        "   parameter  int  b={ >>   { a , b,  c } } ;",
        "parameter int b = {>>{a, b, c}};\n",
    },
    {
        "   parameter  int  b={ >> 4  { a } } ;",
        "parameter int b = {>>4{a}};\n",
    },
    {
        "   parameter  int  b={ >> byte  { a } } ;",
        "parameter int b = {>>byte{a}};\n",
    },
    {
        "   parameter  int  b={ >> my_type_t  { a } } ;",
        "parameter int b = {>>my_type_t{a}};\n",
    },
    {
        "   parameter  int  b={ >> `GET_TYPE  { a } } ;",
        "parameter int b = {>>`GET_TYPE{a}};\n",
    },
    {
        "   parameter  int  b={ >> 4  {{ >> 2 { a }  }} } ;",
        "parameter int b = {>>4{{>>2{a}}}};\n",
    },
    {
        "   parameter  int  b={ <<   { a } } ;",
        "parameter int b = {<<{a}};\n",
    },
    {
        "   parameter  int  b={ <<   { a , b,  c } } ;",
        "parameter int b = {<<{a, b, c}};\n",
    },
    {
        "   parameter  int  b={ << 4  { a } } ;",
        "parameter int b = {<<4{a}};\n",
    },
    {
        "   parameter  int  b={ << byte  { a } } ;",
        "parameter int b = {<<byte{a}};\n",
    },
    {
        "   parameter  int  b={ << my_type_t  { a } } ;",
        "parameter int b = {<<my_type_t{a}};\n",
    },
    {
        "   parameter  int  b={ << `GET_TYPE  { a } } ;",
        "parameter int b = {<<`GET_TYPE{a}};\n",
    },
    {
        "   parameter  int  b={ << 4  {{ << 2 { a }  }} } ;",
        "parameter int b = {<<4{{<<2{a}}}};\n",
    },

    // basic module test cases
    {"module foo;endmodule:foo\n",
     "module foo;\n"
     "endmodule : foo\n"},
    {"module\nfoo\n;\nendmodule\n:\nfoo\n",
     "module foo;\n"
     "endmodule : foo\n"},
    {"module\tfoo\t;\tendmodule\t:\tfoo",
     "module foo;\n"
     "endmodule : foo\n"},
    {"module foo;     // foo\n"
     "endmodule:foo\n",
     "module foo;  // foo\n"
     "endmodule : foo\n"},
    {"module foo;/* foo */endmodule:foo\n",
     "module foo;  /* foo */\n"
     "endmodule : foo\n"},
    {"module pm #(\n"
     "//comment\n"
     ") (wire ww);\n"
     "endmodule\n",
     "module pm #(\n"
     "    //comment\n"  // comment indented
     ") (\n"
     "    wire ww\n"
     ");\n"
     "endmodule\n"},
    {"module pm ( ) ;\n"  // empty ports list
     "endmodule\n",
     "module pm ();\n"
     "endmodule\n"},
    {"module pm #(\n"
     "//comment\n"
     ") ( );\n"
     "endmodule\n",
     "module pm #(\n"
     "    //comment\n"  // comment indented
     ") ();\n"          // (); grouped together
     "endmodule\n"},
    {"`ifdef FOO\n"
     "    `ifndef BAR\n"
     "    `endif\n"
     "`endif\n",
     "`ifdef FOO\n"
     "`ifndef BAR\n"
     "`endif\n"
     "`endif\n"},
    {"module foo(\n"
     "       `include \"ports.svh\"\n"
     "         ) ; endmodule\n",
     "module foo (\n"
     "    `include \"ports.svh\"\n"
     ");\n"
     "endmodule\n"},
    {"module foo(\n"
     "       `define FOO\n"
     "`undef\tFOO\n"
     "         ) ; endmodule\n",
     "module foo (\n"
     "    `define FOO\n"
     "    `undef FOO\n"
     ");\n"
     "endmodule\n"},
    {"module foo(  input x  , output y ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  x,\n"  // aligned
     "    output y\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "// comment\n"
     "  input x  , output y ) ;endmodule:foo\n",
     "module foo (\n"
     "    // comment\n"
     "    input  x,\n"  // aligned
     "    output y\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input[2:0]x  , output y [3:0] ) ;endmodule:foo\n",
     // each port item should be on its own line
     "module foo (\n"
     "    input  [2:0] x,\n"  // aligned
     "    output       y[3:0]\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  , output reg yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x,\n"  // aligned
     "    output reg  yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  ,//c1\n"
     "output reg yyy //c2\n"
     " ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x,   //c1\n"  // aligned
     "    output reg  yyy  //c2\n"  // aligned
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  ,/* c1 */\n"
     "output reg yyy /* c2 */\n"
     " ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x,   /* c1 */\n"  // aligned
     "    output reg  yyy  /* c2 */\n"  // aligned
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "// comment\n"
     "input wire x  ,//c1\n"
     "output reg yyy //c2\n"
     " ) ;endmodule:foo\n",
     "module foo (\n"
     "    // comment\n"
     "    input  wire x,   //c1\n"  // aligned
     "    output reg  yyy  //c2\n"  // aligned
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "/* comment */\n"
     "input wire x  ,/* c1 */\n"
     "output reg yyy /* c2 */\n"
     " ) ;endmodule:foo\n",
     "module foo (\n"
     "    /* comment */\n"
     "    input  wire x,   /* c1 */\n"  // aligned
     "    output reg  yyy  /* c2 */\n"  // aligned
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  ,/* c1\n"
     "c2\n"
     "c3 */\n"
     "output reg yyy /* c4 */\n"
     " ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x,   /* c1\n"
     "c2\n"
     "c3 */\n"  // TODO: align multiline comments
     "    output reg  yyy  /* c4 */\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  ,/* c1 */\n"
     "output reg yyy,\n"
     "output z // c2\n"
     " ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x,    /* c1 */\n"  // aligned
     "    output reg  yyy,\n"
     "    output      z     // c2\n"  // aligned
     ");\n"
     "endmodule : foo\n"},
    {"module m(input logic [4:0] foo,  // comment\n"
     "input logic bar // comment\n"
     " ) ;endmodule:m\n",
     "module m (\n"
     "    input logic [4:0] foo,  // comment\n"  // aligned
     "    input logic       bar   // comment\n"  // aligned
     ");\n"
     "endmodule : m\n"},
    {"module foo(  input wire x  , output yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x,\n"  // aligned
     "    output      yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   x  , output reg yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input      x,\n"  // aligned
     "    output reg yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   x  , output reg[a:b]yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input            x,\n"  // aligned
     "    output reg [a:b] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   [a:b]x  , output reg  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input      [a:b] x,\n"  // aligned
     "    output reg       yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   x  , "
     "  output logic  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input        x,\n"  // aligned
     "    output logic yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   [a:c]x  , "
     "  output logic[a-b: c]  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input        [  a:c] x,\n"  // aligned
     "    output logic [a-b:c] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   [a:c]x  , "
     "  output logic[a - b: c]  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input        [    a:c] x,\n"  // aligned
     "    output logic [a - b:c] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   [a:c]x  , input zzz ,"
     "  output logic[a - b: c]  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input        [    a:c] x,\n"    // aligned []'s
     "    input                  zzz,\n"  // aligned ids
     "    output logic [a - b:c] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   [a:b]x  , "
     "  output reg[e: f]  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input      [a:b] x,\n"  // aligned
     "    output reg [e:f] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   tri[aa: bb]x  , "
     "  output reg[e: f]  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  tri [aa:bb] x,\n"  // aligned
     "    output reg [  e:f] yy\n"  // TODO(b/70310743): align ':'
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   [a:b][c:d]x  , "
     "  output reg[e: f]  yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input      [a:b][c:d] x,\n"  // aligned
     "    output reg [e:f]      yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  [j:k], output reg yy ) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x [j:k],\n"  // aligned
     "    output reg  yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  , output reg yy [j:k]) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x,\n"  // aligned
     "    output reg  yy[j:k]\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  [p:q], output reg yy [j:k]) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire x [p:q],\n"  // aligned
     "    output reg  yy[j:k]\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  [p:q][r:s], output reg yy [j:k]) "
     ";endmodule:foo\n",
     "module foo (\n"
     "    input  wire x [p:q][r:s],\n"  // aligned
     "    output reg  yy[j:k]\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire x  [p:q][rr:ss], output reg yy [jj:kk][m:n]) "
     ";endmodule:foo\n",
     "module foo (\n"
     // TODO(b/70310743): align :'s
     "    input  wire x [  p:q][rr:ss],\n"
     "    output reg  yy[jj:kk][  m:n]\n"  // aligned
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire   [p:q]x, output reg yy [j:k]) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire [p:q] x,\n"  // aligned
     "    output reg        yy[j:k]\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire  x [p:q], output reg[j:k]yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire       x [p:q],\n"  // aligned
     "    output reg  [j:k] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input pkg::bar_t  x , output reg  yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input  pkg::bar_t x,\n"  // aligned
     "    output reg        yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input wire  x , output pkg::bar_t  yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input  wire       x,\n"  // aligned
     "    output pkg::bar_t yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input pkg::bar_t#(1)  x , output reg  yy) ;endmodule:foo\n",
     "module foo (\n"  // with parameterized port type
     "    input  pkg::bar_t#(1) x,\n"
     "    output reg            yy\n"  // aligned
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input signed x , output reg  yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input  signed x,\n"  // aligned
     "    output reg    yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input signed x , output reg [m:n] yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input  signed       x,\n"  // aligned
     "    output reg    [m:n] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input int signed x , output reg [m:n] yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input  int signed       x,\n"  // aligned
     "    output reg        [m:n] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input signed x , output pkg::bar_t  yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input  signed     x,\n"  // aligned
     "    output pkg::bar_t yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module somefunction ("
     "logic clk, int   a, int b);endmodule",
     "module somefunction (\n"
     "    logic clk,\n"  // direction missing
     "    int   a,\n"    // direction missing
     "    int   b\n"     // direction missing
     ");\n"
     "endmodule\n"},
    {"module somefunction ("
     "logic clk, input int   a, int b);endmodule",
     "module somefunction (\n"
     "          logic clk,\n"  // direction missing
     "    input int   a,\n"
     "          int   b\n"  // direction missing
     ");\n"
     "endmodule\n"},
    {"module somefunction ("
     "input logic clk, input int   a, int b);endmodule",
     "module somefunction (\n"
     "    input logic clk,\n"
     "    input int   a,\n"
     "          int   b\n"  // direction missing
     ");\n"
     "endmodule\n"},
    {"module somefunction ("
     "input clk, input int   a, int b);endmodule",
     "module somefunction (\n"
     "    input     clk,\n"  // type missing
     "    input int a,\n"
     "          int b\n"
     ");\n"
     "endmodule\n"},
    {"module somefunction ("
     "input logic clk, input a, int b);endmodule",
     "module somefunction (\n"
     "    input logic clk,\n"
     "    input       a,\n"  // type missing
     "          int   b\n"   // direction missing
     ");\n"
     "endmodule\n"},
    {"module m;foo bar(.baz({larry, moe, curly}));endmodule",
     "module m;\n"
     "  foo bar (.baz({larry, moe, curly}));\n"
     "endmodule\n"},
    {"module m;foo bar(.baz({larry,// expand this\n"
     "moe, curly}));endmodule",
     "module m;\n"
     "  foo bar (\n"
     "      .baz({\n"
     "        larry,  // expand this\n"
     "        moe,\n"
     "        curly\n"
     "      })\n"
     "  );\n"
     "endmodule\n"},
    {"parameter priv_reg_t impl_csr[] = {\n"
     "// Machine mode mode CSR\n"
     "MVENDORID, //\n"
     "MARCHID,   //\n"
     "DSCRATCH0, //\n"
     "DSCRATCH1  //\n"
     "};",
     "parameter priv_reg_t impl_csr[] = {\n"
     "  // Machine mode mode CSR\n"
     "  MVENDORID,  //\n"
     "  MARCHID,  //\n"
     "  DSCRATCH0,  //\n"
     "  DSCRATCH1  //\n"
     "};\n"},
    {"parameter priv_reg_t impl_csr[] = {\n"
     "// Expand elements\n"
     "MVENDORID,\n"
     "MARCHID,\n"
     "DSCRATCH0,\n"
     "DSCRATCH1\n"
     "};",
     "parameter priv_reg_t impl_csr[] = {\n"
     "  // Expand elements\n"
     "  MVENDORID,\n"
     "  MARCHID,\n"
     "  DSCRATCH0,\n"
     "  DSCRATCH1\n"
     "};\n"},
    /* TODO(b/158131099): to fix these, reinterpret 'b' as a kPortDeclaration
    {"module somefunction ("
     "input logic clk, input a, b);endmodule",
     "module somefunction (\n"
     "    input logic clk,\n"
     "    input       a,\n"  // type missing
     "                b\n"  // type and direction missing
     ");\n"
     "endmodule\n"},
    {"module somefunction ("
     "input logic clk, int a, b);endmodule",
     "module somefunction (\n"
     "    input logic clk,\n"
     "          int   a,\n"  // direction missing
     "                b\n"  // type and direction missing
     ");\n"
     "endmodule\n"},
     */
    {"module foo(\n"
     "//c1\n"
     "input wire x , \n"
     "//c2\n"
     "output reg  yy\n"
     "//c3\n"
     ") ;endmodule:foo\n",
     "module foo (\n"
     "    //c1\n"
     "    input  wire x,\n"  // aligned, ignoring comments
     "    //c2\n"
     "    output reg  yy\n"
     "    //c3\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "//c1\n"
     "input wire x , \n"
     "//c2a\n"  // longer comment
     "//c2b\n"
     "output reg  yy\n"
     "//c3\n"
     ") ;endmodule:foo\n",
     "module foo (\n"
     "    //c1\n"
     "    input  wire x,\n"  // aligned, ignoring comments
     "    //c2a\n"           // note: separated by 2 lines of comments
     "    //c2b\n"
     "    output reg  yy\n"
     "    //c3\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "`ifdef   FOO\n"
     "input wire x , \n"
     " `else\n"
     "output reg  yy\n"
     " `endif\n"
     ") ;endmodule:foo\n",
     "module foo (\n"
     "`ifdef FOO\n"
     "    input  wire x,\n"  // aligned, ignoring preprocessor conditionals
     "`else\n"
     "    output reg  yy\n"
     "`endif\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "input w , \n"
     "`define   FOO BAR\n"
     "input wire x , \n"
     " `include  \"stuff.svh\"\n"
     "output reg  yy\n"
     " `undef    FOO\n"
     "output zz\n"
     ") ;endmodule:foo\n",
     "module foo (\n"
     "    input       w,\n"  // aligned, ignoring preprocessor directives
     "    `define FOO BAR\n"
     "    input  wire x,\n"
     "    `include \"stuff.svh\"\n"
     "    output reg  yy\n"
     "    `undef FOO\n"
     "    output      zz\n"  // aligned, ignoring preprocessor directives
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "input wire x , \n  \n"  // blank line, separating alignment groups
     "output reg  yy\n"
     ") ;endmodule:foo\n",
     "module foo (\n"
     "    input wire x,\n"  // not aligned, due to blank line separating groups
     "\n"
     "    output reg yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"
     "input wire x1 [r:s],\n"
     "input [p:q] x2 , \n  \n"  // blank line, separating alignment groups
     "output reg  [jj:kk]yy1,\n"
     "output pkg::barr_t [mm:nn] yy2\n"
     ") ;endmodule:foo\n",
     "module foo (\n"
     "    input wire       x1[r:s],\n"  // aligned in this group, but not across
                                        // groups
     "    input      [p:q] x2,\n"
     "\n"
     "    output reg         [jj:kk] yy1,\n"
     "    output pkg::barr_t [mm:nn] yy2\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(\n"  // same as previous, with comments
     " //c1\n"
     "input wire x1 [r:s],\n"
     "input [p:q] x2 , \n"
     " //c2\n\n"  // blank line, separating alignment groups
     " //c3\n"
     "output reg  [jj:kk]yy1,\n"
     " //c4\n"
     "output pkg::barr_t [mm:nn] yy2\n"
     ") ;endmodule:foo\n",
     "module foo (\n"
     "    //c1\n"
     "    input wire       x1[r:s],\n"  // aligned in this group, but not across
                                        // groups
     "    input      [p:q] x2,\n"
     "    //c2\n"
     "\n"
     "    //c3\n"
     "    output reg         [jj:kk] yy1,\n"
     "    //c4\n"
     "    output pkg::barr_t [mm:nn] yy2\n"
     ");\n"
     "endmodule : foo\n"},

    {// align null-statement (issue #824)
     "class sample;"
     "bit a;;"
     "bit b;"
     "endclass",
     "class sample;\n"
     "  bit a;\n"
     "  ;\n"
     "  bit b;\n"
     "endclass\n"},
    {"class sample;"
     "bit a;;"
     "endclass",
     "class sample;\n"
     "  bit a;\n"
     "  ;\n"
     "endclass\n"},
    {"class sample;"
     "bit a;"
     "bit b;;"
     "endclass",
     "class sample;\n"
     "  bit a;\n"
     "  bit b;\n"
     "  ;\n"
     "endclass\n"},

    {// aligning here just barely fits in the 40col limit
     "module foo(  input int signed x [a:b],"
     "output reg [mm:nn] yy) ;endmodule:foo\n",
     "module foo (\n"
     // ---------------40col----------------->
     "    input  int signed         x [a:b],\n"  // aligned, still fits
     "    output reg        [mm:nn] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {// when aligning would result in exceeding column limit, don't align for
     // now
     "module foo(  input int signed x [aa:bb],"
     "output reg [mm:nn] yy) ;endmodule:foo\n",
     "module foo (\n"
     // ---------------40col----------------->
     //   input  int signed         x [aa:bb],\n"
     //   output reg        [mm:nn] yy\n"
     "    input  int signed         x [aa:bb],\n"  // aligned, still fits
     "    output reg        [mm:nn] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {// when aligning would result in exceeding column limit, don't align for
     // now
     "module foo(  input int signed x [aa:bb],"
     "output reg [mm:nn] yyy) ;endmodule:foo\n",
     "module foo (\n"
     // ---------------40col----------------->
     //   input  int signed         x  [aa:bb],\n"  // over limit, by comma
     //   output reg        [mm:nn] yyy\n"
     "    input int signed x[aa:bb],\n"  // aligned would be 41 columns
     "    output reg [mm:nn] yyy\n"
     ");\n"
     "endmodule : foo\n"},
    {// when aligning would result in exceeding column limit, don't align for
     // now
     "module foo(  input int signed x [a:b],//c\n"
     "output reg [m:n] yy) ;endmodule:foo\n",
     "module foo (\n"
     // ---------------40col---------------->
     //   input  int signed       x [a:b],  //c\n"  // over limit, by comment
     //   output reg        [m:n] yy\n"
     "    input int signed x[a:b],  //c\n"  // aligned would be 42 columns
     "    output reg [m:n] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {// aligning interfaces in port headers like types
     // TODO(b/161181877): flush interface port type left (multi-column)
     "module foo(  input clk , inter.face yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input            clk,\n"  // aligned
     "          inter.face yy\n"
     ");\n"
     "endmodule : foo\n"},
    {// aligning interfaces in port headers like types
     // TODO(b/161181877): flush interface port type left (multi-column)
     "module foo(  input wire   clk , inter.face yy) ;endmodule:foo\n",
     "module foo (\n"
     "    input wire       clk,\n"  // aligned
     "          inter.face yy\n"
     ");\n"
     "endmodule : foo\n"},

    // module local variable/net declaration alignment test cases
    {"module m;\n"
     "logic a;\n"
     "bit b;\n"
     "endmodule\n",
     "module m;\n"
     "  logic a;\n"
     "  bit   b;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic a;\n"
     "bit b;\n"
     "initial e=f;\n"  // separates alignment groups
     "wire c;\n"
     "bit d;\n"
     "endmodule\n",
     "module m;\n"
     "  logic a;\n"
     "  bit   b;\n"
     "  initial e = f;\n"  // separates alignment groups
     "  wire c;\n"
     "  bit  d;\n"
     "endmodule\n"},
    {"module m;\n"
     "// hello a\n"
     "logic a;\n"
     "// hello b\n"
     "bit b;\n"
     "endmodule\n",
     "module m;\n"
     "  // hello a\n"
     "  logic a;\n"
     "  // hello b\n"
     "  bit   b;\n"  // aligned across comments
     "endmodule\n"},
    {"module m;\n"
     "// hello a\n"
     "logic a;\n"
     "\n"  // extra blank line
     "// hello b\n"
     "bit b;\n"
     "endmodule\n",
     "module m;\n"
     "  // hello a\n"
     "  logic a;\n"
     "\n"              // extra blank line
     "  // hello b\n"  // aligned across blank lines
     "  bit   b;\n"    // aligned across comments
     "endmodule\n"},
    {"module m;\n"
     "logic [x:y]a;\n"  // packed dimensions
     "bit b;\n"
     "endmodule\n",
     "module m;\n"
     "  logic [x:y] a;\n"
     "  bit         b;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic a;\n"
     "bit [pp:qq]b;\n"  // packed dimensions
     "endmodule\n",
     "module m;\n"
     "  logic         a;\n"
     "  bit   [pp:qq] b;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic [x:y]a;\n"  // packed dimensions
     "bit [pp:qq]b;\n"  // packed dimensions
     "endmodule\n",
     "module m;\n"
     "  logic [  x:y] a;\n"
     "  bit   [pp:qq] b;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic [x:y]a;\n"         // packed dimensions
     "wire [pp:qq] [e:f]b;\n"  // packed dimensions, 2D
     "endmodule\n",
     "module m;\n"
     "  logic [  x:y]      a;\n"
     "  wire  [pp:qq][e:f] b;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic a [x:y];\n"  // unpacked dimensions
     "bit bbb;\n"
     "endmodule\n",
     "module m;\n"
     "  logic a   [x:y];\n"
     "  bit   bbb;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic aaa ;\n"
     "wire w [yy:zz];\n"  // unpacked dimensions
     "endmodule\n",
     "module m;\n"
     "  logic aaa;\n"
     "  wire  w   [yy:zz];\n"
     "endmodule\n"},
    {"module m;\n"
     "logic aaa [s:t] ;\n"  // unpacked dimensions
     "wire w [yy:zz];\n"    // unpacked dimensions
     "endmodule\n",
     "module m;\n"
     "  logic aaa[  s:t];\n"
     "  wire  w  [yy:zz];\n"
     "endmodule\n"},
    {"module m;\n"
     "logic aaa [s:t] ;\n"     // unpacked dimensions
     "wire w [yy:zz][u:v];\n"  // unpacked dimensions, 2D
     "endmodule\n",
     "module m;\n"
     "  logic aaa[  s:t];\n"
     "  wire  w  [yy:zz] [u:v];\n"
     // TODO(b/165323560): unwanted space between unpacked dimensions of 'w'
     "endmodule\n"},
    {"module m;\n"
     "qqq::rrr s;\n"     // user-defined type
     "wire [pp:qq]w;\n"  // packed dimensions
     "endmodule\n",
     "module m;\n"
     "  qqq::rrr         s;\n"
     "  wire     [pp:qq] w;\n"
     "endmodule\n"},
    {"module m;\n"
     "qqq#(rr) s;\n"     // parameterized type
     "wire [pp:qq]w;\n"  // packed dimensions
     "endmodule\n",
     "module m;\n"
     "  qqq #(rr)         s;\n"
     "  wire      [pp:qq] w;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic a;\n"
     "bit b;\n"
     "my_module  my_inst( );\n"  // module instance separates alignment groups
     "wire c;\n"
     "bit d;\n"
     "endmodule\n",
     "module m;\n"
     "  logic a;\n"  // these two are aligned
     "  bit   b;\n"
     "  my_module my_inst ();\n"  // module instance separates alignment groups
     "  wire c;\n"                // these two are aligned
     "  bit  d;\n"
     "endmodule\n"},
    {"module m;\n"
     "logic aaa = expr1;\n"
     "bit b = expr2;\n"
     "endmodule\n",
     "module m;\n"
     "  logic aaa = expr1;\n"
     "  bit   b = expr2;\n"  // no alignment at '=' yet
     "endmodule\n"},
    {"module mattr;\n"
     "(* attr1=\"value1\" *)\n"  // attribute ignored
     "ex_input_pins_t ex_input_pins;\n"
     "(* attr2=\"value2\" *)\n"  // attribute ignored
     "ex_output_pins_t ex_output_pins;\n"
     "(* attr3=\"value3\" *)\n"  // attribute ignored
     "ex wrap_ex ( );\n"
     "endmodule\n",
     "module mattr;\n"
     "  (* attr1=\"value1\" *)\n"           // indented
     "  ex_input_pins_t  ex_input_pins;\n"  // aligned
     "  (* attr2=\"value2\" *)\n"           // indented
     "  ex_output_pins_t ex_output_pins;\n"
     "  (* attr3=\"value3\" *)\n"  // indented
     "  ex wrap_ex ();\n"
     "endmodule\n"},
    {"module mattr;\n"
     "ex_input_pins_t ex_input_pins;\n"
     "ex_output_pins_t ex_output_pins;\n"
     "(* package_definition=\"ex_pkg\" *)\n"  // attribute ignored
     "ex wrap_ex (\n"
     ".clk(ex_input_pins.clk),\n"
     ".rst(ex_input_pins.rst),\n"
     ".in(ex_input_pins.in)\n"
     ");\n"
     "endmodule\n",
     "module mattr;\n"
     "  ex_input_pins_t  ex_input_pins;\n"  // aligned
     "  ex_output_pins_t ex_output_pins;\n"
     "  (* package_definition=\"ex_pkg\" *)\n"  // indented
     "  ex wrap_ex (\n"
     "      .clk(ex_input_pins.clk),\n"
     "      .rst(ex_input_pins.rst),\n"
     "      .in (ex_input_pins.in)\n"  // aligned
     "  );\n"
     "endmodule\n"},
    {"module test;\n"
     "bind entropy_src tlul_assert #(.EndpointType(\"Device\"))\n"
     "tlul_assert_device (.clk_i, .rst_ni, .h2d(tl_i), .d2h(tl_o));\n"
     "endmodule\n",
     "module test;\n"
     "  bind entropy_src tlul_assert #(\n"
     "      .EndpointType(\"Device\")\n"
     "  ) tlul_assert_device (\n"
     "      .clk_i,\n"
     "      .rst_ni,\n"
     "      .h2d(tl_i),\n"
     "      .d2h(tl_o)\n"
     "  );\n"
     "endmodule\n"},
    {"module test;\n"
     "bind entropy_src tlul_assert #(.EndpointType(\"Device\"))\n"
     "tlul_assert_device (.clk_i, .rst_ni,\n\n .h2d(tl_i),\n\n .d2h(tl_o));\n"
     "endmodule\n",
     "module test;\n"
     "  bind entropy_src tlul_assert #(\n"
     "      .EndpointType(\"Device\")\n"
     "  ) tlul_assert_device (\n"
     "      .clk_i,\n"
     "      .rst_ni,\n"
     "\n"
     "      .h2d(tl_i),\n"
     "\n"
     "      .d2h(tl_o)\n"
     "  );\n"
     "endmodule\n"},
    {"bind expand_me long_name #(.W(W_CONST), .D(D_CONST)) instaaance_name ("
     ".in(iiiiiiiin),\n\n .out(ooooooout),\n .clk(ccccccclk),\n\n"
     ".in1234 (in),\n //c1\n .out1234(out),\n .clk1234(clk),);",
     "bind expand_me long_name #(\n"
     "    .W(W_CONST),\n"
     "    .D(D_CONST)\n"
     ") instaaance_name (\n"
     "    .in(iiiiiiiin),\n"
     "\n"
     "    .out(ooooooout),\n"
     "    .clk(ccccccclk),\n"
     "\n"
     "    .in1234 (in),\n"
     "    //c1\n"
     "    .out1234(out),\n"
     "    .clk1234(clk),\n"
     ");\n"},
    {"initial // clock generation\n begin\n clk = 0;\n forever begin\n"
     "#4ns clk = !clk;\n end\n end\n",
     "initial  // clock generation\n"
     "  begin\n"
     "    clk = 0;\n"
     "    forever begin\n"
     "      #4ns clk = !clk;\n"
     "    end\n"
     "  end\n"},
    {"module foo #(int x,int y) ;endmodule:foo\n",  // parameters
     "module foo #(\n"
     "    int x,\n"
     "    int y\n"
     ");\n"  // each parameter on its own line
     "endmodule : foo\n"},
    {"module foo #(int x)(input y) ;endmodule:foo\n",
     // parameter and port
     "module foo #(\n"
     "    int x\n"
     ") (\n"
     "    input y\n"
     ");\n"  // each paramater and port item should be on its own line
     "endmodule : foo\n"},
    {"module foo #(parameter int x,parameter int y) ;endmodule:foo\n",
     // parameters don't fit (also should be on its own line)
     "module foo #(\n"
     "    parameter int x,\n"
     "    parameter int y\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo #(parameter int xxxx,parameter int yyyy) ;endmodule:foo\n",
     // parameters don't fit
     "module foo #(\n"
     "    parameter int xxxx,\n"
     "    parameter int yyyy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo #(parameter int x = $clog2  (N) ,parameter int y ) "
     ";endmodule:foo\n",
     // parameters don't fit
     "module foo #(\n"
     "    parameter int x = $clog2(N),\n"  // no space after $clog2
     "    parameter int y\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo #(//comment\n"
     "parameter bar =1,\n"
     "localparam baz =2"
     ") ();"
     "endmodule",
     "module foo #(  //comment\n"
     "    parameter  bar = 1,\n"
     "    localparam baz = 2\n"
     ") ();\n"
     "endmodule\n"},
    {"module foo #("
     "parameter  bar =1,//comment\n"
     "localparam baz =2"
     ") ();"
     "endmodule",
     "module foo #(\n"
     "    parameter  bar = 1,  //comment\n"
     "    localparam baz = 2\n"
     ") ();\n"
     "endmodule\n"},
    {"module foo #("
     "parameter  bar =1,"
     "localparam baz =2//comment\n"
     ") ();"
     "endmodule",
     "module foo #(\n"
     "    parameter  bar = 1,\n"
     "    localparam baz = 2   //comment\n"
     ") ();\n"
     "endmodule\n"},
    {"module foo #("
     "parameter  bar =1//comment\n"
     ",localparam baz =2\n"
     ") ();"
     "endmodule",
     "module foo #(\n"
     "      parameter  bar = 1  //comment\n"
     "    , localparam baz = 2\n"
     ") ();\n"
     "endmodule\n"},
    {"module foo;"
     // fit in one line
     "parameter int i = '{\n"
     "1,\n"
     "2,\n"
     "3\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  parameter int i = '{1, 2, 3};\n"
     "endmodule\n"},
    {"module foo;"
     // too long for one line, expand
     "localparam logic [63:0] RC[24] = '{\n"
     "64'h 1,\n"
     "64'h 2,\n"
     "64'h 3\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  localparam logic [63:0] RC[24] = '{\n"
     "      64'h1,\n"
     "      64'h2,\n"
     "      64'h3\n"
     "  };\n"
     "endmodule\n"},
    {"module foo;"
     "parameter int i = '{\n"
     // force expansion
     "1, //\n"
     "2,\n"
     "3\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  parameter int i = '{\n"
     "      1,  //\n"
     "      2,\n"
     "      3\n"
     "  };\n"
     "endmodule\n"},
    {"module foo;"
     "localparam logic [63:0] RC[24] = '{\n"
     "64'h 0000_0000_0000_0001, // 0\n"
     "64'h 0000_0000_0000_8082, // 1\n"
     "64'h 8000_0000_8000_8008 // 23\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  localparam logic [63:0] RC[24] = '{\n"
     "      64'h0000_0000_0000_0001,  // 0\n"
     "      64'h0000_0000_0000_8082,  // 1\n"
     "      64'h8000_0000_8000_8008  // 23\n"
     "  };\n"
     "endmodule\n"},
    {"module foo;"
     // nest two patterns
     "parameter logic [11:0] i = '{\n"
     "'{1,2,3},\n"
     "'{1,2,3}\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  parameter logic [11:0] i = '{\n"
     "      '{1, 2, 3},\n"
     "      '{1, 2, 3}\n"
     "  };\n"
     "endmodule\n"},
    {"module foo;"
     // nest two patterns, expand interior
     "parameter logic [11:0] i = '{\n"
     "'{1, //\n"
     " 2,3},\n"
     "'{1,2,3}\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  parameter logic [11:0] i = '{\n"
     "      '{\n"
     "          1,  //\n"
     "          2,\n"
     "          3\n"
     "      },\n"
     "      '{1, 2, 3}\n"
     "  };\n"
     "endmodule\n"},
    {"module foo;"
     // nest two patterns, expand both
     "parameter nest [2] i = '{\n"
     "'{first : 32'h0000_0001,\n"
     "  second : 32'h0000_0011,\n"
     "  third: 32'h0000_0111},\n"
     "'{first : 32'h1000_0001,\n"
     "  second : 32'h1000_0011,\n"
     "  third: 32'h1000_0111}\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  parameter nest [2] i = '{\n"
     "      '{\n"
     "          first : 32'h0000_0001,\n"
     "          second : 32'h0000_0011,\n"
     "          third: 32'h0000_0111\n"
     "      },\n"
     "      '{\n"
     "          first : 32'h1000_0001,\n"
     "          second : 32'h1000_0011,\n"
     "          third: 32'h1000_0111\n"
     "      }\n"
     "  };\n"
     "endmodule\n"},
    {"module foo;"
     // nest three patterns
     "parameter logic [11:0] i = '{\n"
     "'{'{1,2,3},4}\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  parameter logic [11:0] i = '{\n"
     "      '{'{1, 2, 3}, 4}\n"
     "  };\n"
     "endmodule\n"},
    {"module foo;"
     // nest three patterns, expand interior
     "parameter logic [11:0] i = '{\n"
     "'{\n"
     "'{first : 32'h0000_0001,\n"
     "  second : 32'h0000_0011,\n"
     "  third: 32'h0000_0111},\n"
     "  4},\n"
     "  5,\n"
     "  '{1,2,3}\n"
     "};\n"
     "endmodule",
     "module foo;\n"
     "  parameter logic [11:0] i = '{\n"
     "      '{\n"
     "          '{\n"
     "              first : 32'h0000_0001,\n"
     "              second : 32'h0000_0011,\n"
     "              third: 32'h0000_0111\n"
     "          },\n"
     "          4\n"
     "      },\n"
     "      5,\n"
     "      '{1, 2, 3}\n"
     "  };\n"
     "endmodule\n"},
    {"module    top;"
     "foo#(  \"test\"  ) foo(  );"
     "bar#(  \"test\"  ,5) bar(  );"
     "endmodule\n",
     "module top;\n"
     "  foo #(\"test\") foo ();\n"  // module instantiation, string arg
     "  bar #(\"test\", 5) bar ();\n"
     "endmodule\n"},
    {"module    top;"
     "foo#(  `\"test`\"  ) foo(  );"
     "bar#(  `\"test`\"  ,5) bar(  );"
     "endmodule\n",
     "module top;\n"
     "  foo #(`\"test`\") foo ();\n"  // module instantiation, eval string arg
     "  bar #(`\"test`\", 5) bar ();\n"
     "endmodule\n"},
    {"`ifdef FOO\n"
     "  module bar;endmodule\n"
     "`endif\n",
     "`ifdef FOO\n"
     "module bar;\n"
     "endmodule\n"
     "`endif\n"},
    {"`ifdef FOO\n"
     "  module bar;endmodule\n"
     "`else module baz;endmodule\n"
     "`endif\n",
     "`ifdef FOO\n"
     "module bar;\n"
     "endmodule\n"
     "`else\n"
     "module baz;\n"
     "endmodule\n"
     "`endif\n"},
    {"`ifdef FOO\n"
     "  module bar;endmodule\n"
     "`else /* glue me */ module baz;endmodule\n"
     "`endif\n",
     "`ifdef FOO\n"
     "module bar;\n"
     "endmodule\n"
     "`else  /* glue me */\n"
     "module baz;\n"
     "endmodule\n"
     "`endif\n"},
    {"`ifdef FOO\n"
     "  module bar;endmodule\n"
     "`else// different unit\n"
     "  module baz;endmodule\n"
     "`endif\n",
     "`ifdef FOO\n"
     "module bar;\n"
     "endmodule\n"
     "`else  // different unit\n"
     "module baz;\n"
     "endmodule\n"
     "`endif\n"},

    // unary: + - !  ~ & | ^  ~& ~| ~^ ^~
    {"module m;foo bar(.x(-{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(-{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(!{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(!{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(~{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(~{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(&{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(&{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(|{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(|{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(^{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(^{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(~&{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(~&{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(~|{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(~|{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(~^{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(~^{a, b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(^~{a,b}));endmodule",
     "module m;\n"
     "  foo bar (.x(^~{a, b}));\n"
     "endmodule\n"},

    // binary: + - * / % & | ^ ^~ ~^ && ||
    {"module m;foo bar(.x(a+b));endmodule",
     "module m;\n"
     "  foo bar (.x(a + b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a-b));endmodule",
     "module m;\n"
     "  foo bar (.x(a - b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a*b));endmodule",
     "module m;\n"
     "  foo bar (.x(a * b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a/b));endmodule",
     "module m;\n"
     "  foo bar (.x(a / b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a%b));endmodule",
     "module m;\n"
     "  foo bar (.x(a % b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a&b));endmodule",
     "module m;\n"
     "  foo bar (.x(a & b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a|b));endmodule",
     "module m;\n"
     "  foo bar (.x(a | b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a^b));endmodule",
     "module m;\n"
     "  foo bar (.x(a ^ b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a^~b));endmodule",
     "module m;\n"
     "  foo bar (.x(a ^~ b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a~^b));endmodule",
     "module m;\n"
     "  foo bar (.x(a ~^ b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a&&b));endmodule",
     "module m;\n"
     "  foo bar (.x(a && b));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a||b));endmodule",
     "module m;\n"
     "  foo bar (.x(a || b));\n"
     "endmodule\n"},

    // {a} op {b}
    {"module m;foo bar(.x({a}+{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} + {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}-{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} - {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}*{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} * {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}/{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} / {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}%{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} % {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}&{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} & {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}|{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} | {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}^{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} ^ {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}^~{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} ^~ {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}~^{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} ~^ {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}&&{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} && {b}));\n"
     "endmodule\n"},
    {"module m;foo bar(.x({a}||{b}));endmodule",
     "module m;\n"
     "  foo bar (.x({a} || {b}));\n"
     "endmodule\n"},

    // (a) op (b)
    {"module m;foo bar(.x((a)+(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) + (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)-(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) - (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)*(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) * (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)/(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) / (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)%(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) % (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)&(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) & (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)|(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) | (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)^(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) ^ (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)^~(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) ^~ (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)~^(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) ~^ (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)&&(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) && (b)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a)||(b)));endmodule",
     "module m;\n"
     "  foo bar (.x((a) || (b)));\n"
     "endmodule\n"},

    // a[b] op c
    {"module m;foo bar(.x(a[b]+c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] + c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]-c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] - c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]*c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] * c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]/c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] / c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]%c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] % c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]&c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] & c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]|c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] | c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]^c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] ^ c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]^~c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] ^~ c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]~^c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] ~^ c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]&&c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] && c));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b]||c));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] || c));\n"
     "endmodule\n"},

    // misc
    {"module m;foo bar(.x(a[1:0]^b[2:1]));endmodule",
     "module m;\n"
     "  foo bar (.x(a[1:0] ^ b[2:1]));\n"
     "endmodule\n"},

    {"module m;foo bar(.x(a[b] | b[c]));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] | b[c]));\n"
     "endmodule\n"},
    {"module m;foo bar(.x(a[b] & b[c]));endmodule",
     "module m;\n"
     "  foo bar (.x(a[b] & b[c]));\n"
     "endmodule\n"},

    {"module m;foo bar(.x((a^c)^(b^ ~c)));endmodule",
     "module m;\n"
     "  foo bar (.x((a ^ c) ^ (b ^ ~c)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a^c)^(b^~c)));endmodule",
     "module m;\n"
     "  foo bar (.x((a ^ c) ^ (b ^~ c)));\n"
     "endmodule\n"},
    {"module m;foo bar(.x((a^{c,d})^(b^^{c,d})));endmodule",
     "module m;\n"
     "  foo bar (\n"
     "      .x((a ^ {c, d}) ^ (b ^ ^{c, d}))\n"
     "  );\n"
     "endmodule\n"},

    {// module items mixed with preprocessor conditionals and comments
     "    module foo;\n"
     "// comment1\n"
     "  `ifdef SIM\n"
     "// comment2\n"
     " `elsif SYN\n"
     " // comment3\n"
     "       `else\n"
     "// comment4\n"
     " `endif\n"
     "// comment5\n"
     "  endmodule",
     "module foo;\n"
     "  // comment1\n"
     "`ifdef SIM\n"
     "  // comment2\n"
     "`elsif SYN\n"
     "  // comment3\n"
     "`else\n"
     "  // comment4\n"
     "`endif\n"
     "  // comment5\n"
     "endmodule\n"},
    {"  module bar;wire foo;reg bear;endmodule\n",
     "module bar;\n"
     "  wire foo;\n"
     "  reg  bear;\n"  // aligned
     "endmodule\n"},
    {" module bar;initial\nbegin a<=b . c ; end endmodule\n",
     "module bar;\n"
     "  initial begin\n"
     "    a <= b.c;\n"
     "  end\n"
     "endmodule\n"},
    {"  module bar;for(genvar i = 0 ; i<N ; ++ i  ) begin end endmodule\n",
     "module bar;\n"
     "  for (genvar i = 0; i < N; ++i) begin\n"
     "  end\n"
     "endmodule\n"},
    {"  module bar;for(genvar i = 0 ; i!=N ; i ++  ) begin "
     "foo f;end endmodule\n",
     "module bar;\n"
     "  for (genvar i = 0; i != N; i++) begin\n"
     "    foo f;\n"
     "  end\n"
     "endmodule\n"},
    {
        "module block_generate;\n"
        "`ASSERT(blah)\n"
        "generate endgenerate endmodule\n",
        "module block_generate;\n"
        "  `ASSERT(blah)\n"
        "  generate\n"
        "  endgenerate\n"
        "endmodule\n",
    },
    {
        "module conditional_generate;\n"
        "if(foo)  ; \t"  // null action
        "endmodule\n",
        "module conditional_generate;\n"
        "  if (foo);\n"
        "endmodule\n",
    },
    {
        "module conditional_generate;\n"
        "if(foo[a*b+c])  ; \t"  // null action
        "endmodule\n",
        "module conditional_generate;\n"
        "  if (foo[a*b+c]);\n"  // allow compact expressions inside []
        "endmodule\n",
    },
    {
        "module conditional_generate;\n"
        "if(foo)begin\n"
        "`ASSERT()\n"
        "`COVER()\n"
        " end\n"
        "endmodule\n",
        "module conditional_generate;\n"
        "  if (foo) begin\n"
        "    `ASSERT()\n"
        "    `COVER()\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module conditional_generate;\n"
        "`ASSERT()\n"
        "if(foo)begin\n"
        " end\n"
        "`COVER()\n"
        "endmodule\n",
        "module conditional_generate;\n"
        "  `ASSERT()\n"
        "  if (foo) begin\n"
        "  end\n"
        "  `COVER()\n"
        "endmodule\n",
    },
    {
        "module conditional_generate;\n"
        "if(foo)begin\n"
        "           // comment1\n"
        " // comment2\n"
        " end\n"
        "endmodule\n",
        "module conditional_generate;\n"
        "  if (foo) begin\n"
        "    // comment1\n"
        "    // comment2\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;"
        "for(genvar i=0; ;)\n; "   // null generate statement
        "for(genvar j=0 ;; )\n; "  // null generate statement
        "endmodule",
        "module m;\n"
        "  for (genvar i = 0;;);\n"
        "  for (genvar j = 0;;);\n"
        "endmodule\n",
    },
    {
        "module m ;"
        "for (genvar f = 0; f < N; f++) begin "
        "assign x = y; assign y = z;"
        "end endmodule",
        "module m;\n"
        "  for (genvar f = 0; f < N; f++) begin\n"
        "    assign x = y;\n"
        "    assign y = z;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        // standalone genvar statement
        "module m ;"
        "genvar f;"
        "for(f=0; f<N; f ++ )begin "
        "end endmodule",
        "module m;\n"
        "  genvar f;\n"
        "  for (f = 0; f < N; f++) begin\n"
        "  end\n"
        "endmodule\n",
    },
    {
        // multiple arguments to genvar statement
        "module m ;"
        "genvar f, g;"
        "for(f=0; f<N; f ++ )begin "
        "end for(g=N; g>0; g -- )begin "
        "end endmodule",
        "module m;\n"
        "  genvar f, g;\n"
        "  for (f = 0; f < N; f++) begin\n"
        "  end\n"
        "  for (g = N; g > 0; g--) begin\n"
        "  end\n"
        "endmodule\n",
    },
    {
        // multiple genvar statements
        "module m ;"
        "genvar f;"
        "genvar g;"
        "for(f=0; f<N; f ++ )begin "
        "end for(g=N; g>0; g -- )begin "
        "end endmodule",
        "module m;\n"
        "  genvar f;\n"
        "  genvar g;\n"
        "  for (f = 0; f < N; f++) begin\n"
        "  end\n"
        "  for (g = N; g > 0; g--) begin\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module event_control ;"
        "always@ ( posedge   clk )z<=y;"
        "endmodule\n",
        "module event_control;\n"
        "  always @(posedge clk) z <= y;\n"
        "endmodule\n",
    },
    {
        "module always_if ;"
        "always@ ( posedge   clk ) if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @(posedge clk)\n"  // doesn't fit
        "    if (expr)\n"
        "      z <= y;\n"
        "endmodule\n",
    },
    {
        "module always_if ;"
        "always@*  if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @* if (expr) z <= y;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else ;"
        "always@*  if (expr) z<=y; else g<=0;"
        "endmodule\n",
        "module always_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"  // fits
        "    else g <= 0;\n"       // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;"
        "endmodule\n",
        "module always_if_else_if;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if_else ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
        "endmodule\n",
        "module always_if_else_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "    else h <= 1;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(b,  c)"
        "  for (;;)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(b, c) for (;;) s = y;\n"  // fits
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  for (i=0;i<k;++i)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    for (i = 0; i < k; ++i)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  repeat (jj+kk)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    repeat (jj + kk)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  foreach(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    foreach (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  while(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    while (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  do s=y;while(jj[kk]);\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    do\n"
        "      s = y;\n"
        "    while (jj[kk]);\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)  \n"
        "  case(jj)\tS:s = y;endcase\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    case (jj)\n"
        "      S: s = y;\n"
        "    endcase\n"
        "endmodule\n",
    },

    {
        "module always_if ;"
        "always@ ( posedge   clk ) if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @(posedge clk)\n"  // doesn't fit
        "    if (expr)\n"
        "      z <= y;\n"
        "endmodule\n",
    },
    {
        "module always_if ;"
        "always@*  if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @* if (expr) z <= y;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else ;"
        "always@*  if (expr) z<=y; else g<=0;"
        "endmodule\n",
        "module always_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"  // fits
        "    else g <= 0;\n"       // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;"
        "endmodule\n",
        "module always_if_else_if;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if_else ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
        "endmodule\n",
        "module always_if_else_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "    else h <= 1;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(b,  c)"
        "  for (;;)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(b, c) for (;;) s = y;\n"  // fits
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  for (i=0;i<k;++i)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    for (i = 0; i < k; ++i)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  repeat (jj+kk)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    repeat (jj + kk)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  foreach(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    foreach (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  while(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    while (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  do s=y;while(jj[kk]);\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    do\n"
        "      s = y;\n"
        "    while (jj[kk]);\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)  \n"
        "  case(jj)\tS:s = y;endcase\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    case (jj)\n"
        "      S: s = y;\n"
        "    endcase\n"
        "endmodule\n",
    },

    {
        "module always_if ;"
        "always@ ( posedge   clk ) if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @(posedge clk)\n"  // doesn't fit
        "    if (expr)\n"
        "      z <= y;\n"
        "endmodule\n",
    },
    {
        "module always_if ;"
        "always@*  if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @* if (expr) z <= y;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else ;"
        "always@*  if (expr) z<=y; else g<=0;"
        "endmodule\n",
        "module always_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"  // fits
        "    else g <= 0;\n"       // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;"
        "endmodule\n",
        "module always_if_else_if;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if_else ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
        "endmodule\n",
        "module always_if_else_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "    else h <= 1;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(b,  c)"
        "  for (;;)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(b, c) for (;;) s = y;\n"  // fits
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  for (i=0;i<k;++i)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    for (i = 0; i < k; ++i)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  repeat (jj+kk)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    repeat (jj + kk)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  foreach(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    foreach (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  while(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    while (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  do s=y;while(jj[kk]);\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    do\n"
        "      s = y;\n"
        "    while (jj[kk]);\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)  \n"
        "  case(jj)\tS:s = y;endcase\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    case (jj)\n"
        "      S: s = y;\n"
        "    endcase\n"
        "endmodule\n",
    },

    {
        "module always_if ;"
        "always@ ( posedge   clk ) if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @(posedge clk)\n"  // doesn't fit
        "    if (expr)\n"
        "      z <= y;\n"
        "endmodule\n",
    },
    {
        "module always_if ;"
        "always@*  if (expr) z<=y;"
        "endmodule\n",
        "module always_if;\n"
        "  always @* if (expr) z <= y;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else ;"
        "always@*  if (expr) z<=y; else g<=0;"
        "endmodule\n",
        "module always_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"  // fits
        "    else g <= 0;\n"       // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;"
        "endmodule\n",
        "module always_if_else_if;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "endmodule\n",
    },
    {
        "module \talways_if_else_if_else ;"
        "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
        "endmodule\n",
        "module always_if_else_if_else;\n"
        "  always @*\n"
        "    if (expr) z <= y;\n"    // fits
        "    else if (w) g <= 0;\n"  // fits
        "    else h <= 1;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(b,  c)"
        "  for (;;)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(b, c) for (;;) s = y;\n"  // fits
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  for (i=0;i<k;++i)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    for (i = 0; i < k; ++i)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  repeat (jj+kk)\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    repeat (jj + kk)\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  foreach(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    foreach (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  while(jj[kk])\ts = y;"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    while (jj[kk])\n"
        "      s = y;\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)"
        "  do s=y;while(jj[kk]);\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    do\n"
        "      s = y;\n"
        "    while (jj[kk]);\n"
        "endmodule\n",
    },
    {
        "module m;\n"
        "always @(posedge clk)  \n"
        "  case(jj)\tS:s = y;endcase\t"
        "endmodule",
        "module m;\n"
        "  always @(posedge clk)\n"
        "    case (jj)\n"
        "      S: s = y;\n"
        "    endcase\n"
        "endmodule\n",
    },

    {
        // begin/end with labels
        "module m ;initial  begin:yyy\tend:yyy endmodule",
        "module m;\n"
        "  initial begin : yyy\n"
        "  end : yyy\n"
        "endmodule\n",
    },
    {
        // conditional generate begin/end with labels
        "module m ;if\n( 1)  begin:yyy\tend:yyy endmodule",
        "module m;\n"
        "  if (1) begin : yyy\n"
        "  end : yyy\n"
        "endmodule\n",
    },
    {
        // begin/end with labels, nested
        "module m ;initial  begin:yyy if(1)begin:zzz "
        "end:zzz\tend:yyy endmodule",
        "module m;\n"
        "  initial begin : yyy\n"
        "    if (1) begin : zzz\n"
        "    end : zzz\n"
        "  end : yyy\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin #  1 x<=y ;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    #1 x <= y;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin x<=y ;  y<=z;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    x <= y;\n"
        "    y <= z;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin # 10 x<=y ;  # 20  y<=z;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    #10 x <= y;\n"
        "    #20 y <= z;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        // qualified variables
        "module m ;initial  begin automatic int a; "
        " static byte s=0;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    automatic int a;\n"
        "    static byte   s = 0;\n"  // aligned
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin automatic int a,b; "
        " static byte s,t;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    automatic int a, b;\n"
        "    static byte s, t;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin   static byte a=1,b=0;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    static byte a = 1, b = 0;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin   const int a=0;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    const int a = 0;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin automatic   const int a=0;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    automatic const int a = 0;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin const  var automatic  int a=0;end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    const var automatic int a = 0;\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin static byte s  ={<<{a}};end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    static byte s = {<<{a}};\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m ;initial  begin static int s  ={>>4{a}};end endmodule",
        "module m;\n"
        "  initial begin\n"
        "    static int s = {>>4{a}};\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m; final  assert   (expr ) ;endmodule",
        "module m;\n"
        "  final assert (expr);\n"
        "endmodule\n",
    },
    {
        "module m; final  begin\tassert   (expr ) ;end  endmodule",
        "module m;\n"
        "  final begin\n"
        "    assert (expr);\n"
        "  end\n"
        "endmodule\n",
    },
    {
        "module m; final  assume   (expr ) ;endmodule",
        "module m;\n"
        "  final assume (expr);\n"
        "endmodule\n",
    },
    {
        "module m; final  cover   (expr ) ;endmodule",
        "module m;\n"
        "  final cover (expr);\n"
        "endmodule\n",
    },
    {
        // two consecutive clocking declarations in modules
        " module mcd ; "
        "clocking   cb @( posedge clk);\t\tendclocking "
        "clocking cb2   @ (posedge  clk\n); endclocking endmodule",
        "module mcd;\n"
        "  clocking cb @(posedge clk);\n"
        "  endclocking\n"
        "  clocking cb2 @(posedge clk);\n"
        "  endclocking\n"
        "endmodule\n",
    },
    {
        // two consecutive clocking declarations in modules, with end labels
        " module mcd ; "
        "clocking   cb @( posedge clk);\t\tendclocking:  cb "
        "clocking cb2   @ (posedge  clk\n); endclocking   :cb2 endmodule",
        "module mcd;\n"
        "  clocking cb @(posedge clk);\n"
        "  endclocking : cb\n"
        "  clocking cb2 @(posedge clk);\n"
        "  endclocking : cb2\n"
        "endmodule\n",
    },
    {
        // clocking declarations with ports in modules
        " module mcd ; "
        "clocking cb   @ (posedge  clk\n); input a; output b; endclocking "
        "endmodule",
        "module mcd;\n"
        "  clocking cb @(posedge clk);\n"
        "    input a;\n"
        "    output b;\n"
        "  endclocking\n"
        "endmodule\n",
    },
    {
        // DPI import declarations in modules
        "module mdi;"
        "import   \"DPI-C\" function  int add(\n) ;"
        "import \"DPI-C\"\t\tfunction int\nsleep( input int secs );"
        "import \"DPI-C\"\t\tfunction int\nwake( input int secs, output bit "
        "[2:0] z);"
        "endmodule",
        "module mdi;\n"
        "  import \"DPI-C\" function int add();\n"
        "  import \"DPI-C\" function int sleep(\n"
        "    input int secs\n"
        "  );\n"
        "  import \"DPI-C\" function int wake(\n"
        "    input  int       secs,\n"
        "    output bit [2:0] z\n"
        "  );\n"
        "endmodule\n",
    },
    {
        // DPI export declarations in modules
        "module m;"
        "export \"DPI-C\" function get;"
        "export \"DPI-C\" function mhpmcounter_get;\n"
        "export \"DPI-C\"\t\tfunction int\nwake( input int secs, output bit "
        "[2:0] z);"
        "endmodule",
        "module m;\n"
        "  export \"DPI-C\" function get;\n"
        "  export \"DPI-C\"\n"
        "      function mhpmcounter_get;\n"  // doesn't fit in 40-col
        "  export \"DPI-C\" function int wake(\n"
        "    input  int       secs,\n"
        "    output bit [2:0] z\n"
        "  );\n"
        "endmodule\n",
    },
    {"import \"DPI-C\" context function void func(input bit impl_i,"
     "input bit op_i,"
     "input bit [5:0] mode_i,"
     "input bit [3:0][31:0] iv_i,"
     "input bit [2:0] key_len_i,"
     "input bit [7:0][31:0] key_i,"
     "input bit [7:0] data_i[],"
     "output bit [7:0] data_o[]);",
     "import \"DPI-C\" context\n"
     "    function void func(\n"
     "  input  bit             impl_i,\n"
     "  input  bit             op_i,\n"
     "  input  bit [5:0]       mode_i,\n"
     "  input  bit [3:0][31:0] iv_i,\n"
     "  input  bit [2:0]       key_len_i,\n"
     "  input  bit [7:0][31:0] key_i,\n"
     "  input  bit [7:0]       data_i   [],\n"
     "  output bit [7:0]       data_o   []\n"
     ");\n"},
    {// module with system task call
     "module m; initial begin #10 $display(\"foo\"); $display(\"bar\");"
     "end endmodule",
     "module m;\n"
     "  initial begin\n"
     "    #10 $display(\"foo\");\n"
     "    $display(\"bar\");\n"
     "  end\n"
     "endmodule\n"},

    // interface test cases
    {// two interface declarations
     " interface if1 ; endinterface\t\t"
     "interface  if2; endinterface   ",
     "interface if1;\n"
     "endinterface\n"
     "interface if2;\n"
     "endinterface\n"},
    {// two interface declarations, end labels
     " interface if1 ; endinterface:if1\t\t"
     "interface  if2; endinterface    :  if2   ",
     "interface if1;\n"
     "endinterface : if1\n"
     "interface if2;\n"
     "endinterface : if2\n"},
    {// interface declaration with parameters
     " interface if1#( parameter int W= 8 );endinterface\t\t",
     "interface if1 #(\n"
     "    parameter int W = 8\n"
     ");\n"
     "endinterface\n"},
    {// interface declaration with ports (empty)
     " interface if1()\n;endinterface\t\t",
     "interface if1 ();\n"
     "endinterface\n"},
    {// interface declaration with parameter comment only, empty ports
     " interface if1#( \n"
     "//param\n"
     ")();endinterface\t\t",
     "interface if1 #(\n"
     "    //param\n"
     ") ();\n"
     "endinterface\n"},
    {// interface declaration with parameter, empty ports
     " interface if1#( parameter int W= 8 )();endinterface\t\t",
     "interface if1 #(\n"
     "    parameter int W = 8\n"
     ") ();\n"
     "endinterface\n"},
    {// interface declaration with ports
     " interface if1( input\tlogic   z)\n;endinterface\t\t",
     "interface if1 (\n"
     "    input logic z\n"
     ");\n"
     "endinterface\n"},
    {// interface declaration with multiple ports
     " interface if1( input\tlogic   z, output logic a)\n;endinterface\t\t",
     "interface if1 (\n"
     "    input  logic z,\n"  // should be one-per-line, even it it fits
     "    output logic a\n"   // aligned
     ");\n"
     "endinterface\n"},
    {// interface declaration with parameters and ports
     " interface if1#( parameter int W= 8 )(input logic z);endinterface\t\t",
     // doesn't fit on one line
     "interface if1 #(\n"
     "    parameter int W = 8\n"
     ") (\n"
     "    input logic z\n"
     ");\n"
     "endinterface\n"},
    {
        // interface with modport declarations
        "interface\tfoo_if  ;"
        "modport  mp1\t( output a, input b);"
        "modport\tmp2  (output c,input d );\t"
        "endinterface",
        "interface foo_if;\n"
        "  modport mp1(output a, input b);\n"
        "  modport mp2(output c, input d);\n"
        "endinterface\n",
    },
    {
        // interface with long modport port names
        "interface\tfoo_if  ;"
        "modport  mp1\t( output a_long_output, input detailed_input_name);"
        "endinterface",
        "interface foo_if;\n"
        "  modport mp1(\n"
        "      output a_long_output,\n"
        "      input detailed_input_name\n"
        "  );\n"
        "endinterface\n",
    },
    {
        // interface with modport declaration with multiple ports
        "interface\tfoo_if  ;"
        "modport  mp1\t( output a_long_output, input detailed_input_name);"
        "endinterface",
        "interface foo_if;\n"
        "  modport mp1(\n"
        "      output a_long_output,\n"
        "      input detailed_input_name\n"
        "  );\n"
        "endinterface\n",
    },
    {
        // interface with modport TF port declaration
        "interface\tfoo_if  ;"
        "modport  mp1\t( output a, input b, import c);"
        "endinterface",
        "interface foo_if;\n"
        "  modport mp1(\n"
        "      output a,\n"
        "      input b,\n"
        "      import c\n"
        "  );\n"
        "endinterface\n",
    },
    {
        // interface with complex modport ports list
        "interface\tfoo_if  ;"
        "modport producer\t(input ready,\toutput data, valid, user,"
        " strobe, keep, last,\timport producer_reset, producer_tick);"
        "modport consumer\t(input data, valid, user, strobe, keep, last,"
        " output ready,\timport consumer_reset, consumer_tick, consume);"
        "endinterface",
        "interface foo_if;\n"
        "  modport producer(\n"
        "      input ready,\n"
        "      output data, valid, user, strobe,\n"
        "          keep, last,\n"
        "      import producer_reset,\n"
        "          producer_tick\n"
        "  );\n"
        "  modport consumer(\n"
        "      input data, valid, user, strobe,\n"
        "          keep, last,\n"
        "      output ready,\n"
        "      import consumer_reset,\n"
        "          consumer_tick, consume\n"
        "  );\n"
        "endinterface\n",
    },
    {
        // interface with modports and comments inside
        "interface foo_if;\n"
        " modport mp1(\n"
        "  // Our output\n"
        "     output a,\n"
        "  /* Inputs */\n"
        "      input b1, b_f /*last*/,"
        "  import c\n"
        "  );\n"
        "endinterface\n",
        "interface foo_if;\n"
        "  modport mp1(\n"
        "      // Our output\n"
        "      output a,\n"
        "      /* Inputs */\n"
        "      input b1, b_f  /*last*/,\n"
        "      import c\n"
        "  );\n"
        "endinterface\n",
    },

    // class test cases
    {"class action;int xyz;endclass  :  action\n",
     "class action;\n"
     "  int xyz;\n"
     "endclass : action\n"},
    {"class action  extends mypkg :: inaction;endclass  :  action\n",
     "class action extends mypkg::inaction;\n"  // tests for spacing around ::
     "endclass : action\n"},
    {"class c;function new;endfunction endclass",
     "class c;\n"
     "  function new;\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;function new ( );endfunction endclass",
     "class c;\n"
     "  function new();\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;function new ( string s );endfunction endclass",
     "class c;\n"
     "  function new(string s);\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;function new ( string s ,int i );endfunction endclass",
     "class c;\n"
     "  function new(string s, int i);\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;function void f;endfunction endclass",
     "class c;\n"
     "  function void f;\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;virtual function void f;endfunction endclass",
     "class c;\n"
     "  virtual function void f;\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;function int f ( );endfunction endclass",
     "class c;\n"
     "  function int f();\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;function int f ( int  ii );endfunction endclass",
     "class c;\n"
     "  function int f(int ii);\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;function int f ( int  ii ,bit  bb );endfunction endclass",
     "class c;\n"
     "  function int f(int ii, bit bb);\n"
     "  endfunction\n"
     "endclass\n"},
    {"class c;task t ;endtask endclass",
     "class c;\n"
     "  task t;\n"
     "  endtask\n"
     "endclass\n"},
    {"class c;task t ( int  ii ,bit  bb );endtask endclass",
     "class c;\n"
     "  task t(int ii, bit bb);\n"
     "  endtask\n"
     "endclass\n"},
    {"class c; task automatic repeated_assigner;"
     "repeat (count) y = w;"  // single statement body
     "endtask endclass",
     "class c;\n"
     "  task automatic repeated_assigner;\n"
     "    repeat (count) y = w;\n"
     "  endtask\n"
     "endclass\n"},
    {"class c; task automatic delayed_assigner;"
     "#   100   y = w;"  // delayed assignment
     "endtask endclass",
     "class c;\n"
     "  task automatic delayed_assigner;\n"
     "    #100 y = w;\n"
     "  endtask\n"
     "endclass\n"},
    {"class c; task automatic labeled_assigner;"
     "lbl   :   y = w;"  // delayed assignment
     "endtask endclass",
     "class c;\n"
     "  task automatic labeled_assigner;\n"
     "    lbl : y = w;\n"  // TODO(fangism): no space before ':'
     "  endtask\n"
     "endclass\n"},
    // tasks with control statements
    {"class c; task automatic waiter;"
     "if (count == 0) begin #0; return;end "
     "endtask endclass",
     "class c;\n"
     "  task automatic waiter;\n"
     "    if (count == 0) begin\n"
     "      #0;\n"
     "      return;\n"
     "    end\n"
     "  endtask\n"
     "endclass\n"},
    {"class c; task automatic heartbreaker;"
     "if( c)if( d) break ;"
     "endtask endclass",
     "class c;\n"
     "  task automatic heartbreaker;\n"
     "    if (c) if (d) break;\n"
     "  endtask\n"
     "endclass\n"},
    {"class c; task automatic waiter;"
     "repeat (count) @(posedge clk);"
     "endtask endclass",
     "class c;\n"
     "  task automatic waiter;\n"
     "    repeat (count) @(posedge clk);\n"
     "  endtask\n"
     "endclass\n"},
    {"class c; task automatic repeat_assigner;"
     "repeat( r )\ny = w;"
     "repeat( q )\ny = 1;"
     "endtask endclass",
     "class c;\n"
     "  task automatic repeat_assigner;\n"
     "    repeat (r) y = w;\n"
     "    repeat (q) y = 1;\n"
     "  endtask\n"
     "endclass\n"},
    {"class c; task automatic event_control_assigner;"
     "@ ( posedge clk )\ny = w;"
     "@ ( negedge clk )\nz = w;"
     "endtask endclass",
     "class c;\n"
     "  task automatic event_control_assigner;\n"
     "    @(posedge clk) y = w;\n"
     "    @(negedge clk) z = w;\n"
     "  endtask\n"
     "endclass\n"},
    {
        // classes with surrrounding comments
        // vertical spacing preserved
        "\n// pre-c\n\n"
        "  class   c  ;\n"
        "// c stuff\n"
        "endclass\n"
        "  // pre-d\n"
        "\n\nclass d ;\n"
        " // d stuff\n"
        "endclass\n"
        "\n// the end\n",
        "\n// pre-c\n\n"
        "class c;\n"
        "  // c stuff\n"
        "endclass\n"
        "// pre-d\n\n\n"
        "class d;\n"
        "  // d stuff\n"
        "endclass\n\n"
        "// the end\n",
    },
    {// class with comments around task/function declarations
     "class c;      // c is for cookie\n"
     "    // f is for false\n"
     "\tfunction f(integer size) ; endfunction\n"
     " // t is for true\n"
     "task t();endtask\n"
     " // class is about to end\n"
     "endclass",
     "class c;  // c is for cookie\n"
     "  // f is for false\n"
     "  function f(integer size);\n"
     "  endfunction\n"
     "  // t is for true\n"
     "  task t();\n"
     "  endtask\n"
     "  // class is about to end\n"
     "endclass\n"},
    // class property alignment test cases
    {"class c;\n"
     "int foo  ;\n"
     "byte bar;\n"
     "endclass : c\n",
     "class c;\n"
     "  int  foo;\n"
     "  byte bar;\n"
     "endclass : c\n"},
    {"class c;\n"
     "int foo;\n"
     "const bit b;\n"
     "endclass : c\n",
     "class c;\n"
     "  int       foo;\n"
     "  const bit b;\n"
     "endclass : c\n"},
    {"class c;\n"
     "rand logic l;\n"
     "int foo;\n"
     "endclass : c\n",
     "class c;\n"
     "  rand logic l;\n"
     "  int        foo;\n"
     "endclass : c\n"},
    {"class c;\n"
     "rand logic l;\n"
     "const static int foo;\n"  // more qualifiers
     "endclass : c\n",
     "class c;\n"
     "  rand logic       l;\n"
     "  const static int foo;\n"
     "endclass : c\n"},
    {"class c;\n"
     "static local int foo;\n"
     "const bit b;\n"
     "endclass : c\n",
     "class c;\n"
     "  static local int foo;\n"
     "  const bit        b;\n"
     "endclass : c\n"},
    {// example with queue
     "class c;\n"
     "int foo [$] ;\n"
     "int foo_bar ;\n"
     "endclass : c\n",
     "class c;\n"
     "  int foo     [$];\n"
     "  int foo_bar;\n"
     "endclass : c\n"},
    {// subcolumns
     "class cc;\n"
     "rand bit [A-1:0] foo;\n"
     "rand bit [A-1:0][2] bar;\n"
     "int foobar[X+1:Y];\n"
     "int baz[42];\n"
     "rand bit qux[Z];\n"
     "rand bit [1:0] quux[3:0];\n"
     "rand bit [A:BB][42] quuz[7];\n"
     "endclass\n",
     "class cc;\n"
     "  rand bit [A-1: 0]     foo;\n"
     "  rand bit [A-1: 0][ 2] bar;\n"
     "  int                   foobar[X+1:Y];\n"
     "  int                   baz   [   42];\n"
     "  rand bit              qux   [    Z];\n"
     "  rand bit [  1: 0]     quux  [  3:0];\n"
     "  rand bit [  A:BB][42] quuz  [    7];\n"
     "endclass\n"},
    {"class cc;\n"
     "int qux[2];\n"
     "int quux[SIZE-1+SHIFT:SHIFT];\n"
     "int quuz[SOME_CONSTANT];\n"
     "endclass\n",
     "class cc;\n"
     "  int qux [                 2];\n"
     "  int quux[SIZE-1+SHIFT:SHIFT];\n"
     "  int quuz[     SOME_CONSTANT];\n"
     "endclass\n"},
    {// aligns over comments (ignored)
     "class c;\n"
     "// foo is...\n"
     "int foo;\n"
     "// b is...\n"
     "const bit b;\n"
     " // llama is...\n"
     "logic llama;\n"
     "endclass : c\n",
     "class c;\n"
     "  // foo is...\n"
     "  int       foo;\n"
     "  // b is...\n"
     "  const bit b;\n"
     "  // llama is...\n"
     "  logic     llama;\n"
     "endclass : c\n"},
    {// aligns over comments (ignored), even with blank lines
     "class c;\n"
     "// foo is...\n"
     "int foo;\n"
     "\n"
     "// b is...\n"
     "const bit b;\n"
     "\n"
     " // llama is...\n"
     "logic llama;\n"
     "endclass : c\n",
     "class c;\n"
     "  // foo is...\n"
     "  int       foo;\n"
     "\n"
     "  // b is...\n"
     "  const bit b;\n"
     "\n"
     "  // llama is...\n"
     "  logic     llama;\n"
     "endclass : c\n"},
    {"class c;\n"
     "rand logic l;\n"
     "int [1:0] foo;\n"
     "endclass : c\n",
     "class c;\n"
     "  rand logic       l;\n"
     "  int        [1:0] foo;\n"
     "endclass : c\n"},
    {// non-data-declarations break up groups
     "class c;\n"
     "rand logic l;\n"
     "int foo;\n"
     "`uvm_bar_foo()\n"
     "logic k;\n"
     "rand int bar;\n"
     "endclass : c\n",
     "class c;\n"
     "  rand logic l;\n"
     "  int        foo;\n"
     "  `uvm_bar_foo()\n"  // separates alignment groups above/below
     "  logic    k;\n"
     "  rand int bar;\n"
     "endclass : c\n"},
    {// non-data-declarations break up groups
     "class c;\n"
     "logic k;\n"
     "rand int bar;\n"
     "function void f();\n"
     "endfunction\n"
     "rand logic l;\n"
     "int foo;\n"
     "endclass : c\n",
     "class c;\n"
     "  logic    k;\n"
     "  rand int bar;\n"
     "  function void f();\n"  // function declaration breaks groups
     "  endfunction\n"
     "  rand logic l;\n"
     "  int        foo;\n"
     "endclass : c\n"},
    {// align single-value initializers at the '='
     "class c;\n"
     "const logic foo=0;\n"
     "const bit b=1;\n"
     "endclass : c\n",
     "class c;\n"
     "  const logic foo = 0;\n"
     "  const bit   b   = 1;\n"
     "endclass : c\n"},
    {// align single-value initializers at the '=', over non-initialized
     "class c;\n"
     "const logic foo=0;\n"
     "rand int iidrv;\n"
     "const bit b=1;\n"
     "endclass : c\n",
     "class c;\n"
     "  const logic foo    = 0;\n"
     "  rand int    iidrv;\n"  // no initializer, but align across this
     "  const bit   b      = 1;\n"
     "endclass : c\n"},

    // constraint test cases
    {
        "class foo; constraint c1_c{ } endclass",
        "class foo;\n"
        "  constraint c1_c {}\n"
        "endclass\n",
    },
    {
        "class foo; constraint c1_c{  } constraint c2_c{ } endclass",
        "class foo;\n"
        "  constraint c1_c {}\n"
        "  constraint c2_c {}\n"
        "endclass\n",
    },
    {
        "class foo; constraint c1_c{soft z==y;unique{baz};}endclass",
        "class foo;\n"
        "  constraint c1_c {\n"
        "    soft z == y;\n"
        "    unique {baz};\n"
        "  }\n"
        "endclass\n",
    },
    {
        "class foo; constraint c1_c{ //comment1\n"
        "//comment2\n"
        "//comment3\n"
        "} endclass",
        "class foo;\n"
        "  constraint c1_c {  //comment1\n"
        "    //comment2\n"
        "    //comment3\n"
        "  }\n"
        "endclass\n",
    },

    {"class foo;constraint c { "
     "timer_enable dist { [ 8'h0 : 8'hfe ] :/ 90 , 8'hff :/ 10 }; "
     "} endclass\n",
     "class foo;\n"
     "  constraint c {\n"
     "    timer_enable dist {\n"
     "      [8'h0 : 8'hfe] :/ 90,\n"
     "      8'hff          :/ 10\n"  // aligned
     "    };\n"
     "  }\n"
     "endclass\n"},

    {
        "class Foo; constraint if_c { if (z) { soft x == y; } } endclass\n",
        "class Foo;\n"
        "  constraint if_c {\n"
        "    if (z) {\n"
        "      soft x == y;\n"
        "    }\n"
        "  }\n"
        "endclass\n",
    },
    {
        "class Foo; constraint if_c { if (z) {\n"
        "//comment-a\n"
        "soft x == y;\n"
        "//comment-b\n"
        "} } endclass\n",
        "class Foo;\n"
        "  constraint if_c {\n"
        "    if (z) {\n"
        "      //comment-a\n"  // properly indented
        "      soft x == y;\n"
        "      //comment-b\n"  // properly indented
        "    }\n"
        "  }\n"
        "endclass\n",
    },
    {
        "class c; "
        "constraint c_has_config_error {"
        "if (yyy) {zzzz == 1;} else {yyyyyyy == 0;}} "
        "endclass",
        "class c;\n"
        "  constraint c_has_config_error {\n"
        "    if (yyy) {\n"
        "      zzzz == 1;\n"
        "    } else {\n"
        "      yyyyyyy == 0;\n"
        "    }\n"
        "  }\n"
        "endclass\n",
    },
    // distributions: colon alignment
    {"class c;\n"
     "constraint co {\n"
     "d dist {\n"
     "[1:2]:/2,\n"
     "[11:33]:/22,\n"
     "[111:444]:/8,\n"
     "[1:42]:/10,\n"
     "[11:12]:/3\n"
     "};\n"
     "}\n"
     "endclass\n",
     "class c;\n"
     "  constraint co {\n"
     "    d dist {\n"
     "      [  1 :   2] :/ 2,\n"
     "      [ 11 :  33] :/ 22,\n"
     "      [111 : 444] :/ 8,\n"
     "      [  1 :  42] :/ 10,\n"
     "      [ 11 :  12] :/ 3\n"
     "    };\n"
     "  }\n"
     "endclass\n"},
    // distributions: subcolumns
    {"class foo;\n"
     "constraint bar {\n"
     "baz dist {\n"
     "[1:2]:/2,\n"
     "QUX[3:0]:/10,\n"
     "[11:33]:/22,\n"
     "ID_LONGER_THAN_RANGES:/3,\n"
     "[111:QUUZ[Z]]:/8,\n"
     "[X[4:0]:Y[8:Z-2]]:/8\n"
     "};\n"
     "}\n"
     "endclass\n",
     "class foo;\n"
     "  constraint bar {\n"
     "    baz dist {\n"
     "      [     1 :        2]   :/ 2,\n"
     "      QUX[3:0]              :/ 10,\n"
     "      [    11 :       33]   :/ 22,\n"
     "      ID_LONGER_THAN_RANGES :/ 3,\n"
     "      [   111 :  QUUZ[Z]]   :/ 8,\n"
     "      [X[4:0] : Y[8:Z-2]]   :/ 8\n"
     "    };\n"
     "  }\n"
     "endclass\n"},
    // class with empty parameter list
    {"class foo #(); endclass",
     "class foo #();\n"
     "endclass\n"},
    // class with empty parameter list, with comment
    {"class foo #(  \n"
     "// comment\n"
     "); endclass",
     "class foo #(\n"
     "    // comment\n"
     ");\n"
     "endclass\n"},
    // class with empty parameter list, extends
    {"class foo #()extends bar ; endclass",
     "class foo #() extends bar;\n"
     "endclass\n"},
    // class extends from type with named parameters
    {"class foo extends bar #(.N(N), .M(M)); endclass",
     "class foo extends bar #(\n"
     "    .N(N),\n"
     "    .M(M)\n"
     ");\n"
     "endclass\n"},

    // class with one parameter list
    {"class foo #(type a = b); endclass",
     "class foo #(\n"
     "    type a = b\n"
     ");\n"
     "endclass\n"},

    // class with multiple paramter list
    {"class foo #(type a = b, type c = d, type e = f); endclass",
     "class foo #(\n"
     "    type a = b,\n"
     "    type c = d,\n"
     "    type e = f\n"
     ");\n"
     "endclass\n"},

    // class with data members
    {"class  i_love_data ;const\ninteger  sizer\t;endclass",
     "class i_love_data;\n"
     "  const integer sizer;\n"
     "endclass\n"},
    {"class  i_love_data ;const\ninteger  sizer=3\t;endclass",
     "class i_love_data;\n"
     "  const integer sizer = 3;\n"
     "endclass\n"},
    {"class  i_love_data ;protected\nint  count  \t;endclass",
     "class i_love_data;\n"
     "  protected int count;\n"
     "endclass\n"},
    {"class  i_love_data ;\t\nint  counter\n ;int  countess \t;endclass",
     "class i_love_data;\n"
     "  int counter;\n"
     "  int countess;\n"
     "endclass\n"},
    {"class  i_love_params ;foo#( . bar)  baz\t;endclass",
     "class i_love_params;\n"
     "  foo #(.bar) baz;\n"
     "endclass\n"},
    {"class  i_love_params ;foo#( . bar ( bah ))  baz\t;endclass",
     "class i_love_params;\n"
     "  foo #(.bar(bah)) baz;\n"
     "endclass\n"},
    {"class  i_love_params ;foo#( . bar ( bah\n),"
     ".\ncat( dog) )  baz\t;endclass",
     "class i_love_params;\n"
     "  foo #(\n"
     "      .bar(bah),\n"
     "      .cat(dog)\n"
     "  ) baz;\n"
     "endclass\n"},
    {"class  i_love_params ;foo#( . bar)  baz1,baz2\t;endclass",
     "class i_love_params;\n"
     "  foo #(.bar) baz1, baz2;\n"
     "endclass\n"},
    {"class  i_love_params ;foo#( . bar)  baz\t;baz#(.foo)bar;endclass",
     "class i_love_params;\n"
     "  foo #(.bar) baz;\n"
     "  baz #(.foo) bar;\n"
     "endclass\n"},
    {"class i_love_params // comment\n"
     ";\n"
     "foo#(\n"
     ".foobar(quuuuux) // comment\n"
     ", .cat(dog)\n"
     ") baz // comment\n"
     ";endclass\n",
     "class i_love_params  // comment\n"
     ";\n"
     "  foo #(\n"
     "        .foobar(quuuuux)  // comment\n"
     "      , .cat   (dog)\n"
     "  ) baz  // comment\n"
     "  ;\n"
     "endclass\n"},

    // typedef test cases
    {"typedef enum logic\t{ A=0, B=1 }foo_t;",
     "typedef enum logic {\n"
     "  A = 0,\n"
     "  B = 1\n"
     "} foo_t;\n"},
    {"typedef enum uint8_t\t{ kA=8'b0, kB=8'b1 }foo_t;",
     "typedef enum uint8_t {\n"  // uint8_t is user-defined
     "  kA = 8'b0,\n"
     "  kB = 8'b1\n"
     "} foo_t;\n"},
    {// With comments on same line as enum value
     "typedef enum logic\t{ A=0, // foo\n"
     "B,// bar\n"
     "`ifndef DO_PANIC\n"
     "C=42,// answer\n"
     "`endif\n"
     "D=3    // baz\n"
     "}foo_t;",
     "typedef enum logic {\n"
     "  A = 0,   // foo\n"
     "  B,       // bar\n"
     "`ifndef DO_PANIC\n"
     "  C = 42,  // answer\n"
     "`endif\n"
     "  D = 3    // baz\n"
     "} foo_t;\n"},
    {// with scalar dimensions
     "typedef enum logic[2]\t{ A=0, B=1 }foo_t;",
     "typedef enum logic [2] {\n"
     "  A = 0,\n"
     "  B = 1\n"
     "} foo_t;\n"},
    {// with range dimensions
     "typedef enum logic[1:0]\t{ A=0, B=1 }foo_t;",
     "typedef enum logic [1:0] {\n"
     "  A = 0,\n"
     "  B = 1\n"
     "} foo_t;\n"},
    {"typedef foo_pkg::baz_t#(.L(L), .W(W)) bar_t;\n",
     "typedef foo_pkg::baz_t#(\n"
     "    .L(L),\n"
     "    .W(W)\n"
     ") bar_t;\n"},

    // package test cases
    {"package fedex;localparam  int  www=3 ;endpackage   :  fedex\n",
     "package fedex;\n"
     "  localparam int www = 3;\n"
     "endpackage : fedex\n"},
    {"package   typey ;"
     "typedef enum int{ A=0, B=1 }foo_t;"
     "typedef enum{ C=0, D=1 }bar_t;"
     "endpackage:typey\n",
     "package typey;\n"
     "  typedef enum int {\n"
     "    A = 0,\n"
     "    B = 1\n"
     "  } foo_t;\n"
     "  typedef enum {\n"
     "    C = 0,\n"
     "    D = 1\n"
     "  } bar_t;\n"
     "endpackage : typey\n"},
    {// net type declarations
     "package foo_pkg;"
     "nettype shortreal\t\tfoo  ;"
     "nettype\nbar[1:0 ] baz  with\tquux ;"
     "endpackage",
     "package foo_pkg;\n"
     "  nettype shortreal foo;\n"
     "  nettype bar [1:0] baz with quux;\n"
     "endpackage\n"},
    {"package foo_pkg; \n"
     "// function description.......\n"
     "function automatic void bar();"
     "endfunction "
     "endpackage\n",
     "package foo_pkg;\n"
     "  // function description.......\n"
     "  function automatic void bar();\n"
     "  endfunction\n"
     "endpackage\n"},
    {"package foo_pkg; \n"
     "// function description.......\n"
     "function void bar(string name=\"x\" ) ;"
     "endfunction "
     "endpackage\n",
     "package foo_pkg;\n"
     "  // function description.......\n"
     "  function void bar(string name = \"x\");\n"
     "  endfunction\n"
     "endpackage\n"},
    {" package foo_pkg; \n"
     "// class description.............\n"
     "class classy;"
     "endclass "
     "endpackage\n",
     "package foo_pkg;\n"
     "  // class description.............\n"
     "  class classy;\n"
     "  endclass\n"
     "endpackage\n"},
    {"package\tfoo_pkg; \n"
     "// class description.............\n"
     "class   classy;    \n"
     "// function description.......\n"
     "function\nautomatic   void bar( );"
     "endfunction   "
     "endclass\t"
     "endpackage\n",
     "package foo_pkg;\n"
     "  // class description.............\n"
     "  class classy;\n"
     "    // function description.......\n"
     "    function automatic void bar();\n"
     "    endfunction\n"
     "  endclass\n"
     "endpackage\n"},

    // function test cases
    {"function f ;endfunction", "function f;\nendfunction\n"},
    {"function f ;endfunction:   f", "function f;\nendfunction : f\n"},
    {"function f ( );endfunction", "function f();\nendfunction\n"},
    {"function f (input bit x);endfunction",
     "function f(input bit x);\nendfunction\n"},
    {"function f (input bit x,logic y );endfunction",
     "function f(input bit x, logic y);\nendfunction\n"},
    {"function f;\n// statement comment\nendfunction\n",
     "function f;\n"
     "  // statement comment\n"  // indented
     "endfunction\n"},
    {"function f();\n// statement comment\nendfunction\n",
     "function f();\n"
     "  // statement comment\n"  // indented
     "endfunction\n"},
    {"function f(input int x);\n"
     "// statement comment\n"
     "f=x;\n"
     "// statement comment\n"
     "endfunction\n",
     "function f(input int x);\n"
     "  // statement comment\n"  // indented
     "  f = x;\n"
     "  // statement comment\n"  // indented
     "endfunction\n"},
    {// line breaks around assignments
     "function f;a=b;c+=d;endfunction",
     "function f;\n"
     "  a = b;\n"
     "  c += d;\n"
     "endfunction\n"},
    {"function f;a&=b;c=d;endfunction",
     "function f;\n"
     "  a &= b;\n"
     "  c = d;\n"
     "endfunction\n"},
    {"function f;a<<=b;c=b;d>>>=b;endfunction",
     "function f;\n"
     "  a <<= b;\n"
     "  c = b;\n"
     "  d >>>= b;\n"
     "endfunction\n"},
    {// port declaration exceeds line length limit
     "function f (loooong_type if_it_fits_I_sits);endfunction",
     "function f(\n"
     "    loooong_type if_it_fits_I_sits);\n"
     "endfunction\n"},
    {"function\nvoid\tspace;a=( b+c )\n;endfunction   :space\n",
     "function void space;\n"
     "  a = (b + c);\n"
     "endfunction : space\n"},
    {"function\nvoid\twarranty;return  to_sender\n;endfunction   :warranty\n",
     "function void warranty;\n"
     "  return to_sender;\n"
     "endfunction : warranty\n"},
    {// if statement that fits on one line
     "function if_i_fits_i_sits;"
     "if(x)y=x;"
     "endfunction",
     "function if_i_fits_i_sits;\n"
     "  if (x) y = x;\n"
     "endfunction\n"},
    {// for loop
     "function\nvoid\twarranty;for(j=0; j<k; --k)begin "
     "++j\n;end endfunction   :warranty\n",
     "function void warranty;\n"
     "  for (j = 0; j < k; --k) begin\n"
     "    ++j;\n"
     "  end\n"
     "endfunction : warranty\n"},
    {// for loop that needs wrapping
     "function\nvoid\twarranty;for(jjjjj=0; jjjjj<kkkkk; --kkkkk)begin "
     "++j\n;end endfunction   :warranty\n",
     "function void warranty;\n"
     "  for (\n"
     "      jjjjj = 0; jjjjj < kkkkk; --kkkkk\n"
     "  ) begin\n"
     "    ++j;\n"
     "  end\n"
     "endfunction : warranty\n"},
    {// for loop that needs more wrapping
     "function\nvoid\twarranty;"
     "for(jjjjjjjj=0; jjjjjjjj<kkkkkkkk; --kkkkkkkk)begin "
     "++j\n;end endfunction   :warranty\n",
     "function void warranty;\n"
     "  for (\n"
     "      jjjjjjjj = 0;\n"
     "      jjjjjjjj < kkkkkkkk;\n"
     "      --kkkkkkkk\n"
     "  ) begin\n"
     "    ++j;\n"
     "  end\n"
     "endfunction : warranty\n"},
    {// for loop that fits on one line
     "function loop_fits;"
     "for(x=0;x<N;++x) y=x;"
     "endfunction",
     "function loop_fits;\n"
     "  for (x = 0; x < N; ++x) y = x;\n"
     "endfunction\n"},
    {// for loop that would fit on one line, but is force-split with //comment
     "function loop_fits;"
     "for(x=0;x<N;++x) //\n y=x;"
     "endfunction",
     "function loop_fits;\n"
     "  for (x = 0; x < N; ++x)  //\n"
     "    y = x;\n"
     "endfunction\n"},
    {// for loop with function call in initializer
     "function  void looper(); "
     "for (int i=f(n); i>=0; i -- ) begin end "
     "endfunction",
     "function void looper();\n"
     "  for (int i = f(n); i >= 0; i--) begin\n"
     "  end\n"
     "endfunction\n"},
    {// for loop with function call in condition
     "function  void looper(); "
     "for (int i=0; i<f(m); i -- ) begin end "
     "endfunction",
     "function void looper();\n"
     "  for (int i = 0; i < f(m); i--) begin\n"
     "  end\n"
     "endfunction\n"},
    {// forever loop
     "function\nvoid\tforevah;forever  begin "
     "++k\n;end endfunction\n",
     "function void forevah;\n"
     "  forever begin\n"
     "    ++k;\n"
     "  end\n"
     "endfunction\n"},
    {// forever loop
     "function\nvoid\tforevah;forever  "
     "++k\n;endfunction\n",
     "function void forevah;\n"
     "  forever ++k;\n"
     "endfunction\n"},
    {// forever loop, forced break
     "function\nvoid\tforevah;forever     //\n"
     "++k\n;endfunction\n",
     "function void forevah;\n"
     "  forever  //\n"
     "    ++k;\n"
     "endfunction\n"},
    {// repeat loop
     "function\nvoid\tpete;repeat(3)  begin "
     "++k\n;end endfunction\n",
     "function void pete;\n"
     "  repeat (3) begin\n"
     "    ++k;\n"
     "  end\n"
     "endfunction\n"},
    {// repeat loop
     "function\nvoid\tpete;repeat(3)  "
     "++k\n;endfunction\n",
     "function void pete;\n"
     "  repeat (3)++k;\n"  // TODO(fangism): space before ++
     "endfunction\n"},
    {// repeat loop, forced break
     "function\nvoid\tpete;repeat(3)//\n"
     "++k\n;endfunction\n",
     "function void pete;\n"
     "  repeat (3)  //\n"
     "    ++k;\n"
     "endfunction\n"},
    {// while loop
     "function\nvoid\twily;while( coyote )  begin "
     "++super_genius\n;end endfunction\n",
     "function void wily;\n"
     "  while (coyote) begin\n"
     "    ++super_genius;\n"
     "  end\n"
     "endfunction\n"},
    {// while loop
     "function\nvoid\twily;while( coyote )  "
     "++ super_genius\n;   endfunction\n",
     "function void wily;\n"
     "  while (coyote)++super_genius;\n"  // TODO(fangism): space before ++
     "endfunction\n"},
    {// while loop, forced break
     "function\nvoid\twily;while( coyote ) //\n "
     "++ super_genius\n;   endfunction\n",
     "function void wily;\n"
     "  while (coyote)  //\n"
     "    ++super_genius;\n"
     "endfunction\n"},
    {// do-while loop
     "function\nvoid\tdonot;do  begin "
     "++s\n;end  while( z);endfunction\n",
     "function void donot;\n"
     "  do begin\n"
     "    ++s;\n"
     "  end while (z);\n"
     "endfunction\n"},
    {// do-while loop, single statement
     "function\nvoid\tdonot;do  "
     "++s\n;  while( z);endfunction\n",
     "function void donot;\n"
     "  do ++s; while (z);\n"
     "endfunction\n"},
    {// do-while loop, single statement, forced break
     "function\nvoid\tdonot;do  "
     "++s\n;//\n  while( z);endfunction\n",
     "function void donot;\n"
     "  do\n"
     "    ++s;  //\n"
     "  while (z);\n"
     "endfunction\n"},
    {// foreach loop
     "function\nvoid\tforeacher;foreach( m [n] )  begin "
     "++m\n;end endfunction\n",
     "function void foreacher;\n"
     "  foreach (m[n]) begin\n"
     "    ++m;\n"
     "  end\n"
     "endfunction\n"},
    {// spaces in condition expression
     "function f; return {a}? {b} :{ c };endfunction",
     "function f;\n"
     "  return {a} ? {b} : {c};\n"
     "endfunction\n"},
    {"task t;endtask",
     "task t;\n"
     "endtask\n"},
    {"task t (   );endtask",
     "task t();\n"
     "endtask\n"},
    {"task t (input    bit   drill   ) ;endtask",
     "task t(input bit drill);\n"
     "endtask\n"},
    {"task t; ## 100 ;endtask",
     "task t;\n"
     "  ##100;\n"
     "endtask\n"},
    {"task t; ## (1+1) ;endtask",  // delay expression
     "task t;\n"
     "  ##(1 + 1);\n"
     "endtask\n"},
    {"task t; ## delay_value ;endtask",
     "task t;\n"
     "  ##delay_value;\n"
     "endtask\n"},
    {"task t; ## `DELAY_VALUE ;endtask",
     "task t;\n"
     "  ##`DELAY_VALUE;\n"
     "endtask\n"},
    {"task t;\n"
     "`uvm_error( foo,bar);\n"
     "endtask\n",
     "task t;\n"
     "  `uvm_error(foo, bar);\n"
     "endtask\n"},
    {"task t;\n"
     "`uvm_error(foo,bar)\n"
     ";\n"  // null statement
     "endtask\n",
     "task t;\n"
     "  `uvm_error(foo, bar)\n"
     "  ;\n"
     "endtask\n"},
    {"task t;\n"
     "if(expr)begin\t\n"
     "`uvm_error(foo,bar);\n"
     "end\n"
     "endtask\n",
     "task t;\n"
     "  if (expr) begin\n"
     "    `uvm_error(foo, bar);\n"
     "  end\n"
     "endtask\n"},
    {"task\nrabbit;$kill(the,\nrabbit)\n;endtask:  rabbit\n",
     "task rabbit;\n"
     "  $kill(the, rabbit);\n"
     "endtask : rabbit\n"},
    {"function  int foo( );if( a )a+=1 ; endfunction",
     "function int foo();\n"
     "  if (a) a += 1;\n"
     "endfunction\n"},
    {"function  void foo( );foo=`MACRO(b,c) ; endfunction",
     "function void foo();\n"
     "  foo = `MACRO(b, c);\n"
     "endfunction\n"},
    {"module foo;if    \t  (bar)begin assign a=1; end endmodule",
     "module foo;\n"
     "  if (bar) begin\n"
     "    assign a = 1;\n"
     "  end\n"
     "endmodule\n"},
    {"module proc_cont_assigner;\n"
     "always begin\n"
     "assign x1 =   y1;\n"
     "deassign   x2 ;\n"
     "force x3=y3;\n"
     "release   x4 ;\n"
     "end\n"
     "endmodule\n",
     "module proc_cont_assigner;\n"
     "  always begin\n"
     "    assign x1 = y1;\n"
     "    deassign x2;\n"
     "    force x3 = y3;\n"
     "    release x4;\n"
     "  end\n"
     "endmodule\n"},
    {"module g_test(  );\n"
     "\tinitial begin:main_test \t"
     "for(int i=0;i<k;i++)begin "
     "case(i )\n"
     " 6'd0  :release in[0];  \n"
     "   endcase  "
     " \t\tend \t"
     "\t end:main_test\n"
     "endmodule:g_test\n",
     "module g_test ();\n"
     "  initial begin : main_test\n"
     "    for (int i = 0; i < k; i++) begin\n"
     "      case (i)\n"
     "        6'd0: release in[0];\n"
     "      endcase\n"
     "    end\n"
     "  end : main_test\n"
     "endmodule : g_test\n"},
    {// conditional generate (case)
     "module mc; case(s)a : bb c ; d : ee f; endcase endmodule",
     "module mc;\n"
     "  case (s)\n"
     "    a: bb c;\n"
     "    d: ee f;\n"
     "  endcase\n"
     "endmodule\n"},
    {// conditional generate (case), with comments
     "module mc; case(s)\n//comment a\na:bb  c;\n//comment b\n endcase "
     "endmodule",
     "module mc;\n"
     "  case (s)\n"
     "    //comment a\n"  // indented to case-item level
     "    a: bb c;\n"
     "    //comment b\n"  // indented to case-item level
     "  endcase\n"
     "endmodule\n"},

    {// "default:", not "default :"
     "function f; case (x) default: x=y; endcase endfunction\n",
     "function f;\n"
     "  case (x)\n"
     "    default: x = y;\n"
     "  endcase\n"
     "endfunction\n"},
    {// default with null statement: "default: ;", not "default :;"
     "function f; case (x) default :; endcase endfunction\n",
     "function f;\n"
     "  case (x)\n"
     "    default: ;\n"
     "  endcase\n"
     "endfunction\n"},
    {// case statement
     "function f; case (x) State0 : a=b; State1 : begin a=b; end "
     "endcase endfunction\n",
     "function f;\n"
     "  case (x)\n"
     "    State0: a = b;\n"
     "    State1: begin\n"
     "      a = b;\n"
     "    end\n"
     "  endcase\n"
     "endfunction\n"},
    {// case statement, interleaved with comments
     "function f; case (x) \n//c1\nState0 : a=b;//c2\n//c3\n State1 : "
     "a=b;//c4\n//c5\n "
     "endcase endfunction\n",
     "function f;\n"
     "  case (x)\n"
     "    //c1\n"
     "    State0: a = b;  //c2\n"
     "    //c3\n"
     "    State1: a = b;  //c4\n"
     "    //c5\n"
     "  endcase\n"
     "endfunction\n"},
    {// case inside statement, comments
     "function f; case (x)inside \n//comment\n"
     "[0:1]:x=y; \n"
     "    //comment\n"
     "endcase endfunction\n",
     "function f;\n"
     "  case (x) inside\n"
     "    //comment\n"
     "    [0 : 1]: x = y;\n"
     "    //comment\n"
     "  endcase\n"
     "endfunction\n"},
    {// case inside statement
     "function f; case (x)inside k1 : return b; k2 : begin return b; end "
     "endcase endfunction\n",
     "function f;\n"
     "  case (x) inside\n"
     "    k1: return b;\n"
     "    k2: begin\n"
     "      return b;\n"
     "    end\n"
     "  endcase\n"
     "endfunction\n"},
    {// case inside statement, with ranges
     "function f; case (x) inside[a:b] : return b; [c:d] : return b; "
     "default :return z;"
     "endcase endfunction\n",
     "function f;\n"
     "  case (x) inside\n"
     "    [a : b]: return b;\n"
     "    [c : d]: return b;\n"
     "    default: return z;\n"
     "  endcase\n"
     "endfunction\n"},
    {// case pattern statement
     "function f;"
     "case (y) matches "
     ".foo   : return 0;"
     ".*\t: return 1;"
     "endcase "
     "case (z) matches "
     ".foo\t\t: return 0;"
     ".*   : return 1;"
     "endcase "
     "endfunction",
     "function f;\n"
     "  case (y) matches\n"
     "    .foo: return 0;\n"
     "    .*: return 1;\n"
     "  endcase\n"
     "  case (z) matches\n"
     "    .foo: return 0;\n"
     "    .*: return 1;\n"
     "  endcase\n"
     "endfunction\n"},
    {// keep short case items on one line
     "function f; case (x)k1 : if( b )break; default :return 2;"
     "endcase endfunction\n",
     "function f;\n"
     "  case (x)\n"
     "    k1:      if (b) break;\n"  // aligned
     "    default: return 2;\n"
     "  endcase\n"
     "endfunction\n"},
    {// keep short default items on one line
     "function f; case (x)k1 :break; default :if( c )return 2;"
     "endcase endfunction\n",
     "function f;\n"
     "  case (x)\n"
     "    k1:      break;\n"  // aligned
     "    default: if (c) return 2;\n"
     "  endcase\n"
     "endfunction\n"},
    {// keep short case inside items on one line
     "function f; case (x)inside k1 : if( b )return c; k2 : return a;"
     "endcase endfunction\n",
     "function f;\n"
     "  case (x) inside\n"
     "    k1: if (b) return c;\n"
     "    k2: return a;\n"
     "  endcase\n"
     "endfunction\n"},
    {// keep short case pattern items on one line
     "function f;"
     "case (y) matches "
     ".foo   :if( n )return 0;"
     ".*\t: return 1;"
     "endcase "
     "endfunction",
     "function f;\n"
     "  case (y) matches\n"
     "    .foo: if (n) return 0;\n"
     "    .*: return 1;\n"
     "  endcase\n"
     "endfunction\n"},
    {// randcase
     "function f; randcase k1 : return c; k2 : return a;"
     "endcase endfunction\n",
     "function f;\n"
     "  randcase\n"
     "    k1: return c;\n"
     "    k2: return a;\n"
     "  endcase\n"
     "endfunction\n"},

    // This tests checks for not breaking around hierarchy operators.
    {"function\nvoid\twarranty;"
     "foo.bar = fancyfunction(aaaaaaaa.bbbbbbb,"
     "    ccccccccc.ddddddddd) ;"
     "endfunction   :warranty\n",
     "function void warranty;\n"
     "  foo.bar = fancyfunction(\n"
     "      aaaaaaaa.bbbbbbb,\n"
     "      ccccccccc.ddddddddd\n"
     "  );\n"
     "endfunction : warranty\n"},

    // Group of tests testing partitioning of arguments inside function calls
    {// function with function call inside if statement header
     "function foo;if(aa(bb,cc));endfunction\n",
     "function foo;\n"
     "  if (aa(bb, cc));\n"
     "endfunction\n"},
    {// function with function call inside if statement header and with
     // begin-end block
     "function foo;if (aa(bb,cc,dd,ee))begin end endfunction\n",
     "function foo;\n"
     "  if (aa(bb, cc, dd, ee)) begin\n"
     "  end\n"
     "endfunction\n"},
    {// function with kMethodCallExtension inside if statement header and with
     // begin-end block
     "function foo;if (aa.bb(cc,dd,ee))begin end endfunction\n",
     "function foo;\n"
     "  if (aa.bb(cc, dd, ee)) begin\n"
     "  end\n"
     "endfunction\n"},
    {// nested kMethodCallExtension calls - one level
     "function foo;aa.bb(cc.dd(a1), ee.ff(a2));endfunction\n",
     "function foo;\n"
     "  aa.bb(cc.dd(a1), ee.ff(a2));\n"
     "endfunction\n"},
    {// nested kMethodCallExtension calls - two level
     "function foo;aa.bb(cc.dd(a1.b1(a2), b1), ee.ff(c1, d1));endfunction\n",
     "function foo;\n"
     "  aa.bb(cc.dd(a1.b1(a2), b1), ee.ff(\n"
     "        c1, d1));\n"
     "endfunction\n"},

    {// simple initial statement with function call
     "module m;initial aa(bb,cc,dd,ee);endmodule\n",
     "module m;\n"
     "  initial aa(bb, cc, dd, ee);\n"
     "endmodule\n"},
    {// expressions and function calls inside if-statement headers
     "module m;initial begin if(aa(bb)==cc(dd))a=b;if (xx()) b = a;end "
     "endmodule\n",
     "module m;\n"
     "  initial begin\n"
     "    if (aa(bb) == cc(dd)) a = b;\n"
     "    if (xx()) b = a;\n"
     "  end\n"
     "endmodule\n"},
    {// fuction with two arguments inside if-statement headers
     "module\nm;initial\nbegin\nif(aa(bb,cc))x=y;end\nendmodule\n",
     "module m;\n"
     "  initial begin\n"
     "    if (aa(bb, cc)) x = y;\n"
     "  end\n"
     "endmodule\n"},
    {// kMethodCallExtension inside if-statement headers
     "module m;initial begin if (aa.bb(cc)) x = y;end endmodule",
     "module m;\n"
     "  initial begin\n"
     "    if (aa.bb(cc)) x = y;\n"
     "  end\n"
     "endmodule\n"},
    {// initial statement with object method call
     "module m; initial a.b(a,b,c); endmodule\n",
     "module m;\n"
     "  initial a.b(a, b, c);\n"
     "endmodule\n"},
    {// initial statement with method call on indexed object
     "module m; initial a[i].b(a,b,c); endmodule\n",
     "module m;\n"
     "  initial a[i].b(a, b, c);\n"
     "endmodule\n"},
    {// initial statement with method call on function returned object
     "module m; initial a(d,e,f).b(a,b,c); endmodule\n",
     "module m;\n"
     "  initial a(d, e, f).b(a, b, c);\n"
     "endmodule\n"},
    {// initial statement with indexed access to function returned object
     "module m; initial a(a,b,c)[i]; endmodule\n",
     "module m;\n"
     "  initial a(a, b, c) [i];\n"
     "endmodule\n"},
    {// method call with no arguments on an object
     "module m; initial foo.bar();endmodule\n",
     "module m;\n"
     "  initial foo.bar();\n"
     "endmodule\n"},
    {// method call with one argument on an object
     "module m; initial foo.bar(aa);endmodule\n",
     "module m;\n"
     "  initial foo.bar(aa);\n"
     "endmodule\n"},
    {// method call with two arguments on an object
     "module m; initial foo.bar(aa,bb);endmodule\n",
     "module m;\n"
     "  initial foo.bar(aa, bb);\n"
     "endmodule\n"},
    {// method call with three arguments on an object
     "module m; initial foo.bar(aa,bb,cc);endmodule\n",
     "module m;\n"
     "  initial foo.bar(aa, bb, cc);\n"
     "endmodule\n"},

    {
        // This tests for if-statements with null statements
        "function foo;"
        "if (zz) ; "
        "if (yy) ; "
        "endfunction",
        "function foo;\n"
        "  if (zz);\n"
        "  if (yy);\n"
        "endfunction\n",
    },

    {
        // This tests for if-statements starting on their own line.
        "function foo;"
        "if (zz) begin "
        "return 0;"
        "end "
        "if (yy) begin "
        "return 1;"
        "end "
        "endfunction",
        "function foo;\n"
        "  if (zz) begin\n"
        "    return 0;\n"
        "  end\n"
        "  if (yy) begin\n"
        "    return 1;\n"
        "  end\n"
        "endfunction\n",
    },

    {
        // This tests for if-statements with single statement bodies
        "function foo;"
        "if (zz) return 0;"
        "if (yy) return 1;"
        "endfunction",
        "function foo;\n"
        "  if (zz) return 0;\n"
        "  if (yy) return 1;\n"
        "endfunction\n",
    },

    {
        // This tests for if-statement mixed with plain statements
        "function foo;"
        "a=b;"
        "if (zz) return 0;"
        "c=d;"
        "endfunction",
        "function foo;\n"
        "  a = b;\n"
        "  if (zz) return 0;\n"
        "  c = d;\n"
        "endfunction\n",
    },

    {
        // This tests for if-statement with forced break mixed with others
        "function foo;"
        "a=b;"
        "if (zz)//\n return 0;"
        "c=d;"
        "endfunction",
        "function foo;\n"
        "  a = b;\n"
        "  if (zz)  //\n"
        "    return 0;\n"
        "  c = d;\n"
        "endfunction\n",
    },
    {
        "function t;"
        "if (r == t)"
        "a.b(c);"
        "endfunction",
        "function t;\n"
        "  if (r == t) a.b(c);\n"
        "endfunction\n",
    },

    {// This tests for for-statement with forced break mixed with others
     "function f;"
     "x=y;"
     "for (int i=0; i<S*IPS; i++) #1ps a += $urandom();"
     "return 2;"
     "endfunction",
     "function f;\n"
     "  x = y;\n"
     "  for (int i = 0; i < S * IPS; i++)\n"  // doesn't fit, so indents
     "    #1ps a += $urandom();\n"
     "  return 2;\n"
     "endfunction\n"},

    {
        // This tests for-statements with null statements
        "function foo;"
        "for(;;)  ;\t"
        "for(;;)  ;\t"
        "endfunction",
        "function foo;\n"
        "  for (;;);\n"
        "  for (;;);\n"
        "endfunction\n",
    },

    {
        // This tests for if-else-statements with null statements
        "function foo;"
        "if (zz) ;  else  ;"
        "if (yy) ;   else   ;"
        "endfunction",
        "function foo;\n"
        "  if (zz);\n"
        "  else;\n"
        "  if (yy);\n"
        "  else;\n"
        "endfunction\n",
    },

    {
        // This tests for end-else-begin.
        "function foo;"
        "if (zz) begin "
        "return 0;"
        "end "
        "else "
        "begin "
        "return 1;"
        "end "
        "endfunction",
        "function foo;\n"
        "  if (zz) begin\n"
        "    return 0;\n"
        "  end else begin\n"
        "    return 1;\n"
        "  end\n"
        "endfunction\n",
    },
    {
        // This tests for end-else-if
        "function foo;"
        "if (zz) begin "
        "return 0;"
        "end "
        "else "
        "if(yy)begin "
        "return 1;"
        "end "
        "endfunction",
        "function foo;\n"
        "  if (zz) begin\n"
        "    return 0;\n"
        "  end else if (yy) begin\n"
        "    return 1;\n"
        "  end\n"
        "endfunction\n",
    },
    {
        // This tests labeled end-else-if
        "function foo;"
        "if (zz) begin : label1 "
        "return 0;"
        "end : label1 "
        "else if (yy) begin : label2 "
        "return 1;"
        "end : label2 "
        "endfunction",
        "function foo;\n"
        "  if (zz) begin : label1\n"
        "    return 0;\n"
        "  end : label1\n"
        "  else if (yy) begin : label2\n"
        "    return 1;\n"
        "  end : label2\n"
        "endfunction\n",
    },
    {
        // This tests labeled end-else-if-else
        "function foo;"
        "if (zz) begin : label1 "
        "return 0;"
        "end : label1 "
        "else if (yy) begin : label2 "
        "return 1;"
        "end : label2 "
        "else begin : label3 "
        "return 2;"
        "end : label3 "
        "endfunction",
        "function foo;\n"
        "  if (zz) begin : label1\n"
        "    return 0;\n"
        "  end : label1\n"
        "  else if (yy) begin : label2\n"
        "    return 1;\n"
        "  end : label2\n"
        "  else begin : label3\n"
        "    return 2;\n"
        "  end : label3\n"
        "endfunction\n",
    },

    {
        // randomize function
        "function r ;"
        "if ( ! randomize (bar )) begin    end "
        "if ( ! obj.randomize (bar )) begin    end "
        "endfunction",
        "function r;\n"
        "  if (!randomize(bar)) begin\n"
        "  end\n"
        "  if (!obj.randomize(bar)) begin\n"
        "  end\n"
        "endfunction\n",
    },
    {
        // randomize-with call, with comments
        "function f;"
        "s = std::randomize() with {\n"
        "// comment1\n"
        "a == e;\n"
        "// comment2\n"
        "};"
        "endfunction\n",
        "function f;\n"
        "  s = std::randomize() with {\n"
        "    // comment1\n"
        "    a == e;\n"
        "    // comment2\n"
        "  };\n"
        "endfunction\n",
    },
    {
        // randomize-with call, with comments, one joined
        "function f;"
        "s = std::randomize() with {\n"
        "// comment1\n"
        "a == e;// comment2\n"
        "};"
        "endfunction\n",
        "function f;\n"
        "  s = std::randomize() with {\n"
        "    // comment1\n"
        "    a == e;  // comment2\n"
        "  };\n"
        "endfunction\n",
    },
    {
        // randomize-with call, with comment, and conditional
        "function f;"
        "s = std::randomize() with {\n"
        "// comment\n"
        "a == e;"
        "if (x) {"
        "a;"
        "}"
        "};"
        "endfunction\n",
        "function f;\n"
        "  s = std::randomize() with {\n"
        "    // comment\n"
        "    a == e;\n"
        "    if (x) {a;}\n"  // TODO(fangism): consider expanding
        "  };\n"
        "endfunction\n",
    },

    // module declaration test cases
    {"   module       foo  ;     endmodule\n",
     "module foo;\n"
     "endmodule\n"},
    {"   module       foo   (    )   ;     endmodule\n",
     "module foo ();\n"
     "endmodule\n"},
    {"   module       foo   (  .x (  x) );     endmodule\n",
     "module foo (\n"
     "    .x(x)\n"
     ");\n"
     "endmodule\n"},
    {"   module       foo   (  .x (  x)  \n,\n . y "
     "  ( \ny) );     endmodule\n",
     "module foo (\n"
     "    .x(x),\n"
     "    .y(y)\n"
     ");\n"
     "endmodule\n"},

    // module instantiation test cases
    {"  module foo   ; bar bq();endmodule\n",
     "module foo;\n"
     "  bar bq ();\n"  // single instance
     "endmodule\n"},
    {"  module foo   ; bar bq(), bq2(  );endmodule\n",
     "module foo;\n"
     "  bar bq (), bq2 ();\n"  // multiple instances, still fitting on one line
     "endmodule\n"},
    {"module foo; bar #(.N(N)) bq (.bus(bus));endmodule\n",
     // instance parameter and port fits on line
     "module foo;\n"
     "  bar #(.N(N)) bq (.bus(bus));\n"
     "endmodule\n"},
    {"module foo; bar #(.N(N),.M(M)) bq ();endmodule\n",  // two named params
     "module foo;\n"
     "  bar #(\n"
     "      .N(N),\n"
     "      .M(M)\n"
     "  ) bq ();\n"
     "endmodule\n"},
    {"module foo; bar #(//comment\n.N(N),.M(M)) bq ();endmodule\n",
     "module foo;\n"
     "  bar #(  //comment\n"  // EOL comment before first param
     "      .N(N),\n"
     "      .M(M)\n"
     "  ) bq ();\n"
     "endmodule\n"},
    {"module foo; bar #(.N(N),//comment\n.M(M)) bq ();endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      .N(N),  //comment\n"  // EOL comment after first param
     "      .M(M)\n"
     "  ) bq ();\n"
     "endmodule\n"},
    {"module foo; bar #(.N(N),.M(M)//comment\n) bq ();endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      .N(N),\n"
     "      .M(M)   //comment\n"  // EOL comment after last param
     "  ) bq ();\n"
     "endmodule\n"},
    {"  module foo   ; bar bq(aa,bb,cc);endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      aa,\n"
     "      bb,\n"
     "      cc\n"
     "  );\n"  // multiple positional ports, one per line
     "endmodule\n"},
    {"  module foo   ; bar bq(aa,\n"
     "`ifdef BB\n"
     "bb,\n"
     "`endif\n"
     "cc);endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      aa,\n"
     "`ifdef BB\n"
     "      bb,\n"  // keep same indentation as outside conditional
     "`endif\n"
     "      cc\n"
     "  );\n"  // multiple positional ports, one per line
     "endmodule\n"},
    {"  module foo   ; bar bq(.aa,.bb);endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      .aa,\n"
     "      .bb\n"
     "  );\n"  // multiple named ports, one per line
     "endmodule\n"},
    {"  module foo   ; bar bq(.aa(aa),.bb(bb));endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      .aa(aa),\n"
     "      .bb(bb)\n"
     "  );\n"  // multiple named ports, one per line
     "endmodule\n"},
    {"  module foo   ; bar bq(.aa(aa),\n"
     "`ifdef ZZ\n"
     ".zz(  zz  ),\n"
     "`else\n"
     ".yy(  yy  ),\n"
     "`endif\n"
     ".bb(bb)\n"
     ");endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      .aa(aa),\n"
     "`ifdef ZZ\n"
     "      .zz(zz),\n"
     "`else\n"
     "      .yy(yy),\n"
     "`endif\n"
     "      .bb(bb)\n"
     "  );\n"  // multiple named ports, one per line
     "endmodule\n"},
    {"  module foo   ; bar#(NNNNNNNN)"
     "bq(.aa(aaaaaa),.bb(bbbbbb));endmodule\n",
     "module foo;\n"
     "  bar #(NNNNNNNN) bq (\n"
     "      .aa(aaaaaa),\n"
     "      .bb(bbbbbb)\n"
     "  );\n"
     "endmodule\n"},
    {" module foo   ; barrrrrrr "
     "bq(.aaaaaa(aaaaaa),.bbbbbb(bbbbbb));endmodule\n",
     "module foo;\n"
     "  barrrrrrr bq (\n"
     "      .aaaaaa(aaaaaa),\n"
     "      .bbbbbb(bbbbbb)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo; bar #(.NNNNN(NNNNN)) bq (.bussss(bussss));endmodule\n",
     // instance parameter and port does not fit on line
     "module foo;\n"
     "  bar #(\n"
     "      .NNNNN(NNNNN)\n"
     "  ) bq (\n"
     "      .bussss(bussss)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo; bar #(//\n.N(N)) bq (.bus(bus));endmodule\n",
     "module foo;\n"
     "  bar #(  //\n"  // would fit on one line, but forced to expand by //
     "      .N(N)\n"
     "  ) bq (\n"
     "      .bus(bus)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo; bar #(\n"
     "`ifdef MM\n"
     ".M(M)\n"
     "`else\n"
     ".N(N)\n"
     "`endif\n"
     ") bq (.bus(bus));endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "`ifdef MM\n"
     "      .M(M)\n"
     "`else\n"
     "      .N(N)\n"
     "`endif\n"
     "  ) bq (\n"
     "      .bus(bus)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo; bar #(.N(N)//\n) bq (.bus(bus));endmodule\n",
     "module foo;\n"
     "  bar #(\n"  // would fit on one line, but forced to expand by //
     "      .N(N)  //\n"
     "  ) bq (\n"
     "      .bus(bus)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo; bar #(.N(N)) bq (//\n.bus(bus));endmodule\n",
     "module foo;\n"
     "  bar #(\n"  // would fit on one line, but forced to expand by //
     "      .N(N)\n"
     "  ) bq (  //\n"
     "      .bus(bus)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo; bar #(.N(N)) bq (.bus(bus)//\n);endmodule\n",
     "module foo;\n"
     "  bar #(\n"  // would fit on one line, but forced to expand by //
     "      .N(N)\n"
     "  ) bq (\n"
     "      .bus(bus)  //\n"
     "  );\n"
     "endmodule\n"},
    {" module foo   ; bar "
     "bq(.aaa(aaa),.bbb(bbb),.ccc(ccc),.ddd(ddd));endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      .aaa(aaa),\n"  // ports don't fit on one line, so expanded
     "      .bbb(bbb),\n"
     "      .ccc(ccc),\n"
     "      .ddd(ddd)\n"
     "  );\n"
     "endmodule\n"},
    {" module foo   ; bar "
     "bq(.aa(aa),.bb(bb),.cc(cc),.dd(dd));endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      .aa(aa),\n"  // one named port per line
     "      .bb(bb),\n"
     "      .cc(cc),\n"
     "      .dd(dd)\n"
     "  );\n"
     "endmodule\n"},
    {" module foo   ; bar "
     "bq(.aa(aa),//\n.bb(bb),.cc(cc),.dd(dd));endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      .aa(aa),  //\n"  // forced to expand by //
     "      .bb(bb),\n"
     "      .cc(cc),\n"
     "      .dd(dd)\n"
     "  );\n"
     "endmodule\n"},
    {" module foo   ; bar "
     "bq(.aa(aa),.bb(bb),.cc(cc),.dd(dd)//\n);endmodule\n",
     "module foo;\n"
     "  bar bq (\n"
     "      .aa(aa),\n"
     "      .bb(bb),\n"
     "      .cc(cc),\n"
     "      .dd(dd)   //\n"  // forced to expand by //
     "  );\n"
     "endmodule\n"},
    {// gate instantiation test
     "module m;"
     "and\tx0(a, \t\tb,c);"
     "or\nx1(a,  \n b,    d);"
     "endmodule\n",
     "module m;\n"
     "  and x0 (a, b, c);\n"
     "  or x1 (a, b, d);\n"
     "endmodule\n"},
    {// ifdef inside port actuals
     "module m;  foo bar   (\n"
     "`ifdef   BAZ\n"
     "`endif\n"
     ")  ;endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "`ifdef BAZ\n"
     "`endif\n"
     "  );\n"
     "endmodule\n"},
    {// ifdef inside port actuals after a port connection
     "module m;  foo bar   ( .a (a) ,\n"
     "`ifdef   BAZ\n"
     "`endif\n"
     ")  ;endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a(a),\n"
     "`ifdef BAZ\n"
     "`endif\n"
     "  );\n"
     "endmodule\n"},
    {// ifdef inside port actuals before a port connection
     "module m;  foo bar   (\n"
     "`ifdef   BAZ\n"
     "`endif\n"
     ". b(b) )  ;endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "`ifdef BAZ\n"
     "`endif\n"
     "      .b(b)\n"
     "  );\n"
     "endmodule\n"},
    {// ifdef-conditional port connection
     "module m;  foo bar   (\n"
     "`ifdef   BAZ\n"
     ". c (\tc) \n"
     "`endif\n"
     " )  ;endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "`ifdef BAZ\n"
     "      .c(c)\n"
     "`endif\n"
     "  );\n"
     "endmodule\n"},
    {// ifndef-else-conditional port connection
     "module m;  foo bar   (\n"
     "`ifndef   BAZ\n"
     ". c (\tc) \n"
     "  `else\n"
     " . d(d\t)\n"
     "  `endif\n"
     " )  ;endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "`ifndef BAZ\n"
     "      .c(c)\n"
     "`else\n"
     "      .d(d)\n"
     "`endif\n"
     "  );\n"
     "endmodule\n"},

    {
        // test that alternate top-syntax mode works
        "// verilog_syntax: parse-as-module-body\n"
        "`define           FOO\n",
        "// verilog_syntax: parse-as-module-body\n"
        "`define FOO\n",
    },
    {
        // test alternate parsing mode in macro expansion
        "class foo;\n"
        "`MY_MACRO(\n"
        " // verilog_syntax: parse-as-statements\n"
        " // EOL comment\n"
        " int count;\n"
        " if(cfg.enable) begin\n"
        " count = 1;\n"
        " end,\n"
        " utils_pkg::decrement())\n"
        "endclass\n",
        "class foo;\n"
        "  `MY_MACRO(\n"
        "      // verilog_syntax: parse-as-statements\n"
        "      // EOL comment\n"
        "      int count;\n"
        "      if (cfg.enable) begin\n"
        "        count = 1;\n"
        "      end, utils_pkg::decrement())\n"
        "endclass\n",
    },

    // tests top-level data declarations
    {"a;",  // implicit type
     "a;\n"},
    {"a\tb;",  // explicit type
     "a b;\n"},
    {"a;b;",
     "a;\n"
     "b;\n"},
    {"a ,b;",  // implicit type
     "a, b;\n"},
    /* TODO(b/149591599): implicit type data declarations in module body
    {"module\tm ;a ;endmodule",
     "module m;\n"
     "  a;\n"
     "endmodule\n"},
    */
    {"package\tp ;a ;endpackage",  // implicit type
     "package p;\n"
     "  a;\n"
     "endpackage\n"},
    {"package\tp ;a ,b ;endpackage",  // implicit type
     "package p;\n"
     "  a, b;\n"
     "endpackage\n"},
    {"package\tp ;a ;b ;endpackage",  // implicit type
     "package p;\n"
     "  a;\n"
     "  b;\n"
     "endpackage\n"},
    /* TODO(b/149591627) : implicit type data declarations in class body
    {"class\tc ;a ;endclass",
     "class c;\n"
     "  a;\n"
     "endclass\n"},
     */
    {"function\tf ;a ;endfunction",  // implicit type
     "function f;\n"
     "  a;\n"
     "endfunction\n"},
    {"function\tf ;a   ;x ;endfunction",  // implicit type
     "function f;\n"
     "  a;\n"
     "  x;\n"
     "endfunction\n"},
    /* TODO(b/149592527): multi-variable data declaration as block_item_decl
     // same inside tasks
    {"function\tf ;a  \t,x ;endfunction",  // implicit type
     "function f;\n"
     "  a, x;\n"
     "endfunction\n"},
     */
    {"task\tt ;a ;endtask",  // implicit type
     "task t;\n"
     "  a;\n"
     "endtask\n"},
    {"task\tt ;a   ;x ;endtask",  // implicit type
     "task t;\n"
     "  a;\n"
     "  x;\n"
     "endtask\n"},

    {// tests bind declaration
     "bind   foo   bar baz  ( . clk ( clk  ) ) ;",
     "bind foo bar baz (.clk(clk));\n"},
    {// tests bind declaration, with type params
     "bind   foo   bar# ( . W ( W ) ) baz  ( . clk ( clk  ) ) ;",
     "bind foo bar #(.W(W)) baz (.clk(clk));\n"},
    {// tests bind declarations
     "bind   foo   bar baz  ( ) ;"
     "bind goo  car  caz (   );",
     "bind foo bar baz ();\n"
     "bind goo car caz ();\n"},

    {"bind blah foo #( .MaxCount(MaxCount), .MaxDelta(MaxDelta)) bar ("
     "    .clk(clk), .rst(rst), .value(value) );",
     "bind blah foo #(\n"
     "    .MaxCount(MaxCount),\n"
     "    .MaxDelta(MaxDelta)\n"
     ") bar (\n"
     "    .clk  (clk),\n"
     "    .rst  (rst),\n"
     "    .value(value)\n"
     ");\n"},
    {
        "bind expaaaaaaaaaaand_meeee looooooooong_name# ("
        ".W(W_CONST), .H(H_CONST), .D(D_CONST)  )"
        "instaaance_name (.in(iiiiiiiin), .out(ooooooout), .clk(ccccccclk));",
        "bind expaaaaaaaaaaand_meeee\n"
        "    looooooooong_name #(\n"
        "    .W(W_CONST),\n"
        "    .H(H_CONST),\n"
        "    .D(D_CONST)\n"
        ") instaaance_name (\n"
        "    .in (iiiiiiiin),\n"
        "    .out(ooooooout),\n"
        "    .clk(ccccccclk)\n"
        ");\n",
    },

    {
        "bind expand_inst name# ("
        ".W(W_CONST), .H(H_CONST), .D(D_CONST)  )"
        "instaaance_name (.in(iiiiiiiin), .out(ooooooout), .clk(ccccccclk));",
        "bind expand_inst name #(\n"
        "    .W(W_CONST),\n"
        "    .H(H_CONST),\n"
        "    .D(D_CONST)\n"
        ") instaaance_name (\n"
        "    .in (iiiiiiiin),\n"
        "    .out(ooooooout),\n"
        "    .clk(ccccccclk)\n"
        ");\n",
    },

    {
        // tests import declaration
        "import  foo_pkg :: bar ;",
        "import foo_pkg::bar;\n",
    },
    {
        // tests import declaration with wildcard
        "import  foo_pkg :: * ;",
        "import foo_pkg::*;\n",
    },
    {
        // tests import declarations
        "import  foo_pkg :: *\t;"
        "import  goo_pkg\n:: thing ;",
        "import foo_pkg::*;\n"
        "import goo_pkg::thing;\n",
    },
    // preserve spaces inside [] dimensions, but limit spaces around ':' to one
    // and adjust everything else
    {"foo[W-1:0]a[0:K-1];",  // data declaration
     "foo [W-1:0] a[0:K-1];\n"},
    {"foo[W-1 : 0]a[0 : K-1];", "foo [W-1 : 0] a[0 : K-1];\n"},
    {"foo[W  -  1 : 0 ]a [ 0  :  K  -  1] ;",
     "foo [W  -  1 : 0] a[0 : K  -  1];\n"},
    // remove spaces between [...] [...] in multi-dimension arrays
    {"foo[K] [W]a;",  //
     "foo [K][W] a;\n"},
    {"foo b [K] [W] ;",  //
     "foo b[K][W];\n"},
    {"logic[K:1] [W:1]a;",  //
     "logic [K:1][W:1] a;\n"},
    {"logic b [K:1] [W:1] ;",  //
     "logic b[K:1][W:1];\n"},
    // spaces in bit slicing
    {
        // preserve 0 spaces
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7:2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7:2];\n"
        "end\n",
    },
    {
        // preserve 1 space
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7 : 2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7 : 2];\n"
        "end\n",
    },
    {
        // limit multiple spaces to 1
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7  :  2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7 : 2];\n"
        "end\n",
    },
    {
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7  : 2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7 : 2];\n"
        "end\n",
    },
    {
        // keep value on the left when symmetrizing
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7: 2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7:2];\n"
        "end\n",
    },
    {
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7:  2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7:2];\n"
        "end\n",
    },
    {
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7 :2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7 : 2];\n"
        "end\n",
    },
    {
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7 :  2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7 : 2];\n"
        "end\n",
    },
    {
        // use value on the left, but limit to 1 space
        "always_ff @(posedge clk) begin "
        "dummy  <=\tfoo  [  7  :2  ] ; "
        "end",
        "always_ff @(posedge clk) begin\n"
        "  dummy <= foo[7 : 2];\n"
        "end\n",
    },

    // task test cases
    {"task t ;endtask:t",  //
     "task t;\n"
     "endtask : t\n"},
    {"task t ;#   10 ;# 5ns ; # 0.1 ; # 1step ;endtask",
     "task t;\n"
     "  #10;\n"  // no space in delay expression
     "  #5ns;\n"
     "  #0.1;\n"
     "  #1step;\n"
     "endtask\n"},
    {"task t\n;a<=b ;c<=d ;endtask\n",
     "task t;\n"
     "  a <= b;\n"
     "  c <= d;\n"
     "endtask\n"},
    {"class c;   virtual protected task\tt  ( foo bar);"
     "a.a<=b.b;\t\tc.c\n<=   d.d; endtask   endclass",
     "class c;\n"
     "  virtual protected task t(foo bar);\n"
     "    a.a <= b.b;\n"
     "    c.c <= d.d;\n"
     "  endtask\n"
     "endclass\n"},
    {"task t;\n// statement comment\nendtask\n",
     "task t;\n"
     "  // statement comment\n"  // indented
     "endtask\n"},
    {"task t( );\n// statement comment\nendtask\n",
     "task t();\n"
     "  // statement comment\n"  // indented
     "endtask\n"},
    {"task t( input x  );\n"
     "// statement comment\n"
     "s();\n"
     "// statement comment\n"
     "endtask\n",
     "task t(input x);\n"
     "  // statement comment\n"  // indented
     "  s();\n"
     "  // statement comment\n"  // indented
     "endtask\n"},
    {"task fj;fork join fork join\tendtask",
     "task fj;\n"
     "  fork\n"
     "  join\n"
     "  fork\n"
     "  join\n"
     "endtask\n"},
    {"task fj;fork join_any fork join_any\tendtask",
     "task fj;\n"
     "  fork\n"
     "  join_any\n"
     "  fork\n"
     "  join_any\n"
     "endtask\n"},
    {"task fj;fork join_none fork join_none\tendtask",
     "task fj;\n"
     "  fork\n"
     "  join_none\n"
     "  fork\n"
     "  join_none\n"
     "endtask\n"},
    {"task fj;fork\n"
     "//c1\njoin\n"
     "//c2\n"
     "fork  \n"
     "//c3\n"
     "join\tendtask",
     "task fj;\n"
     "  fork\n"
     "    //c1\n"
     "  join\n"
     "  //c2\n"
     "  fork\n"
     "    //c3\n"
     "  join\n"
     "endtask\n"},
    {"task fj;\n"
     "fork "
     "begin "
     "end "
     "foo();"
     "begin "
     "end "
     "join_any endtask",
     "task fj;\n"
     "  fork\n"
     "    begin\n"
     "    end\n"
     "    foo();\n"
     "    begin\n"
     "    end\n"
     "  join_any\n"
     "endtask\n"},
    {
        // call and assertion statements
        "task  t ;Fire() ;assert ( x);assert(y );endtask",
        "task t;\n"
        "  Fire();\n"
        "  assert (x);\n"
        "  assert (y);\n"
        "endtask\n",
    },
    {
        // assertion statements with body clause
        "task  t ;Fire() ;assert ( x) fee ( );assert(y ) foo ( ) ;endtask",
        "task t;\n"
        "  Fire();\n"
        "  assert (x) fee();\n"
        "  assert (y) foo();\n"
        "endtask\n",
    },
    {
        // assertion statements with else clause
        "task  t ;Fire() ;assert ( x) else  fee ( );"
        "assert(y ) else  foo ( ) ;endtask",
        "task t;\n"
        "  Fire();\n"
        "  assert (x)\n"
        "  else fee();\n"
        "  assert (y)\n"
        "  else foo();\n"
        "endtask\n",
    },
    {
        // assertion statements with else clause
        "task  t ;Fire() ;assert ( x) fa(); else  fee ( );"
        "assert(y ) fi(); else  foo ( ) ;endtask",
        "task t;\n"
        "  Fire();\n"
        "  assert (x) fa();\n"
        "  else fee();\n"
        "  assert (y) fi();\n"
        "  else foo();\n"
        "endtask\n",
    },
    {
        // assume statements
        "task  t ;Fire() ;assume ( x);assume(y );endtask",
        "task t;\n"
        "  Fire();\n"
        "  assume (x);\n"
        "  assume (y);\n"
        "endtask\n",
    },
    {
        // cover statements
        "task  t ;Fire() ;cover ( x);cover(y );endtask",
        "task t;\n"
        "  Fire();\n"
        "  cover (x);\n"
        "  cover (y);\n"
        "endtask\n",
    },
    {
        // cover statements, with action
        "task  t ;Fire() ;cover ( x)g( );cover(y ) h();endtask",
        "task t;\n"
        "  Fire();\n"
        "  cover (x) g();\n"
        "  cover (y) h();\n"
        "endtask\n",
    },
    {
        // cover statements, with action block
        "task  t ;Fire() ;cover ( x) begin g( ); end "
        "cover(y ) begin h(); end endtask",
        "task t;\n"
        "  Fire();\n"
        "  cover (x) begin\n"
        "    g();\n"
        "  end\n"
        "  cover (y) begin\n"
        "    h();\n"
        "  end\n"
        "endtask\n",
    },
    {// shuffle calls
     "task t; foo. shuffle  ( );bar .shuffle( ); endtask",
     "task t;\n"
     "  foo.shuffle();\n"
     "  bar.shuffle();\n"
     "endtask\n"},
    {// wait statements (null)
     "task t; wait  (a==b);wait(c<d); endtask",
     "task t;\n"
     "  wait (a == b);\n"
     "  wait (c < d);\n"
     "endtask\n"},
    {// wait statements, single action statement
     "task t; wait  (a==b) p();wait(c<d) q(); endtask",
     "task t;\n"
     "  wait (a == b) p();\n"
     "  wait (c < d) q();\n"
     "endtask\n"},
    {// wait statements, block action statement
     "task t; wait  (a==b) begin p(); end "
     "wait(c<d) begin q(); end endtask",
     "task t;\n"
     "  wait (a == b) begin\n"
     "    p();\n"
     "  end\n"
     "  wait (c < d) begin\n"
     "    q();\n"
     "  end\n"
     "endtask\n"},
    {// wait fork statements
     "task t ; wait\tfork;wait   fork ;endtask",
     "task t;\n"
     "  wait fork;\n"
     "  wait fork;\n"
     "endtask\n"},
    {// labeled single statements (prefix-style)
     "task t;l1:x<=y ;endtask",
     "task t;\n"
     "  l1 : x <= y;\n"
     "endtask\n"},
    {// labeled block statements (prefix-style)
     "task t;l1:begin end:l1 endtask",
     "task t;\n"
     "  l1 : begin\n"
     "  end : l1\n"
     "endtask\n"},
    {// labeled seq block statements
     "task t;begin:l1 end:l1 endtask",
     "task t;\n"
     "  begin : l1\n"
     "  end : l1\n"
     "endtask\n"},
    {// labeled par block statements
     "task t;fork:l1 join:l1 endtask",
     "task t;\n"
     "  fork : l1\n"
     "  join : l1\n"
     "endtask\n"},
    {// task with disable statements
     "task  t ;fork\tjoin\tdisable\tfork;endtask",
     "task t;\n"
     "  fork\n"
     "  join\n"
     "  disable fork;\n"
     "endtask\n"},
    {"task  t ;fork\tjoin_any\tdisable\tfork  ;endtask",
     "task t;\n"
     "  fork\n"
     "  join_any\n"
     "  disable fork;\n"
     "endtask\n"},
    {"task  t ;disable\tbean_counter  ;endtask",
     "task t;\n"
     "  disable bean_counter;\n"
     "endtask\n"},
    {
        // task with if-statement
        "task t;"
        "if (r == t)"
        "a.b(c);"
        "endtask",
        "task t;\n"
        "  if (r == t) a.b(c);\n"
        "endtask\n",
    },
    {
        // task with system call inside if header
        "task t;"
        "if (!$cast(ssssssssssssssss,vvvvvvvvvv,gggggggg))begin end endtask:t",
        "task t;\n"
        "  if (!$cast(\n"
        "          ssssssssssssssss,\n"
        "          vvvvvvvvvv,\n"
        "          gggggggg\n"
        "      )) begin\n"
        "  end\n"
        "endtask : t\n",
    },
    {
        // task with nested subtask call and arguments passed by name
        "task t;"
        "if (!$cast(ssssssssssssssss, vvvvvvvvvv.gggggggg("
        ".ppppppp(ppppppp),"
        ".yyyyy(\"xxxxxxxxxxxxx\")"
        "))) begin "
        "end "
        "endtask : t",
        "task t;\n"
        "  if (!$cast(\n"
        "          ssssssssssssssss,\n"
        "          vvvvvvvvvv.gggggggg(\n"
        "              .ppppppp(ppppppp),\n"
        "              .yyyyy(\"xxxxxxxxxxxxx\")\n"
        "          )\n"
        "      )) begin\n"
        "  end\n"
        "endtask : t\n",
    },

    {
        // assert property statements
        "task  t ;assert  property( x);assert\tproperty(y );endtask",
        "task t;\n"
        "  assert property (x);\n"
        "  assert property (y);\n"
        "endtask\n",
    },
    {
        // assert property statements, with action
        "task  t ;assert  property( x) j();assert\tproperty(y )k( );endtask",
        "task t;\n"
        "  assert property (x) j();\n"
        "  assert property (y) k();\n"
        "endtask\n",
    },
    {
        // assert property statements, with action block
        "task  t ;assert  property( x) begin j();end "
        " assert\tproperty(y )begin\tk( );  end endtask",
        "task t;\n"
        "  assert property (x) begin\n"
        "    j();\n"
        "  end\n"
        "  assert property (y) begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },
    {
        // assert property statements, else with null
        "task  t ;assert  property( x) else;assert\tproperty(y )else;endtask",
        "task t;\n"
        "  assert property (x)\n"
        "  else;\n"
        "  assert property (y)\n"
        "  else;\n"
        "endtask\n",
    },
    {
        // assert property statements, else with actions
        "task  t ;assert  property( x) f(); else p(); "
        "\tassert\tproperty(y ) g();else  q( );endtask",
        "task t;\n"
        "  assert property (x) f();\n"
        "  else p();\n"
        "  assert property (y) g();\n"
        "  else q();\n"
        "endtask\n",
    },
    {
        // assert property statement, with action block, else statement
        "task  t ;assert  property( x) begin j();end  else\tk( );  endtask",
        "task t;\n"
        "  assert property (x) begin\n"
        "    j();\n"
        "  end else k();\n"
        "endtask\n",
    },
    {
        // assert property statement, with action statement, else block
        "task  t ;assert  property( x) j();  else  begin\tk( );end  endtask",
        "task t;\n"
        "  assert property (x) j();\n"
        "  else begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },
    {
        // assert property statement, with action block, else block
        "task  t ;assert  property( x)begin j();end  "
        "else  begin\tk( );end  endtask",
        "task t;\n"
        "  assert property (x) begin\n"
        "    j();\n"
        "  end else begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },

    {
        // assume property statements
        "task  t ;assume  property( x);assume\tproperty(y );endtask",
        "task t;\n"
        "  assume property (x);\n"
        "  assume property (y);\n"
        "endtask\n",
    },
    {
        // assume property statements, with action
        "task  t ;assume  property( x) j();assume\tproperty(y )k( );endtask",
        "task t;\n"
        "  assume property (x) j();\n"
        "  assume property (y) k();\n"
        "endtask\n",
    },
    {
        // assume property statements, with action block
        "task  t ;assume  property( x) begin j();end "
        " assume\tproperty(y )begin\tk( );  end endtask",
        "task t;\n"
        "  assume property (x) begin\n"
        "    j();\n"
        "  end\n"
        "  assume property (y) begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },
    {
        // assume property statements, else with null
        "task  t ;assume  property( x) else;assume\tproperty(y )else;endtask",
        "task t;\n"
        "  assume property (x)\n"
        "  else;\n"
        "  assume property (y)\n"
        "  else;\n"
        "endtask\n",
    },
    {
        // assume property statements, else with actions
        "task  t ;assume  property( x) f(); else p(); "
        "\tassume\tproperty(y ) g();else  q( );endtask",
        "task t;\n"
        "  assume property (x) f();\n"
        "  else p();\n"
        "  assume property (y) g();\n"
        "  else q();\n"
        "endtask\n",
    },
    {
        // assume property statement, with action block, else statement
        "task  t ;assume  property( x) begin j();end  else\tk( );  endtask",
        "task t;\n"
        "  assume property (x) begin\n"
        "    j();\n"
        "  end else k();\n"
        "endtask\n",
    },
    {
        // assume property statement, with action statement, else block
        "task  t ;assume  property( x) j();  else  begin\tk( );end  endtask",
        "task t;\n"
        "  assume property (x) j();\n"
        "  else begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },
    {
        // assume property statement, with action block, else block
        "task  t ;assume  property( x)begin j();end  "
        "else  begin\tk( );end  endtask",
        "task t;\n"
        "  assume property (x) begin\n"
        "    j();\n"
        "  end else begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },

    {
        // expect property statements
        "task  t ;expect  ( x);expect\t(y );endtask",
        "task t;\n"
        "  expect (x);\n"
        "  expect (y);\n"
        "endtask\n",
    },
    {
        // expect property statements, with action
        "task  t ;expect  ( x) j();expect\t(y )k( );endtask",
        "task t;\n"
        "  expect (x) j();\n"
        "  expect (y) k();\n"
        "endtask\n",
    },
    {
        // expect property statements, with action block
        "task  t ;expect  ( x) begin j();end "
        " expect\t(y )begin\tk( );  end endtask",
        "task t;\n"
        "  expect (x) begin\n"
        "    j();\n"
        "  end\n"
        "  expect (y) begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },
    {
        // expect property statements, else with null
        "task  t ;expect  ( x) else;expect\t(y )else;endtask",
        "task t;\n"
        "  expect (x)\n"
        "  else;\n"
        "  expect (y)\n"
        "  else;\n"
        "endtask\n",
    },
    {
        // expect property statements, else with actions
        "task  t ;expect  ( x) f(); else p(); "
        "\texpect\t(y ) g();else  q( );endtask",
        "task t;\n"
        "  expect (x) f();\n"
        "  else p();\n"
        "  expect (y) g();\n"
        "  else q();\n"
        "endtask\n",
    },
    {
        // expect property statement, with action block, else statement
        "task  t ;expect  ( x) begin j();end  else\tk( );  endtask",
        "task t;\n"
        "  expect (x) begin\n"
        "    j();\n"
        "  end else k();\n"
        "endtask\n",
    },
    {
        // expect property statement, with action statement, else block
        "task  t ;expect  ( x) j();  else  begin\tk( );end  endtask",
        "task t;\n"
        "  expect (x) j();\n"
        "  else begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },
    {
        // expect property statement, with action block, else block
        "task  t ;expect  ( x)begin j();end  "
        "else  begin\tk( );end  endtask",
        "task t;\n"
        "  expect (x) begin\n"
        "    j();\n"
        "  end else begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },

    {
        // cover property statements
        "task  t ;cover  property( x);cover\tproperty(y );endtask",
        "task t;\n"
        "  cover property (x);\n"
        "  cover property (y);\n"
        "endtask\n",
    },
    {
        // cover property statements, with action
        "task  t ;cover  property( x) j();cover\tproperty(y )k( );endtask",
        "task t;\n"
        "  cover property (x) j();\n"
        "  cover property (y) k();\n"
        "endtask\n",
    },
    {
        // cover property statements, with action block
        "task  t ;cover  property( x) begin j();end "
        " cover\tproperty(y )begin\tk( );  end endtask",
        "task t;\n"
        "  cover property (x) begin\n"
        "    j();\n"
        "  end\n"
        "  cover property (y) begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },

    {
        // cover sequence statements
        "task  t ;cover  sequence( x);cover\tsequence(y );endtask",
        "task t;\n"
        "  cover sequence (x);\n"
        "  cover sequence (y);\n"
        "endtask\n",
    },
    {
        // cover sequence statements, with action
        "task  t ;cover  sequence( x) j();cover\tsequence(y )k( );endtask",
        "task t;\n"
        "  cover sequence (x) j();\n"
        "  cover sequence (y) k();\n"
        "endtask\n",
    },
    {
        // cover sequence statements, with action block
        "task  t ;cover  sequence( x) begin j();end "
        " cover\tsequence(y )begin\tk( );  end endtask",
        "task t;\n"
        "  cover sequence (x) begin\n"
        "    j();\n"
        "  end\n"
        "  cover sequence (y) begin\n"
        "    k();\n"
        "  end\n"
        "endtask\n",
    },

    {// module with disable statements
     "module m;always begin :block disable m.block; end endmodule",
     "module m;\n"
     "  always begin : block\n"
     "    disable m.block;\n"
     "  end\n"
     "endmodule\n"},
    {"module m;always begin disable m.block; end endmodule",
     "module m;\n"
     "  always begin\n"
     "    disable m.block;\n"
     "  end\n"
     "endmodule\n"},

    // property test cases
    {"module mp ;property p1 ; a|->b;endproperty endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    a |-> b;\n"
     "  endproperty\n"
     "endmodule\n"},
    {"module mp ;property p1 ; a|->b;endproperty:p1 endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    a |-> b;\n"
     "  endproperty : p1\n"  // with end label
     "endmodule\n"},
    {"module mp ;property p1 ; a|->## 1  b;endproperty endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    a |-> ##1 b;\n"  // with delay
     "  endproperty\n"
     "endmodule\n"},
    {"module mp ;property p1 ; a|->## [0: 1]  b;endproperty endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    a |-> ##[0:1] b;\n"  // prefer no spaces with delay range
     "  endproperty\n"
     "endmodule\n"},
    {"module mp ;property p1 ; a|->## [0  : 1]  b;endproperty endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    a |-> ##[0 : 1] b;\n"  // limit to one space, symmetrize
     "  endproperty\n"
     "endmodule\n"},
    {"module mp ;property p1 ; a## 1  b;endproperty endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    a ##1 b;\n"
     "  endproperty\n"
     "endmodule\n"},
    {"module mp ;property p1 ; (a^c)## 1  b;endproperty endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    (a ^ c) ##1 b;\n"
     "  endproperty\n"
     "endmodule\n"},

    // covergroup test cases
    {// Minimal case
     "covergroup c; endgroup\n",
     "covergroup c;\n"
     "endgroup\n"},
    {// Minimal useful case
     "covergroup c @ (posedge clk); coverpoint a; endgroup\n",
     "covergroup c @(posedge clk);\n"
     "  coverpoint a;\n"
     "endgroup\n"},
    {// Multiple coverpoints
     "covergroup foo @(posedge clk); coverpoint a; coverpoint b; "
     "coverpoint c; coverpoint d; endgroup\n",
     "covergroup foo @(posedge clk);\n"
     "  coverpoint a;\n"
     "  coverpoint b;\n"
     "  coverpoint c;\n"
     "  coverpoint d;\n"
     "endgroup\n"},
    {// Multiple bins
     "covergroup memory @ (posedge ce); address  :coverpoint addr {"
     "bins low={0,127}; bins high={128,255};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  address: coverpoint addr {\n"
     "    bins low = {0, 127};\n"
     "    bins high = {128, 255};\n"
     "  }\n"
     "endgroup\n"},
    {// Custom sample() function
     "covergroup c with function sample(bit i); endgroup\n",
     "covergroup c with function sample (\n"
     "    bit i\n"  // test cases are wrapped at 40
     ");\n"
     "endgroup\n"},

    // comment-controlled formatter disabling
    {"// verilog_format: off\n"  // disables whole file
     "  `include  \t\t  \"path/to/file.vh\"\n",
     "// verilog_format: off\n"
     "  `include  \t\t  \"path/to/file.vh\"\n"},  // keeps bad spacing
    {"/* verilog_format: off */\n"                // disables whole file
     "  `include  \t\t  \"path/to/file.svh\"  \n",
     "/* verilog_format: off */\n"
     "  `include  \t\t  \"path/to/file.svh\"  \n"},  // keeps bad spacing
    {"// verilog_format: on\n"  // already enabled, no effect
     "  `include  \t  \"path/to/file.svh\"  \t\n",
     "// verilog_format: on\n"
     "`include \"path/to/file.svh\"\n"},
    {"// verilog_format: off\n"
     "// verilog_format: on\n"  // re-enable right away
     "  `include  \t\t  \"path/to/file.svh\"  \n",
     "// verilog_format: off\n"
     "// verilog_format: on\n"
     "`include \"path/to/file.svh\"\n"},
    {"/* aaa *//* bbb */\n"                         // not formatting controls
     "  `include  \t\t  \"path/to/file.svh\"  \n",  // should format
     "/* aaa */  /* bbb */\n"  // currently inserts 2 spaces
     "`include \"path/to/file.svh\"\n"},
    {"/* verilog_format: off *//* verilog_format: on */\n"  // re-enable
     "  `include  \t\t  \"path/to/file.svh\"  \n",
     // Note that this case normally wouldn't fit in 40 columns,
     // but disabling formatting lets it overflow.
     "/* verilog_format: off *//* verilog_format: on */\n"
     "`include \"path/to/file.svh\"\n"},
    {"// verilog_format: off\n"
     "  `include  \t\t  \"path/to/fileA.svh\"  // verilog_format: on\n"
     "  `include  \t\t  \"path/to/fileB.svh\"  \n",
     // re-enable formatting with comment trailing other tokens
     "// verilog_format: off\n"
     "  `include  \t\t  \"path/to/fileA.svh\"  // verilog_format: on\n"
     "`include \"path/to/fileB.svh\"\n"},
    {"  `include  \t\t  \"path/to/file1.vh\" \n"  // format this
     "// verilog_format: off\n"                   // start disabling
     "  `include  \t\t  \"path/to/file2.vh\" \n"
     "\t\t\n"
     "  `include  \t\t  \"path/to/file3.vh\" \n"
     "// verilog_format: on\n"                     // stop disabling
     "  `include  \t\t  \"path/to/file4.vh\" \n",  // format this
     "`include \"path/to/file1.vh\"\n"
     "// verilog_format: off\n"  // start disabling
     "  `include  \t\t  \"path/to/file2.vh\" \n"
     "\t\t\n"
     "  `include  \t\t  \"path/to/file3.vh\" \n"
     "// verilog_format: on\n"  // stop disabling
     "`include \"path/to/file4.vh\"\n"},
    {// disabling formatting on a module (to end of file)
     "// verilog_format: off\n"
     "module m;endmodule\n",
     "// verilog_format: off\n"
     "module m;endmodule\n"},
    {// disabling formatting on a module (to end of file)
     "// verilog_format: off\n"
     "module m;\n"
     "unindented instantiation;\n"
     "endmodule\n",
     "// verilog_format: off\n"
     "module m;\n"
     "unindented instantiation;\n"
     "endmodule\n"},
    {// disabling formatting inside a port declaration list disables alignment,
     // but falls back to standard compaction.
     "module align_off(\n"
     "input w  ,\n"
     "    // verilog_format: off\n"
     "input wire  [y:z] wwww,\n"
     "    // verilog_format: on\n"
     "output  reg    xx\n"
     ");\n"
     "endmodule",
     "module align_off (\n"
     "    input w  ,\n"  // preserved because group is partially disabled
     "    // verilog_format: off\n"
     "input wire  [y:z] wwww,\n"  // not compacted
     "    // verilog_format: on\n"
     "    output  reg    xx\n"  // preserved because group is partially disabled
     ");\n"
     "endmodule\n"},

    {// multiple tokens with EOL comment
     "module please;  // don't break before the comment\n"
     "endmodule\n",
     "module please\n"
     "    ;  // don't break before the comment\n"
     "endmodule\n"},
    {// one token with EOL comment
     "module please;\n"
     "endmodule  // don't break before the comment\n",
     "module please;\n"
     "endmodule  // don't break before the comment\n"},
    {
        // line with only an EOL comment
        "module wild;\n"
        "// a really long comment on its own line to be left alone\n"
        "endmodule",
        "module wild;\n"
        "  // a really long comment on its own line to be left alone\n"
        "endmodule\n",
    },
    {
        // primitive declaration
        "primitive primitive1(o, s, r);output o;reg o;input s;input r;table 1 "
        "? :"
        " ? : 0; ? 1    : 0   : -; endtable endprimitive",
        "primitive primitive1(o, s, r);\n"
        "  output o;\n"
        "  reg o;\n"
        "  input s;\n"
        "  input r;\n"
        "  table\n"
        "    1 ? : ? : 0;\n"
        "    ? 1 : 0 : -;\n"
        "  endtable\n"
        "endprimitive\n",
    },
    {
        // one-input combinatorial UDP
        "primitive primitive1 ( o,i ) ;output o;input i;"
        " table 1  :   0 ;   0  :  1 ; endtable endprimitive",
        "primitive primitive1(o, i);\n"
        "  output o;\n"
        "  input i;\n"
        "  table\n"
        "    1 : 0;\n"
        "    0 : 1;\n"
        "  endtable\n"
        "endprimitive\n",
    },
    {
        // two-input combinatorial UDP
        "primitive primitive2(o, s, r);output o;input s;input r;"
        "table 1 ? : 0;? 1 : -; endtable endprimitive",
        "primitive primitive2(o, s, r);\n"
        "  output o;\n"
        "  input s;\n"
        "  input r;\n"
        "  table\n"
        "    1 ? : 0;\n"
        "    ? 1 : -;\n"
        "  endtable\n"
        "endprimitive\n",
    },
    {
        // ten-input combinatorial UDP
        "primitive comb10(o, i0, i1, i2, i3, i4, i5, i6, i7, i8, i9);"
        "output o;input i0, i1, i2, i3, i4, i5, i6, i7, i8, i9;"
        "table 0 ? ? ? ? ? ? ? ? 0 : 0;1 ? ? ? ? ? ? ? ? 0 : 1;"
        "1 ? ? ? ? ? ? ? ? 1 : 1;0 ? ? ? ? ? ? ? ? 1 : 0;endtable endprimitive",
        "primitive comb10(o, i0, i1, i2, i3, i4,\n"
        "                 i5, i6, i7, i8, i9);\n"
        "  output o;\n"
        "  input i0, i1, i2, i3, i4, i5, i6, i7,\n"
        "      i8, i9;\n"
        "  table\n"
        "    0 ? ? ? ? ? ? ? ? 0 : 0;\n"
        "    1 ? ? ? ? ? ? ? ? 0 : 1;\n"
        "    1 ? ? ? ? ? ? ? ? 1 : 1;\n"
        "    0 ? ? ? ? ? ? ? ? 1 : 0;\n"
        "  endtable\n"
        "endprimitive\n",
    },
    {
        // sequential level-sensitive UDP
        "primitive level_seq(o, c, d);output o;reg o;"
        "  input c;input d;table\n"
        "//  C D  state O\n"
        "0   ? : ? :  -;  // No Change\n"
        "? 0   : 0 :  0;  // Unknown\n"
        "endtable endprimitive",
        "primitive level_seq(o, c, d);\n"
        "  output o;\n"
        "  reg o;\n"
        "  input c;\n"
        "  input d;\n"
        "  table\n"
        "    //  C D  state O\n"
        "    0 ? : ? : -;  // No Change\n"
        "    ? 0 : 0 : 0;  // Unknown\n"
        "  endtable\n"
        "endprimitive\n",
    },
    {
        // sequential edge-sensitive UDP
        "primitive edge_seq(o, c, d);output o;reg o;input c;input d;"
        "table (01) 0 : ? :  0;(01) 1 : ? :  1;(0?) 1 : 1 :  1;(0?) 0 : 0 :  "
        "0;\n"
        "// ignore negative c\n"
        "(?0) ? : ? :  -;\n"
        "// ignore changes on steady c\n"
        "?  (?\?) : ? :  -; endtable endprimitive",
        "primitive edge_seq(o, c, d);\n"
        "  output o;\n"
        "  reg o;\n"
        "  input c;\n"
        "  input d;\n"
        "  table\n"
        "    (01) 0 : ? : 0;\n"
        "    (01) 1 : ? : 1;\n"
        "    (0?) 1 : 1 : 1;\n"
        "    (0?) 0 : 0 : 0;\n"
        "    // ignore negative c\n"
        "    (?0) ? : ? : -;\n"
        "    // ignore changes on steady c\n"
        "    ? (?\?) : ? : -;\n"
        "  endtable\n"
        "endprimitive\n",
    },
    {
        // mixed sequential UDP
        "primitive mixed(o, clk, j, k, preset, clear);output o;reg o;"
        "input c;input j, k;input preset, clear;table "
        "?  ??  01:?:1 ; // preset logic\n"
        "?  ??  *1:1:1 ;?  ??  10:?:0 ; // clear logic\n"
        "?  ??  1*:0:0 ;r  00  00:0:1 ; // normal\n"
        "r  00  11:?:- ;r  01  11:?:0 ;r  10  11:?:1 ;r  11  11:0:1 ;"
        "r  11  11:1:0 ;f  ??  ??:?:- ;b  *?  ??:?:- ;"
        " // j and k\n"
        "b  ?*  ??:?:- ;endtable endprimitive\n",
        "primitive mixed(o, clk, j, k, preset,\n"
        "                clear);\n"
        "  output o;\n"
        "  reg o;\n"
        "  input c;\n"
        "  input j, k;\n"
        "  input preset, clear;\n"
        "  table\n"
        "    ? ? ? 0 1 : ? : 1;  // preset logic\n"
        "    ? ? ? * 1 : 1 : 1;\n"
        "    ? ? ? 1 0 : ? : 0;  // clear logic\n"
        "    ? ? ? 1 * : 0 : 0;\n"
        "    r 0 0 0 0 : 0 : 1;  // normal\n"
        "    r 0 0 1 1 : ? : -;\n"
        "    r 0 1 1 1 : ? : 0;\n"
        "    r 1 0 1 1 : ? : 1;\n"
        "    r 1 1 1 1 : 0 : 1;\n"
        "    r 1 1 1 1 : 1 : 0;\n"
        "    f ? ? ? ? : ? : -;\n"
        "    b * ? ? ? : ? : -;  // j and k\n"
        "    b ? * ? ? : ? : -;\n"
        "  endtable\n"
        "endprimitive\n",
    },
    // un-lexed multiline macro arg token
    {
        " task  S ; "
        "`ppgJH3JoxhwyTmZ2dgPiuMQzpRAWiSs("
        "{xYtxuh6.FIMcVPEWfhtoI2FSe, xYtxuh6.ZVL5XASVGLYz32} == "
        "SqRgavM[15:2];\n"
        "JgQLBG == 4'h0;, \"foo\" )\n"
        "endtask\n",
        "task S;\n"
        "  `ppgJH3JoxhwyTmZ2dgPiuMQzpRAWiSs(\n"
        "      {xYtxuh6.FIMcVPEWfhtoI2FSe, xYtxuh6.ZVL5XASVGLYz32} == "
        "SqRgavM[15:2];\n"
        "JgQLBG == 4'h0;,\n"
        "      \"foo\")\n"
        "endtask\n",
    },

    // module instantiation named ports tabular alignment
    //{// module instantiation with no port, only comments
    //    "module m;\n"
    //    "foo bar(\n"
    //    "\t//comment1\n"
    //    "//comment2\n"
    //    ");\n"
    //    "endmodule\n",
    //    "module m;\n"
    //    "  foo bar (\n"
    //    "      //comment1\n"
    //    "      //comment2\n"
    //    "  );\n"
    //    "endmodule\n"
    //},
    {// all named ports
     "module m;\n"
     "foo bar(.a(a), .aa(aa), .aaa(aaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aa (aa),\n"
     "      .aaa(aaa)\n"
     "  );\n"
     "endmodule\n"},
    {// named ports left unconnected
     "module m;\n"
     "foo bar(.a(), .aa(), .aaa());\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (),\n"
     "      .aa (),\n"
     "      .aaa()\n"
     "  );\n"
     "endmodule\n"},
    {// multiple named ports groups separated by blank line
     "module m;\n"
     "foo bar(.a(a), .aaa(aaa),\n\n .b(b), .bbbbbb(bbbbb));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aaa(aaa),\n"
     "\n"
     "      .b     (b),\n"
     "      .bbbbbb(bbbbb)\n"
     "  );\n"
     "endmodule\n"},
    {// named ports with concatenation
     "module m;\n"
     "foo bar(.a(a), .aaa({a,b,c}));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aaa({a, b, c})\n"
     "  );\n"
     "endmodule\n"},
    {// name ports with slices
     "module m;\n"
     "foo bar(.a(a), .aaa(q[r:s]));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aaa(q[r:s])\n"
     "  );\n"
     "endmodule\n"},
    {// named ports with pre-proc directives
     "module m;\n"
     "foo bar(.a(a), `ifdef MACRO .aa(aa), `endif .aaa(aaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "`ifdef MACRO\n"
     "      .aa (aa),\n"
     "`endif\n"
     "      .aaa(aaa)\n"
     "  );\n"
     "endmodule\n"},
    {// named ports with macros
     "module m;\n"
     "foo bar(.a(a), .aa(aa[`RANGE]), .aaa(aaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aa (aa[`RANGE]),\n"
     "      .aaa(aaa)\n"
     "  );\n"
     "endmodule\n"},
    {"module m;\n"
     "foo bar(.a(a), .AA, .aaa(aaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .AA,\n"
     "      .aaa(aaa)\n"
     "  );\n"
     "endmodule\n"},
    {// name ports with comments
     "module m;\n"
     "foo bar(.a(a), .aa(aa)/*comment*/, .aaa(aaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aa (aa)  /*comment*/,\n"
     "      .aaa(aaa)\n"
     "  );\n"
     "endmodule\n"},
    {"module m;\n"
     "foo bar(.a(a),//comment1\n .aaa(aaa)//comment2\n);\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),   //comment1\n"
     "      .aaa(aaa)  //comment2\n"
     "  );\n"
     "endmodule\n"},
    {"module m;\n"
     "foo bar(.a(a),\n"
     " //.aa(aa),\n"
     ".aaa(aaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      //.aa(aa),\n"
     "      .aaa(aaa)\n"
     "  );\n"
     "endmodule\n"},
    {"module m;\n"
     "foo bar(\n"
     ".a(a) //comment1\n"
     ", .aaa(aaa) //comment2\n"
     ") //comment3\n"
     ";\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "        .a  (a)    //comment1\n"
     "      , .aaa(aaa)  //comment2\n"
     "  )  //comment3\n"
     "  ;\n"
     "endmodule\n"},
    {// module instantiation with all implicit connections
     "module m;\n"
     "foo bar(.a, .aa, .aaaaa);\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a,\n"
     "      .aa,\n"
     "      .aaaaa\n"
     "  );\n"
     "endmodule\n"},
    {// named ports corssed with implicit connections
     "module m;\n"
     "foo bar(.a(a), .aa, .aaaaa(aaaaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a    (a),\n"
     "      .aa,\n"
     "      .aaaaa(aaaaa)\n"
     "  );\n"
     "endmodule\n"},
    {// named ports corssed with wildcard connections
     "module m;\n"
     "foo bar(.a(a), .aaa(aaa), .*);\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aaa(aaa),\n"
     "      .*\n"
     "  );\n"
     "endmodule\n"},
    {"module m;\n"
     "foo bar(.a(a), .aa(aa), .* , .aaa(aaa));\n"
     "endmodule\n",
     "module m;\n"
     "  foo bar (\n"
     "      .a  (a),\n"
     "      .aa (aa),\n"
     "      .*,\n"
     "      .aaa(aaa)\n"
     "  );\n"
     "endmodule\n"},

    // Parameterized data types, declarations inside #() tabular alignment
    {// parameterized module with 'list_of_param_assignments'
     "module foo #(A = 2, AA = 22, AAA = 222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    A   = 2,\n"
     "    AA  = 22,\n"
     "    AAA = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'parameter_declaration'
     "module foo #(parameter int a = 2, parameter int aa = 22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter int a  = 2,\n"
     "    parameter int aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'parameter_declaration' and comments
     "module foo #(//comment\nparameter int a = 2, parameter int aa = 22);\n"
     "endmodule\n",
     "module foo #(  //comment\n"
     "    parameter int a  = 2,\n"
     "    parameter int aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'parameter_declaration' and trailing comments
     "module foo #(parameter int a = 2,//comment\n parameter int aa = 22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter int a  = 2,  //comment\n"
     "    parameter int aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'parameter_declaration' and pre-proc
     "module foo #(parameter int a = 2,\n"
     "`ifdef MACRO parameter int aa = 22, `endif\n"
     "parameter int aaa = 222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter int a   = 2,\n"
     "`ifdef MACRO\n"
     "    parameter int aa  = 22,\n"
     "`endif\n"
     "    parameter int aaa = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'parameter_declaration' and packed dimensions
     "module foo #(parameter logic [3:0] a = 2, parameter logic [30:0] aa = "
     "22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter logic [ 3:0] a  = 2,\n"
     "    parameter logic [30:0] aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'parameter_declaration' and unpacked
     // dimensions
     "module foo #(parameter logic a[3:0] = 2, parameter logic  aa [30:0] = "
     "22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter logic a [ 3:0] = 2,\n"
     "    parameter logic aa[30:0] = 22\n"
     ");\n"
     "endmodule\n"},

    {// parameterized module with 'local_parameter_declaration'
     "module foo #(localparam int a = 2, localparam int aa = 22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    localparam int a  = 2,\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'local_parameter_declaration' and comments
     "module foo #(//comment\nlocalparam int a = 2, localparam int aa = 22);\n"
     "endmodule\n",
     "module foo #(  //comment\n"
     "    localparam int a  = 2,\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'local_parameter_declaration' and trailing
     // comments
     "module foo #(localparam int a = 2,//comment\n localparam int aa = 22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    localparam int a  = 2,  //comment\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'local_parameter_declaration' and pre-proc
     "module foo #(localparam int a = 2,\n"
     "`ifdef MACRO localparam int aa = 22, `endif\n"
     "localparam int aaa = 222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    localparam int a   = 2,\n"
     "`ifdef MACRO\n"
     "    localparam int aa  = 22,\n"
     "`endif\n"
     "    localparam int aaa = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'local_parameter_declaration' and packed
     // dimensions
     "module foo #(localparam logic [3:0] a = 2, localparam logic [30:0] aa = "
     "22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    localparam logic [ 3:0] a  = 2,\n"
     "    localparam logic [30:0] aa = 22\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'local_parameter_declaration' and unpacked
     // dimensions
     "module foo #(localparam logic a[3:0] = 2, localparam logic  aa [30:0] = "
     "22);\n"
     "endmodule\n",
     "module foo #(\n"
     "    localparam logic a [ 3:0] = 2,\n"
     "    localparam logic aa[30:0] = 22\n"
     ");\n"
     "endmodule\n"},

    {// parameterized module with 'data_type list_of_param_assignments'
     "module foo #( int a = 2,  real aa = 22, longint aaa = 222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    int     a   = 2,\n"
     "    real    aa  = 22,\n"
     "    longint aaa = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // comments
     "module foo #(//comment\nint a = 2,  shortreal aa = 22, longint aaa = "
     "222);\n"
     "endmodule\n",
     "module foo #(  //comment\n"
     "    int       a   = 2,\n"
     "    shortreal aa  = 22,\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // trailing comments
     "module foo #(int a = 2,  shortreal aa = 22,//comment\n longint aaa = "
     "222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    int       a   = 2,\n"
     "    shortreal aa  = 22,  //comment\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // pre-proc
     "module foo #(int a = 2,\n"
     "`ifdef MACRO shortreal aa = 22, `endif\n"
     " longint aaa = 222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    int       a   = 2,\n"
     "`ifdef MACRO\n"
     "    shortreal aa  = 22,\n"
     "`endif\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // packed dimensions
     "module foo #(bit [1:0] a = 2,  reg [12:0] aa = 22, logic [123:0] aaa = "
     "222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    bit   [  1:0] a   = 2,\n"
     "    reg   [ 12:0] aa  = 22,\n"
     "    logic [123:0] aaa = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // unpacked dimensions
     "module foo #(bit  a[1:0] = 2,  reg  aa[12:0] = 22, logic aaa [123:0]  = "
     "222);\n"
     "endmodule\n",
     "module foo #(\n"
     "    bit   a  [  1:0] = 2,\n"
     "    reg   aa [ 12:0] = 22,\n"
     "    logic aaa[123:0] = 222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'type list_of_type_assignments'
     "module foo #(type T = int, type TT = bit, type TTT= C#(logic) );\n"
     "endmodule\n",
     "module foo #(\n"
     "    type T   = int,\n"
     "    type TT  = bit,\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'type list_of_type_assignments' and comments
     "module foo #(//comment\ntype T = int, type TT = bit, type TTT= C#(logic) "
     ");\n"
     "endmodule\n",
     "module foo #(  //comment\n"
     "    type T   = int,\n"
     "    type TT  = bit,\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // trailing comments
     "module foo #(type T = int, type TT = bit, //comment\n type TTT= "
     "C#(logic) );\n"
     "endmodule\n",
     "module foo #(\n"
     "    type T   = int,\n"
     "    type TT  = bit,       //comment\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // pre-proc
     "module foo #(type T = int,\n"
     "`ifdef MACRO type TT = bit, `endif\n"
     " type TTT= C#(logic));\n"
     "endmodule\n",
     "module foo #(\n"
     "    type T   = int,\n"
     "`ifdef MACRO\n"
     "    type TT  = bit,\n"
     "`endif\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and
     // packed dimensions
     "module foo #(type T = int [3:0], type TT = bit [250:0]);\n"
     "endmodule\n",
     "module foo #(\n"
     "    type T  = int [  3:0],\n"
     "    type TT = bit [250:0]\n"
     ");\n"
     "endmodule\n"},
    {"module foo #(type T = int, "
     "A = 2, int AA = 22, parameter AAA = 222, parameter longint AAAA = 2222, "
     "localparam AAAAA = 22222, localparam real AAAAAA = 222222"
     ");\n"
     "endmodule\n",
     "module foo #(\n"
     "               type    T      = int,\n"
     "                       A      = 2,\n"
     "               int     AA     = 22,\n"
     "    parameter          AAA    = 222,\n"
     "    parameter  longint AAAA   = 2222,\n"
     "    localparam         AAAAA  = 22222,\n"
     "    localparam real    AAAAAA = 222222\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with built-in data type
     "module foo #(int a = 2, real abc = 2234);\n"
     "endmodule\n",
     "module foo #(\n"
     "    int  a   = 2,\n"
     "    real abc = 2234\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with type
     "module foo #(type TYPE1 = int, type TYPE2 = boo);\n"
     "endmodule\n",
     "module foo #(\n"
     "    type TYPE1 = int,\n"
     "    type TYPE2 = boo\n"
     ");\n"
     "endmodule\n"},
    {"module foo#(localparam type TYPE1 = int, type TYPE22 = bool, parameter   "
     " type TYPE333 = real);\n"
     "endmodule\n",
     "module foo #(\n"
     "    localparam type TYPE1   = int,\n"
     "               type TYPE22  = bool,\n"
     "    parameter  type TYPE333 = real\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and 1D
     // packed dimensions
     "module foo #(parameter type T = int [3:0], type TT = bit [123:0]);\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter type T  = int [  3:0],\n"
     "              type TT = bit [123:0]\n"
     ");\n"
     "endmodule\n"},
    {// parameterized module with 'data_type list_of_param_assignments' and 2D
     // packed dimensions
     "module foo #(type T = int [3:0][123:0], type TT = bit [123:0][1:0]);\n"
     "endmodule\n",
     "module foo #(\n"
     "    type T  = int [  3:0][123:0],\n"
     "    type TT = bit [123:0][  1:0]\n"
     ");\n"
     "endmodule\n"},
    {// parametrized module with user defined data types
     "module foo #(type T = my_type1_t, type TT = my_pkg::my_type2_t);\n"
     "endmodule\n",
     "module foo #(\n"
     "    type T  = my_type1_t,\n"
     "    type TT = my_pkg::my_type2_t\n"
     ");\n"
     "endmodule\n"},

    {// parameterized class with 'list_of_param_assignments'
     "class foo #(A = 2, AA = 22, AAA = 222);\n"
     "endclass\n",
     "class foo #(\n"
     "    A   = 2,\n"
     "    AA  = 22,\n"
     "    AAA = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'parameter_declaration'
     "class foo #(parameter int a = 2, parameter int aa = 22);\n"
     "endclass\n",
     "class foo #(\n"
     "    parameter int a  = 2,\n"
     "    parameter int aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'parameter_declaration' and comments
     "class foo #(//comment\nparameter int a = 2, parameter int aa = 22);\n"
     "endclass\n",
     "class foo #(  //comment\n"
     "    parameter int a  = 2,\n"
     "    parameter int aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'parameter_declaration' and trailing comments
     "class foo #(parameter int a = 2,//comment\n parameter int aa = 22);\n"
     "endclass\n",
     "class foo #(\n"
     "    parameter int a  = 2,  //comment\n"
     "    parameter int aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'parameter_declaration' and pre-proc
     "class foo #(parameter int a = 2,\n"
     "`ifdef MACRO parameter int aa = 22, `endif\n"
     "parameter int aaa = 222);\n"
     "endclass\n",
     "class foo #(\n"
     "    parameter int a   = 2,\n"
     "`ifdef MACRO\n"
     "    parameter int aa  = 22,\n"
     "`endif\n"
     "    parameter int aaa = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'parameter_declaration' and packed dimensions
     "class foo #(parameter logic [3:0] a = 2, parameter logic [30:0] aa = "
     "22);\n"
     "endclass\n",
     "class foo #(\n"
     "    parameter logic [ 3:0] a  = 2,\n"
     "    parameter logic [30:0] aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'parameter_declaration' and unpacked dimensions
     "class foo #(parameter logic a[3:0] = 2, parameter logic  aa [30:0] = "
     "22);\n"
     "endclass\n",
     "class foo #(\n"
     "    parameter logic a [ 3:0] = 2,\n"
     "    parameter logic aa[30:0] = 22\n"
     ");\n"
     "endclass\n"},

    {// parameterized class with 'local_parameter_declaration'
     "class foo #(localparam int a = 2, localparam int aa = 22);\n"
     "endclass\n",
     "class foo #(\n"
     "    localparam int a  = 2,\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'local_parameter_declaration' and comments
     "class foo #(//comment\nlocalparam int a = 2, localparam int aa = 22);\n"
     "endclass\n",
     "class foo #(  //comment\n"
     "    localparam int a  = 2,\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'local_parameter_declaration' and trailing
     // comments
     "class foo #(localparam int a = 2,//comment\n localparam int aa = 22);\n"
     "endclass\n",
     "class foo #(\n"
     "    localparam int a  = 2,  //comment\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'local_parameter_declaration' and pre-proc
     "class foo #(localparam int a = 2,\n"
     "`ifdef MACRO localparam int aa = 22, `endif\n"
     "localparam int aaa = 222);\n"
     "endclass\n",
     "class foo #(\n"
     "    localparam int a   = 2,\n"
     "`ifdef MACRO\n"
     "    localparam int aa  = 22,\n"
     "`endif\n"
     "    localparam int aaa = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'local_parameter_declaration' and packed
     // dimensions
     "class foo #(localparam logic [3:0] a = 2, localparam logic [30:0] aa = "
     "22);\n"
     "endclass\n",
     "class foo #(\n"
     "    localparam logic [ 3:0] a  = 2,\n"
     "    localparam logic [30:0] aa = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'local_parameter_declaration' and unpacked
     // dimensions
     "class foo #(localparam logic a[3:0] = 2, localparam logic  aa [30:0] = "
     "22);\n"
     "endclass\n",
     "class foo #(\n"
     "    localparam logic a [ 3:0] = 2,\n"
     "    localparam logic aa[30:0] = 22\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments'
     "class foo #( int a = 2,  real aa = 22, longint aaa = 222);\n"
     "endclass\n",
     "class foo #(\n"
     "    int     a   = 2,\n"
     "    real    aa  = 22,\n"
     "    longint aaa = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // comments
     "class foo #(//comment\nint a = 2,  shortreal aa = 22, longint aaa = "
     "222);\n"
     "endclass\n",
     "class foo #(  //comment\n"
     "    int       a   = 2,\n"
     "    shortreal aa  = 22,\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // trailing comments
     "class foo #(int a = 2,  shortreal aa = 22,//comment\n longint aaa = "
     "222);\n"
     "endclass\n",
     "class foo #(\n"
     "    int       a   = 2,\n"
     "    shortreal aa  = 22,  //comment\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // pre-proc
     "class foo #(int a = 2,\n"
     "`ifdef MACRO shortreal aa = 22, `endif\n"
     " longint aaa = 222);\n"
     "endclass\n",
     "class foo #(\n"
     "    int       a   = 2,\n"
     "`ifdef MACRO\n"
     "    shortreal aa  = 22,\n"
     "`endif\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // packed dimensions
     "class foo #(bit [1:0] a = 2,  reg [12:0] aa = 22, logic [123:0] aaa = "
     "222);\n"
     "endclass\n",
     "class foo #(\n"
     "    bit   [  1:0] a   = 2,\n"
     "    reg   [ 12:0] aa  = 22,\n"
     "    logic [123:0] aaa = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // unpacked dimensions
     "class foo #(bit  a[1:0] = 2,  reg  aa[12:0] = 22, logic aaa [123:0]  = "
     "222);\n"
     "endclass\n",
     "class foo #(\n"
     "    bit   a  [  1:0] = 2,\n"
     "    reg   aa [ 12:0] = 22,\n"
     "    logic aaa[123:0] = 222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'type list_of_type_assignments'
     "class foo #(type T = int, type TT = bit, type TTT= C#(logic) );\n"
     "endclass\n",
     "class foo #(\n"
     "    type T   = int,\n"
     "    type TT  = bit,\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'type list_of_type_assignments' and comments
     "class foo #(//comment\ntype T = int, type TT = bit, type TTT= C#(logic) "
     ");\n"
     "endclass\n",
     "class foo #(  //comment\n"
     "    type T   = int,\n"
     "    type TT  = bit,\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // trailing comments
     "class foo #(type T = int, type TT = bit, //comment\n type TTT= C#(logic) "
     ");\n"
     "endclass\n",
     "class foo #(\n"
     "    type T   = int,\n"
     "    type TT  = bit,       //comment\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // pre-proc
     "class foo #(type T = int,\n"
     "`ifdef MACRO type TT = bit, `endif\n"
     " type TTT= C#(logic));\n"
     "endclass\n",
     "class foo #(\n"
     "    type T   = int,\n"
     "`ifdef MACRO\n"
     "    type TT  = bit,\n"
     "`endif\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and
     // packed dimensions
     "class foo #(type T = int [3:0], type TT = bit [250:0]);\n"
     "endclass\n",
     "class foo #(\n"
     "    type T  = int [  3:0],\n"
     "    type TT = bit [250:0]\n"
     ");\n"
     "endclass\n"},
    {"class foo #(type T = int, "
     "A = 2, int AA = 22, parameter AAA = 222, parameter longint AAAA = 2222, "
     "localparam AAAAA = 22222, localparam real AAAAAA = 222222"
     ");\n"
     "endclass\n",
     "class foo #(\n"
     "               type    T      = int,\n"
     "                       A      = 2,\n"
     "               int     AA     = 22,\n"
     "    parameter          AAA    = 222,\n"
     "    parameter  longint AAAA   = 2222,\n"
     "    localparam         AAAAA  = 22222,\n"
     "    localparam real    AAAAAA = 222222\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with built-in data type
     "class foo #(int a = 2, real abc = 2234);\n"
     "endclass\n",
     "class foo #(\n"
     "    int  a   = 2,\n"
     "    real abc = 2234\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with type
     "class foo #(type TYPE1 = int, type TYPE2 = boo);\n"
     "endclass\n",
     "class foo #(\n"
     "    type TYPE1 = int,\n"
     "    type TYPE2 = boo\n"
     ");\n"
     "endclass\n"},
    {"class foo#(localparam type TYPE1 = int, type TYPE22 = bool, parameter    "
     "type TYPE333 = real);\n"
     "endclass\n",
     "class foo #(\n"
     "    localparam type TYPE1   = int,\n"
     "               type TYPE22  = bool,\n"
     "    parameter  type TYPE333 = real\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and 1D
     // packed dimensions
     "class foo #(parameter type T = int [3:0], type TT = bit [123:0]);\n"
     "endclass\n",
     "class foo #(\n"
     "    parameter type T  = int [  3:0],\n"
     "              type TT = bit [123:0]\n"
     ");\n"
     "endclass\n"},
    {// parameterized class with 'data_type list_of_param_assignments' and 2D
     // packed dimensions
     "class foo #(type T = int [3:0][123:0], type TT = bit [123:0][1:0]);\n"
     "endclass\n",
     "class foo #(\n"
     "    type T  = int [  3:0][123:0],\n"
     "    type TT = bit [123:0][  1:0]\n"
     ");\n"
     "endclass\n"},
    {// parametrized class with user defined data types
     "class foo #(type T = my_type1_t, type TT = my_pkg::my_type2_t);\n"
     "endclass\n",
     "class foo #(\n"
     "    type T  = my_type1_t,\n"
     "    type TT = my_pkg::my_type2_t\n"
     ");\n"
     "endclass\n"},

    {// parameterized interface with 'local_parameter_declaration'
     "interface foo #(localparam int a = 2, localparam int aa = 22);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    localparam int a  = 2,\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'local_parameter_declaration' and comments
     "interface foo #(//comment\nlocalparam int a = 2, localparam int aa = "
     "22);\n"
     "endinterface\n",
     "interface foo #(  //comment\n"
     "    localparam int a  = 2,\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'local_parameter_declaration' and trailing
     // comments
     "interface foo #(localparam int a = 2,//comment\n localparam int aa = "
     "22);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    localparam int a  = 2,  //comment\n"
     "    localparam int aa = 22\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'local_parameter_declaration' and pre-proc
     "interface foo #(localparam int a = 2,\n"
     "`ifdef MACRO localparam int aa = 22, `endif\n"
     "localparam int aaa = 222);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    localparam int a   = 2,\n"
     "`ifdef MACRO\n"
     "    localparam int aa  = 22,\n"
     "`endif\n"
     "    localparam int aaa = 222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'local_parameter_declaration' and packed
     // dimensions
     "interface foo #(localparam logic [3:0] a = 2, localparam logic [30:0] aa "
     "= 22);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    localparam logic [ 3:0] a  = 2,\n"
     "    localparam logic [30:0] aa = 22\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'local_parameter_declaration' and unpacked
     // dimensions
     "interface foo #(localparam logic a[3:0] = 2, localparam logic  aa [30:0] "
     "= 22);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    localparam logic a [ 3:0] = 2,\n"
     "    localparam logic aa[30:0] = 22\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments'
     "interface foo #( int a = 2,  real aa = 22, longint aaa = 222);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    int     a   = 2,\n"
     "    real    aa  = 22,\n"
     "    longint aaa = 222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // comments
     "interface foo #(//comment\nint a = 2,  shortreal aa = 22, longint aaa = "
     "222);\n"
     "endinterface\n",
     "interface foo #(  //comment\n"
     "    int       a   = 2,\n"
     "    shortreal aa  = 22,\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // trailing comments
     "interface foo #(int a = 2,  shortreal aa = 22,//comment\n longint aaa = "
     "222);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    int       a   = 2,\n"
     "    shortreal aa  = 22,  //comment\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // pre-proc
     "interface foo #(int a = 2,\n"
     "`ifdef MACRO shortreal aa = 22, `endif\n"
     " longint aaa = 222);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    int       a   = 2,\n"
     "`ifdef MACRO\n"
     "    shortreal aa  = 22,\n"
     "`endif\n"
     "    longint   aaa = 222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // packed dimensions
     "interface foo #(bit [1:0] a = 2,  reg [12:0] aa = 22, logic [123:0] aaa "
     "= 222);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    bit   [  1:0] a   = 2,\n"
     "    reg   [ 12:0] aa  = 22,\n"
     "    logic [123:0] aaa = 222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // unpacked dimensions
     "interface foo #(bit  a[1:0] = 2,  reg  aa[12:0] = 22, logic aaa [123:0]  "
     "= 222);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    bit   a  [  1:0] = 2,\n"
     "    reg   aa [ 12:0] = 22,\n"
     "    logic aaa[123:0] = 222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'type list_of_type_assignments'
     "interface foo #(type T = int, type TT = bit, type TTT= C#(logic) );\n"
     "endinterface\n",
     "interface foo #(\n"
     "    type T   = int,\n"
     "    type TT  = bit,\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'type list_of_type_assignments' and
     // comments
     "interface foo #(//comment\ntype T = int, type TT = bit, type TTT= "
     "C#(logic) );\n"
     "endinterface\n",
     "interface foo #(  //comment\n"
     "    type T   = int,\n"
     "    type TT  = bit,\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // trailing comments
     "interface foo #(type T = int, type TT = bit, //comment\n type TTT= "
     "C#(logic) );\n"
     "endinterface\n",
     "interface foo #(\n"
     "    type T   = int,\n"
     "    type TT  = bit,       //comment\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // pre-proc
     "interface foo #(type T = int,\n"
     "`ifdef MACRO type TT = bit, `endif\n"
     " type TTT= C#(logic));\n"
     "endinterface\n",
     "interface foo #(\n"
     "    type T   = int,\n"
     "`ifdef MACRO\n"
     "    type TT  = bit,\n"
     "`endif\n"
     "    type TTT = C#(logic)\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // packed dimensions
     "interface foo #(type T = int [3:0], type TT = bit [250:0]);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    type T  = int [  3:0],\n"
     "    type TT = bit [250:0]\n"
     ");\n"
     "endinterface\n"},
    {"interface foo #(type T = int, "
     "A = 2, int AA = 22, parameter AAA = 222, parameter longint AAAA = 2222, "
     "localparam AAAAA = 22222, localparam real AAAAAA = 222222"
     ");\n"
     "endinterface\n",
     "interface foo #(\n"
     "               type    T      = int,\n"
     "                       A      = 2,\n"
     "               int     AA     = 22,\n"
     "    parameter          AAA    = 222,\n"
     "    parameter  longint AAAA   = 2222,\n"
     "    localparam         AAAAA  = 22222,\n"
     "    localparam real    AAAAAA = 222222\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with built-in data type
     "interface foo #(int a = 2, real abc = 2234);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    int  a   = 2,\n"
     "    real abc = 2234\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with type
     "interface foo #(type TYPE1 = int, type TYPE2 = boo);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    type TYPE1 = int,\n"
     "    type TYPE2 = boo\n"
     ");\n"
     "endinterface\n"},
    {"interface foo#(localparam type TYPE1 = int, type TYPE22 = bool, "
     "parameter    type TYPE333 = real);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    localparam type TYPE1   = int,\n"
     "               type TYPE22  = bool,\n"
     "    parameter  type TYPE333 = real\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // 1D packed dimensions
     "interface foo #(parameter type T = int [3:0], type TT = bit [123:0]);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    parameter type T  = int [  3:0],\n"
     "              type TT = bit [123:0]\n"
     ");\n"
     "endinterface\n"},
    {// parameterized interface with 'data_type list_of_param_assignments' and
     // 2D packed dimensions
     "interface foo #(type T = int [3:0][123:0], type TT = bit [123:0][1:0]);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    type T  = int [  3:0][123:0],\n"
     "    type TT = bit [123:0][  1:0]\n"
     ");\n"
     "endinterface\n"},
    {// parametrized interface with user defined data types
     "interface foo #(type T = my_type1_t, type TT = my_pkg::my_type2_t);\n"
     "endinterface\n",
     "interface foo #(\n"
     "    type T  = my_type1_t,\n"
     "    type TT = my_pkg::my_type2_t\n"
     ");\n"
     "endinterface\n"},
    {// wildcard import package at module header
     "module foo import bar::*; (baz); endmodule\n",
     "module foo\n"
     "  import bar::*;\n"
     "(\n"
     "    baz\n"
     ");\n"
     "endmodule\n"},
    {// import package at module header
     "module foo import bar::baz; (qux); endmodule\n",
     "module foo\n"
     "  import bar::baz;\n"
     "(\n"
     "    qux\n"
     ");\n"
     "endmodule\n"},
    {// wildcard import multiple packages at module header
     "module foo import bar::*,baz::*; (qux); endmodule\n",
     "module foo\n"
     "  import bar::*, baz::*;\n"
     "(\n"
     "    qux\n"
     ");\n"
     "endmodule\n"},
    {// separate package import declarations in module header
     "module foo import bar::*,baz::*; import q_pkg::qux; (qux); endmodule\n",
     "module foo\n"
     "  import bar::*, baz::*;\n"
     "  import q_pkg::qux;\n"
     "(\n"
     "    qux\n"
     ");\n"
     "endmodule\n"},
    {// import package at module header
     "module foo import bar::baz; #(int p = 3)(qux); endmodule\n",
     "module foo\n"
     "  import bar::baz;\n"
     "#(\n"
     "    int p = 3\n"
     ") (\n"
     "    qux\n"
     ");\n"
     "endmodule\n"},
    // Space between return keyword and return value
    {"function int foo(logic [31:0] data); return{<<8{data}}; endfunction",
     "function int foo(logic [31:0] data);\n"
     "  return {<<8{data}};\n"
     "endfunction\n"},
    {"function int f;return(1);endfunction",
     "function int f;\n"
     "  return (1);\n"
     "endfunction\n"},
    {"function int f;return-1;endfunction",
     "function int f;\n"
     "  return -1;\n"
     "endfunction\n"},
    {"function int f ;return    ! x\n;endfunction",
     "function int f;\n"
     "  return !x;\n"
     "endfunction\n"},
    {"function int f ;return    ~ x\n;endfunction",
     "function int f;\n"
     "  return ~x;\n"
     "endfunction\n"},
    {"function int f ;return    $x\n;endfunction",
     "function int f;\n"
     "  return $x;\n"
     "endfunction\n"},
    // String initializers
    {"string a[] = {\n\"a\"\n};\n", "string a[] = {\"a\"};\n"},
    {"string abc[] = {\n\"a\",\n\"b\",\n\"c\"\n};\n",
     "string abc[] = {\"a\", \"b\", \"c\"};\n"},
    {"string abc[] = {\n"
     "\"a\",//\n"
     "\"b\", \"c\"\n"
     "};\n",
     "string abc[] = {\"a\",  //\n"
     "                \"b\",\n"
     "                \"c\"};\n"},
    {"string abc[] = {//\n"
     "\"a\", \"b\", \"c\""
     "};\n",
     "string abc[] = {  //\n"
     "  \"a\",\n"
     "  \"b\",\n"
     "  \"c\"\n"
     "};\n"},
    {"string abc[] = {\n"
     "\"a\", \"b\", \"c\"//\n"
     "};\n",
     "string abc[] = {\"a\",\n"
     "                \"b\",\n"
     "                \"c\"  //\n"
     "                };\n"},
    {"string abc[] = {//\n"
     "\"a\",//\n"
     "\"b\", \"c\"//\n"
     "};\n",
     "string abc[] = {  //\n"
     "  \"a\",  //\n"
     "  \"b\",\n"
     "  \"c\"  //\n"
     "};\n"},
    {"string abc[] = {\n"
     "\"a\",\n"
     "// comment\n"
     "// comment\n"
     "\"b\",\n"
     "\"c\"\n"
     "};\n",
     "string abc[] = {\"a\",\n"
     "                // comment\n"
     "                // comment\n"
     "                \"b\",\n"
     "                \"c\"};\n"},
    {"string numbers[] = {\"one\", \"two\", \"three\", \"four\"};\n",
     "string numbers[] = {\"one\",\n"
     "                    \"two\",\n"
     "                    \"three\",\n"
     "                    \"four\"};\n"},
    {"string numbers[] = {\"one\", \"two\", THREE, \"four\"};\n",
     "string numbers[] = {\n"
     "  \"one\", \"two\", THREE, \"four\"\n"
     "};\n"},
    {"string numbers[] = {\"one\", {\"two\", \"three\"}, \"four\"};\n",
     "string numbers[] = {\n"
     "  \"one\", {\"two\", \"three\"}, \"four\"\n"
     "};\n"},
    {"string numbers[] = {\"one\", {\"two\", //\n"
     "\"three\"}, \"four\"};\n",
     "string numbers[] = {\n"
     "  \"one\",\n"
     "  {\n"
     "    \"two\",  //\n"
     "    \"three\"\n"
     "  },\n"
     "  \"four\"\n"
     "};\n"},
    {"string years[] = {\"two_thousand_nineteen\", \"two_thousand_twenty\",\n"
     "\"two_thousand_twenty_one\"};\n",
     // line with 3rd string would exceed column limit in unwrapped style
     "string years[] = {\n"
     "  \"two_thousand_nineteen\",\n"
     "  \"two_thousand_twenty\",\n"
     "  \"two_thousand_twenty_one\"\n"
     "};\n"},
    {"class class_name;\n"
     "var_type var_name = new(\"the_string\");\n"
     "endclass\n",
     "class class_name;\n"
     "  var_type var_name = new(\"the_string\");\n"
     "endclass\n"},

    //{   // parameterized class with 'parameter_declaration' and MACRO
    //    "class foo #(parameter int a = 2,\n"
    //    "parameter int aaa = `MACRO);\n"
    //    "endclass\n",
    //    "class foo #(\n"
    //    "    parameter int a   = 2,\n"
    //    "    parameter int aaa = `MACRO\n"
    //    ");\n"
    //    "endclass\n"
    //},
    // Struct/Union alignment
    {"typedef struct {\n"
     "bit [3:0] first; bit [31:0] second; generic_type_name_t third;\n"
     "} type_t;",
     "typedef struct {\n"
     "  bit [3:0]           first;\n"
     "  bit [31:0]          second;\n"
     "  generic_type_name_t third;\n"
     "} type_t;\n"},
    {"typedef struct {\n"
     "// comment\n"
     "bit [3:0] first; bit [31:0] second; generic_type_name_t third;\n"
     "} type_t;",
     "typedef struct {\n"
     "  // comment\n"
     "  bit [3:0]           first;\n"
     "  bit [31:0]          second;\n"
     "  generic_type_name_t third;\n"
     "} type_t;\n"},
    {"typedef struct {\n"
     "// comment 0\n"
     "bit [31:0] first; // a\n"
     "bit [31:0] second; // b\n"
     "bit third; // c\n"
     "uint fourth; // d\n"
     "\n"
     "// comment 1\n"
     "int fifth;\n"
     "// comment 2.1\n"
     "// comment 2.2\n"
     "uint sixth;\n"
     "} timing_cfg_t;",
     "typedef struct {\n"
     "  // comment 0\n"
     "  bit [31:0] first;   // a\n"
     "  bit [31:0] second;  // b\n"
     "  bit        third;   // c\n"
     "  uint       fourth;  // d\n"
     "\n"
     "  // comment 1\n"
     "  int  fifth;\n"
     "  // comment 2.1\n"
     "  // comment 2.2\n"
     "  uint sixth;\n"
     "} timing_cfg_t;\n"},
    {"typedef struct {\n"
     "// comment\n"
     "rand int r;\n"
     "int a;\n"
     "int aa = 0;\n"
     "int aaa = 1; // comment\n"
     "foo#(bar) z;\n"
     "int [x:y] zz; // comment\n"
     "int zzz[a:b];\n"
     "} type_t;",
     "typedef struct {\n"
     "  // comment\n"
     "  rand int   r;\n"
     "  int        a;\n"
     "  int        aa        = 0;\n"
     "  int        aaa       = 1;  // comment\n"
     "  foo #(bar) z;\n"
     "  int [x:y]  zz;             // comment\n"
     "  int        zzz[a:b];\n"
     "} type_t;\n"},
    {"typedef struct packed {\n"
     "struct packed { bit q; logic qq; logic qqq; } a_few_qs;\n"
     "struct packed {\n"
     "logic [1:0]  q;\n"
     "} one_q;\n"
     "int q;\n"
     "uint qq;\n"
     "} nested_qs_t;",
     "typedef struct packed {\n"
     "  struct packed {\n"
     "    bit   q;\n"
     "    logic qq;\n"
     "    logic qqq;\n"
     "  } a_few_qs;\n"
     "  struct packed {logic [1:0] q;} one_q;\n"
     "  int  q;\n"
     "  uint qq;\n"
     "} nested_qs_t;\n"},
    {"typedef struct packed {\n"
     "struct packed { bit q; logic qq; logic qqq; } a_few_qs;\n"
     "struct packed {\n"
     "// comment\n"
     "logic [1:0]  q;\n"
     "} one_q;\n"
     "int q;\n"
     "uint qq;\n"
     "} nested_qs_t;",
     "typedef struct packed {\n"
     "  struct packed {\n"
     "    bit   q;\n"
     "    logic qq;\n"
     "    logic qqq;\n"
     "  } a_few_qs;\n"
     "  struct packed {\n"
     "    // comment\n"
     "    logic [1:0] q;\n"
     "  } one_q;\n"
     "  int  q;\n"
     "  uint qq;\n"
     "} nested_qs_t;\n"},
    {"typedef struct {bit [3:0] first;\n"
     "`ifdef MACRO\n"
     "bit [31:0] second; generic_type_name_t third;\n"
     "`endif\n"
     "} type_t;\n",
     "typedef struct {\n"
     "  bit [3:0]           first;\n"
     "`ifdef MACRO\n"
     "  bit [31:0]          second;\n"
     "  generic_type_name_t third;\n"
     "`endif\n"
     "} type_t;\n"},
    {"typedef struct {\n"
     "bit [3:0] first // c\n"
     "; bit [31:0] second"
     "// c\n"
     "; generic_type_name_t third // c\n"
     ";} type_t;\n",
     "typedef struct {\n"
     "  bit [3:0]           first  // c\n"
     ";\n"
     "  bit [31:0]          second  // c\n"
     ";\n"
     "  generic_type_name_t third  // c\n"
     ";\n"
     "} type_t;\n"},
    // Continuation comment alignment
    {"`define BAR 1 // A\n"
     "module foo(); // B\n"
     "wire baz;     // C\n"
     "endmodule:foo // D\n",
     "`define BAR 1 // A\n"
     "module foo ();  // B\n"
     "  wire baz;  // C\n"
     "endmodule : foo  // D\n"},
    {"`define BAR 1 // A\n"
     "module foo(); // B\n"
     "              // B.1\n"
     "              // B.2\n"
     "wire baz;     // C\n"
     "              // C.1\n"
     "              // C.2\n"
     "endmodule:foo // D\n"
     "              // D.1\n"
     "              // D.2\n",
     "`define BAR 1 // A\n"
     "module foo ();  // B\n"
     "                // B.1\n"
     "                // B.2\n"
     "  wire baz;  // C\n"
     "             // C.1\n"
     "             // C.2\n"
     "endmodule : foo  // D\n"
     "                 // D.1\n"
     "                 // D.2\n"},
    {"// W\n"
     "`define BAR 1 // A\n"
     "   // X\n"
     "module foo(); // B\n"
     "              // B.1\n"
     "              // B.2\n"
     " // Y\n"
     "wire baz;     // C\n"
     "              // C.1\n"
     "              // C.2\n"
     "    // Z\n"
     "endmodule:foo // D\n"
     "              // D.1\n"
     "              // D.2\n",
     "// W\n"
     "`define BAR 1 // A\n"
     "// X\n"
     "module foo ();  // B\n"
     "                // B.1\n"
     "                // B.2\n"
     "  // Y\n"
     "  wire baz;  // C\n"
     "             // C.1\n"
     "             // C.2\n"
     "  // Z\n"
     "endmodule : foo  // D\n"
     "                 // D.1\n"
     "                 // D.2\n"},
    {"module foo( // A\n"
     "            // A.1\n"
     "// X\n"
     "input wire i1 [a:b], // B\n"
     "                     // B.1\n"
     "input [c:d] i2, // C\n"
     "                // C.1\n"
     "\n"
     "// Y\n"
     "output reg o1 // D\n"
     "              // D.1\n"
     ");endmodule:foo\n",
     "module foo (  // A\n"
     "              // A.1\n"
     "    // X\n"
     "    input wire       i1[a:b],  // B\n"
     "                               // B.1\n"
     "    input      [c:d] i2,       // C\n"
     "                               // C.1\n"
     "\n"
     "    // Y\n"
     "    output reg o1  // D\n"
     "                   // D.1\n"
     ");\n"
     "endmodule : foo\n"},
    // Continuation comment's original starting column is allowed to differ from
    // starting comment's original starting column at most by 1.
    // Starting column of comments B and C will change after formatting.
    // TODO: refine the column alignment depending on current vs previous line.
    // https://github.com/chipsalliance/verible/pull/858#discussion_r672844015
    {"module foo ();  // A\n"     // Starting comment; already on correct column
     "                 // A.1\n"  // A's column + 1
     "               // A.2\n"    // A's column - 1
     "wire baz;      // B\n"      // Starting comment; will be moved left
     "              // B.1\n"     // B's column - 1
     "                // B.2\n"   // B's column + 1
     "               // B.3\n"    // B's column
     "endmodule:foo // C\n"       // Starting comment; will be moved right
     "               // C.1\n"    // C's column + 1
     "             // C.2\n",     // C's column - 1
     "module foo ();  // A\n"
     "                // A.1\n"
     "                // A.2\n"
     "  wire baz;  // B\n"
     "             // B.1\n"
     "             // B.2\n"
     "             // B.3\n"
     "endmodule : foo  // C\n"
     "                 // C.1\n"
     "                 // C.2\n"},
    // Check that comments with too large starting column difference are not
    // aligned as continuation comments.
    // Check that starting comments are not linked with a comment in
    // comment-only line above them, even when the starting column is the same.
    // All comments in this test case are aligned independently, i.e. none are
    // "continuation comments".
    {"                // comment1\n"  // A's column, but is above it
     "module foo ();  // A\n"
     "                  // comment2\n"  // A's column + 2
     "              // comment3\n"      // A's column - 2
     "wire baz;     // B\n"
     "            // comment4\n"      // B's column - 2
     "                // comment5\n"  // B's column + 2
     "              // comment6\n"    // B's column, but not directly under B
     "endmodule:foo // C\n"
     "                // comment7\n"  // C's column + 2
     "              // comment8\n",   // C's column, but not directly under B
     "// comment1\n"
     "module foo ();  // A\n"
     "  // comment2\n"
     "  // comment3\n"
     "  wire baz;  // B\n"
     "  // comment4\n"
     "  // comment5\n"
     "  // comment6\n"
     "endmodule : foo  // C\n"
     "// comment7\n"
     "// comment8\n"},
    // Continuation comment alignment when a line with the starting comment is
    // wrapped.
    {"module foo(output logic very_very_very_very_long_name // A\n"
     "                                                      // A.1\n"
     "); endmodule\n",
     "module foo (\n"
     "    output logic\n"
     "        very_very_very_very_long_name  // A\n"
     "                                       // A.1\n"
     ");\n"
     "endmodule\n"},

    // Attachment of ',' to elements in enum list (with and without comments)
    {"typedef enum {\n"
     "  first , // c1\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first,  // c1\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first ,\n"
     "  // c1\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first,\n"
     "  // c1\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first // c1\n"
     "  , second\n"
     "} e;\n",
     "typedef enum {\n"
     "    first   // c1\n"
     "  , second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first\n"
     "  // c1\n"
     "  , second\n"
     "} e;\n",
     "typedef enum {\n"
     "    first\n"
     "  // c1\n"
     "  , second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first // c1\n"
     "  , // c2\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first   // c1\n"
     "  ,  // c2\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first\n"
     "  // c1\n"
     "  ,\n"
     "  // c2\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first\n"
     "  // c1\n"
     "  ,  // c2\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first\n"
     "  // c1\n"
     "  , // c2\n"
     "  // c3\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first\n"
     "  // c1\n"
     "  ,  // c2\n"
     "  // c3\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  // c1\n"
     "  first\n"
     "  // c2\n"
     "  , // c3\n"
     "  // c4\n"
     "  second\n"
     "  // c5\n"
     "} e;\n",
     "typedef enum {\n"
     "  // c1\n"
     "  first\n"
     "  // c2\n"
     "  ,  // c3\n"
     "  // c4\n"
     "  second\n"
     "  // c5\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  // c1\n"
     "  // c1+\n"
     "  first // c2\n"
     "        // c2+\n"
     "  , // c3\n"
     "    // c3+\n"
     "  // c4\n"
     "  // c4+\n"
     "  second // c5\n"
     "         // c5+\n"
     "} e;\n",
     "typedef enum {\n"
     "  // c1\n"
     "  // c1+\n"
     "  first   // c2\n"
     "          // c2+\n"
     "  ,  // c3\n"
     "     // c3+\n"
     "  // c4\n"
     "  // c4+\n"
     "  second  // c5\n"
     "          // c5+\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first , /* c1 */\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first,  /* c1 */\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first ,\n"
     "  /* c1 */\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first,\n"
     "  /* c1 */\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first /* c1 */\n"
     "  , second\n"
     "} e;\n",
     "typedef enum {\n"
     "    first   /* c1 */\n"
     "  , second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first\n"
     "  /* c1 */\n"
     "  , second\n"
     "} e;\n",
     "typedef enum {\n"
     "    first\n"
     "  /* c1 */\n"
     "  , second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first /* c1 */\n"
     "  , /* c2 */\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first   /* c1 */\n"
     "  ,  /* c2 */\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  first\n"
     "  /* c1 */\n"
     "  ,\n"
     "  /* c2 */\n"
     "  second\n"
     "} e;\n",
     "typedef enum {\n"
     "  first\n"
     "  /* c1 */\n"
     "  ,\n"
     "  /* c2 */\n"
     "  second\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  /* c1 */\n"
     "  first\n"
     "  /* c2 */\n"
     "  , /* c3 */\n"
     "  /* c4 */\n"
     "  second\n"
     "  /* c5 */\n"
     "} e;\n",
     "typedef enum {\n"
     "  /* c1 */\n"
     "  first\n"
     "  /* c2 */\n"
     "  ,  /* c3 */\n"
     "  /* c4 */\n"
     "  second\n"
     "  /* c5 */\n"
     "} e;\n"},
    {"typedef enum {\n"
     "  /* c1  */\n"
     "  /* c1+ */\n"
     "  first /* c2  */\n"
     "        /* c2+ */\n"
     "  , /* c3  */\n"
     "    /* c3+ */\n"
     "  /* c4  */\n"
     "  /* c4+ */\n"
     "  second /* c5  */\n"
     "         /* c5+ */\n"
     "} e;\n",
     "typedef enum {\n"
     "  /* c1  */\n"
     "  /* c1+ */\n"
     "  first   /* c2  */\n"
     "  /* c2+ */\n"
     "  ,  /* c3  */\n"
     "  /* c3+ */\n"
     "  /* c4  */\n"
     "  /* c4+ */\n"
     "  second  /* c5  */\n"
     "  /* c5+ */\n"
     "} e;\n"},
    {"module m;\n"
     "typedef enum {\n"
     "  first,\n"
     "  second\n"
     "} // c\n"
     "e;\n"
     "endmodule\n",
     "module m;\n"
     "  typedef enum {\n"
     "    first,\n"
     "    second\n"
     "  }  // c\n"
     "  e;\n"
     "endmodule\n"},
    // Attachment of ';' preceded by EOL comment
    {"module m;\n"
     "typedef enum {\n"
     "  first,\n"
     "  second\n"
     "} e // c\n"
     ";\n"
     "endmodule\n",
     "module m;\n"
     "  typedef enum {\n"
     "    first,\n"
     "    second\n"
     "  } e  // c\n"
     "  ;\n"
     "endmodule\n"},
    {"module m;\n"
     "typedef enum {\n"
     "  first,\n"
     "  second\n"
     "} // c1\n"
     "e // c2\n"
     ";\n"
     "endmodule\n",
     "module m;\n"
     "  typedef enum {\n"
     "    first,\n"
     "    second\n"
     "  }  // c1\n"
     "  e  // c2\n"
     "  ;\n"
     "endmodule\n"},
    {"assign foo = bar\n"
     "// comment\n"
     ";\n",
     "assign foo = bar\n"
     "    // comment\n"
     "    ;\n"},
    {"assign foo = bar // comment\n"
     ";\n",
     "assign foo = bar  // comment\n"
     ";\n"},

    // Attachment of ',' to elements in PortActualList (with and without
    // comments)
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1) // c1\n"
     "    ,\n"
     "    .second(2) // c2\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)  // c1\n"
     "      , .second(2)  // c2\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1) // c1\n"
     "    , .second(2) // c2\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)  // c1\n"
     "      , .second(2)  // c2\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1)\n"
     "    // c1\n"
     "    ,\n"
     "    .second(2)\n"
     "    // c2\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)\n"
     "      // c1\n"
     "      , .second(2)\n"
     "      // c2\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1)\n"
     "    // c1\n"
     "    , .second(2)\n"
     "    // c2\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)\n"
     "      // c1\n"
     "      , .second(2)\n"
     "      // c2\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1)\n"
     "    // c1\n"
     "    , // c2\n"
     "    .second(2)\n"
     "    // c3\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "      .first (1)\n"
     "      // c1\n"
     "      ,  // c2\n"
     "      .second(2)\n"
     "      // c3\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    // c1\n"
     "    // c1+\n"
     "    .first(1) // c2\n"
     "              // c2+\n"
     "    , // c3\n"
     "      // c3+\n"
     "    .second(2) // c4\n"
     "               // c4+\n"
     "    // c5\n"
     "    // c5+\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "      // c1\n"
     "      // c1+\n"
     "      .first (1)  // c2\n"
     "                  // c2+\n"
     "      ,  // c3\n"
     "         // c3+\n"
     "      .second(2)  // c4\n"
     "                  // c4+\n"
     "      // c5\n"
     "      // c5+\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1) /* c1 */\n"
     "    ,\n"
     "    .second(2) /* c2 */\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)  /* c1 */\n"
     "      , .second(2)  /* c2 */\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    /* c1 */ .first(1),\n"
     "    /* c2 */ .second(2)\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "      /* c1 */.first (1),\n"
     "      /* c2 */.second(2)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1) /* c1 */\n"
     "    , .second(2) /* c2 */\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)  /* c1 */\n"
     "      , .second(2)  /* c2 */\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    /* c1 */.first(1)\n"
     "    /* c2 */, .second(2)\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "      /* c1 */  .first (1)\n"
     "      /* c2 */, .second(2)\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    ,\n"
     "    .second(2)\n"
     "    /* c2 */\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)\n"
     "      /* c1 */\n"
     "      , .second(2)\n"
     "      /* c2 */\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    , .second(2)\n"
     "    /* c2 */\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "        .first (1)\n"
     "      /* c1 */\n"
     "      , .second(2)\n"
     "      /* c2 */\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    , /* c2 */\n"
     "    .second(2)\n"
     "    /* c3 */\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "      .first (1)\n"
     "      /* c1 */\n"
     "      ,  /* c2 */\n"
     "      .second(2)\n"
     "      /* c3 */\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    , /* c2 */\n"
     "    .second(2)\n"
     "    /* c3 */\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "      .first (1)\n"
     "      /* c1 */\n"
     "      ,  /* c2 */\n"
     "      .second(2)\n"
     "      /* c3 */\n"
     "  );\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar foobar(\n"
     "    /* c1  */\n"
     "    /* c1+ */\n"
     "    .first(1) /* c2  */\n"
     "              /* c2+ */\n"
     "    , /* c3  */\n"
     "      /* c3+ */\n"
     "    .second(2) /* c4  */\n"
     "               /* c4+ */\n"
     "    /* c5  */\n"
     "    /* c5+ */\n"
     "  );\n"
     "endmodule\n",
     "module foo;\n"
     "  bar foobar (\n"
     "      /* c1  */\n"
     "      /* c1+ */\n"
     "      .first (1)  /* c2  */\n"
     "      /* c2+ */\n"
     "      ,  /* c3  */\n"
     "      /* c3+ */\n"
     "      .second(2)  /* c4  */\n"
     "      /* c4+ */\n"
     "      /* c5  */\n"
     "      /* c5+ */\n"
     "  );\n"
     "endmodule\n"},

    // Attachment of ',' to elements in ActualNamedParameterList (with and
    // without comments)
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1) // c1\n"
     "    ,\n"
     "    .second(2) // c2\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)  // c1\n"
     "      , .second(2)  // c2\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1) // c1\n"
     "    , .second(2) // c2\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)  // c1\n"
     "      , .second(2)  // c2\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1)\n"
     "    // c1\n"
     "    ,\n"
     "    .second(2)\n"
     "    // c2\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)\n"
     "      // c1\n"
     "      , .second(2)\n"
     "      // c2\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1)\n"
     "    // c1\n"
     "    , .second(2)\n"
     "    // c2\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)\n"
     "      // c1\n"
     "      , .second(2)\n"
     "      // c2\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1)\n"
     "    // c1\n"
     "    , // c2\n"
     "    .second(2)\n"
     "    // c3\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      .first (1)\n"
     "      // c1\n"
     "      ,  // c2\n"
     "      .second(2)\n"
     "      // c3\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    // c1\n"
     "    // c1+\n"
     "    .first(1) // c2\n"
     "              // c2+\n"
     "    , // c3\n"
     "      // c3+\n"
     "    .second(2) // c4\n"
     "               // c4+\n"
     "    // c5\n"
     "    // c5+\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      // c1\n"
     "      // c1+\n"
     "      .first (1)  // c2\n"
     "                  // c2+\n"
     "      ,  // c3\n"
     "         // c3+\n"
     "      .second(2)  // c4\n"
     "                  // c4+\n"
     "      // c5\n"
     "      // c5+\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1) /* c1 */\n"
     "    ,\n"
     "    .second(2) /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)  /* c1 */\n"
     "      , .second(2)  /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    /* c1 */ .first(1),\n"
     "    /* c2 */ .second(2)\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      /* c1 */.first (1),\n"
     "      /* c2 */.second(2)\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1) /* c1 */\n"
     "    , .second(2) /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)  /* c1 */\n"
     "      , .second(2)  /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    /* c1 */.first(1)\n"
     "    /* c2 */, .second(2)\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      /* c1 */  .first (1)\n"
     "      /* c2 */, .second(2)\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    ,\n"
     "    .second(2)\n"
     "    /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)\n"
     "      /* c1 */\n"
     "      , .second(2)\n"
     "      /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    , .second(2)\n"
     "    /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "        .first (1)\n"
     "      /* c1 */\n"
     "      , .second(2)\n"
     "      /* c2 */\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    , /* c2 */\n"
     "    .second(2)\n"
     "    /* c3 */\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      .first (1)\n"
     "      /* c1 */\n"
     "      ,  /* c2 */\n"
     "      .second(2)\n"
     "      /* c3 */\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    .first(1)\n"
     "    /* c1 */\n"
     "    , /* c2 */\n"
     "    .second(2)\n"
     "    /* c3 */\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      .first (1)\n"
     "      /* c1 */\n"
     "      ,  /* c2 */\n"
     "      .second(2)\n"
     "      /* c3 */\n"
     "  ) baz ();\n"
     "endmodule\n"},
    {"module foo;\n"
     "  bar#(\n"
     "    /* c1  */\n"
     "    /* c1+ */\n"
     "    .first(1) /* c2  */\n"
     "              /* c2+ */\n"
     "    , /* c3  */\n"
     "      /* c3+ */\n"
     "    .second(2) /* c4  */\n"
     "               /* c4+ */\n"
     "    /* c5  */\n"
     "    /* c5+ */\n"
     "  ) baz ();\n"
     "endmodule\n",
     "module foo;\n"
     "  bar #(\n"
     "      /* c1  */\n"
     "      /* c1+ */\n"
     "      .first (1)  /* c2  */\n"
     "      /* c2+ */\n"
     "      ,  /* c3  */\n"
     "      /* c3+ */\n"
     "      .second(2)  /* c4  */\n"
     "      /* c4+ */\n"
     "      /* c5  */\n"
     "      /* c5+ */\n"
     "  ) baz ();\n"
     "endmodule\n"},

    // ":" and "'{" in a single line
    {"assign foo[2] =\n"
     "'{\n"
     "bar: 1'b1,  // c\n"
     "baz: 1'b0,  // c\n"
     "foobar: CONSTANT,\n"
     "qux:\n"
     "{\n"
     "a,  // c\n"
     "b\n"
     "}\n"
     "};\n",
     "assign foo[2] = '{\n"
     "        bar: 1'b1,  // c\n"
     "        baz: 1'b0,  // c\n"
     "        foobar: CONSTANT,\n"
     "        qux: {\n"
     "          a,  // c\n"
     "          b\n"
     "        }\n"
     "    };\n"},
    {"assign a = (b) ? '{c: d[e], f: '1} : g;\n",
     "assign a = (b) ?\n"
     "    '{c: d[e], f: '1}\n"
     "    : g;\n"},

    // -----------------------------------------------------------------
    // Comments around `else`.
    // Check whether `else` partition is found correctly and that actual code is
    // not appended to EOL comments.

    // generate if

    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else if (r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else if (r) assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else if (r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  // eol-c\n"
     "  else if (r) assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "else // eol-c\n"
     "if (r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  else  // eol-c\n"
     "  if (r) assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "else\n"
     "// eol-c\n"
     "if (r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  else\n"
     "  // eol-c\n"
     "  if (r)\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "if (r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  if (r) assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  if (r)\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  if (r)\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "(r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  if\n"
     "      // eol-c\n"
     "      (r)\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "// eol-c\n"
     "(r) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  // eol-c\n"
     "  if\n"
     "      // eol-c\n"
     "      // eol-c\n"
     "      (r)\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},

    // generate if with function call

    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else if (foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else if (foo(x) == bar(1, 2))\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else if (foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  // eol-c\n"
     "  else if (foo(x) == bar(1, 2))\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "else // eol-c\n"
     "if (foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  else  // eol-c\n"
     "  if (foo(x) == bar(1, 2)) assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "else\n"
     "// eol-c\n"
     "if (foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  else\n"
     "  // eol-c\n"
     "  if (foo(\n"
     "          x\n"
     "      ) == bar(\n"
     "          1, 2\n"
     "      ))\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "if (foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  if (foo(x) == bar(1, 2)) assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  if (foo(\n"
     "          x\n"
     "      ) == bar(\n"
     "          1, 2\n"
     "      ))\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  if (foo(\n"
     "          x\n"
     "      ) == bar(\n"
     "          1, 2\n"
     "      ))\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "(foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  if\n"
     "      // eol-c\n"
     "      (foo(\n"
     "          x\n"
     "      ) == bar(\n"
     "          1, 2\n"
     "      ))\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},
    {"module zx;\n"
     "if (x) assign z=y;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "// eol-c\n"
     "(foo(x) == bar(1, 2)) assign z=w;\n"
     "else assign x=y;\n"
     "endmodule\n",
     "module zx;\n"
     "  if (x) assign z = y;\n"
     "  // eol-c\n"
     "  // eol-c\n"
     "  else  // eol-c\n"
     "  // eol-c\n"
     "  // eol-c\n"
     "  if\n"
     "      // eol-c\n"
     "      // eol-c\n"
     "      (foo(\n"
     "          x\n"
     "      ) == bar(\n"
     "          1, 2\n"
     "      ))\n"
     "    assign z = w;\n"
     "  else assign x = y;\n"
     "endmodule\n"},

    // else begin

    {"module zx;\n"
     "always begin\n"
     "if (a) b<=1;\n"
     "// eol-c\n"
     "else begin b<=2;\n"
     "end\n"
     "end\n"
     "endmodule\n",
     "module zx;\n"
     "  always begin\n"
     "    if (a) b <= 1;\n"
     "    // eol-c\n"
     "    else begin\n"
     "      b <= 2;\n"
     "    end\n"
     "  end\n"
     "endmodule\n"},
    {"module zx;\n"
     "always begin\n"
     "if (a) b<=1;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else begin b<=2;\n"
     "end\n"
     "end\n"
     "endmodule\n",
     "module zx;\n"
     "  always begin\n"
     "    if (a) b <= 1;\n"
     "    // eol-c\n"
     "    // eol-c\n"
     "    else begin\n"
     "      b <= 2;\n"
     "    end\n"
     "  end\n"
     "endmodule\n"},
    {"module zx;\n"
     "always begin\n"
     "if (a) b<=1;\n"
     "else // eol-c\n"
     "begin b<=2;\n"
     "end\n"
     "end\n"
     "endmodule\n",
     "module zx;\n"
     "  always begin\n"
     "    if (a) b <= 1;\n"
     "    else  // eol-c\n"
     "    begin\n"
     "      b <= 2;\n"
     "    end\n"
     "  end\n"
     "endmodule\n"},
    {"module zx;\n"
     "always begin\n"
     "if (a) b<=1;\n"
     "else\n"
     "// eol-c\n"
     "begin b<=2;\n"
     "end\n"
     "end\n"
     "endmodule\n",
     "module zx;\n"
     "  always begin\n"
     "    if (a) b <= 1;\n"
     "    else\n"
     "    // eol-c\n"
     "    begin\n"
     "      b <= 2;\n"
     "    end\n"
     "  end\n"
     "endmodule\n"},
    {"module zx;\n"
     "always begin\n"
     "if (a) b<=1;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "begin b<=2;\n"
     "end\n"
     "end\n"
     "endmodule\n",
     "module zx;\n"
     "  always begin\n"
     "    if (a) b <= 1;\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    begin\n"
     "      b <= 2;\n"
     "    end\n"
     "  end\n"
     "endmodule\n"},
    {"module zx;\n"
     "always begin\n"
     "if (a) b<=1;\n"
     "else // eol-c\n"
     "// eol-c\n"
     "begin b<=2;\n"
     "end\n"
     "end\n"
     "endmodule\n",
     "module zx;\n"
     "  always begin\n"
     "    if (a) b <= 1;\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    begin\n"
     "      b <= 2;\n"
     "    end\n"
     "  end\n"
     "endmodule\n"},

    // else if

    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else if (set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else if (set) assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else if (set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    // eol-c\n"
     "    else if (set) assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "else // eol-c\n"
     "if (set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    else  // eol-c\n"
     "    if (set) assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "else\n"
     "// eol-c\n"
     "if (set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    else\n"
     "    // eol-c\n"
     "    if (set)\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "if (set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    if (set) assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    if (set)\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    if (set)\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "(set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    if\n"
     "        // eol-c\n"
     "        (set)\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "// eol-c\n"
     "(set) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    // eol-c\n"
     "    if\n"
     "        // eol-c\n"
     "        // eol-c\n"
     "        (set)\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},

    // else if with function call

    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else if (foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else if (foo(clr, set, 1))\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else if (foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    // eol-c\n"
     "    else if (foo(clr, set, 1))\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "else // eol-c\n"
     "if (foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    else  // eol-c\n"
     "    if (foo(clr, set, 1)) assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "else\n"
     "// eol-c\n"
     "if (foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    else\n"
     "    // eol-c\n"
     "    if (foo(\n"
     "            clr, set, 1\n"
     "        ))\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "if (foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    if (foo(clr, set, 1)) assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    if (foo(\n"
     "            clr, set, 1\n"
     "        ))\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if (foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    if (foo(\n"
     "            clr, set, 1\n"
     "        ))\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "(foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    if\n"
     "        // eol-c\n"
     "        (foo(\n"
     "            clr, set, 1\n"
     "        ))\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},
    {"module zx;\n"
     "always @(clr or set)\n"
     "if (clr) assign q=0;\n"
     "// eol-c\n"
     "// eol-c\n"
     "else // eol-c\n"
     "// eol-c\n"
     "// eol-c\n"
     "if\n"
     "// eol-c\n"
     "// eol-c\n"
     "(foo(clr, set, 1)) assign q=1;\n"
     "else deassign q;\n"
     "endmodule\n",
     "module zx;\n"
     "  always @(clr or set)\n"
     "    if (clr) assign q = 0;\n"
     "    // eol-c\n"
     "    // eol-c\n"
     "    else  // eol-c\n"
     "    // eol-c\n"
     "    // eol-c\n"
     "    if\n"
     "        // eol-c\n"
     "        // eol-c\n"
     "        (foo(\n"
     "            clr, set, 1\n"
     "        ))\n"
     "      assign q = 1;\n"
     "    else deassign q;\n"
     "endmodule\n"},

    // -----------------------------------------------------------------
    // Comments around and inside macro calls.

    // between identifier and '(', no args

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c */ ();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */ ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */ /* c2 */ ();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */ ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */ /* c2 */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */ // c2\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  // c2\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */ ();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */ ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */ ();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */ ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ // c2\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  // c2\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c1\n"
     "// c2\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c1\n"
     "    // c2\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */ ();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */ ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "/* c2 */ ();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */ ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */ ();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "/* c2 */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "// c2\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    // c2\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "// c2\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    ();\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "// c2\n"
     "// c3\n"
     "();\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    ();\n"},

    // between identifier and '(', with arg

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c */ (arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */ (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */ /* c2 */ (arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */ (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */ /* c2 */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */ // c2\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  // c2\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */ (arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */ (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */ (arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */ (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ // c2\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  // c2\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c1\n"
     "// c2\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c1\n"
     "    // c2\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */ (arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */ (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "/* c2 */ (arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */ (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */ (arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "/* c2 */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "// c2\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    // c2\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "// c2\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    (arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz // c1\n"
     "// c2\n"
     "// c3\n"
     "(arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    (arg);\n"},

    // after '(', no args

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    // c1\n"
     "    // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    /* c2 */\n"
     "/* c3 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "    /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "    // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "// c2\n"
     "// c3\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "    // c2\n"
     "    // c3\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c1\n"
     "    // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "/* c3 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     "// c3\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     "    // c3\n"
     ");\n"},

    // after '(', with arg

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */ /* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */  /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */ /* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */  /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */ // c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */  // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */ /* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c1 */  /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */ /* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c1 */  /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "// c\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    // c\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */ // c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c1 */  // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "// c1\n"
     "// c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(\n"
     "    // c1\n"
     "    // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "/* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "    /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "/* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "    /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "// c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "// c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "    // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(// c1\n"
     "// c2\n"
     "// c3\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ // c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ // c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c1\n"
     "// c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c1\n"
     "    // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    /* c2 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    /* c2 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "// c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    arg);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     "// c3\n"
     "arg);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    arg);\n"},

    // after single arg

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "           /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "           /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "           // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "           /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg\n"
     "           // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg// c1\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  // c1\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */\n"
     "           /* c2 */\n"
     "/* c3 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg// c1\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  // c1\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg// c1\n"
     "// c2\n"
     "// c3\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg  // c1\n"
     "           // c2\n"
     "           // c3\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "           /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "           /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "           // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "           /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg\n"
     "           // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg// c1\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  // c1\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */\n"
     "           /* c2 */\n"
     "/* c3 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg// c1\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  // c1\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg// c1\n"
     "// c2\n"
     "// c3\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg  // c1\n"
     "           // c2\n"
     "           // c3\n"
     ");\n"},

    // before colon

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */ /* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */  /* c2 */,\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */ /* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */  /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1// c\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  // c\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */ // c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */  // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "/* c */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           /* c */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "/* c1 */ /* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           /* c1 */  /* c2 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "/* c */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           /* c */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "/* c1 */ /* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           /* c1 */  /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "// c\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           // c\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "/* c1 */ // c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           /* c1 */  // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "// c1\n"
     "// c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1\n"
     "           // c1\n"
     "           // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */\n"
     "/* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */\n"
     "           /* c2 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1// c1\n"
     "/* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  // c1\n"
     "           /* c2 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */\n"
     "/* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */\n"
     "           /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1// c1\n"
     "/* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  // c1\n"
     "           /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */\n"
     "// c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */\n"
     "           // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1// c1\n"
     "// c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  // c1\n"
     "           // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1// c1\n"
     "// c2\n"
     "// c3\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1  // c1\n"
     "           // c2\n"
     "           // c3\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */ /* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */  /* c2 */,\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */ /* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */  /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1// c\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  // c\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */ // c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */  // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "/* c */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           /* c */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "/* c1 */ /* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           /* c1 */  /* c2 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "/* c */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           /* c */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "/* c1 */ /* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           /* c1 */  /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "// c\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           // c\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "/* c1 */ // c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           /* c1 */  // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "// c1\n"
     "// c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1\n"
     "           // c1\n"
     "           // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */\n"
     "/* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */\n"
     "           /* c2 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1// c1\n"
     "/* c2 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  // c1\n"
     "           /* c2 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */, arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */, arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */\n"
     "/* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */\n"
     "           /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1// c1\n"
     "/* c2 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  // c1\n"
     "           /* c2 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */\n"
     "// c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */\n"
     "           // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1// c1\n"
     "// c2\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  // c1\n"
     "           // c2\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     "           , arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1// c1\n"
     "// c2\n"
     "// c3\n"
     ", arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1  // c1\n"
     "           // c2\n"
     "           // c3\n"
     "           , arg2);\n"},

    // after colon

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */ /* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */  /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */ /* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */  /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,// c\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  // c\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */ // c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */  // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "/* c */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           /* c */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "/* c1 */ /* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           /* c1 */  /* c2 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "/* c */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           /* c */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "/* c1 */ /* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           /* c1 */  /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "// c\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           // c\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "/* c1 */ // c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           /* c1 */  // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "// c1\n"
     "// c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           // c1\n"
     "           // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */\n"
     "/* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */\n"
     "           /* c2 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,// c1\n"
     "/* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  // c1\n"
     "           /* c2 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */\n"
     "/* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */\n"
     "           /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,// c1\n"
     "/* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  // c1\n"
     "           /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */\n"
     "// c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */\n"
     "           // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,// c1\n"
     "// c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  // c1\n"
     "           // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,// c1\n"
     "// c2\n"
     "// c3\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,  // c1\n"
     "           // c2\n"
     "           // c3\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */ /* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */  /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */ /* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */  /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,// c\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  // c\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */ // c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */  // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "/* c */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           /* c */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "/* c1 */ /* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           /* c1 */  /* c2 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "/* c */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           /* c */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "/* c1 */ /* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           /* c1 */  /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "// c\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           // c\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "/* c1 */ // c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           /* c1 */  // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "// c1\n"
     "// c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           // c1\n"
     "           // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */\n"
     "/* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */\n"
     "           /* c2 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,// c1\n"
     "/* c2 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  // c1\n"
     "           /* c2 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */ arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */\n"
     "/* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */\n"
     "           /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,// c1\n"
     "/* c2 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  // c1\n"
     "           /* c2 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */\n"
     "// c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */\n"
     "           // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,// c1\n"
     "// c2\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  // c1\n"
     "           // c2\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     "           arg2);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,// c1\n"
     "// c2\n"
     "// c3\n"
     "arg2);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,  // c1\n"
     "           // c2\n"
     "           // c3\n"
     "           arg2);\n"},

    // after last arg

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           arg2,  /* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1,\n"
     "           arg2,  /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "           /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "           /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "           // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "           /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,\n"
     "           // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c1 */\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,// c1\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  // c1\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c1 */\n"
     "           /* c2 */\n"
     "/* c3 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,// c1\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  // c1\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c1 */\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,// c1\n"
     "// c2\n"
     "// c3\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg1, arg2,  // c1\n"
     "           // c2\n"
     "           // c3\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           arg2,  /* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1,\n"
     "           arg2,  /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c1 */ /* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c1 */  /* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "           /* c */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c1 */ /* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "           /* c1 */  /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "// c\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "           // c\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c1 */ // c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "           /* c1 */  // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "           /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,\n"
     "           // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,// c1\n"
     "/* c2 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  // c1\n"
     "/* c2 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */);\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
     "           /* c2 */\n"
     "/* c3 */);\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,// c1\n"
     "/* c2 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  // c1\n"
     "           /* c2 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,// c1\n"
     "// c2\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  // c1\n"
     "           // c2\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
     "           /* c2 */\n"
     "           /* c3 */\n"
     ");\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,// c1\n"
     "// c2\n"
     "// c3\n"
     ");\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg1, arg2,  // c1\n"
     "           // c2\n"
     "           // c3\n"
     ");\n"},

    // after ')', no args

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  // c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "// c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()\n"
     "// c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()// c1\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  // c1\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()// c1\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  // c1\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  // c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()// c1\n"
     "// c2\n"
     "// c3\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz()  // c1\n"
     "// c2\n"
     "// c3\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  // c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "// c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()\n"
     "// c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()// c1\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  // c1\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()// c1\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  // c1\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  // c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()// c1\n"
     "// c2\n"
     "// c3\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ()  // c1\n"
     "// c2\n"
     "// c3\n"
     ";\n"},

    // after ')', with arg

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  // c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  // c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)\n"
     "// c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)// c1\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  // c1\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)// c1\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  // c1\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  // c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)// c1\n"
     "// c2\n"
     "// c3\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg)  // c1\n"
     "// c2\n"
     "// c3\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  // c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */ /* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */  /* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */ /* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */  /* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "// c\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "// c\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */ // c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */  // c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)\n"
     "// c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)// c1\n"
     "/* c2 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  // c1\n"
     "/* c2 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)// c1\n"
     "/* c2 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  // c1\n"
     "/* c2 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)// c1\n"
     "// c2\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  // c1\n"
     "// c2\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)// c1\n"
     "// c2\n"
     "// c3\n"
     ";\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg)  // c1\n"
     "// c2\n"
     "// c3\n"
     ";\n"},

    // after ';', no args

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();// c1 c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  // c1 c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  // c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz();  // c1\n"
     "// c2\n"
     "// c3\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  // c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ();  // c1\n"
     "// c2\n"
     "// c3\n"},

    // after ';', with arg

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);// c1 c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  // c1 c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  // c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz(arg);  // c1\n"
     "// c2\n"
     "// c3\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  // c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(arg);  // c1\n"
     "// c2\n"
     "// c3\n"},

    // everywhere, no args

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c */(/* c */)/* c */;/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */\n"
     "    (  /* c */)  /* c */;  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */ /* c2 */(/* c1 */ /* c2 */)/* c1 */ /* c2 */;/* c1 */ "
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */\n"
     "    (  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c */\n"
     "(/* c */\n"
     ")/* c */\n"
     ";/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */\n"
     "    (  /* c */\n"
     "    )  /* c */\n"
     ";  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */ /* c2 */\n"
     "(/* c1 */ /* c2 */\n"
     ")/* c1 */ /* c2 */\n"
     ";/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */\n"
     "    (  /* c1 */  /* c2 */\n"
     "    )  /* c1 */  /* c2 */\n"
     ";  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c\n"
     "(// c\n"
     ")// c\n"
     ";// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c\n"
     "    (  // c\n"
     "    )  // c\n"
     ";  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */ // c2\n"
     "(/* c1 */ // c2\n"
     ")/* c1 */ // c2\n"
     ";/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  // c2\n"
     "    (  /* c1 */  // c2\n"
     "    )  /* c1 */  // c2\n"
     ";  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */(\n"
     "/* c */)\n"
     "/* c */;\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */ (\n"
     "    /* c */)\n"
     "/* c */;\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */(\n"
     "/* c1 */ /* c2 */)\n"
     "/* c1 */ /* c2 */;\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */ (\n"
     "    /* c1 */  /* c2 */)\n"
     "/* c1 */  /* c2 */;\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */\n"
     "(\n"
     "/* c */\n"
     ")\n"
     "/* c */\n"
     ";\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */\n"
     "    (\n"
     "        /* c */\n"
     "    )\n"
     "/* c */\n"
     ";\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */\n"
     "(\n"
     "/* c1 */ /* c2 */\n"
     ")\n"
     "/* c1 */ /* c2 */\n"
     ";\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */\n"
     "    (\n"
     "        /* c1 */  /* c2 */\n"
     "    )\n"
     "/* c1 */  /* c2 */\n"
     ";\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c\n"
     "(\n"
     "// c\n"
     ")\n"
     "// c\n"
     ";\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c\n"
     "    (\n"
     "        // c\n"
     "    )\n"
     "// c\n"
     ";\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ // c2\n"
     "(\n"
     "/* c1 */ // c2\n"
     ")\n"
     "/* c1 */ // c2\n"
     ";\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  // c2\n"
     "    (\n"
     "        /* c1 */  // c2\n"
     "    )\n"
     "/* c1 */  // c2\n"
     ";\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ")\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    (\n"
     "        /* c1 */\n"
     "        /* c2 */\n"
     "    )\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c1\n"
     "// c2\n"
     "(\n"
     "// c1\n"
     "// c2\n"
     ")\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c1\n"
     "    // c2\n"
     "    (\n"
     "        // c1\n"
     "        // c2\n"
     "    )\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */(/* c1 */\n"
     "/* c2 */)/* c1 */\n"
     "/* c2 */;/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */ (  /* c1 */\n"
     "    /* c2 */)  /* c1 */\n"
     "/* c2 */;  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "/* c2 */(// c1\n"
     "/* c2 */)// c1\n"
     "/* c2 */;// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */ (  // c1\n"
     "    /* c2 */)  // c1\n"
     "/* c2 */;  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ (  /* c1 */\n"
     "        /* c2 */\n"
     "    /* c3 */)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */\n"
     "(/* c1 */\n"
     "/* c2 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     ";/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    (  /* c1 */\n"
     "        /* c2 */\n"
     "    )  /* c1 */\n"
     "/* c2 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "/* c2 */\n"
     "(// c1\n"
     "/* c2 */\n"
     ")// c1\n"
     "/* c2 */\n"
     ";// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */\n"
     "    (  // c1\n"
     "        /* c2 */\n"
     "    )  // c1\n"
     "/* c2 */\n"
     ";  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "// c2\n"
     "(/* c1 */\n"
     "// c2\n"
     ")/* c1 */\n"
     "// c2\n"
     ";/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    // c2\n"
     "    (  /* c1 */\n"
     "        // c2\n"
     "    )  /* c1 */\n"
     "// c2\n"
     ";  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "// c2\n"
     "(// c1\n"
     "// c2\n"
     ")// c1\n"
     "// c2\n"
     ";// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    (  // c1\n"
     "       // c2\n"
     "    )  // c1\n"
     "       // c2\n"
     ";  // c1\n"
     "   // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    (  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */\n"
     "    )  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "// c2\n"
     "// c3\n"
     "(// c1\n"
     "// c2\n"
     "// c3\n"
     ")// c1\n"
     "// c2\n"
     "// c3\n"
     ";// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    (  // c1\n"
     "       // c2\n"
     "       // c3\n"
     "    )  // c1\n"
     "       // c2\n"
     "       // c3\n"
     ";  // c1\n"
     "   // c2\n"
     "   // c3\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */)/* c */;/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */)  /* c */;  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */)/* c1 */ /* c2 */;/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 */  /* c2 "
     "*/\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */\n"
     ")/* c */\n"
     ";/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */\n"
     ")  /* c */\n"
     ";  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */\n"
     ")/* c1 */ /* c2 */\n"
     ";/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
     ")  /* c1 */  /* c2 */\n"
     ";  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c\n"
     ")// c\n"
     ";// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c\n"
     ")  // c\n"
     ";  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ // c2\n"
     ")/* c1 */ // c2\n"
     ";/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  // c2\n"
     ")  /* c1 */  // c2\n"
     ";  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */)\n"
     "/* c */;\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */)\n"
     "/* c */;\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */)\n"
     "/* c1 */ /* c2 */;\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */  /* c2 */)\n"
     "/* c1 */  /* c2 */;\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */\n"
     ")\n"
     "/* c */\n"
     ";\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c */\n"
     ")\n"
     "/* c */\n"
     ";\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */\n"
     ")\n"
     "/* c1 */ /* c2 */\n"
     ";\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  /* c2 */\n"
     ")\n"
     "/* c1 */  /* c2 */\n"
     ";\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c\n"
     ")\n"
     "// c\n"
     ";\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c\n"
     ")\n"
     "// c\n"
     ";\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ // c2\n"
     ")\n"
     "/* c1 */ // c2\n"
     ";\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  // c2\n"
     ")\n"
     "/* c1 */  // c2\n"
     ";\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ")\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     ")\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c1\n"
     "// c2\n"
     ")\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c1\n"
     "    // c2\n"
     ")\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */)/* c1 */\n"
     "/* c2 */;/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "/* c2 */)  /* c1 */\n"
     "/* c2 */;  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */)// c1\n"
     "/* c2 */;// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "/* c2 */)  // c1\n"
     "/* c2 */;  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "/* c3 */)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     ";/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     ")  /* c1 */\n"
     "/* c2 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */\n"
     ")// c1\n"
     "/* c2 */\n"
     ";// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    /* c2 */\n"
     ")  // c1\n"
     "/* c2 */\n"
     ";  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "// c2\n"
     ")/* c1 */\n"
     "// c2\n"
     ";/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    // c2\n"
     ")  /* c1 */\n"
     "// c2\n"
     ";  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     ")// c1\n"
     "// c2\n"
     ";// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     ")  // c1\n"
     "   // c2\n"
     ";  // c1\n"
     "   // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     ")  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     "// c3\n"
     ")// c1\n"
     "// c2\n"
     "// c3\n"
     ";// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     "    // c3\n"
     ")  // c1\n"
     "   // c2\n"
     "   // c3\n"
     ";  // c1\n"
     "   // c2\n"
     "   // c3\n"},

    // everywhere, with args

    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c */(/* c */arg1/* c */,/* c */arg2/* c */)/* c */;/* c "
     "*/\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */ (  /* c */\n"
     "    arg1  /* c */,  /* c */\n"
     "    arg2  /* c */)  /* c */;  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */ /* c2 */(/* c1 */ /* c2 */arg1/* c1 */ /* c2 */,/* c1 "
     "*/ /* c2 */arg2/* c1 */ /* c2 */)/* c1 */ /* c2 */;/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */ (  /* c1 */  /* c2 */\n"
     "    arg1  /* c1 */  /* c2 */,  /* c1 */  /* c2 */\n"
     "    arg2  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 */  /* c2 "
     "*/\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c */\n"
     "(/* c */\n"
     "arg1/* c */\n"
     ",/* c */\n"
     "arg2/* c */\n"
     ")/* c */\n"
     ";/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c */\n"
     "    (  /* c */\n"
     "        arg1  /* c */\n"
     "        ,  /* c */\n"
     "        arg2  /* c */\n"
     "    )  /* c */\n"
     ";  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */ /* c2 */\n"
     "(/* c1 */ /* c2 */\n"
     "arg1/* c1 */ /* c2 */\n"
     ",/* c1 */ /* c2 */\n"
     "arg2/* c1 */ /* c2 */\n"
     ")/* c1 */ /* c2 */\n"
     ";/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  /* c2 */\n"
     "    (  /* c1 */  /* c2 */\n"
     "        arg1  /* c1 */  /* c2 */\n"
     "        ,  /* c1 */  /* c2 */\n"
     "        arg2  /* c1 */  /* c2 */\n"
     "    )  /* c1 */  /* c2 */\n"
     ";  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c\n"
     "(// c\n"
     "arg1// c\n"
     ",// c\n"
     "arg2// c\n"
     ")// c\n"
     ";// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c\n"
     "    (  // c\n"
     "        arg1  // c\n"
     "        ,  // c\n"
     "        arg2  // c\n"
     "    )  // c\n"
     ";  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */ // c2\n"
     "(/* c1 */ // c2\n"
     "arg1/* c1 */ // c2\n"
     ",/* c1 */ // c2\n"
     "arg2/* c1 */ // c2\n"
     ")/* c1 */ // c2\n"
     ";/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */  // c2\n"
     "    (  /* c1 */  // c2\n"
     "        arg1  /* c1 */  // c2\n"
     "        ,  /* c1 */  // c2\n"
     "        arg2  /* c1 */  // c2\n"
     "    )  /* c1 */  // c2\n"
     ";  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */(\n"
     "/* c */arg1\n"
     "/* c */,\n"
     "/* c */arg2\n"
     "/* c */)\n"
     "/* c */;\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */ (\n"
     "        /* c */ arg1\n"
     "        /* c */,\n"
     "        /* c */ arg2\n"
     "    /* c */)\n"
     "/* c */;\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */(\n"
     "/* c1 */ /* c2 */arg1\n"
     "/* c1 */ /* c2 */,\n"
     "/* c1 */ /* c2 */arg2\n"
     "/* c1 */ /* c2 */)\n"
     "/* c1 */ /* c2 */;\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */ (\n"
     "        /* c1 */  /* c2 */ arg1\n"
     "        /* c1 */  /* c2 */,\n"
     "        /* c1 */  /* c2 */ arg2\n"
     "    /* c1 */  /* c2 */)\n"
     "/* c1 */  /* c2 */;\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c */\n"
     "(\n"
     "/* c */\n"
     "arg1\n"
     "/* c */\n"
     ",\n"
     "/* c */\n"
     "arg2\n"
     "/* c */\n"
     ")\n"
     "/* c */\n"
     ";\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c */\n"
     "    (\n"
     "        /* c */\n"
     "        arg1\n"
     "        /* c */\n"
     "        ,\n"
     "        /* c */\n"
     "        arg2\n"
     "        /* c */\n"
     "    )\n"
     "/* c */\n"
     ";\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ /* c2 */\n"
     "(\n"
     "/* c1 */ /* c2 */\n"
     "arg1\n"
     "/* c1 */ /* c2 */\n"
     ",\n"
     "/* c1 */ /* c2 */\n"
     "arg2\n"
     "/* c1 */ /* c2 */\n"
     ")\n"
     "/* c1 */ /* c2 */\n"
     ";\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  /* c2 */\n"
     "    (\n"
     "        /* c1 */  /* c2 */\n"
     "        arg1\n"
     "        /* c1 */  /* c2 */\n"
     "        ,\n"
     "        /* c1 */  /* c2 */\n"
     "        arg2\n"
     "        /* c1 */  /* c2 */\n"
     "    )\n"
     "/* c1 */  /* c2 */\n"
     ";\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c\n"
     "(\n"
     "// c\n"
     "arg1\n"
     "// c\n"
     ",\n"
     "// c\n"
     "arg2\n"
     "// c\n"
     ")\n"
     "// c\n"
     ";\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c\n"
     "    (\n"
     "        // c\n"
     "        arg1\n"
     "        // c\n"
     "        ,\n"
     "        // c\n"
     "        arg2\n"
     "        // c\n"
     "    )\n"
     "// c\n"
     ";\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */ // c2\n"
     "(\n"
     "/* c1 */ // c2\n"
     "arg1\n"
     "/* c1 */ // c2\n"
     ",\n"
     "/* c1 */ // c2\n"
     "arg2\n"
     "/* c1 */ // c2\n"
     ")\n"
     "/* c1 */ // c2\n"
     ";\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */  // c2\n"
     "    (\n"
     "        /* c1 */  // c2\n"
     "        arg1\n"
     "        /* c1 */  // c2\n"
     "        ,\n"
     "        /* c1 */  // c2\n"
     "        arg2\n"
     "        /* c1 */  // c2\n"
     "    )\n"
     "/* c1 */  // c2\n"
     ";\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg1\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ",\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg2\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ")\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    (\n"
     "        /* c1 */\n"
     "        /* c2 */\n"
     "        arg1\n"
     "        /* c1 */\n"
     "        /* c2 */\n"
     "        ,\n"
     "        /* c1 */\n"
     "        /* c2 */\n"
     "        arg2\n"
     "        /* c1 */\n"
     "        /* c2 */\n"
     "    )\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "// c1\n"
     "// c2\n"
     "(\n"
     "// c1\n"
     "// c2\n"
     "arg1\n"
     "// c1\n"
     "// c2\n"
     ",\n"
     "// c1\n"
     "// c2\n"
     "arg2\n"
     "// c1\n"
     "// c2\n"
     ")\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz\n"
     "    // c1\n"
     "    // c2\n"
     "    (\n"
     "        // c1\n"
     "        // c2\n"
     "        arg1\n"
     "        // c1\n"
     "        // c2\n"
     "        ,\n"
     "        // c1\n"
     "        // c2\n"
     "        arg2\n"
     "        // c1\n"
     "        // c2\n"
     "    )\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */(/* c1 */\n"
     "/* c2 */arg1/* c1 */\n"
     "/* c2 */,/* c1 */\n"
     "/* c2 */arg2/* c1 */\n"
     "/* c2 */)/* c1 */\n"
     "/* c2 */;/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */ (  /* c1 */\n"
     "        /* c2 */ arg1  /* c1 */\n"
     "        /* c2 */,  /* c1 */\n"
     "        /* c2 */ arg2  /* c1 */\n"
     "    /* c2 */)  /* c1 */\n"
     "/* c2 */;  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "/* c2 */(// c1\n"
     "/* c2 */arg1// c1\n"
     "/* c2 */,// c1\n"
     "/* c2 */arg2// c1\n"
     "/* c2 */)// c1\n"
     "/* c2 */;// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */ (  // c1\n"
     "        /* c2 */ arg1  // c1\n"
     "        /* c2 */,  // c1\n"
     "        /* c2 */ arg2  // c1\n"
     "    /* c2 */)  // c1\n"
     "/* c2 */;  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg2/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ (  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */ arg1  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */,  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */ arg2  /* c1 */\n"
     "        /* c2 */\n"
     "    /* c3 */)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */\n"
     "(/* c1 */\n"
     "/* c2 */\n"
     "arg1/* c1 */\n"
     "/* c2 */\n"
     ",/* c1 */\n"
     "/* c2 */\n"
     "arg2/* c1 */\n"
     "/* c2 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     ";/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    (  /* c1 */\n"
     "        /* c2 */\n"
     "        arg1  /* c1 */\n"
     "        /* c2 */\n"
     "        ,  /* c1 */\n"
     "        /* c2 */\n"
     "        arg2  /* c1 */\n"
     "        /* c2 */\n"
     "    )  /* c1 */\n"
     "/* c2 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "/* c2 */\n"
     "(// c1\n"
     "/* c2 */\n"
     "arg1// c1\n"
     "/* c2 */\n"
     ",// c1\n"
     "/* c2 */\n"
     "arg2// c1\n"
     "/* c2 */\n"
     ")// c1\n"
     "/* c2 */\n"
     ";// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    /* c2 */\n"
     "    (  // c1\n"
     "        /* c2 */\n"
     "        arg1  // c1\n"
     "        /* c2 */\n"
     "        ,  // c1\n"
     "        /* c2 */\n"
     "        arg2  // c1\n"
     "        /* c2 */\n"
     "    )  // c1\n"
     "/* c2 */\n"
     ";  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "// c2\n"
     "(/* c1 */\n"
     "// c2\n"
     "arg1/* c1 */\n"
     "// c2\n"
     ",/* c1 */\n"
     "// c2\n"
     "arg2/* c1 */\n"
     "// c2\n"
     ")/* c1 */\n"
     "// c2\n"
     ";/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    // c2\n"
     "    (  /* c1 */\n"
     "        // c2\n"
     "        arg1  /* c1 */\n"
     "        // c2\n"
     "        ,  /* c1 */\n"
     "        // c2\n"
     "        arg2  /* c1 */\n"
     "        // c2\n"
     "    )  /* c1 */\n"
     "// c2\n"
     ";  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "// c2\n"
     "(// c1\n"
     "// c2\n"
     "arg1// c1\n"
     "// c2\n"
     ",// c1\n"
     "// c2\n"
     "arg2// c1\n"
     "// c2\n"
     ")// c1\n"
     "// c2\n"
     ";// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    (  // c1\n"
     "       // c2\n"
     "        arg1  // c1\n"
     "        // c2\n"
     "        ,  // c1\n"
     "           // c2\n"
     "        arg2  // c1\n"
     "        // c2\n"
     "    )  // c1\n"
     "       // c2\n"
     ";  // c1\n"
     "   // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ",/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg2/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    (  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */\n"
     "        arg1  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */\n"
     "        ,  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */\n"
     "        arg2  /* c1 */\n"
     "        /* c2 */\n"
     "        /* c3 */\n"
     "    )  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz// c1\n"
     "// c2\n"
     "// c3\n"
     "(// c1\n"
     "// c2\n"
     "// c3\n"
     "arg1// c1\n"
     "// c2\n"
     "// c3\n"
     ",// c1\n"
     "// c2\n"
     "// c3\n"
     "arg2// c1\n"
     "// c2\n"
     "// c3\n"
     ")// c1\n"
     "// c2\n"
     "// c3\n"
     ";// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "$foobarbaz  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    (  // c1\n"
     "       // c2\n"
     "       // c3\n"
     "        arg1  // c1\n"
     "        // c2\n"
     "        // c3\n"
     "        ,  // c1\n"
     "           // c2\n"
     "           // c3\n"
     "        arg2  // c1\n"
     "        // c2\n"
     "        // c3\n"
     "    )  // c1\n"
     "       // c2\n"
     "       // c3\n"
     ";  // c1\n"
     "   // c2\n"
     "   // c3\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */arg1/* c */,/* c */arg2/* c */)/* c */;/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */\n"
     "    arg1  /* c */,  /* c */\n"
     "    arg2  /* c */)  /* c */;  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */arg1/* c1 */ /* c2 */,/* c1 */ /* c2 "
     "*/arg2/* c1 */ /* c2 */)/* c1 */ /* c2 */;/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
     "    arg1  /* c1 */  /* c2 */,  /* c1 */  /* c2 */\n"
     "    arg2  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 */  /* c2 "
     "*/\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c */\n"
     "arg1/* c */\n"
     ",/* c */\n"
     "arg2/* c */\n"
     ")/* c */\n"
     ";/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c */\n"
     "    arg1  /* c */\n"
     "    ,  /* c */\n"
     "    arg2  /* c */\n"
     ")  /* c */\n"
     ";  /* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ /* c2 */\n"
     "arg1/* c1 */ /* c2 */\n"
     ",/* c1 */ /* c2 */\n"
     "arg2/* c1 */ /* c2 */\n"
     ")/* c1 */ /* c2 */\n"
     ";/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
     "    arg1  /* c1 */  /* c2 */\n"
     "    ,  /* c1 */  /* c2 */\n"
     "    arg2  /* c1 */  /* c2 */\n"
     ")  /* c1 */  /* c2 */\n"
     ";  /* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c\n"
     "arg1// c\n"
     ",// c\n"
     "arg2// c\n"
     ")// c\n"
     ";// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c\n"
     "    arg1  // c\n"
     "    ,  // c\n"
     "    arg2  // c\n"
     ")  // c\n"
     ";  // c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */ // c2\n"
     "arg1/* c1 */ // c2\n"
     ",/* c1 */ // c2\n"
     "arg2/* c1 */ // c2\n"
     ")/* c1 */ // c2\n"
     ";/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */  // c2\n"
     "    arg1  /* c1 */  // c2\n"
     "    ,  /* c1 */  // c2\n"
     "    arg2  /* c1 */  // c2\n"
     ")  /* c1 */  // c2\n"
     ";  /* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */arg1\n"
     "/* c */,\n"
     "/* c */arg2\n"
     "/* c */)\n"
     "/* c */;\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c */ arg1\n"
     "    /* c */,\n"
     "    /* c */ arg2\n"
     "/* c */)\n"
     "/* c */;\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */arg1\n"
     "/* c1 */ /* c2 */,\n"
     "/* c1 */ /* c2 */arg2\n"
     "/* c1 */ /* c2 */)\n"
     "/* c1 */ /* c2 */;\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  /* c2 */ arg1\n"
     "    /* c1 */  /* c2 */,\n"
     "    /* c1 */  /* c2 */ arg2\n"
     "/* c1 */  /* c2 */)\n"
     "/* c1 */  /* c2 */;\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c */\n"
     "arg1\n"
     "/* c */\n"
     ",\n"
     "/* c */\n"
     "arg2\n"
     "/* c */\n"
     ")\n"
     "/* c */\n"
     ";\n"
     "/* c */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c */\n"
     "    arg1\n"
     "    /* c */\n"
     "    ,\n"
     "    /* c */\n"
     "    arg2\n"
     "    /* c */\n"
     ")\n"
     "/* c */\n"
     ";\n"
     "/* c */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ /* c2 */\n"
     "arg1\n"
     "/* c1 */ /* c2 */\n"
     ",\n"
     "/* c1 */ /* c2 */\n"
     "arg2\n"
     "/* c1 */ /* c2 */\n"
     ")\n"
     "/* c1 */ /* c2 */\n"
     ";\n"
     "/* c1 */ /* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  /* c2 */\n"
     "    arg1\n"
     "    /* c1 */  /* c2 */\n"
     "    ,\n"
     "    /* c1 */  /* c2 */\n"
     "    arg2\n"
     "    /* c1 */  /* c2 */\n"
     ")\n"
     "/* c1 */  /* c2 */\n"
     ";\n"
     "/* c1 */  /* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c\n"
     "arg1\n"
     "// c\n"
     ",\n"
     "// c\n"
     "arg2\n"
     "// c\n"
     ")\n"
     "// c\n"
     ";\n"
     "// c\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c\n"
     "    arg1\n"
     "    // c\n"
     "    ,\n"
     "    // c\n"
     "    arg2\n"
     "    // c\n"
     ")\n"
     "// c\n"
     ";\n"
     "// c\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */ // c2\n"
     "arg1\n"
     "/* c1 */ // c2\n"
     ",\n"
     "/* c1 */ // c2\n"
     "arg2\n"
     "/* c1 */ // c2\n"
     ")\n"
     "/* c1 */ // c2\n"
     ";\n"
     "/* c1 */ // c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */  // c2\n"
     "    arg1\n"
     "    /* c1 */  // c2\n"
     "    ,\n"
     "    /* c1 */  // c2\n"
     "    arg2\n"
     "    /* c1 */  // c2\n"
     ")\n"
     "/* c1 */  // c2\n"
     ";\n"
     "/* c1 */  // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg1\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ",\n"
     "/* c1 */\n"
     "/* c2 */\n"
     "arg2\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ")\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    arg1\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    ,\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     "    arg2\n"
     "    /* c1 */\n"
     "    /* c2 */\n"
     ")\n"
     "/* c1 */\n"
     "/* c2 */\n"
     ";\n"
     "/* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "// c1\n"
     "// c2\n"
     "arg1\n"
     "// c1\n"
     "// c2\n"
     ",\n"
     "// c1\n"
     "// c2\n"
     "arg2\n"
     "// c1\n"
     "// c2\n"
     ")\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(\n"
     "    // c1\n"
     "    // c2\n"
     "    arg1\n"
     "    // c1\n"
     "    // c2\n"
     "    ,\n"
     "    // c1\n"
     "    // c2\n"
     "    arg2\n"
     "    // c1\n"
     "    // c2\n"
     ")\n"
     "// c1\n"
     "// c2\n"
     ";\n"
     "// c1\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */arg1/* c1 */\n"
     "/* c2 */,/* c1 */\n"
     "/* c2 */arg2/* c1 */\n"
     "/* c2 */)/* c1 */\n"
     "/* c2 */;/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */ arg1  /* c1 */\n"
     "    /* c2 */,  /* c1 */\n"
     "    /* c2 */ arg2  /* c1 */\n"
     "/* c2 */)  /* c1 */\n"
     "/* c2 */;  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */arg1// c1\n"
     "/* c2 */,// c1\n"
     "/* c2 */arg2// c1\n"
     "/* c2 */)// c1\n"
     "/* c2 */;// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    /* c2 */ arg1  // c1\n"
     "    /* c2 */,  // c1\n"
     "    /* c2 */ arg2  // c1\n"
     "/* c2 */)  // c1\n"
     "/* c2 */;  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */,/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */arg2/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */)/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ arg1  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */,  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */ arg2  /* c1 */\n"
     "    /* c2 */\n"
     "/* c3 */)  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */;  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "arg1/* c1 */\n"
     "/* c2 */\n"
     ",/* c1 */\n"
     "/* c2 */\n"
     "arg2/* c1 */\n"
     "/* c2 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     ";/* c1 */\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    arg1  /* c1 */\n"
     "    /* c2 */\n"
     "    ,  /* c1 */\n"
     "    /* c2 */\n"
     "    arg2  /* c1 */\n"
     "    /* c2 */\n"
     ")  /* c1 */\n"
     "/* c2 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "/* c2 */\n"
     "arg1// c1\n"
     "/* c2 */\n"
     ",// c1\n"
     "/* c2 */\n"
     "arg2// c1\n"
     "/* c2 */\n"
     ")// c1\n"
     "/* c2 */\n"
     ";// c1\n"
     "/* c2 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    /* c2 */\n"
     "    arg1  // c1\n"
     "    /* c2 */\n"
     "    ,  // c1\n"
     "    /* c2 */\n"
     "    arg2  // c1\n"
     "    /* c2 */\n"
     ")  // c1\n"
     "/* c2 */\n"
     ";  // c1\n"
     "/* c2 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "// c2\n"
     "arg1/* c1 */\n"
     "// c2\n"
     ",/* c1 */\n"
     "// c2\n"
     "arg2/* c1 */\n"
     "// c2\n"
     ")/* c1 */\n"
     "// c2\n"
     ";/* c1 */\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    // c2\n"
     "    arg1  /* c1 */\n"
     "    // c2\n"
     "    ,  /* c1 */\n"
     "    // c2\n"
     "    arg2  /* c1 */\n"
     "    // c2\n"
     ")  /* c1 */\n"
     "// c2\n"
     ";  /* c1 */\n"
     "// c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     "arg1// c1\n"
     "// c2\n"
     ",// c1\n"
     "// c2\n"
     "arg2// c1\n"
     "// c2\n"
     ")// c1\n"
     "// c2\n"
     ";// c1\n"
     "// c2\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     "    arg1  // c1\n"
     "    // c2\n"
     "    ,  // c1\n"
     "       // c2\n"
     "    arg2  // c1\n"
     "    // c2\n"
     ")  // c1\n"
     "   // c2\n"
     ";  // c1\n"
     "   // c2\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg1/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ",/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     "arg2/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ")/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";/* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    arg1  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    ,  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     "    arg2  /* c1 */\n"
     "    /* c2 */\n"
     "    /* c3 */\n"
     ")  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"
     ";  /* c1 */\n"
     "/* c2 */\n"
     "/* c3 */\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(// c1\n"
     "// c2\n"
     "// c3\n"
     "arg1// c1\n"
     "// c2\n"
     "// c3\n"
     ",// c1\n"
     "// c2\n"
     "// c3\n"
     "arg2// c1\n"
     "// c2\n"
     "// c3\n"
     ")// c1\n"
     "// c2\n"
     "// c3\n"
     ";// c1\n"
     "// c2\n"
     "// c3\n",
     "// verilog_syntax: parse-as-module-body\n"
     "`FOOBARBAZ(  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    arg1  // c1\n"
     "    // c2\n"
     "    // c3\n"
     "    ,  // c1\n"
     "       // c2\n"
     "       // c3\n"
     "    arg2  // c1\n"
     "    // c2\n"
     "    // c3\n"
     ")  // c1\n"
     "   // c2\n"
     "   // c3\n"
     ";  // c1\n"
     "   // c2\n"
     "   // c3\n"},

    // -----------------------------------------------------------------
};

// Tests that formatter produces expected results, end-to-end.
TEST(FormatterEndToEndTest, VerilogFormatTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kFormatterTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, AutoInferAlignment) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"class  cc ;\n"
       "endclass:cc\n",
       "class cc;\n"
       "endclass : cc\n"},

      // module port declarations
      {"module pd(\n"
       "input wire foo,\n"
       "output reg bar\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  wire foo,\n"  // flush-left vs. align are similar enough,
       "    output reg  bar\n"   // so automatic policy will align.
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input  foo_pkg::baz_t foo,\n"
       "output reg  bar\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input foo_pkg::baz_t foo,\n"  // alignment would add too many spaces
       "    output reg bar\n"             // so infer intent to flush-left.
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input  foo_pkg::baz_t foo,\n"
       "output     reg  bar\n"  // user injects 4 excess spaces here ...
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  foo_pkg::baz_t foo,\n"
       "    output reg            bar\n"  // ... and triggers alignment.
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "`ifdef FAA\n"  // inside preprocessing conditional
       "input  baaaz_t foo,\n"
       "output reg      bar\n"  // user injects 4 excess spaces here ...
       "`endif\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "`ifdef FAA\n"
       "    input  baaaz_t foo,\n"
       "    output reg     bar\n"  // ... and triggers alignment.
       "`endif\n"
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "`ifdef FAA\n"  // inside preprocessing conditional
       "input  baaaz_t foo,\n"
       "`else\n"
       "output reg      bar\n"  // user injects 4 excess spaces here ...
       "`endif\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "`ifdef FAA\n"
       "    input  baaaz_t foo,\n"  // aligned
       "`else\n"                    // aligned across preprocessing directives
       "    output reg     bar\n"   // ... and triggers alignment.
       "`endif\n"
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input logic [31:0] bus,\n"
       "input logic [7:0] bus2,\n"
       "`ifdef FAA\n"  // inside preprocessing conditional
       "input  baaaz_t foo,\n"
       "`else\n"
       "output reg      bar,\n"  // user injects 4 excess spaces here ...
       "`endif\n"
       "output out_t zout1,\n"
       "output out_t zout2\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  logic   [31:0] bus,\n"  // treated as one large group
       "    input  logic   [ 7:0] bus2,\n"
       "`ifdef FAA\n"
       "    input  baaaz_t        foo,\n"  // aligned
       "`else\n"  // aligned across preprocessing directives
       "    output reg            bar,\n"  // ... and triggers alignment.
       "`endif\n"
       "    output out_t          zout1,\n"
       "    output out_t          zout2\n"
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input logic [7:0] bus2,\n"
       "`ifndef FAA\n"  // inside preprocessing conditional
       "input logic [31:0] bus,\n"
       "input  baaaz_t foo,\n"
       "`elsif BLA\n"
       "output reg      bar,\n"  // user injects 4 excess spaces here ...
       "output out_t zout1,\n"
       "`endif\n"
       "output out_t zout2\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  logic   [ 7:0] bus2,\n"
       "`ifndef FAA\n"
       "    input  logic   [31:0] bus,\n"  // treated as one large group
       "    input  baaaz_t        foo,\n"  // aligned
       "`elsif BLA\n"  // aligned across preprocessing directives
       "    output reg            bar,\n"  // ... and triggers alignment.
       "    output out_t          zout1,\n"
       "`endif\n"
       "    output out_t          zout2\n"
       ");\n"
       "endmodule : pd\n"},
      {// data declaration and net declaration in ports
       "module m(\n"
       "logic [x:y]a    ,\n"    // packed dimensions, induce alignment
       "wire [pp:qq] [e:f]b\n"  // packed dimensions, 2D
       ") ;\n"
       "endmodule\n",
       "module m (\n"
       "    logic [  x:y]      a,\n"
       "    wire  [pp:qq][e:f] b\n"
       ");\n"
       "endmodule\n"},
      {// used-defined data declarations in ports
       "module m(\n"
       "a::bb [x:y]a    ,\n"       // packed dimensions, induce alignment
       "c#(d,e) [pp:qq] [e:f]b\n"  // packed dimensions, 2D
       ") ;\n"
       "endmodule\n",
       "module m (\n"
       "    a::bb    [  x:y]      a,\n"
       "    c#(d, e) [pp:qq][e:f] b\n"
       ");\n"
       "endmodule\n"},

      // named parameter arguments
      {"module  mm ;\n"
       "foo #(\n"
       ".a(a),\n"
       ".bb(bb)\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a (a),\n"  // align doesn't add too many spaces, so align
       "      .bb(bb)\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       ".a(a),\n"
       ".bbcccc(bb)\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a(a),\n"  // align would add too many spaces, so flush-left
       "      .bbcccc(bb)\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       ".a(a    ),\n"  // user manually triggers alignment with excess spaces
       ".bbcccc(bb)\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a     (a),\n"  // induced alignment
       "      .bbcccc(bb)\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       "//c1\n"        // with comments (indented but not aligned)
       ".a(a    ),\n"  // user manually triggers alignment with excess spaces
       "//c2\n"
       ".bbcccc(bb)\n"
       "//c3\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      //c1\n"
       "      .a     (a),\n"  // induced alignment
       "      //c2\n"
       "      .bbcccc(bb)\n"
       "      //c3\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       ".a( (1     +2)),\n"  // excess spaces, testing extra parentheses
       ".bbcccc((c*d)+(e*f))\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a     ((1 + 2)),\n"  // induced alignment
       "      .bbcccc((c * d) + (e * f))\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},

      // named port connections
      {"module  mm ;\n"
       "foo bar(\n"
       ".a(a),\n"
       ".bb(bb)\n"
       ");\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo bar (\n"
       "      .a (a),\n"  // align doesn't add too many spaces, so align
       "      .bb(bb)\n"
       "  );\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo bar(\n"
       ".a(a),\n"
       ".bbbbbb(bb)\n"
       ");\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo bar (\n"
       "      .a(a),\n"  // align would add too many spaces, so flush-left
       "      .bbbbbb(bb)\n"
       "  );\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo bar(\n"
       ".a    (a),\n"  // user manually triggers alignment with excess spaces
       ".bbbbbb(bb)\n"
       ");\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo bar (\n"
       "      .a     (a),\n"  // alignment fixed
       "      .bbbbbb(bb)\n"
       "  );\n"
       "endmodule : mm\n"},

      // net variable declarations
      {"module nn;\n"
       "wire wwwww;\n"
       "logic lll;\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  wire  wwwww;\n"  // alignment adds few spaces, so align
       "  logic lll;\n"
       "endmodule : nn\n"},
      {"module nn;\n"
       "wire wwwww;\n"
       "foo_pkg::baz_t lll;\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  wire wwwww;\n"  // alignment adds too many spaces, so flush-left
       "  foo_pkg::baz_t lll;\n"
       "endmodule : nn\n"},
      {"module nn;\n"
       "wire     wwwww;\n"  // user injects spaces to trigger alignment
       "foo_pkg::baz_t lll;\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  wire           wwwww;\n"  // ... and gets alignment
       "  foo_pkg::baz_t lll;\n"
       "endmodule : nn\n"},
      {// data/net declarations as generate items (conditional)
       "module nn;\n"
       "if (cc)begin:fff\n"
       "wire wwwww;\n"
       "logic lll;\n"
       "end:fff\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  if (cc) begin : fff\n"
       "    wire  wwwww;\n"  // alignment adds few spaces, so align
       "    logic lll;\n"
       "  end : fff\n"
       "endmodule : nn\n"},

      // continuous assignments
      {"module m_assign;\n"
       "assign foo = 1'b1;\n"  // alignment adds few spaces, so align
       "assign baar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  assign foo  = 1'b1;\n"  // aligned
       "  assign baar = 1'b0;\n"
       "endmodule\n"},
      {"module m_assign;\n"
       "assign foo  =  1'b1;\n"  // alignment adds too many spaces, so
                                 // flush-left
       "assign baaaaaar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  assign foo = 1'b1;\n"  // flush-left
       "  assign baaaaaar = 1'b0;\n"
       "endmodule\n"},
      {"module m_assign;\n"
       "assign foo  =     1'b1;\n"  // induce alignment with excess spaces
       "assign baaaaaar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  assign foo      = 1'b1;\n"  // aligned
       "  assign baaaaaar = 1'b0;\n"
       "endmodule\n"},
      {// currently, does not assign across ifdefs
       "module m_assign;\n"
       "`ifdef FOO\n"
       "assign foo  =     1'b1;\n"  // induce alignment with excess spaces
       "assign baaaaaar = 1'b0;\n"
       "`else\n"
       "assign zooo = 2'b11;\n"
       "assign yoo = 2'b00;\n"
       "`endif\n"
       "endmodule\n",
       "module m_assign;\n"
       "`ifdef FOO\n"
       "  assign foo      = 1'b1;\n"  // aligned
       "  assign baaaaaar = 1'b0;\n"
       "`else\n"                   // aligned separately above/below
       "  assign zooo = 2'b11;\n"  // aligned
       "  assign yoo  = 2'b00;\n"  // aligned
       "`endif\n"
       "endmodule\n"},
      {// mixed net declaration and continuous assignment, both groups aligned
       "module m_assign;\n"
       "wire     wwwww;\n"  // induce alignment
       "foo_pkg::baz_t lll;\n"
       "assign foo  =     1'b1;\n"  // induce alignment
       "assign baaaaaar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  wire           wwwww;\n"  // aligned
       "  foo_pkg::baz_t lll;\n"
       "  assign foo      = 1'b1;\n"  // aligned
       "  assign baaaaaar = 1'b0;\n"
       "endmodule\n"},
      {// continuous assignments as generate items (conditional)
       "module m_assign;\n"
       "if (xy) begin\n"
       "assign foo  =  1'b0;\n"  // align: adds few spaces
       "assign baaar = 1'b1;\n"
       "end else begin\n"
       "assign goo  =      1'b1;\n"  // induce alignment with excess spaces
       "assign zaaaaaar = 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module m_assign;\n"
       "  if (xy) begin\n"
       "    assign foo   = 1'b0;\n"  // aligned
       "    assign baaar = 1'b1;\n"
       "  end else begin\n"
       "    assign goo      = 1'b1;\n"  // induce alignment with excess spaces
       "    assign zaaaaaar = 1'b0;\n"
       "  end\n"
       "endmodule\n"},
      {// continuous assignments as generate items (loop)
       "module m_assign;\n"
       "for(genvar i=0; i<k; ++i ) begin\n"
       "assign foo  =  1'b0;\n"  // align: adds few spaces
       "assign baaar = 1'b1;\n"
       "end\n"
       "endmodule\n",
       "module m_assign;\n"
       "  for (genvar i = 0; i < k; ++i) begin\n"
       "    assign foo   = 1'b0;\n"  // aligned
       "    assign baaar = 1'b1;\n"
       "  end\n"
       "endmodule\n"},
      {// continuous assignments as generate items (case)
       "module m_assign;\n"
       "case (c)\n"
       "jk:begin\n"
       "assign foo  =  1'b0;\n"  // align: adds few spaces
       "assign baaar = 1'b1;\n"
       "end\n"
       "endcase\n"
       "endmodule\n",
       "module m_assign;\n"
       "  case (c)\n"
       "    jk: begin\n"
       "      assign foo   = 1'b0;\n"  // aligned
       "      assign baaar = 1'b1;\n"
       "    end\n"
       "  endcase\n"
       "endmodule\n"},
      {// continuous assignment with comment
       "module m;\n"
       "// comment1\n"
       "assign aaaaa = (bbbbb != ccccc) &\n"
       "// comment2\n"
       "(ddddd | (eeeee & ffffff));\n"
       "endmodule\n",
       "module m;\n"
       "  // comment1\n"
       "  assign aaaaa = (bbbbb != ccccc) &\n"
       "      // comment2\n"
       "      (ddddd | (eeeee & ffffff));\n"
       "endmodule\n"},

      // net/variable assignments: blocking and nonblocking
      {"module  ma ;\n"
       "initial  begin\n"
       "aa = b;\n"
       "c = 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  initial begin\n"
       "    aa = b;\n"
       "    c  = 1'b0;\n"  // only one space to align
       "  end\n"
       "endmodule\n"},
      {"function void  fa ;\n"
       "c = 1'b0;\n"
       "aa = b;\n"
       "endfunction\n",
       "function void fa;\n"
       "  c  = 1'b0;\n"  // only one space to align
       "  aa = b;\n"
       "endfunction\n"},
      {"task  ta ; \n"
       "aa =  b;\n"
       "c = 1'b0;\n"
       "endtask\n",
       "task ta;\n"
       "  aa = b;\n"
       "  c  = 1'b0;\n"  // only one space to align
       "endtask\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaa <= b;\n"
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaa <= b;\n"
       "    c   <= 1'b0;\n"  // only two spaces to align
       "  end\n"
       "endmodule\n"},
      {"function int  fa ;\n"
       "c <= 1'b0;\n"
       "aa <= b;\n"
       "return 0 ;\n"
       "endfunction\n",
       "function int fa;\n"
       "  c  <= 1'b0;\n"  // only one space to align
       "  aa <= b;\n"
       "  return 0;\n"
       "endfunction\n"},
      {"task  ta ; \n"
       "$display (\"hello\" );\n"
       "aa <=  b;\n"
       "c <= 1'b0;\n"
       "endtask\n",
       "task ta;\n"
       "  $display(\"hello\");\n"
       "  aa <= b;\n"
       "  c  <= 1'b0;\n"  // only one space to align
       "endtask\n"},
      {// mixed blocking and nonblocking assignments
       "module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaaa  = b;\n"
       "ccc  = 1'b0;\n"
       "aaa <= b;\n"
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaaa = b;\n"
       "    ccc   = 1'b0;\n"  // only two spaces to align
       "    aaa <= b;\n"
       "    c   <= 1'b0;\n"  // only two spaces to align (separate group)
       "  end\n"
       "endmodule\n"},
      {"task  ta ; \n"
       "aa <=  b;\n"
       "c <= 1'b0;\n"
       "$display (\"hello\" );\n"  // separates above/below alignment groups
       "zzaa <=  b;\n"
       "zzc <= 1'b0;\n"
       "endtask\n",
       "task ta;\n"
       "  aa <= b;\n"
       "  c  <= 1'b0;\n"  // only one space to align
       "  $display(\"hello\");\n"
       "  zzaa <= b;\n"
       "  zzc  <= 1'b0;\n"  // only one space to align
       "endtask\n"},
      {"task  ta ; \n"
       "$display (\"hello\" );\n"
       "aaaaa <=  b;\n"
       "c <= 1'b0;\n"  // need too many spaces to align
       "endtask\n",
       "task ta;\n"
       "  $display(\"hello\");\n"
       "  aaaaa <= b;\n"
       "  c <= 1'b0;\n"  // so keep flush-left
       "endtask\n"},
      {"function void  fa ; \n"
       "$display (\"hello\" );\n"
       "aaaaa =  b;\n"
       "c = 1'b0;\n"  // need too many spaces to align
       "endfunction\n",
       "function void fa;\n"
       "  $display(\"hello\");\n"
       "  aaaaa = b;\n"
       "  c = 1'b0;\n"  // so keep flush-left
       "endfunction\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaxx <= b;\n"
       "c <= 1'b0;\n"  // need too many spaces to align
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaxx <= b;\n"
       "    c <= 1'b0;\n"  // so keep flush-left
       "  end\n"
       "endmodule\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaxx <= b    ;\n"  // inject 4 spaces to induce alignment
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaxx <= b;\n"
       "    c     <= 1'b0;\n"  // induced alignment
       "  end\n"
       "endmodule\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaxx <= b    ;\n"  // inject 4 spaces to induce alignment
       "//comment\n"
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaxx <= b;\n"
       "    //comment\n"       // ignored within alignment group
       "    c     <= 1'b0;\n"  // induced alignment
       "  end\n"
       "endmodule\n"},

      // local variable declarations as statements
      {"task tt ;\n"
       "int foo;\n"  // only 2 spaces needed to align
       "bar_t baz;\n"
       "endtask\n",
       "task tt;\n"
       "  int   foo;\n"  // aligned
       "  bar_t baz;\n"
       "endtask\n"},
      {"function ff ;\n"
       "bar_t baz;\n"
       "int foo;\n"  // only 2 spaces needed to align
       "endfunction\n",
       "function ff;\n"
       "  bar_t baz;\n"
       "  int   foo;\n"  // aligned
       "endfunction\n"},
      {"task tt ;\n"
       "int  foo;\n"  // too many spaces needed to align
       "baaaar_t baz;\n"
       "endtask\n",
       "task tt;\n"
       "  int foo;\n"  // so flush-left
       "  baaaar_t baz;\n"
       "endtask\n"},
      {"function ff ;\n"
       "baaaar_t baz;\n"
       "int  foo;\n"  // too many spaces needed to align
       "endfunction\n",
       "function ff;\n"
       "  baaaar_t baz;\n"
       "  int foo;\n"  // so flush-left
       "endfunction\n"},
      {"task tt ;\n"
       "int        foo;\n"  // injected spaces to induce alignment
       "baaaar_t baz;\n"
       "endtask\n",
       "task tt;\n"
       "  int      foo;\n"  // aligned
       "  baaaar_t baz;\n"
       "endtask\n"},
      {"function ff ;\n"
       "baaaar_t baz    ;\n"  // injected spaces to induce alignment
       "int  foo;\n"
       "endfunction\n",
       "function ff;\n"
       "  baaaar_t baz;\n"
       "  int      foo;\n"  // so aligned
       "endfunction\n"},

      // formal parameters
      {"module pp #(\n"
       "int W,\n"
       "type T\n"
       ") ();\n"
       "endmodule : pp\n",
       "module pp #(\n"
       "    int  W,\n"  // alignment adds few spaces, so do it
       "    type T\n"
       ") ();\n"
       "endmodule : pp\n"},
      {"module pp #(\n"
       "int W,\n"
       "int[xx:yy] T\n"
       ") ();\n"
       "endmodule : pp\n",
       "module pp #(\n"
       "    int W,\n"  // alignment adds many spaces, so flush-left
       "    int [xx:yy] T\n"
       ") ();\n"
       "endmodule : pp\n"},
      {"module pp #(\n"
       "int W,\n"
       "int[xx:yy]     T\n"  // user injected spaces intentionally
       ") ();\n"
       "endmodule : pp\n",
       "module pp #(\n"
       "    int         W,\n"  // ... trigger alignment
       "    int [xx:yy] T\n"
       ") ();\n"
       "endmodule : pp\n"},

      // class member variables
      {"class  cc ;\n"
       "int my_int;\n"
       "bar_t my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int   my_int;\n"  // align doesn't add too many spaces, so align
       "  bar_t my_bar;\n"
       "endclass : cc\n"},
      {"class  cc ;\n"
       "int   my_int;\n"
       "foo_pkg::bar_t my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int my_int;\n"  // align would add too many spaces, so flush-left
       "  foo_pkg::bar_t my_bar;\n"
       "endclass : cc\n"},
      {"class  cc ;\n"
       "int     my_int;\n"  // intentional excessive spaces, trigger alignment
       "foo_pkg::bar_t my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int            my_int;\n"
       "  foo_pkg::bar_t my_bar;\n"
       "endclass : cc\n"},
      {"class  cc ;\n"
       "int    my_int;\n"  // unable to infer user's intent, so preserve
       "foo_pkg::bar_t  my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int    my_int;\n"  // ... but still indent
       "  foo_pkg::bar_t  my_bar;\n"
       "endclass : cc\n"},

      // case item test cases
      {// small difference between flush-left and align, so align
       "function f; case (x)kZZZZ  :if( b )break; default :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZZZZ:   if (b) break;\n"  // aligned, only adds 2 spaces
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// small error relative to flush-left, so flush-left
       "function f; case (x)kZ  :if( b )break; default :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ: if (b) break;\n"  // flush-left
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// intentional spacing error (delta=4) induces alignment
       "function f; case (x)kZ  :if( b )break; default    :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:      if (b) break;\n"
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, with ignored comments
       "function f; case (x)kZ  :if( b )break; \n//c1\n kXX: g = f; "
       "\n//c2\ndefault    :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:      if (b) break;\n"
       "    //c1\n"
       "    kXX:     g = f;\n"
       "    //c2\n"
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, ignore multiline case item in the middle
       "function f; case (x)"
       "kZ  :if( b )break; "
       "kYY    :return 2;"          // excess spaces induce alignment
       "    kXXXXXXXXX: begin end"  // multi-line, ignored
       "    kWWWWW: cc = 23;\n"
       "    kVVV: cd = 24;\n"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:     if (b) break;\n"
       "    kYY:    return 2;\n"
       "    kXXXXXXXXX: begin\n"  // separates above/below groups
       "    end\n"
       "    kWWWWW: cc = 23;\n"
       "    kVVV:   cd = 24;\n"  // aligned
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, ignore multiline case item in the middle
       "function f; case (x)"
       "kZ  :if( b )break; "
       "kYY    :return 2;"               // excess spaces induce alignment
       "    kXXXXXXXXX: if(w)begin end"  // multi-line, ignored
       "    kWWWWW: cc = 23;\n"
       "    kVVV: cd = 24;\n"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:     if (b) break;\n"
       "    kYY:    return 2;\n"
       "    kXXXXXXXXX:\n"   // TODO(fangism): allow this to merge with if()
                             // else indent the following two lines
       "    if (w) begin\n"  // separates above/below groups
       "    end\n"
       "    kWWWWW: cc = 23;\n"
       "    kVVV:   cd = 24;\n"  // aligned
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, ignore multiline case item in the middle
       "task t; case (x)"
       "kZ  :if( b )break; "
       "kYY    :return 2;"           // excess spaces induce alignment
       "    kXXXXXXXXX: fork  join"  // multi-line, ignored
       "    kWWWWW: cc = 23;\n"
       "    kVVV: cd = 24;\n"
       "endcase endtask\n",
       "task t;\n"
       "  case (x)\n"
       "    kZ:     if (b) break;\n"
       "    kYY:    return 2;\n"
       "    kXXXXXXXXX:\n"  // TODO(fangism): allow this to merge with fork
                            // else indent the following two lines
       "    fork\n"         // separates above/below groups
       "    join\n"
       "    kWWWWW: cc = 23;\n"
       "    kVVV:   cd = 24;\n"  // aligned
       "  endcase\n"
       "endtask\n"},
      {// case-inside: small difference between flush-left and align, so align
       "function f; case (x)inside [0:3]  :yy=zzz; [4:11] :yy=zz;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    [0 : 3]:  yy = zzz;\n"  // aligned, only adds 1 spaces
       "    [4 : 11]: yy = zz;\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-inside: align with comments
       "function f; case (x)inside \n//c1\n[0:3]  :yy=zzz;\n//c2\n"
       " [4:11] :yy=zz;\n//c3\n"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    //c1\n"
       "    [0 : 3]:  yy = zzz;\n"  // aligned, only adds 1 spaces
       "    //c2\n"
       "    [4 : 11]: yy = zz;\n"
       "    //c3\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-inside: flush left
       "function f; case (x)inside [0:3]  :yy=zzz; [4:999999] :yy=zz;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    [0 : 3]: yy = zzz;\n"  // flush-left
       "    [4 : 999999]: yy = zz;\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-inside: induce alignment
       "function f; case (x)inside [0:3    ]  :yy=zzz; [4:999999] :yy=zz;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    [0 : 3]:      yy = zzz;\n"  // aligned
       "    [4 : 999999]: yy = zz;\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-generate: align would add few spaces, so align
       "module mc ; case (x)kZ  : gg h(); kXYY :j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ:   gg h ();\n"  // align
       "    kXYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// case-generate + comment: align would add few spaces, so align
       "module mc ; case (x)kZ  : gg h(); \n//c1\n kXYY :j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ:   gg h ();\n"  // align
       "    //c1\n"
       "    kXYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// case-generate: align would add too many space, so flush-left
       "module mc ; case (x)kZ  : gg h(); kXYYYY :j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ: gg h ();\n"  // flush-left
       "    kXYYYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// case-generate: inject spaces to induce alignment
       "module mc ; case (x)kZ  : gg h(); kXYYYY :     j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ:     gg h ();\n"  // align
       "    kXYYYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// randcase: align (small difference from flush-left)
       "task trc  ;randcase 10: x = 1; 1: x = 3; endcase endtask",
       "task trc;\n"
       "  randcase\n"
       "    10: x = 1;\n"
       "    1:  x = 3;\n"  // aligned
       "  endcase\n"
       "endtask\n"},
      {// randcase: inferred flush-left
       "task trc  ;randcase 10000: x = 1; 1: x = 3; endcase endtask",
       "task trc;\n"
       "  randcase\n"
       "    10000: x = 1;\n"
       "    1: x = 3;\n"
       "  endcase\n"
       "endtask\n"},
      {// randcase: induce alignment
       "task trc  ;randcase 10000: x = 1    ; 1: x = 3; endcase endtask",
       "task trc;\n"
       "  randcase\n"
       "    10000: x = 1;\n"
       "    1:     x = 3;\n"  // aligned
       "  endcase\n"
       "endtask\n"},

      // distributions
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "8'hfe :=  9 , \n"
       "12'hfff  := 1 }; "
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      8'hfe   := 9,\n"  // only two spaces needed to align
       "      12'hfff := 1\n"   // so align
       "    };\n"
       "  }\n"
       "endclass\n"},
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "[ 8'h0 : 8'hfe ] :/  9 , \n"
       "8'hff  :/ 1 }; "  // takes many spaces to align this, so...
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      [8'h0 : 8'hfe] :/ 9,\n"
       "      8'hff :/ 1\n"  // flush-left
       "    };\n"
       "  }\n"
       "endclass\n"},
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "[ 8'h0 : 8'hfe ] :/  9 , \n"
       "8'hff  :/     1 }; "  // inject excess spaces to trigger alignment
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      [8'h0 : 8'hfe] :/ 9,\n"
       "      8'hff          :/ 1\n"  // aligned
       "    };\n"
       "  }\n"
       "endclass\n"},
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "//comment1\n"
       "[ 8'h0 : 8'hfe ] :/  9 , \n"
       "//comment2\n"
       "8'hff  :/     1 \n"
       "//comment3\n"
       "}; "  // inject excess spaces to trigger alignment
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      //comment1\n"
       "      [8'h0 : 8'hfe] :/ 9,\n"
       "      //comment2\n"           // align across comment
       "      8'hff          :/ 1\n"  // aligned
       "      //comment3\n"
       "    };\n"
       "  }\n"
       "endclass\n"},
  };
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Override some settings to test auto-inferred alignment.
  style.ApplyToAllAlignmentPolicies(AlignmentPolicy::kInferUserIntent);

  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}  // NOLINT(readability/fn_size)

static constexpr FormatterTestCase kFormatterWideTestCases[] = {
    // specify blocks
    {"module  specify_tests ;\n"
     "specify\n"  // empty list
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify\n"
     "$recrem (posedge R, posedge C,\n"
     "t1, t2);\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    $recrem(posedge R, posedge C, t1, t2);\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify\n"
     "// TODO: add this\n"
     "endspecify \n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    // TODO: add this\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify  \n"
     "  //c1\n"
     "$setup (  posedge A, posedge B,\n"
     "t1);//c2\n"
     " //c3\n"
     "$hold (  posedge B , posedge A,t2);    //c4\n"
     "\t//c5\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    //c1\n"
     "    $setup(posedge A, posedge B, t1);  //c2\n"
     "    //c3\n"
     "    $hold(posedge B, posedge A, t2);  //c4\n"
     "    //c5\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify  \n"
     "$setup (  posedge A, posedge B,\n"
     "t1);\n"
     "$hold (  posedge B , posedge A,t2);\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    $setup(posedge A, posedge B, t1);\n"
     "    $hold(posedge B, posedge A, t2);\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify  \n"
     "  `ifdef CCC\n"
     "$setup (  posedge A, posedge B,\n"
     "t1);\n"
     " `else\n"
     "$hold (  posedge B , posedge A,t2);   \n"
     "\t`endif\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "`ifdef CCC\n"
     "    $setup(posedge A, posedge B, t1);\n"
     "`else\n"
     "    $hold(posedge B, posedge A, t2);\n"
     "`endif\n"
     "  endspecify\n"
     "endmodule\n"},
};

// These tests just need a larger column limit to fit on one line.
TEST(FormatterEndToEndTest, VerilogFormatWideTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 60;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kFormatterWideTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, DisableModulePortDeclarations) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"  // empty ports formatted compactly
       "endmodule\n"},
      {// for a single port, the alignment handler doesn't even consider it a
       // group so it falls back to standard flush-left behavior.
       "module  m   ( input     clk  )\t;\n"
       "  endmodule\n",
       "module m (\n"
       "    input clk\n"
       ");\n"
       "endmodule\n"},
      {// example with two ports
       "module  m   (\n"
       "input  clk,\n"
       "output bar\n"
       ")\t;\n"
       "  endmodule\n",
       "module m (\n"
       "    input  clk,\n"  // indented, but internal pre-existing spacing
                            // preserved
       "    output bar\n"
       ");\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.port_declarations_alignment = verible::AlignmentPolicy::kPreserve;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, DisableModuleInstantiations) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo bar();"
       "  endmodule\n",
       "module m;\n"
       "  foo bar ();\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "logic   xyz;"
       "wire\tabc;"
       "  endmodule\n",
       "module m;\n"
       "  logic xyz;\n"  // indentation still takes effect
       "  wire  abc;\n"  // aligned too
       "endmodule\n"},
      {"  function f  ;\t\n"
       " endfunction\n",
       "function f;\n"
       "endfunction\n"},
      {"  function f  ;\t"
       "foo  bar,baz; "
       " endfunction\n",
       "function f;\n"
       "  foo bar, baz;\n"
       "endfunction\n"},
      {"  task  t  ;\t"
       "foo  bar,baz; "
       " endtask\n",
       "task t;\n"
       "  foo bar, baz;\n"
       "endtask\n"},
      {"module  m  ;\t\n"
       "foo  bar(   .baz(baz)   );"
       "  endmodule\n",
       "module m;\n"
       "  foo bar (.baz(baz));\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo  bar(\n"
       "        .baz  (baz  ),\n"  // example of user-manual alignment
       "        .blaaa(blaaa)\n"
       ");"
       "  endmodule\n",
       "module m;\n"
       "  foo bar (\n"           // indentation still takes effect
       "      .baz  (baz  ),\n"  // named port connections preserved
       "      .blaaa(blaaa)\n"   // named port connections preserved
       "  );\n"                  // this indentation is fixed
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo  #(   .baz(baz)   ) bar();"  // named parameters
       "  endmodule\n",
       "module m;\n"
       "  foo #(.baz(baz)) bar ();\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo  #(\n"
       "        .baz  (baz  ),\n"  // example of user-manual alignment
       "        .blaaa(blaaa)\n"
       ")  bar( );"
       "  endmodule\n",
       "module m;\n"
       "  foo #(\n"              // indentation still takes effect
       "      .baz  (baz  ),\n"  // named parameter arguments preserved
       "      .blaaa(blaaa)\n"   // named parameter arguments preserved
       "  ) bar ();\n"           // this indentation is fixed
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Testing preservation of spaces
  style.named_parameter_alignment = AlignmentPolicy::kPreserve;
  style.named_port_alignment = AlignmentPolicy::kPreserve;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, DisableTryWrapLongLines) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "initial assign a = b;\n"
       "  endmodule\n",
       "module m ();\n"
       "  initial assign a = b;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "initial assign a = {never +gonna +give +you +up,\n"
       "never + gonna +Let +you +down};\n"
       "  endmodule\n",
       "module m ();\n"
       "  initial\n"
       "    assign a = {\n"
       "      never + gonna + give + you + up,\n"
       "      never + gonna + Let + you + down\n"
       "    };\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "initial assign a = {never +gonna +give +you +up+\n"  // over 40 columns,
                                                             // give up
       "never + gonna +Let +you +down};\n"
       "  endmodule\n",
       "module m ();\n"
       "  initial\n"
       "    assign a = {\n"
       "      never +gonna +give +you +up+\n"  // indented properly, but
       "never + gonna +Let +you +down\n"       // preserved original
       "    };\n"
       "endmodule\n"},
      {// The if-header is a single leaf partition, and does not fit,
       // so its original spacing should be preserved.
       // We deliberately insert weird spacing to show that it is preserved.
       "function f;\n"
       "if ((xxx.aaaa >= bbbbbbbbbbbbbbb) &&\n"
       "      ((ccc.ddd  +  eee.ffffff * g) <=\n"
       "       (hhhhhhhhhhhhhhh+iiiiiiiiiiiiiiiiiiii))) begin\n"
       "end\n"
       "endfunction\n",
       "function f;\n"
       "  if ((xxx.aaaa >= bbbbbbbbbbbbbbb) &&\n"  // indentation fixed
       "      ((ccc.ddd  +  eee.ffffff * g) <=\n"
       "       (hhhhhhhhhhhhhhh+iiiiiiiiiiiiiiiiiiii))) begin\n"
       "  end\n"  // indentation fixed
       "endfunction\n"},

      // Make sure indentation still works with wrapping disabled,
      // and leaf partitions fit on one line.
      {"function void f();\n"
       "for (int i = N; i > 0; i--) begin\n"
       "end\n"
       "endfunction\n",
       "function void f();\n"
       "  for (int i = N; i > 0; i--) begin\n"
       "  end\n"
       "endfunction\n"},
      {"function void f();"
       "if(i > 0 ) begin end "
       "endfunction",
       "function void f();\n"
       "  if (i > 0) begin\n"
       "  end\n"
       "endfunction\n"},
      {"function void f();"  // newlines absent from input
       "for (int i=N; i>0; i--) begin end "
       "endfunction",
       "function void f();\n"
       "  for (int i = N; i > 0; i--) begin\n"  // spacing corrected
       "  end\n"
       "endfunction\n"},
      {"module m( );\n"
       "  always_ff  @  (  posedge  (  clk  )  ) begin\n"
       "out  <=  rst_clk  ?  0 : in  ;\n"
       "end\n"
       "endmodule : simple\n",
       "module m ();\n"
       "  always_ff @(posedge (clk)) begin\n"
       "    out <= rst_clk ? 0 : in;\n"
       "  end\n"
       "endmodule : simple\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.try_wrap_long_lines = false;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, ModulePortDeclarationsIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"  // empty ports formatted compactly
       "endmodule\n"},
      {// single port example
       "module  m   ( input     clk  )\t;\n"
       "  endmodule\n",
       "module m (\n"
       "  input clk\n"  // 2 spaces
       ");\n"
       "endmodule\n"},
      {// example with two ports
       "module  m   (\n"
       "input  clk,\n"
       "output bar\n"
       ")\t;\n"
       "  endmodule\n",
       "module m (\n"
       "  input  clk,\n"  // indented 2 spaces, and aligned
       "  output bar\n"
       ");\n"
       "endmodule\n"},
      {// interface example
       "interface  handshake   (\n"
       "wire req,\n"
       "wire ack\n"
       ")\t;\n"
       "  endinterface\n",
       "interface handshake (\n"
       "  wire req,\n"  // indented 2 spaces
       "  wire ack\n"
       ");\n"
       "endinterface\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.port_declarations_indentation = IndentationStyle::kIndent;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, NamedPortConnectionsIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"  // empty ports formatted compactly
       "endmodule\n"},
      {// single port example
       "module  m ;\n"
       "foo bar( .clk( clk ) )\t;\n"
       "  endmodule\n",
       "module m;\n"
       "  foo bar (.clk(clk));\n"
       "endmodule\n"},
      {// two port example
       "module  m ;\n"
       "foo bar( .clk2( clk ),.data (data) )\t;\n"
       "  endmodule\n",
       "module m;\n"
       "  foo bar (\n"
       "    .clk2(clk),\n"  // indent only +2 spaces
       "    .data(data)\n"
       "  );\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.named_port_indentation = IndentationStyle::kIndent;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, FormalParametersIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m #(   ) ;\n"  // empty parameters
       "  endmodule\n",
       "module m #();\n"
       "endmodule\n"},
      {// single parameter example
       "module  m   #( int W = 2)\t;\n"
       "  endmodule\n",
       "module m #(\n"
       "  int W = 2\n"  // indented 2 spaces
       ");\n"
       "endmodule\n"},
      {// module with two parameters
       "module  m   #(\n"
       "int W = 2,\n"
       "int L = 4\n"
       ")\t;\n"
       "  endmodule\n",
       "module m #(\n"
       "  int W = 2,\n"  // indented 2 spaces
       "  int L = 4\n"
       ");\n"
       "endmodule\n"},
      {// interface with two parameters
       "interface  m   #(\n"
       "int W = 2,\n"
       "int L = 4\n"
       ")\t;\n"
       "  endinterface\n",
       "interface m #(\n"
       "  int W = 2,\n"  // indented 2 spaces
       "  int L = 4\n"
       ");\n"
       "endinterface\n"},
      {// class with two parameters
       "class  c   #(\n"
       "int W = 2,\n"
       "int L = 4\n"
       ")\t;\n"
       "  endclass\n",
       "class c #(\n"
       "  int W = 2,\n"  // indented 2 spaces
       "  int L = 4\n"
       ");\n"
       "endclass\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.formal_parameters_indentation = IndentationStyle::kIndent;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, NamedParametersIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m #(   ) ;\n"  // empty parameters
       "  endmodule\n",
       "module m #();\n"
       "endmodule\n"},
      {"module  m  ;\t\n"
       " foo #()bar();\n"
       "  endmodule\n",
       "module m;\n"
       "  foo #() bar ();\n"
       "endmodule\n"},
      {// one named parameter
       "module  m ;\n"
       "foo #(.W(1)) bar();\n"
       "  endmodule\n",
       "module m;\n"
       "  foo #(.W(1)) bar ();\n"
       "endmodule\n"},
      {// two named parameters
       "module  m ;\n"
       "foo #(.W(1), .L(2)) bar();\n"
       "  endmodule\n",
       "module m;\n"
       "  foo #(\n"
       "    .W(1),\n"  // indent +2 spaces only
       "    .L(2)\n"
       "  ) bar ();\n"
       "endmodule\n"},
      {// class data member with two parameters
       "class  c  ;\n"
       " foo_pkg::bar_t#(\n"
       ".W(2),.L(4)"
       ") baz;\n"
       "  endclass\n",
       "class c;\n"
       "  foo_pkg::bar_t #(\n"
       "    .W(2),\n"  // indent +2 spaces only
       "    .L(4)\n"
       "  ) baz;\n"
       "endclass\n"},
      {// typedef with two parameters
       "typedef \n"
       " foo_pkg::bar_t  #("
       ".W(2),.L(4)"
       ") baz;\n",
       "typedef foo_pkg::bar_t#(\n"
       "  .W(2),\n"  // indent +2 spaces only
       "  .L(4)\n"
       ") baz;\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.named_parameter_indentation = IndentationStyle::kIndent;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

struct SelectLinesTestCase {
  absl::string_view input;
  LineNumberSet lines;  // explicit set of lines to enable formatting
  absl::string_view expected;
};

// Tests that formatter honors selected line numbers.
TEST(FormatterEndToEndTest, SelectLines) {
  const SelectLinesTestCase kTestCases[] = {
      {"", {}, ""},
      {"", {{1, 2}}, ""},
      {// expect all three lines for format
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {},
       "parameter int foo_line1 = 0;\n"
       "parameter int foo_line2 = 0;\n"
       "parameter int foo_line3 = 0;\n"},
      {// expect only one line to format
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{1, 2}},
       "parameter int foo_line1 = 0;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"},
      {// expect only one line to format
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{2, 3}},
       "  parameter    int foo_line1 =     0 ;\n"
       "parameter int foo_line2 = 0;\n"
       "  parameter    int foo_line3 =     0 ;\n"},
      {// expect only one line to format
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{3, 4}},
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "parameter int foo_line3 = 0;\n"},
      {// expect to format two lines
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{1, 3}},
       "parameter int foo_line1 = 0;\n"
       "parameter int foo_line2 = 0;\n"
       "  parameter    int foo_line3 =     0 ;\n"},
      {// expect to format two lines
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{2, 4}},
       "  parameter    int foo_line1 =     0 ;\n"
       "parameter int foo_line2 = 0;\n"
       "parameter int foo_line3 = 0;\n"},
      {// expect to format two lines
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{1, 2}, {3, 4}},
       "parameter int foo_line1 = 0;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "parameter int foo_line3 = 0;\n"},
      {// expect to format all lines
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{1, 4}},
       "parameter int foo_line1 = 0;\n"
       "parameter int foo_line2 = 0;\n"
       "parameter int foo_line3 = 0;\n"},
      {// expect to format no lines (line numbers out of bounds)
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n",
       {{4, 6}},
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"},
      {// expect to format all lines
       "// verilog_format: on\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "  parameter    int foo_line4 =     0 ;\n",
       {},
       "// verilog_format: on\n"
       "parameter int foo_line2 = 0;\n"
       "parameter int foo_line3 = 0;\n"
       "parameter int foo_line4 = 0;\n"},
      {// expect to format no lines
       "// verilog_format: off\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "  parameter    int foo_line4 =     0 ;\n",
       {},
       "// verilog_format: off\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "  parameter    int foo_line4 =     0 ;\n"},
      {// expect to format some lines
       "// verilog_format: on\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "  parameter    int foo_line4 =     0 ;\n",
       {{3, 5}},
       "// verilog_format: on\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "parameter int foo_line3 = 0;\n"  // disable lines 3,4
       "parameter int foo_line4 = 0;\n"},
      {// enable all lines, but respect format-off
       "  parameter    int foo_line1 =     0 ;\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "// verilog_format: off\n"
       "  parameter    int foo_line4 =     0 ;\n",
       {{1, 5}},
       "parameter int foo_line1 = 0;\n"
       "parameter int foo_line2 = 0;\n"
       "// verilog_format: off\n"
       "  parameter    int foo_line4 =     0 ;\n"},
      {// enable all lines, but respect format-off
       "  parameter    int foo_line1 =     0 ;\n"
       "// verilog_format: off\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "// verilog_format: on\n"
       "  parameter    int foo_line5 =     0 ;\n",
       {{1, 6}},
       "parameter int foo_line1 = 0;\n"
       "// verilog_format: off\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "// verilog_format: on\n"
       "parameter int foo_line5 = 0;\n"},
      {// enable all lines, but respect format-off
       "// verilog_format: off\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "// verilog_format: on\n"
       "  parameter    int foo_line5 =     0 ;\n",
       {{1, 6}},
       "// verilog_format: off\n"
       "  parameter    int foo_line2 =     0 ;\n"
       "  parameter    int foo_line3 =     0 ;\n"
       "// verilog_format: on\n"
       "parameter int foo_line5 = 0;\n"},
      {"module m;\n"
       "  if (foo) begin:l1\n"
       "    if (foo) begin:l2\n"
       "      always_comb\n"  // normally this line and next would fit together
       "        d<=#1ps   x_lat\t;\n"  // only format this line, 5
       "    end : l2\n"
       "  end : l1\n"
       "endmodule\n",
       {{5, 6}},
       "module m;\n"
       "  if (foo) begin:l1\n"
       "    if (foo) begin:l2\n"
       "      always_comb\n"  // incremental mode prevents joining next line
       "        d <= #1ps x_lat;\n"  // only this line changed
       "    end : l2\n"
       "  end : l1\n"
       "endmodule\n"},

      // Next three test cases: one whole-file, two incremental
      {"module m(\n"
       "  input wire f,\n"
       "  input  foo::bar  ggg\n"
       ");\n"
       "endmodule:m\n",
       {},  // format all lines
       "module m (\n"
       "    input wire     f,\n"
       "    input foo::bar ggg\n"
       ");\n"
       "endmodule : m\n"},
      {"module m(\n"
       "  input wire f,\n"
       "  input  foo::bar  ggg\n"  // "new line", formatted incrementally
       ");\n"
       "endmodule:m\n",
       {{3, 4}},  // format incrementally
       "module m(\n"
       "  input wire f,\n"
       "    input  foo::bar  ggg\n"  // "new line" remains untouched
       ");\n"
       "endmodule:m\n"},
      {"module m(\n"
       "  input  wire   f,\n"  // "new line", formatted incrementally
       "  input  foo::bar  ggg\n"
       ");\n"
       "endmodule:m\n",
       {{2, 3}},  // format incrementally
       "module m(\n"
       "    input  wire   f,\n"  // "new line" indented, but other spaces kept
       "  input  foo::bar  ggg\n"
       ");\n"
       "endmodule:m\n"},
  };
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                      stream, test_case.lines);
    EXPECT_OK(status) << status.message() << '\n'
                      << "Lines: " << test_case.lines;
    EXPECT_EQ(stream.str(), test_case.expected)
        << "code:\n"
        << test_case.input << "\nlines: " << test_case.lines;
  }
}

// These tests verify the mode where horizontal spacing is discarded while
// vertical spacing is preserved.
TEST(FormatterEndToEndTest, PreserveVSpacesOnly) {
  static constexpr FormatterTestCase kTestCases[] = {
      // {input, expected},
      // No tokens cases: still preserve vertical spacing, but not horizontal
      {"", ""},
      {"    ", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"  \n", "\n"},
      {"\n  ", "\n"},
      {"  \n  ", "\n"},
      {"  \n  \t\t\n\t  ", "\n\n"},

      // The remaining cases have at least one non-whitespace token.

      // single comment
      {"//\n", "//\n"},
      {"//  \n", "//  \n"},  // trailing spaces inside comment untouched
      {"\n//\n", "\n//\n"},
      {"\n\n//\n", "\n\n//\n"},
      {"\n//\n\n", "\n//\n\n"},
      {"      //\n", "//\n"},  // spaces before comment discarded
      {"   \n   //\n", "\n//\n"},
      {"   \n   //\n  \n  ", "\n//\n\n"},  // trailing spaces discarded

      // multi-comment
      {"//\n//\n", "//\n//\n"},
      {"\n//\n\n//\n\n", "\n//\n\n//\n\n"},
      {"\n//\n\n//\n", "\n//\n\n//\n"},  // blank line between comments

      // Module cases with token partition boundary (before 'endmodule').
      {"module foo;endmodule\n", "module foo;\nendmodule\n"},
      {"module foo;\nendmodule\n", "module foo;\nendmodule\n"},
      {"module foo;\n\nendmodule\n", "module foo;\n\nendmodule\n"},
      {"\nmodule foo;endmodule\n", "\nmodule foo;\nendmodule\n"},
      {"\nmodule foo     ;    endmodule\n", "\nmodule foo;\nendmodule\n"},
      {"\nmodule\nfoo\n;endmodule\n", "\nmodule foo;\nendmodule\n"},
      {"\nmodule foo;endmodule\n\n\n", "\nmodule foo;\nendmodule\n\n\n"},
      {"\n\n\nmodule foo;endmodule\n", "\n\n\nmodule foo;\nendmodule\n"},
      {"\nmodule\nfoo\n;\n\n\nendmodule\n", "\nmodule foo;\n\n\nendmodule\n"},

      // Module cases with one indented item, various original vertical spacing
      {"module foo;wire w;endmodule\n", "module foo;\n  wire w;\nendmodule\n"},
      {"  module   foo  ;wire    w  ;endmodule  \n  ",
       "module foo;\n  wire w;\nendmodule\n"},
      {"\nmodule\nfoo\n;\nwire\nw\n;endmodule\n\n",
       "\nmodule foo;\n  wire w;\nendmodule\n\n"},
      {"\n\nmodule\nfoo\n;\n\n\nwire\nw\n;\n\nendmodule\n\n",
       "\n\nmodule foo;\n\n\n  wire w;\n\nendmodule\n\n"},

      // The following cases show that some horizontal whitespace is discarded,
      // while vertical spacing is preserved on partition boundaries.
      {"     module  foo\t   \t;    endmodule   \n",
       "module foo;\nendmodule\n"},
      {"\t\n     module  foo\t\t;    endmodule   \n",
       "\nmodule foo;\nendmodule\n"},

      // Module with comments intermingled.
      {
          "//1\nmodule foo;//2\nwire w;//3\n//4\nendmodule\n",
          "//1\nmodule foo;  //2\n  wire w;  //3\n  //4\nendmodule\n"
          // TODO(fangism): whether or not //4 should be indented is
          // questionable (in similar cases below too).
      },
      {// now with extra blank lines
       "//1\n\nmodule foo;//2\n\nwire w;//3\n\n//4\n\nendmodule\n\n",
       "//1\n\nmodule foo;  //2\n\n  wire w;  //3\n\n  //4\n\nendmodule\n\n"},

      {
          // module with comments-only in some empty blocks, properly indented
          "  // humble module\n"
          "  module foo (// non-port comment\n"
          "// port comment 1\n"
          "// port comment 2\n"
          ");// header trailing comment\n"
          "// item comment 1\n"
          "// item comment 2\n"
          "endmodule\n",
          "// humble module\n"
          "module foo (  // non-port comment\n"
          "    // port comment 1\n"
          "    // port comment 2\n"
          ");  // header trailing comment\n"
          "  // item comment 1\n"
          "  // item comment 2\n"
          "endmodule\n",
      },

      {
          // module with comments around non-empty blocks
          "  // humble module\n"
          "  module foo (// non-port comment\n"
          "// port comment 1\n"
          "input   logic   f  \n"
          "// port comment 2\n"
          ");// header trailing comment\n"
          "// item comment 1\n"
          "wire w ; \n"
          "// item comment 2\n"
          "endmodule\n",
          "// humble module\n"
          "module foo (  // non-port comment\n"
          "    // port comment 1\n"
          "    input logic f\n"
          "    // port comment 2\n"
          ");  // header trailing comment\n"
          "  // item comment 1\n"
          "  wire w;\n"
          "  // item comment 2\n"
          "endmodule\n",
      },
  };
  FormatStyle style;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kFormatterTestCasesElseStatements[] = {
    {"module m;"
     "task static t; if (r == t) a.b(c); else d.e(f); endtask;"
     "endmodule",
     "module m;\n"
     "  task static t;\n"
     "    if (r == t) a.b(c);\n"
     "    else d.e(f);\n"
     "  endtask\n"
     "  ;\n"  // possibly unintended stray ';'
     "endmodule\n"},
    {"module m;"
     "task static t; if (r == t) begin a.b(c); end else begin d.e(f); end "
     "endtask;"
     "endmodule",
     "module m;\n"
     "  task static t;\n"
     "    if (r == t) begin\n"
     "      a.b(c);\n"
     "    end else begin\n"
     "      d.e(f);\n"
     "    end\n"
     "  endtask\n"
     "  ;\n"  // stray ';'
     "endmodule\n"},
    {"module m;initial begin if(a==b)"
     "c.d(e);else\n"
     "f.g(h);end endmodule",
     "module m;\n"
     "  initial begin\n"
     "    if (a == b) c.d(e);\n"
     "    else f.g(h);\n"
     "  end\n"
     "endmodule\n"},
    {"   module m;  always_comb    begin     \n"
     "        if      ( a   ) b =  16'hdead    ; \n"
     "  else if (   c     )  d= 16 'hbeef  ;   \n"
     "     else        if (e) f=16'hca_fe ;     \n"
     "end   \n endmodule\n",
     "module m;\n"
     "  always_comb begin\n"
     "    if (a) b = 16'hdead;\n"
     "    else if (c) d = 16'hbeef;\n"
     "    else if (e) f = 16'hca_fe;\n"
     "  end\n"
     "endmodule\n"},
    {"module foo();\n"
     "always_comb begin\n"
     "value = function_name(.long_parameter(8'hA), .parameter_three(foobar));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(\n"
     "      .long_parameter(8'hA),\n"
     "      .parameter_three(foobar)\n"
     "    );\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo();\n"
     "always_comb begin\n"
     "value = function_name(.a(1), .b(2));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(.a(1), .b(2));\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo ();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, foobar, signal_1234); end\n"
     "always_comb begin\n"
     "value = function_name(8'hA, foobar); end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(8'hA, foobar,\n"
     "                          signal_1234);\n"
     "  end\n"
     "  always_comb begin\n"
     "    value = function_name(8'hA, foobar);\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo ();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, foobar, signal_1234);\n"
     "value = function_name(8'hA, foobar, signal_1234); end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(8'hA, foobar,\n"
     "                          signal_1234);\n"
     "    value = function_name(8'hA, foobar,\n"
     "                          signal_1234);\n"
     "  end\n"
     "endmodule : foo\n"},
    {"always_comb begin\n"
     "value = "
     "f(long_parameter_exceeding_col_limit, foo, bar); end\n",
     "always_comb begin\n"
     "  value = f(\n"
     "    long_parameter_exceeding_col_limit,\n"
     "    foo,\n"
     "    bar\n"
     "  );\n"
     "end\n"},
    {"module foo();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, .parameter_three(foobar));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(\n"
     "      8'hA,\n"
     "      .parameter_three(foobar)\n"
     "    );\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo ();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, 8'hB, 8'hC, .parameter_four(foo), "
     ".par_five(bar));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(\n"
     "      8'hA,\n"
     "      8'hB,\n"
     "      8'hC,\n"
     "      .parameter_four(foo),\n"
     "      .par_five(bar)\n"
     "    );\n"
     "  end\n"
     "endmodule : foo\n"},
    {"class dv_base_mem; function void configure(); \nbegin\n"
     "value = func(8'hA, foobar, signal_1234);\n"
     "value = new(8'hA, foobar, signal_1234); end\n"
     "endfunction : configure endclass\n",
     "class dv_base_mem;\n"
     "  function void configure();\n"
     "    begin\n"
     "      value = func(8'hA, foobar,\n"
     "                   signal_1234);\n"
     "      value = new(8'hA, foobar,\n"
     "                  signal_1234);\n"
     "    end\n"
     "  endfunction : configure\n"
     "endclass\n"},
    {"module m; initial begin\n"
     "        if     (a||b)        c         = 1'b1;\n"
     "d =        1'b1; if         (e)\n"
     "begin f = 1'b0; end else begin\n"
     "    g = h;\n"
     "        end \n"
     " i = 1'b1; "
     "end endmodule\n",
     "module m;\n"
     "  initial begin\n"
     "    if (a || b) c = 1'b1;\n"
     "    d = 1'b1;\n"
     "    if (e) begin\n"
     "      f = 1'b0;\n"
     "    end else begin\n"
     "      g = h;\n"
     "    end\n"
     "    i = 1'b1;\n"
     "  end\n"
     "endmodule\n"},
    {"module m; initial begin\n"
     "if (a&&b&&c) begin\n"
     "         d         = 1'b1;\n"
     "     if (e) begin\n"
     "   f = ff;\n"
     "       end  else   if  (    g  )   begin\n"
     "     h = hh;\n"
     "end else if (i) begin\n"
     "    j   =   (kk == ll) ? mm :\n"
     "      gg;\n"
     "   end     else   if    (  qq )  begin\n"
     "    if      (  xx   ||yy        ) begin    d0 = 1'b0;   d1   =       "
     "1'b1;\n"
     "  end else if (oo) begin aa =    bb; cc      = dd;"
     "         if (zz) zx = xz; else ba = ab;"
     "    end   else  \n begin      vv   =  tt  ;  \n"
     "   end   end "
     "end \n  else if   (uu)\nbegin\n\na=b;if (aa)   b =    c;\n"
     "\nelse    if    \n (bb) \n\nc        =d    ;\n\n\n\n\n    "
     "      else         e\n\n   =   h;\n\n"
     "end \n  else    \n  begin if(x)y=a;else\nbegin\n"
     "\n\n\na=y; if (a)       b     = c;\n\n\n\nelse\n\n\nd=e;end \n"
     "end\n"
     "end endmodule\n",
     "module m;\n"
     "  initial begin\n"
     "    if (a && b && c) begin\n"
     "      d = 1'b1;\n"
     "      if (e) begin\n"
     "        f = ff;\n"
     "      end else if (g) begin\n"
     "        h = hh;\n"
     "      end else if (i) begin\n"
     "        j = (kk == ll) ? mm : gg;\n"
     "      end else if (qq) begin\n"
     "        if (xx || yy) begin\n"
     "          d0 = 1'b0;\n"
     "          d1 = 1'b1;\n"
     "        end else if (oo) begin\n"
     "          aa = bb;\n"
     "          cc = dd;\n"
     "          if (zz) zx = xz;\n"
     "          else ba = ab;\n"
     "        end else begin\n"
     "          vv = tt;\n"
     "        end\n"
     "      end\n"
     "    end else if (uu) begin\n\n"
     "      a = b;\n"
     "      if (aa) b = c;\n\n"
     "      else if (bb) c = d;\n\n\n\n\n"
     "      else e = h;\n\n"
     "    end else begin\n"
     "      if (x) y = a;\n"
     "      else begin\n\n\n\n"
     "        a = y;\n"
     "        if (a) b = c;\n\n\n\n"
     "        else d = e;\n"
     "      end\n"
     "    end\n"
     "  end\n"
     "endmodule\n"}};

TEST(FormatterEndToEndTest, FormatElseStatements) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kFormatterTestCasesElseStatements) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, ConstraintExpressions) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},

      // class members
      {"class Foo; constraint if_c { if (zzzzzzzzzzzzzzzzzzzzz)"
       "{ soft xxxxxxxxxxxxxxxxxxxxxx == yyyyyyyyyyyyyyyyyyy; } } endclass",
       "class Foo;\n"
       "  constraint if_c {\n"
       "    if (zzzzzzzzzzzzzzzzzzzzz) {\n"
       "      soft xxxxxxxxxxxxxxxxxxxxxx == yyyyyyyyyyyyyyyyyyy;\n"
       "    }\n"
       "  }\n"
       "endclass\n"},

      // constraints with if-constraint expressions
      {"constraint xx { if (a) b; }\n", "constraint xx {if (a) b;}\n"},

      {"constraint xx { if (a) {b;} }\n",
       "constraint xx {\n"
       "  if (a) {\n"
       "    b;\n"
       "  }\n"
       "}\n"},

      // multi item constraint
      {"constraint yy { a == b;c==d;}",
       "constraint yy {\n"
       "  a == b;\n"
       "  c == d;\n"
       "}\n"},

      // one-line constraints
      {"constraint only_vec_instr_c {soft only_vec_instr == 0;}",
       "constraint only_vec_instr_c {soft only_vec_instr == 0;}\n"},

      {"constraint\nnum_trans_c\n\n\n{\n\n\nnum_trans inside{[800:1000]};}",
       "constraint num_trans_c {num_trans inside {[800 : 1000]};}\n"},

      // if-vs-concatenation expression
      {"constraint c_operation{  if(fixed_operation_en){"
       "aes_operation == fixed_operation"
       ";}}",
       "constraint c_operation {\n"
       "  if (fixed_operation_en) {\n"
       "    aes_operation == fixed_operation;\n"
       "  }\n"
       "}\n"},

      {"constraint c_operation{  if(fixed_operation_en){"
       "aes_operation == fixed_operation"
       "};}",
       "constraint c_operation {\n"
       "  if (fixed_operation_en)\n"
       "  {aes_operation == fixed_operation};\n"
       "}\n"},

      // concatenation expression
      {"constraint c_iv {if (fixed_iv_en) {aes_iv == fixed_iv};}",
       "constraint c_iv {\n"
       "  if (fixed_iv_en)\n"
       "  {aes_iv == fixed_iv};\n"
       "}\n"},

      // looooong test
      {"constraint xx {"
       "if (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)"
       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=="
       "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb;"
       "ccccccccccccccccccccccc==dddddddddddddddddddddd;}",
       "constraint xx {\n"
       "  if (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)\n"
       "  aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa == "
       "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb;\n"
       "  ccccccccccccccccccccccc == dddddddddddddddddddddd;\n"
       "}\n"},
  };
  FormatStyle style;
  style.column_limit =
      100;  // smaller column_limit forces expansion of constraint blocks
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.port_declarations_alignment = verible::AlignmentPolicy::kPreserve;
  for (const auto& test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kFormatterTestCasesEnumDeclarations[] = {
    // Inferring user intent: not too many spaces to be added: align.
    {"typedef enum { kA=1, kAB=2, kABC=3} x;",
     "typedef enum {\n"
     "  kA   = 1,\n"
     "  kAB  = 2,\n"
     "  kABC = 3\n} x;\n"},

    // Lots of spaces would've to be be added - keep flush
    {"typedef enum { kA=1, kAB=2, kABCDEFGHIJKLMN=3} x;",
     "typedef enum {\n"
     "  kA = 1,\n"
     "  kAB = 2,\n"
     "  kABCDEFGHIJKLMN = 3\n} x;\n"},

    // Source spaces indicate alignment wish.
    {"typedef enum { kA     =1, kAB=2, kABCDEFGHIJKLMN=3} x;",
     "typedef enum {\n"
     "  kA              = 1,\n"
     "  kAB             = 2,\n"
     "  kABCDEFGHIJKLMN = 3\n} x;\n"},

    // An empty line groups into several sections, to be aligned independently
    {"typedef enum { kA=1, kAB=2, \n\n kABC=3} x;",
     "typedef enum {\n"
     "  kA  = 1,\n"
     "  kAB = 2,\n"
     "\n"
     "  kABC = 3\n} x;\n"},

    // Lines with comments don't interrupt alignment
    {"typedef enum { kA=1,\n"
     "// hello world\n"
     "kAB=2, kABC=3} x;",
     "typedef enum {\n"
     "  kA   = 1,\n"
     "  // hello world\n"
     "  kAB  = 2,\n"
     "  kABC = 3\n} x;\n"},

    // Generally, comment locations are preserved, and full line comments
    // indented to enum name level.
    {"typedef enum { kA=1,// value kA\n"
     "// hello world\n"
     "kAB=2,// value kAB\n"
     "kABC=3// value kABC\n"
     "} x;",
     "typedef enum {\n"
     "  kA   = 1,  // value kA\n"
     "  // hello world\n"
     "  kAB  = 2,  // value kAB\n"
     "  kABC = 3   // value kABC"
     "\n} x;\n"},

    {"typedef enum { kA=1,// value kA\n"
     "kAB=2,\n"
     "kABC=3// value kABC\n"
     "} x;",
     "typedef enum {\n"
     "  kA   = 1,  // value kA\n"
     "  kAB  = 2,\n"
     "  kABC = 3   // value kABC"
     "\n} x;\n"},

    // Numeric constants are currently flushed left, but maybe todo
    // align right in the future ?
    {"typedef enum { kA=1, kAB=10, kABC=100} x;",
     "typedef enum {\n"
     "  kA   = 1,\n"
     "  kAB  = 10,\n"
     "  kABC = 100\n} x;\n"},
};

TEST(FormatterEndToEndTest, FormatAlignEnumDeclarations) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.enum_assignment_statement_alignment = AlignmentPolicy::kInferUserIntent;
  for (const auto& test_case : kFormatterTestCasesEnumDeclarations) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, DiagnosticShowFullTree) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kFormatterTestCases) {
    std::ostringstream stream, debug_stream;
    ExecutionControl control;
    control.stream = &debug_stream;
    control.show_token_partition_tree = true;
    const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                      stream, kEnableAllLines, control);
    EXPECT_EQ(status.code(), StatusCode::kCancelled);
    EXPECT_TRUE(
        absl::StartsWith(debug_stream.str(), "Full token partition tree"));
  }
}

TEST(FormatterEndToEndTest, DiagnosticLargestPartitions) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kFormatterTestCases) {
    std::ostringstream stream, debug_stream;
    ExecutionControl control;
    control.stream = &debug_stream;
    control.show_largest_token_partitions = 2;
    const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                      stream, kEnableAllLines, control);
    EXPECT_EQ(status.code(), StatusCode::kCancelled);
    EXPECT_TRUE(absl::StartsWith(debug_stream.str(), "Showing the "))
        << "got: " << debug_stream.str();
  }
}

TEST(FormatterEndToEndTest, DiagnosticEquallyOptimalWrappings) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto& test_case : kFormatterTestCases) {
    std::ostringstream stream, debug_stream;
    ExecutionControl control;
    control.stream = &debug_stream;
    control.show_equally_optimal_wrappings = true;
    const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                      stream, kEnableAllLines, control);
    EXPECT_OK(status) << status.message();
    if (!debug_stream.str().empty()) {
      EXPECT_TRUE(absl::StartsWith(debug_stream.str(), "Showing the "))
          << "got: " << debug_stream.str();
      // Cannot guarantee among unit tests that there will be >1 solution.
    }
  }
}

// Test that hitting search space limit results in correct error status.
TEST(FormatterEndToEndTest, UnfinishedLineWrapSearching) {
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  const absl::string_view code("parameter int x = 1+1;\n");

  std::ostringstream stream, debug_stream;
  ExecutionControl control;
  control.max_search_states = 2;  // Cause search to abort early.
  control.stream = &debug_stream;
  const auto status = FormatVerilog(code, "<filename>", style, stream,
                                    kEnableAllLines, control);
  EXPECT_EQ(status.code(), StatusCode::kResourceExhausted);
  EXPECT_TRUE(absl::StartsWith(status.message(), "***"));
}

static constexpr FormatterTestCase kOnelineFormatBaselineTestCases[] = {
    // Reference - following test cases should not be affected by the switch
    {// Minimal useful case
     "covergroup c @ (posedge clk); coverpoint a; endgroup\n",
     "covergroup c @(posedge clk);\n"
     "  coverpoint a;\n"
     "endgroup\n"},
    {// Multiple coverpoints
     "covergroup foo @(posedge clk); coverpoint a; coverpoint b; "
     "coverpoint c; coverpoint d; endgroup\n",
     "covergroup foo @(posedge clk);\n"
     "  coverpoint a;\n"
     "  coverpoint b;\n"
     "  coverpoint c;\n"
     "  coverpoint d;\n"
     "endgroup\n"},
    {// Multiple bins
     "covergroup memory @ (posedge ce); address  :coverpoint addr {"
     "bins low={LOW}; bins high={HIGH};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  address: coverpoint addr {\n"
     "    bins low = {LOW};\n"
     "    bins high = {HIGH};\n"
     "  }\n"
     "endgroup\n"},
    {// Multiple bins with multiple elements
     "covergroup memory @ (posedge ce); address  :coverpoint addr {"
     "bins low={0,127}; bins high={128,255};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  address: coverpoint addr {\n"
     "    bins low = {0, 127};\n"
     "    bins high = {128, 255};\n"
     "  }\n"
     "endgroup\n"},
};

// Tests that constructs that could be formatted as one-liners are formatted
// correctly. This is the baseline test with default style, test cases should
// not be affected by the setting.
TEST(FormatterEndToEndTest, OnelineFormatBaselineTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.expand_coverpoints = false;
  for (const auto& test_case : kOnelineFormatBaselineTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
  // Test again with the switch, should not affect formatting
  style.expand_coverpoints = true;
  for (const auto& test_case : kOnelineFormatBaselineTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Following two test sets are affected by the expand_coverponits switch
// They should contain corresponding test cases
static constexpr FormatterTestCase kOnelineFormatReferenceTestCases[] = {
    {// Coverpoint that could fit on one line
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {bins l = {0};}\n"
     "endgroup\n"},
    {// Fit with a reference
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={LOW};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {bins l = {LOW};}\n"
     "endgroup\n"},
    {// Fit with multiple elements
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0,8};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {bins l = {0, 8};}\n"
     "endgroup\n"},
};

static constexpr FormatterTestCase kOnelineFormatExpandTestCases[] = {
    {// Coverpoint that could fit on one line
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {\n"
     "    bins l = {0};\n"
     "  }\n"
     "endgroup\n"},
    {// Fit with a reference
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={LOW};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {\n"
     "    bins l = {LOW};\n"
     "  }\n"
     "endgroup\n"},
    {// Fit with multiple elements
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0,8};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {\n"
     "    bins l = {0, 8};\n"
     "  }\n"
     "endgroup\n"},
};

TEST(FormatterEndToEndTest, OnelineFormatReferenceTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.expand_coverpoints = false;

  for (const auto& test_case : kOnelineFormatReferenceTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Tests that constructs that could be formatted as one-liners are expanded
// correctly. This is the baseline test with default style
TEST(FormatterEndToEndTest, OnelineFormatExpandTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.expand_coverpoints = true;

  for (const auto& test_case : kOnelineFormatExpandTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

struct DimensionsAlignmentTestCase {
  absl::string_view input;
  absl::string_view expected[4];
};

static constexpr DimensionsAlignmentTestCase
    kPortDeclarationDimensionsAlignmentTestCases[] = {
        {"import \"DPI-C\" context function void func(\n"
         "input bit foo,\n"
         "input bit [2] foobar,\n"
         "input bit [24:16] bar,\n"
         "input bit [2:0][31:0] baz,\n"
         "input bit [4][24:0][16:0] foobarbaz[],\n"
         "input bit [FOOBAZ][31:24] qux[16],\n"
         "input bit [16] quux[8:2][16][32],\n"
         "output bit [2:0][2:0][2:0] quuz[8][16:12]\n"
         ");\n",
         {
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                       foo,\n"
             "  input  bit [     2]              foobar,\n"
             "  input  bit [ 24:16]              bar,\n"
             "  input  bit [   2:0][ 31:0]       baz,\n"
             "  input  bit [     4][ 24:0][16:0] foobarbaz[   ],\n"
             "  input  bit [FOOBAZ][31:24]       qux      [ 16],\n"
             "  input  bit [    16]              quux     [8:2][   16][32],\n"
             "  output bit [   2:0][  2:0][ 2:0] quuz     [  8][16:12]\n"
             ");\n",
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                       foo,\n"
             "  input  bit [     2]              foobar,\n"
             "  input  bit [ 24:16]              bar,\n"
             "  input  bit [   2:0][ 31:0]       baz,\n"
             "  input  bit [     4][ 24:0][16:0] foobarbaz         [     ],\n"
             "  input  bit [FOOBAZ][31:24]       qux               [   16],\n"
             "  input  bit [    16]              quux     [8:2][16][   32],\n"
             "  output bit [   2:0][  2:0][ 2:0] quuz          [ 8][16:12]\n"
             ");\n",
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                      foo,\n"
             "  input  bit              [    2] foobar,\n"
             "  input  bit              [24:16] bar,\n"
             "  input  bit      [   2:0][ 31:0] baz,\n"
             "  input  bit [  4][  24:0][ 16:0] foobarbaz[   ],\n"
             "  input  bit      [FOOBAZ][31:24] qux      [ 16],\n"
             "  input  bit              [   16] quux     [8:2][   16][32],\n"
             "  output bit [2:0][   2:0][  2:0] quuz     [  8][16:12]\n"
             ");\n",
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                      foo,\n"
             "  input  bit              [    2] foobar,\n"
             "  input  bit              [24:16] bar,\n"
             "  input  bit      [   2:0][ 31:0] baz,\n"
             "  input  bit [  4][  24:0][ 16:0] foobarbaz         [     ],\n"
             "  input  bit      [FOOBAZ][31:24] qux               [   16],\n"
             "  input  bit              [   16] quux     [8:2][16][   32],\n"
             "  output bit [2:0][   2:0][  2:0] quuz          [ 8][16:12]\n"
             ");\n",
         }},
        {"module m(\n"
         "output bit [2:0][2:0][2:0] quuz[8][16:12],\n"
         "input bit [16] quux[8:2][16][32],\n"
         "input bit [FOOBAZ][31:24] qux[16],\n"
         "input bit [4][24:0][16:0] foobaz[],\n"
         "input bit [2:0][31:0] baz,\n"
         "input bit [24:16] bar,\n"
         "input bit [2] foobar,\n"
         "input bit foo\n"
         "); endmodule:m\n",
         {
             "module m (\n"
             "    output bit [   2:0][  2:0][ 2:0] quuz  [  8][16:12],\n"
             "    input  bit [    16]              quux  [8:2][   16][32],\n"
             "    input  bit [FOOBAZ][31:24]       qux   [ 16],\n"
             "    input  bit [     4][ 24:0][16:0] foobaz[   ],\n"
             "    input  bit [   2:0][ 31:0]       baz,\n"
             "    input  bit [ 24:16]              bar,\n"
             "    input  bit [     2]              foobar,\n"
             "    input  bit                       foo\n"
             ");\n"
             "endmodule : m\n",
             "module m (\n"
             "    output bit [   2:0][  2:0][ 2:0] quuz       [ 8][16:12],\n"
             "    input  bit [    16]              quux  [8:2][16][   32],\n"
             "    input  bit [FOOBAZ][31:24]       qux            [   16],\n"
             "    input  bit [     4][ 24:0][16:0] foobaz         [     ],\n"
             "    input  bit [   2:0][ 31:0]       baz,\n"
             "    input  bit [ 24:16]              bar,\n"
             "    input  bit [     2]              foobar,\n"
             "    input  bit                       foo\n"
             ");\n"
             "endmodule : m\n",
             "module m (\n"
             "    output bit [2:0][   2:0][  2:0] quuz  [  8][16:12],\n"
             "    input  bit              [   16] quux  [8:2][   16][32],\n"
             "    input  bit      [FOOBAZ][31:24] qux   [ 16],\n"
             "    input  bit [  4][  24:0][ 16:0] foobaz[   ],\n"
             "    input  bit      [   2:0][ 31:0] baz,\n"
             "    input  bit              [24:16] bar,\n"
             "    input  bit              [    2] foobar,\n"
             "    input  bit                      foo\n"
             ");\n"
             "endmodule : m\n",
             "module m (\n"
             "    output bit [2:0][   2:0][  2:0] quuz       [ 8][16:12],\n"
             "    input  bit              [   16] quux  [8:2][16][   32],\n"
             "    input  bit      [FOOBAZ][31:24] qux            [   16],\n"
             "    input  bit [  4][  24:0][ 16:0] foobaz         [     ],\n"
             "    input  bit      [   2:0][ 31:0] baz,\n"
             "    input  bit              [24:16] bar,\n"
             "    input  bit              [    2] foobar,\n"
             "    input  bit                      foo\n"
             ");\n"
             "endmodule : m\n",
         }},
};

TEST(FormatterEndToEndTest, PortDeclarationDimensionsAlignmentTest) {
  FormatStyle style;
  style.column_limit = 64;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  // All combinations of port_declaration_right_align_{un,}packed_dimensions
  static const bool right_align_combinations[][2] = {
      {false, false}, {false, true}, {true, false}, {true, true}};

  for (const auto& test_case : kPortDeclarationDimensionsAlignmentTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    size_t expected_index = 0;
    for (const auto& right_align : right_align_combinations) {
      VLOG(1) << "style variant:\n"
              << "port_declarations_right_align_packed_dimensions: "
              << right_align[0] << "\n"
              << "port_declarations_right_align_unpacked_dimensions: "
              << right_align[1] << "\n";
      style.port_declarations_right_align_packed_dimensions = right_align[0];
      style.port_declarations_right_align_unpacked_dimensions = right_align[1];

      std::ostringstream stream;

      const auto status =
          FormatVerilog(test_case.input, "<filename>", style, stream);
      // Require these test cases to be valid.
      EXPECT_OK(status) << status.message();
      EXPECT_EQ(stream.str(), test_case.expected[expected_index++])
          << "code:\n"
          << test_case.input;
    }
  }
}

// TODO(fangism): directed tests using style variations

static constexpr FormatterTestCase kNestedFunctionsTestCases40ColumnsLimit[] = {
    {"module foo;"
     "`uvm_info(`gfn, $sformatf(\n"
     "\"\\n  base_vseq: generate %0d pulse in channel %0d\", cfg.num_pulses, "
     "i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n  base_vseq: generate %0d pulse in channel %0d\",\n"
     "          cfg.num_pulses, i), UVM_DEBUG)\n"
     "endmodule\n"},
    {"module foo;`uvm_info(`gfn, $sformatf("
     "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
     "cfg.convert2string()), UVM_LOW)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\",\n"
     "          cfg.convert2string()),\n"
     "      UVM_LOW)\n"  // FIXME: Wrapped by SearchLineWraps
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases40ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto& test_case : kNestedFunctionsTestCases40ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kNestedFunctionsTestCases60ColumnsLimit[] = {
    {"module foo;"
     "`uvm_info(`gfn, $sformatf(\n"
     "\"\\n  base_vseq: generate %0d pulse in channel %0d\", cfg.num_pulses, "
     "i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n  base_vseq: generate %0d pulse in channel %0d\",\n"
     "          cfg.num_pulses, i), UVM_DEBUG)\n"
     "endmodule\n"},
    {"module foo;`uvm_info(`gfn, $sformatf("
     "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
     "cfg.convert2string()), UVM_LOW)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\",\n"
     "          cfg.convert2string()), UVM_LOW)\n"
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases60ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 60;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto& test_case : kNestedFunctionsTestCases60ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kNestedFunctionsTestCases80ColumnsLimit[] = {
    {"module foo;"
     "`uvm_info(`gfn, $sformatf(\n"
     "\"\\n  base_vseq: generate %0d pulse in channel %0d\", cfg.num_pulses, "
     "i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d pulse in "
     "channel %0d\",\n"
     "                            cfg.num_pulses, i), UVM_DEBUG)\n"
     "endmodule\n"},
    {"module foo;`uvm_info(`gfn, $sformatf("
     "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
     "cfg.convert2string()), UVM_LOW)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, $sformatf(\n"
     "                      \"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE "
     "|----\\n %s\",\n"
     "                      cfg.convert2string()), UVM_LOW)\nendmodule\n"},
    {"module x;"
     "`uvm_fatal(`gfn, $sformatf("
     "\"The data 0x%0h written to the signature address is formatted "
     "incorrectly.\","
     "signature_data))\n"
     "endmodule",
     "module x;\n"
     "  `uvm_fatal(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"The data 0x%0h written to the signature address is formatted "
     "incorrectly.\",\n"
     "          signature_data))\n"
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases80ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 80;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto& test_case : kNestedFunctionsTestCases80ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kNestedFunctionsTestCases100ColumnsLimit[] =
    {
        {"module foo;"
         "`uvm_info(`gfn, $sformatf(\n"
         "\"\\n  base_vseq: generate %0d pulse in channel %0d\", "
         "cfg.num_pulses, i), UVM_DEBUG)\n"
         "endmodule",
         "module foo;\n"
         "  `uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d pulse in "
         "channel %0d\", cfg.num_pulses, i),\n"
         "            UVM_DEBUG)\n"
         "endmodule\n"},
        {"module foo;`uvm_info(`gfn, $sformatf("
         "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
         "cfg.convert2string()), UVM_LOW)\n"
         "endmodule",
         "module foo;\n"
         "  `uvm_info(`gfn, $sformatf(\"\\n\\n\\t ----| STARTING AES MAIN "
         "SEQUENCE |----\\n %s\",\n"
         "                            cfg.convert2string()), UVM_LOW)\n"
         "endmodule\n"},
        {"module x;"
         "`uvm_fatal(`gfn, $sformatf("
         "\"The data 0x%0h written to the signature address is formatted "
         "incorrectly.\","
         "signature_data))\n"
         "endmodule",
         "module x;\n"
         "  `uvm_fatal(`gfn, $sformatf(\n"
         "                       \"The data 0x%0h written to the signature "
         "address is formatted incorrectly.\",\n"
         "                       signature_data))\n"
         "endmodule\n"},
        {// nested modules, three-levels
         "module x; module y; module z;"
         "`uvm_fatal(`gfn, $sformatf("
         "\"The data 0x%0h written to the signature address is formatted "
         "incorrectly.\","
         "signature_data))\n"
         "endmodule endmodule endmodule",
         "module x;\n"
         "  module y;\n"
         "    module z;\n"
         "      `uvm_fatal(`gfn,\n"
         "                 $sformatf(\n"
         "                     \"The data 0x%0h written to the signature "
         "address is formatted incorrectly.\",\n"
         "                     signature_data))\n"
         "    endmodule\n"
         "  endmodule\n"
         "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases100ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 100;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto& test_case : kNestedFunctionsTestCases100ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kFunctionCallsWithComments[] = {
    {// no comments
     "module foo;\n"
     "`uvm_info(`gfn, \"xx\",\n"
     "cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, \"xx\", cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule\n"},
    {// one comment
     "module foo;\n"
     "`uvm_info(`gfn, \"xx\",  // yy\n"
     "cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, \"xx\",  // yy\n"
     "            cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule\n"},
    {// two comments
     "module foo;\n"
     "`uvm_info(`gfn, \"xx\",  "
     "    cfg.num_pulses, // xx\n"
     " i, UVM_DEBUG)// uuu\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, \"xx\", cfg.num_pulses,  // xx\n"
     "            i, UVM_DEBUG)  // uuu\n"
     "endmodule\n"},

    {// nested function calls with comments
     "module foo;"
     "`uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d"
     " pulse in channel %0d\", cfg.num_pulses, // comment\n"
     " i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d pulse in "
     "channel %0d\",\n"
     "                            cfg.num_pulses,  // comment\n"
     "                            i), UVM_DEBUG)\n"
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FunctionCallsWithComments) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 100;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto& test_case : kFunctionCallsWithComments) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Extracts the first non-whitespace token and prepends it with number of
// of newlines seen in front of it. So "\n \n\n  foo" -> "3foo"
std::string NLCountAndfirstWord(absl::string_view str) {
  std::string result;
  int newline_count = 0;
  const char* begin = str.begin();
  for (/**/; begin < str.end(); ++begin) {
    if (!isspace(*begin)) break;
    newline_count += (*begin == '\n');
  }
  // Emit number of newlines seen up to first token.
  result.append(1,
                static_cast<char>(newline_count + '0'));  // single digit itoa
  const char* end_str = begin;
  for (/**/; end_str < str.end() && !isspace(*end_str); ++end_str)
    ;
  result.append(begin, end_str);
  return result;
}

// Similar to NLCountAndfirstWord() but looking at the last token and trailing
// newlines.
std::string lastWordAndNLCount(absl::string_view str) {
  std::string result;
  int newline_count = 0;
  const char* back = str.end() - 1;
  for (/**/; back >= str.begin(); --back) {
    if (!isspace(*back)) break;
    newline_count += (*back == '\n');
  }

  const char* start_str = back;
  for (/**/; start_str >= str.begin() && !isspace(*start_str); --start_str)
    ;
  result.append(start_str + 1, back - start_str);
  // Emit number of newlines seen following last token.
  result.append(1,
                static_cast<char>(newline_count + '0'));  // single digit itoa
  return result;
}

// Testing the tester...
TEST(FormatterTestInternal, TokenExtractorAndLineCounterTestFixtureTest) {
  absl::string_view str = "\n \nhello world \n \n";
  ASSERT_EQ(NLCountAndfirstWord(str), "2hello");
  ASSERT_EQ(NLCountAndfirstWord(str.substr(1)), "1hello");
  ASSERT_EQ(NLCountAndfirstWord(str.substr(3)), "0hello");

  ASSERT_EQ(lastWordAndNLCount(str), "world2");
  ASSERT_EQ(lastWordAndNLCount(str.substr(0, str.length() - 1)), "world1");
  ASSERT_EQ(lastWordAndNLCount(str.substr(0, str.length() - 2)), "world1");
  ASSERT_EQ(lastWordAndNLCount(str.substr(0, str.length() - 3)), "world0");

  str = "aworld \n \n";  // Make sure to not overshoot begin.
  ASSERT_EQ(lastWordAndNLCount(str.substr(1)), "world2");
}

TEST(FormatterEndToEndTest, RangeFormattingOnlyEmittingRelevantLines) {
  // Including some empty lines to make sure the formatting
  static constexpr absl::string_view unformatted =
      R"(     module foo (// non-port comment

     // some comment

  input  logic  a, input logic  b,input bit [2]
foobar, input    bit [4] foobaz,
    input bit [2]    quux


        ); endmodule
)";

  std::vector<absl::string_view> lines = absl::StrSplit(unformatted, '\n');
  const int kLineCount = lines.size();

  FormatStyle style;

  // Go through all possible sub-ranges, format these, and compare that the
  // output of the range output is contained inside the full format given the
  // same sub-range.
  for (int start_line = 0; start_line < kLineCount; ++start_line) {
    for (int end_line = start_line; end_line < kLineCount; ++end_line) {
      // Line numbers are 1-index based.
      const verible::Interval<int> range = {start_line + 1, end_line + 1};

      // Format full text for reference
      std::ostringstream full_format;
      absl::Status status =
          FormatVerilog(unformatted, "<filename>", style, full_format, {range});
      EXPECT_OK(status) << status.message();

      // To test: range formatted.
      std::string range_formatted;
      status = FormatVerilogRange(unformatted, "<filename>", style,
                                  &range_formatted, range);
      EXPECT_OK(status) << status.message();
      if (range.empty()) {  // Nothing to format: expect empty output.
        EXPECT_TRUE(range_formatted.empty());
        continue;
      }

      // Area we cover in the input (include the final newline);
      const auto source_begin = lines[start_line].begin();
      const auto source_end = lines[end_line - 1].end() + 1;  // include \n
      const absl::string_view range_unformatted(source_begin,
                                                source_end - source_begin);

      // To compare that we indeed formatted the requested reqgion, we make
      // sure that the first and last token (simplified: non-whitespace word)
      // in the input of the range to be formatted is
      // exactly the first and last token that comes out of the range
      // formatted snippet.
      //
      // While the whitespace might be different at the beginning and end
      // due to formatting, the number of _newlines_ at the beginning and end
      // should be the same, so we include the newline count in the comparison.

      EXPECT_EQ(NLCountAndfirstWord(range_unformatted),
                NLCountAndfirstWord(range_formatted))
          << "'" << range_unformatted << "' vs. '" << range_formatted << "'";

      // ... same for the last word.
      EXPECT_EQ(lastWordAndNLCount(range_unformatted),
                lastWordAndNLCount(range_formatted))
          << "'" << range_unformatted << "' vs. '" << range_formatted << "'";

      EXPECT_LE(range_formatted.length(), full_format.str().length());
      EXPECT_THAT(full_format.str(), HasSubstr(range_formatted));
    }
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
