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

#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "common/text/text_structure.h"
#include "common/util/logging.h"
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

using absl::StatusCode;

// Tests that clean output passes.
TEST(VerifyFormattingTest, NoError) {
  const absl::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<filename>");
  const auto& text_structure = ABSL_DIE_IF_NULL(analyzer)->Data();
  const auto status = VerifyFormatting(text_structure, code, "<filename>");
  EXPECT_OK(status);
}

// Tests that un-lexable outputs are caught as errors.
TEST(VerifyFormattingTest, LexError) {
  const absl::string_view code("class c;endclass\n");
  const std::unique_ptr<VerilogAnalyzer> analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<filename>");
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
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<filename>");
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
      VerilogAnalyzer::AnalyzeAutomaticMode(code, "<filename>");
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
  const std::initializer_list<FormatterTestCase> kTestCases = {
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

static const std::initializer_list<FormatterTestCase> kFormatterTestCases = {
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
     "`FOOOO(aa, bb,  //cc\n"
     // TODO(b/152324712): //cc comment should start its own line
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
        "parameter int foo = bar[a + b];\n",
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
     "parameter int a = {b ^ {c[d ^ e]}};\n"},
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
     "    input  wire x,  //c1\n"   // aligned
     "    output reg  yyy  //c2\n"  // trailing comments not aligned
     ");\n"
     "endmodule : foo\n"},
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
     // TODO(b/70310743): flush right in []
     "    input        [a:c  ] x,\n"  // aligned
     "    output logic [a-b:c] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {"module foo(  input   [a:c]x  , "
     "  output logic[a - b: c]  yy ) ;endmodule:foo\n",
     "module foo (\n"
     // TODO(b/70310743): flush right in []
     "    input        [a:c    ] x,\n"  // aligned
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
                                    // TODO(b/70310743): flush right inside
                                    // [:]'s
     "    output reg [e:f  ] yy\n"
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
     // TODO(b/70310743): flush right inside [:]'s
     "    input  wire x [p:q  ][rr:ss],\n"
     "    output reg  yy[jj:kk][m:n  ]\n"  // aligned
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
     "module foo (\n"                   // with parameterized port type
     "    input  pkg::bar_t #(1) x,\n"  // TODO(b/149364228): remove space
                                        // before #
     "    output reg             yy\n"  // aligned
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

    {// aligning here just barely fits in the 40col limit
     "module foo(  input int signed x [a:b],"
     "output reg [mm:nn] yy) ;endmodule:foo\n",
     "module foo (\n"
     // ---------------40col---------------->
     "    input  int signed         x [a:b],\n"  // aligned, still fits
     "    output reg        [mm:nn] yy\n"
     ");\n"
     "endmodule : foo\n"},
    {// when aligning would result in exceeding column limit, don't align for
     // now
     "module foo(  input int signed x [aa:bb],"
     "output reg [mm:nn] yy) ;endmodule:foo\n",
     "module foo (\n"
     // ---------------40col---------------->
     //   input  int signed         x [aa:bb],\n"  // over limit, by comma
     //   output reg        [mm:nn] yy\n"
     "    input int signed x[aa:bb],\n"  // aligned would be 41 columns
     "    output reg [mm:nn] yy\n"
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
    {"module foo #(//comment\n"
     "parameter bar =1,\n"
     "localparam baz =2"
     ") ();"
     "endmodule",
     "module foo #(  //comment\n"
     "    parameter bar = 1,\n"
     "    localparam baz = 2\n"
     ") (\n"
     ");\n"
     "endmodule\n"},
    {"module foo #("
     "parameter bar =1,//comment\n"
     "localparam baz =2"
     ") ();"
     "endmodule",
     "module foo #(\n"
     "    parameter bar = 1,  //comment\n"
     "    localparam baz = 2\n"
     ") (\n"
     ");\n"
     "endmodule\n"},
    {"module foo #("
     "parameter bar =1,"
     "localparam baz =2//comment\n"
     ") ();"
     "endmodule",
     "module foo #(\n"
     "    parameter bar = 1,\n"
     "    localparam baz = 2  //comment\n"
     ") (\n"
     ");\n"
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
     "  reg bear;\n"
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
        "    static byte s = 0;\n"
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
        "endmodule",
        "module mdi;\n"
        "  import \"DPI-C\" function int add();\n"
        "  import \"DPI-C\" function int sleep(\n"  // doesn't fit in 40-col
        "      input int secs);\n"
        "endmodule\n",
    },

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
     "      8'hff :/ 10\n"
     "    };\n"
     "  }\n"
     "endclass\n"},

    {
        "class Foo; constraint if_c { if (z) { soft x == y; } } endclass\n",
        "class Foo;\n"
        "  constraint if_c {\n"
        "    if (z) {soft x == y;}\n"  // TODO(fangism): always expand?
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

    // class with empty parameter list
    {"class foo #(); endclass",
     "class foo #();\n"
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
     "    k1: if (b) break;\n"
     "    default: return 2;\n"
     "  endcase\n"
     "endfunction\n"},
    {// keep short default items on one line
     "function f; case (x)k1 :break; default :if( c )return 2;"
     "endcase endfunction\n",
     "function f;\n"
     "  case (x)\n"
     "    k1: break;\n"
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
     // TODO(fangism): ccccccccc is indented too much because it thinks it is
     // part of a run-on string of tokens starting with aaaaaaaa.
     // We need to inform that it should be on the same level as aaaaaaaa as a
     // sibling item within the same balance group.
     // Right now, there is no notion of sibling items within a balance group.
     "          ccccccccc.ddddddddd);\n"
     "endfunction : warranty\n"},

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
     "      .M(M)  //comment\n"  // EOL comment after last param
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
     "      .dd(dd)  //\n"  // forced to expand by //
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
        "      end,\n"
        "      utils_pkg::decrement())\n"
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
     "    .clk(clk), .rst(rst), .value(value)\n"
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
        "    .in(iiiiiiiin),\n"
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
        "    .in(iiiiiiiin),\n"
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
     "  wait(a == b);\n"
     "  wait(c < d);\n"
     "endtask\n"},
    {// wait statements, single action statement
     "task t; wait  (a==b) p();wait(c<d) q(); endtask",
     "task t;\n"
     "  wait(a == b) p();\n"
     "  wait(c < d) q();\n"
     "endtask\n"},
    {// wait statements, block action statement
     "task t; wait  (a==b) begin p(); end "
     "wait(c<d) begin q(); end endtask",
     "task t;\n"
     "  wait(a == b) begin\n"
     "    p();\n"
     "  end\n"
     "  wait(c < d) begin\n"
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
    {"module mp ;property p1 ; a|->## [0:1]  b;endproperty endmodule",
     "module mp;\n"
     "  property p1;\n"
     "    a |-> ##[0: 1] b;\n"  // with delay range
                                // TODO(b/152411381): fix spacing between "0:"
                                // (context-sensitive)
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
     "    input w,\n"  // compacted, but not aligned
     "    // verilog_format: off\n"
     "input wire  [y:z] wwww,\n"  // not compacted
     "    // verilog_format: on\n"
     "    output reg xx\n"  // compacted, but not aligned
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
        "JgQLBG == 4'h0;, \"foo\")\n"
        "endtask\n",
    },
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

TEST(FormatterEndToEndTest, DisableModulePortDeclarations) {
  const std::initializer_list<FormatterTestCase> kTestCases = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m (   );\n"  // space between () preserved
       "endmodule\n"},
      {"module  m   ( input     clk  )\t;\n"
       "  endmodule\n",
       "module m ( input     clk  );\n"
       "endmodule\n"},
      {"module  m   (\n"
       "input  clk,\n"
       "output bar\n"
       ")\t;\n"
       "  endmodule\n",
       "module m (\n"
       "input  clk,\n"  // disabled, pre-existing alignment maintained
       "output bar\n"   // disabled, pre-existing alignment maintained
       ");\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.format_module_port_declarations = false;
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
  const std::initializer_list<FormatterTestCase> kTestCases = {
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
       "  foo bar();\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "logic   xyz;"
       "wire\tabc;"
       "  endmodule\n",
       "module m;\n"
       "  logic xyz;\n"  // indentation still takes effect
       "  wire abc;\n"   // indentation still takes effect
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
       "  foo  bar(   .baz(baz)   );\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo  bar(\n"
       "        .baz  (baz  ),\n"  // example of user-manual alignment
       "        .blaaa(blaaa)\n"
       ");"
       "  endmodule\n",
       "module m;\n"
       "  foo  bar(\n"             // indentation still takes effect
       "        .baz  (baz  ),\n"  // named port connections preserved
       "        .blaaa(blaaa)\n"   // named port connections preserved
       ");\n"                      // this indentation remains untouched
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.format_module_instantiations = false;
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
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected)
        << "code:\n"
        << test_case.input << "\nlines: " << test_case.lines;
  }
}

// These tests verify the mode where horizontal spacing is discarded while
// vertical spacing is preserved.
TEST(FormatterEndToEndTest, PreserveVSpacesOnly) {
  const std::initializer_list<FormatterTestCase> kTestCases = {
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

static const std::initializer_list<FormatterTestCase>
    kFormatterTestCasesElseStatements = {
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

// TODO(fangism): directed tests using style variations

}  // namespace
}  // namespace formatter
}  // namespace verilog
