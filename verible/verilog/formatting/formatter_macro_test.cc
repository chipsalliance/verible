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

#include "gtest/gtest.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter-test-utils.h"
#include "verible/verilog/formatting/formatter.h"

namespace verilog {
namespace formatter {
namespace {

static constexpr FormatterTestCase kMacroFormatterTestCases[] = {
    // preprocessor test cases
    {"`include    \"path/to/file.vh\"\n", "`include \"path/to/file.vh\"\n"},
    {"`include    `\"path/to/file.vh`\"\n", "`include `\"path/to/file.vh`\"\n"},
    {"`define    FOO\n", "`define FOO\n"},
    {"`define    FOO   BAR\n", "`define FOO BAR\n"},
    {"`define    FOO\n"
     "`define  BAR\n",
     "`define FOO\n"
     "`define BAR\n"},
    {"`define FOO_``BAR 1\n", "`define FOO_``BAR 1\n"},
    {"`define FOO_```BAR 1\n", "`define FOO_```BAR 1\n"},
    {"`define FOO ``BAR\n", "`define FOO ``BAR\n"},
    {"`define A``B``C 2\n", "`define A``B``C 2\n"},
    {"`define A(x)``y\n", "`define A(x) ``y\n"},
    {"`define    FOO_``BAR\n", "`define FOO_``BAR\n"},
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
    {// macro call nested with function call containing an ifdef
     "`J(D(`ifdef e))\n",
     "`J(D(\n"
     "   `ifdef e))\n"},

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

    // Multiple compiler directives should remain on separate lines
    // (regression test for bug where they were merged onto one line)
    {"`timescale 1 ps / 1 ps\n"
     "`default_nettype none\n",
     "`timescale 1 ps / 1 ps\n"
     "`default_nettype none\n"},
    {"`resetall\n"
     "`timescale 1ns/1ps\n"
     "`default_nettype wire\n",
     "`resetall\n"
     "`timescale 1ns / 1ps\n"
     "`default_nettype wire\n"},
    // Test with multiple different compiler directives
    {"`resetall\n"
     "`celldefine\n"
     "`timescale 1ns/1ps\n"
     "`default_nettype none\n",
     "`resetall\n"
     "`celldefine\n"
     "`timescale 1ns / 1ps\n"
     "`default_nettype none\n"},
    // Test with compiler directives before module
    {"`timescale 1ps/1ps\n"
     "`default_nettype none\n"
     "module foo;endmodule\n",
     "`timescale 1ps / 1ps\n"
     "`default_nettype none\n"
     "module foo;\nendmodule\n"},
    // Test with various compiler directives
    {"`suppress_faults\n"
     "`enable_portfaults\n"
     "`delay_mode_distributed\n",
     "`suppress_faults\n"
     "`enable_portfaults\n"
     "`delay_mode_distributed\n"},
    {"`default_decay_time 10\n"
     "`default_trireg_strength 50\n",
     "`default_decay_time 10\n"
     "`default_trireg_strength 50\n"},
    // Alignment should ignore compiler directives; declarations align normally.
    // Place directives at top-level between modules (valid SystemVerilog).
    {"`timescale 1ns/1ps\n"
     "module m;\n"
     "  logic a;\n"
     "  logic very_long_name;\n"
     "endmodule\n"
     "`default_nettype none\n"
     "module n;\n"
     "  logic b;\n"
     "endmodule\n",
     "`timescale 1ns / 1ps\n"
     "module m;\n"
     "  logic a;\n"
     "  logic very_long_name;\n"
     "endmodule\n"
     "`default_nettype none\n"
     "module n;\n"
     "  logic b;\n"
     "endmodule\n"},
    {"`begin_keywords \"1800-2017\"\n"
     "module test;endmodule\n"
     "`end_keywords\n",
     "`begin_keywords \"1800-2017\"\n"
     "module test;\nendmodule\n"
     "`end_keywords\n"},

};

TEST(FormatterEndToEndTest, MacroFormatterTestCases) {
  RunFormatterTestCases40(kMacroFormatterTestCases);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
