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

static constexpr FormatterTestCase kDisableFormatterTestCases[] = {
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

    // between identifier and '(', no args

    {"// verilog_syntax: parse-as-module-body",
     "// verilog_syntax: parse-as-module-body\n"},
    {"// verilog_syntax: parse-as-statements",
     "// verilog_syntax: parse-as-statements\n"},
    {"// verilog_syntax: parse-as-class-body",
     "// verilog_syntax: parse-as-class-body\n"},
    {"// verilog_syntax: parse-as-package-body",
     "// verilog_syntax: parse-as-package-body\n"},
    {"// verilog_syntax: parse-as-library-map",
     "// verilog_syntax: parse-as-library-map\n"},
    {"// verilog_syntax: does-not-exist-mode",
     "// verilog_syntax: does-not-exist-mode\n"},
    {"// verilog_syntax: parse-as-module-body\n",
     "// verilog_syntax: parse-as-module-body\n"},
    {"// verilog_syntax: parse-as-module-body\n"
     "// comment",
     "// verilog_syntax: parse-as-module-body\n"
     "// comment\n"},
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
    {"module indent();\n"
     "   reg     a;\n"
     "   reg [32:0] b;\n"
     "   wire    c;\n"
     "   wire    d = e ? kFoo : kBar;\n"
     "endmodule\n",
     "module indent ();\n"
     "  reg         a;\n"
     "  reg  [32:0] b;\n"
     "  wire        c;\n"
     "  wire        d = e ? kFoo : kBar;\n"
     "endmodule\n"},

    {"class C; T1 b; logic [$] a; T1 [$] c; endclass\n",
     "class C;\n"
     "  T1        b;\n"
     "  logic [$] a;\n"
     "  T1    [$] c;\n"
     "endclass\n"},
    {"class C;\n"
     "  T1 b; //test\n"
     "  logic [$] a; //test\n"
     "  T1    [$] c; //test\n"
     "endclass\n",
     "class C;\n"
     "  T1        b;  //test\n"
     "  logic [$] a;  //test\n"
     "  T1    [$] c;  //test\n"
     "endclass\n"},
    {"class C;\n"
     "  T1\n"
     "  b;\n"
     "  logic\n"
     "  [$]\n"
     "  a;\n"
     "  T1\n"
     "  [$]\n"
     "  c;\n"
     "endclass\n",
     "class C;\n"
     "  T1        b;\n"
     "  logic [$] a;\n"
     "  T1    [$] c;\n"
     "endclass\n"},
    {"class C;\n"
     "  logic/*t*/ [0 : 1] /*t*/\n"
     "  a;/*t*/\n"
     "  T1/*t*/[0 : 1]/*t*/\n"
     "  c;/*t*/\n"
     "endclass\n",
     "class C;\n"
     "  logic/*t*/ [0 : 1]  /*t*/ a;  /*t*/\n"
     "  T1/*t*/    [0 : 1]  /*t*/ c;  /*t*/\n"
     "endclass\n"},
    {"always @(*/*t*/) begin\n"
     "end\n",
     "always @(*  /*t*/) begin\n"
     "end\n"},
    {"always @(/*t*/*) begin\n"
     "end\n",
     "always @(  /*t*/ *) begin\n"
     "end\n"},
    {"always @(/*t*/*/*t*/) begin\n"
     "end\n",
     "always @(  /*t*/ *  /*t*/) begin\n"
     "end\n"},
    {"always @(*) begin\n"
     "end\n",
     "always @(*) begin\n"
     "end\n"},
    {"always @(* ) begin\n"
     "end\n",
     "always @(*) begin\n"
     "end\n"},
    {"always @( *) begin\n"
     "end\n",
     "always @(*) begin\n"
     "end\n"},
    {"always @( * ) begin\n"
     "end\n",
     "always @(*) begin\n"
     "end\n"},
    {"always @(  /*t*/  *   /*t*/    ) begin\n"
     "end\n",
     "always @(  /*t*/ *  /*t*/) begin\n"
     "end\n"},
    {
        // Don't touch verilog_format:off region #1538
        R"(
module testcode;
  // verilog_format: off
  assign a = b
           & c;
  // verilog_format: on
      assign e = d;
endmodule
)",
        R"(
module testcode;
  // verilog_format: off
  assign a = b
           & c;
  // verilog_format: on
  assign e = d;
endmodule
)",

    },

    // -----------------------------------------------------------------
};

TEST(FormatterEndToEndTest, DisableFormatterTestCases) {
  RunFormatterTestCases40(kDisableFormatterTestCases);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
