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

#include "gtest/gtest.h"
#include "verible/verilog/formatting/formatter-test-utils.h"

namespace verilog {
namespace formatter {
namespace {

static constexpr FormatterTestCase kDisableFormatterTestCases[] = {
    // comment-controlled formatter disabling
    {.input = "// verilog_format: off\n"  // disables whole file
              "  `include  \t\t  \"path/to/file.vh\"\n",
     .expected =
         "// verilog_format: off\n"
         "  `include  \t\t  \"path/to/file.vh\"\n"},  // keeps bad spacing
    {.input = "/* verilog_format: off */\n"           // disables whole file
              "  `include  \t\t  \"path/to/file.svh\"  \n",
     .expected =
         "/* verilog_format: off */\n"
         "  `include  \t\t  \"path/to/file.svh\"  \n"},  // keeps bad spacing
    {.input = "// verilog_format: on\n"  // already enabled, no effect
              "  `include  \t  \"path/to/file.svh\"  \t\n",
     .expected = "// verilog_format: on\n"
                 "`include \"path/to/file.svh\"\n"},
    {.input = "// verilog_format: off\n"
              "// verilog_format: on\n"  // re-enable right away
              "  `include  \t\t  \"path/to/file.svh\"  \n",
     .expected = "// verilog_format: off\n"
                 "// verilog_format: on\n"
                 "`include \"path/to/file.svh\"\n"},
    {.input = "/* aaa *//* bbb */\n"  // not formatting controls
              "  `include  \t\t  \"path/to/file.svh\"  \n",  // should format
     .expected = "/* aaa */  /* bbb */\n"  // currently inserts 2 spaces
                 "`include \"path/to/file.svh\"\n"},
    {.input =
         "/* verilog_format: off *//* verilog_format: on */\n"  // re-enable
         "  `include  \t\t  \"path/to/file.svh\"  \n",
     // Note that this case normally wouldn't fit in 40 columns,
     // but disabling formatting lets it overflow.
     .expected = "/* verilog_format: off *//* verilog_format: on */\n"
                 "`include \"path/to/file.svh\"\n"},
    {.input = "// verilog_format: off\n"
              "  `include  \t\t  \"path/to/fileA.svh\"  // verilog_format: on\n"
              "  `include  \t\t  \"path/to/fileB.svh\"  \n",
     // re-enable formatting with comment trailing other tokens
     .expected =
         "// verilog_format: off\n"
         "  `include  \t\t  \"path/to/fileA.svh\"  // verilog_format: on\n"
         "`include \"path/to/fileB.svh\"\n"},
    {.input = "  `include  \t\t  \"path/to/file1.vh\" \n"  // format this
              "// verilog_format: off\n"                   // start disabling
              "  `include  \t\t  \"path/to/file2.vh\" \n"
              "\t\t\n"
              "  `include  \t\t  \"path/to/file3.vh\" \n"
              "// verilog_format: on\n"                     // stop disabling
              "  `include  \t\t  \"path/to/file4.vh\" \n",  // format this
     .expected = "`include \"path/to/file1.vh\"\n"
                 "// verilog_format: off\n"  // start disabling
                 "  `include  \t\t  \"path/to/file2.vh\" \n"
                 "\t\t\n"
                 "  `include  \t\t  \"path/to/file3.vh\" \n"
                 "// verilog_format: on\n"  // stop disabling
                 "`include \"path/to/file4.vh\"\n"},
    {// disabling formatting on a module (to end of file)
     .input = "// verilog_format: off\n"
              "module m;endmodule\n",
     .expected = "// verilog_format: off\n"
                 "module m;endmodule\n"},
    {// disabling formatting on a module (to end of file)
     .input = "// verilog_format: off\n"
              "module m;\n"
              "unindented instantiation;\n"
              "endmodule\n",
     .expected = "// verilog_format: off\n"
                 "module m;\n"
                 "unindented instantiation;\n"
                 "endmodule\n"},
    {// disabling formatting inside a port declaration list disables alignment,
     // but falls back to standard compaction.
     .input = "module align_off(\n"
              "input w  ,\n"
              "    // verilog_format: off\n"
              "input wire  [y:z] wwww,\n"
              "    // verilog_format: on\n"
              "output  reg    xx\n"
              ");\n"
              "endmodule",
     .expected =
         "module align_off (\n"
         "    input w  ,\n"  // preserved because group is partially disabled
         "    // verilog_format: off\n"
         "input wire  [y:z] wwww,\n"  // not compacted
         "    // verilog_format: on\n"
         "    output  reg    xx\n"  // preserved because group is partially
                                    // disabled
         ");\n"
         "endmodule\n"},

    {// multiple tokens with EOL comment
     .input = "module please;  // don't break before the comment\n"
              "endmodule\n",
     .expected = "module please\n"
                 "    ;  // don't break before the comment\n"
                 "endmodule\n"},
    {// one token with EOL comment
     .input = "module please;\n"
              "endmodule  // don't break before the comment\n",
     .expected = "module please;\n"
                 "endmodule  // don't break before the comment\n"},
    {
        // line with only an EOL comment
        .input = "module wild;\n"
                 "// a really long comment on its own line to be left alone\n"
                 "endmodule",
        .expected =
            "module wild;\n"
            "  // a really long comment on its own line to be left alone\n"
            "endmodule\n",
    },
    {
        // primitive declaration
        .input = "primitive primitive1(o, s, r);output o;reg o;input s;input "
                 "r;table 1 "
                 "? :"
                 " ? : 0; ? 1    : 0   : -; endtable endprimitive",
        .expected = "primitive primitive1(o, s, r);\n"
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
        .input = "primitive primitive1 ( o,i ) ;output o;input i;"
                 " table 1  :   0 ;   0  :  1 ; endtable endprimitive",
        .expected = "primitive primitive1(o, i);\n"
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
        .input = "primitive primitive2(o, s, r);output o;input s;input r;"
                 "table 1 ? : 0;? 1 : -; endtable endprimitive",
        .expected = "primitive primitive2(o, s, r);\n"
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
        .input = "primitive comb10(o, i0, i1, i2, i3, i4, i5, i6, i7, i8, i9);"
                 "output o;input i0, i1, i2, i3, i4, i5, i6, i7, i8, i9;"
                 "table 0 ? ? ? ? ? ? ? ? 0 : 0;1 ? ? ? ? ? ? ? ? 0 : 1;"
                 "1 ? ? ? ? ? ? ? ? 1 : 1;0 ? ? ? ? ? ? ? ? 1 : 0;endtable "
                 "endprimitive",
        .expected = "primitive comb10(o, i0, i1, i2, i3, i4,\n"
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
        .input = "primitive level_seq(o, c, d);output o;reg o;"
                 "  input c;input d;table\n"
                 "//  C D  state O\n"
                 "0   ? : ? :  -;  // No Change\n"
                 "? 0   : 0 :  0;  // Unknown\n"
                 "endtable endprimitive",
        .expected = "primitive level_seq(o, c, d);\n"
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
        .input = "primitive edge_seq(o, c, d);output o;reg o;input c;input d;"
                 "table (01) 0 : ? :  0;(01) 1 : ? :  1;(0?) 1 : 1 :  1;(0?) 0 "
                 ": 0 :  "
                 "0;\n"
                 "// ignore negative c\n"
                 "(?0) ? : ? :  -;\n"
                 "// ignore changes on steady c\n"
                 "?  (?\?) : ? :  -; endtable endprimitive",
        .expected = "primitive edge_seq(o, c, d);\n"
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
        .input = "primitive mixed(o, clk, j, k, preset, clear);output o;reg o;"
                 "input c;input j, k;input preset, clear;table "
                 "?  ??  01:?:1 ; // preset logic\n"
                 "?  ??  *1:1:1 ;?  ??  10:?:0 ; // clear logic\n"
                 "?  ??  1*:0:0 ;r  00  00:0:1 ; // normal\n"
                 "r  00  11:?:- ;r  01  11:?:0 ;r  10  11:?:1 ;r  11  11:0:1 ;"
                 "r  11  11:1:0 ;f  ??  ??:?:- ;b  *?  ??:?:- ;"
                 " // j and k\n"
                 "b  ?*  ??:?:- ;endtable endprimitive\n",
        .expected = "primitive mixed(o, clk, j, k, preset,\n"
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
        .input = " task  S ; "
                 "`ppgJH3JoxhwyTmZ2dgPiuMQzpRAWiSs("
                 "{xYtxuh6.FIMcVPEWfhtoI2FSe, xYtxuh6.ZVL5XASVGLYz32} == "
                 "SqRgavM[15:2];\n"
                 "JgQLBG == 4'h0;, \"foo\" )\n"
                 "endtask\n",
        .expected =
            "task S;\n"
            "  `ppgJH3JoxhwyTmZ2dgPiuMQzpRAWiSs(\n"
            "      {xYtxuh6.FIMcVPEWfhtoI2FSe, xYtxuh6.ZVL5XASVGLYz32} == "
            "SqRgavM[15:2];\n"
            "JgQLBG == 4'h0;,\n"
            "      \"foo\")\n"
            "endtask\n",
    },

    // between identifier and '(', no args

    {.input = "// verilog_syntax: parse-as-module-body",
     .expected = "// verilog_syntax: parse-as-module-body\n"},
    {.input = "// verilog_syntax: parse-as-statements",
     .expected = "// verilog_syntax: parse-as-statements\n"},
    {.input = "// verilog_syntax: parse-as-class-body",
     .expected = "// verilog_syntax: parse-as-class-body\n"},
    {.input = "// verilog_syntax: parse-as-package-body",
     .expected = "// verilog_syntax: parse-as-package-body\n"},
    {.input = "// verilog_syntax: parse-as-library-map",
     .expected = "// verilog_syntax: parse-as-library-map\n"},
    {.input = "// verilog_syntax: does-not-exist-mode",
     .expected = "// verilog_syntax: does-not-exist-mode\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "// comment",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "// comment\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c */ ();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */ ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */ /* c2 */ ();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  /* c2 */ ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */ /* c2 */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  /* c2 */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */ // c2\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  // c2\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c */ ();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c */ ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ /* c2 */ ();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  /* c2 */ ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ /* c2 */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  /* c2 */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "// c\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    // c\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ // c2\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  // c2\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */\n"
              "/* c2 */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */\n"
                 "    /* c2 */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "// c1\n"
              "// c2\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    // c1\n"
                 "    // c2\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */ ();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */ ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "/* c2 */ ();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    /* c2 */ ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */\n"
              "/* c3 */ ();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */ ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "/* c2 */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    /* c2 */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "// c2\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    // c2\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "// c2\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    // c2\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */\n"
                 "    ();\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "// c2\n"
              "// c3\n"
              "();\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    // c2\n"
                 "    // c3\n"
                 "    ();\n"},

    // between identifier and '(', with arg

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c */ (arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */ (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */ /* c2 */ (arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  /* c2 */ (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */ /* c2 */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  /* c2 */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */ // c2\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  // c2\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c */ (arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c */ (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ /* c2 */ (arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  /* c2 */ (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ /* c2 */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  /* c2 */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "// c\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    // c\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ // c2\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  // c2\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */\n"
              "/* c2 */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */\n"
                 "    /* c2 */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "// c1\n"
              "// c2\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    // c1\n"
                 "    // c2\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */ (arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */ (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "/* c2 */ (arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    /* c2 */ (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */\n"
              "/* c3 */ (arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */ (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "/* c2 */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    /* c2 */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "// c2\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    // c2\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "// c2\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    // c2\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz /* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */\n"
                 "    (arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz // c1\n"
              "// c2\n"
              "// c3\n"
              "(arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    // c2\n"
                 "    // c3\n"
                 "    (arg);\n"},

    // after '(', no args

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "/* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "/* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c1 */\n"
                 "    /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    // c1\n"
                 "    // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    /* c2 */\n"
                 "/* c3 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "    /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "    // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "// c2\n"
              "// c3\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "    // c2\n"
                 "    // c3\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "/* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "/* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */\n"
                 "    /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    // c1\n"
                 "    // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 "/* c3 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "// c2\n"
              "// c3\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    // c2\n"
                 "    // c3\n"
                 ");\n"},

    // after '(', with arg

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */ /* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */  /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */ /* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */  /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */ // c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */  // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */ /* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c1 */  /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */ /* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c1 */  /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "// c\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    // c\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */ // c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c1 */  // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "/* c1 */\n"
              "/* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    /* c1 */\n"
                 "    /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(\n"
              "// c1\n"
              "// c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(\n"
                 "    // c1\n"
                 "    // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "/* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "    /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "/* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "    /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "// c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "// c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "    // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(// c1\n"
              "// c2\n"
              "// c3\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(  // c1\n"
                 "    // c2\n"
                 "    // c3\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ /* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ /* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ // c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ /* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ /* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "// c\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    // c\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ // c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */\n"
              "/* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */\n"
                 "    /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "// c1\n"
              "// c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    // c1\n"
                 "    // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "/* c2 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    /* c2 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */ arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "/* c2 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    /* c2 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "// c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "// c2\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    // c2\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */\n"
                 "    arg);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "// c2\n"
              "// c3\n"
              "arg);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    // c2\n"
                 "    // c3\n"
                 "    arg);\n"},

    // after single arg

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "/* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "/* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "           /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "           /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "           // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "           /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg\n"
              "// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg\n"
                 "           // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg// c1\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  // c1\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */\n"
                 "           /* c2 */\n"
                 "/* c3 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg// c1\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  // c1\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg// c1\n"
              "// c2\n"
              "// c3\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "/* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "/* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "           /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "           /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "           // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "           /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg\n"
              "// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg\n"
                 "           // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg// c1\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  // c1\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */\n"
                 "           /* c2 */\n"
                 "/* c3 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg// c1\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  // c1\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg// c1\n"
              "// c2\n"
              "// c3\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 ");\n"},

    // before colon

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */ /* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */  /* c2 */,\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */ /* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */  /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1// c\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  // c\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */ // c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */  // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "/* c */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           /* c */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "/* c1 */ /* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           /* c1 */  /* c2 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "/* c */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           /* c */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "/* c1 */ /* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           /* c1 */  /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "// c\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           // c\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "/* c1 */ // c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           /* c1 */  // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1\n"
              "// c1\n"
              "// c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1\n"
                 "           // c1\n"
                 "           // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */\n"
              "/* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */\n"
                 "           /* c2 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1// c1\n"
              "/* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  // c1\n"
                 "           /* c2 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */\n"
              "/* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */\n"
                 "           /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1// c1\n"
              "/* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  // c1\n"
                 "           /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */\n"
              "// c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */\n"
                 "           // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1// c1\n"
              "// c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  // c1\n"
                 "           // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1// c1\n"
              "// c2\n"
              "// c3\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */ /* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */  /* c2 */,\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */ /* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */  /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1// c\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  // c\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */ // c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */  // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "/* c */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           /* c */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "/* c1 */ /* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           /* c1 */  /* c2 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "/* c */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           /* c */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "/* c1 */ /* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           /* c1 */  /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "// c\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           // c\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "/* c1 */ // c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           /* c1 */  // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1\n"
              "// c1\n"
              "// c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1\n"
                 "           // c1\n"
                 "           // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */\n"
              "/* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */\n"
                 "           /* c2 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1// c1\n"
              "/* c2 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  // c1\n"
                 "           /* c2 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */, arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */, arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */\n"
              "/* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */\n"
                 "           /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1// c1\n"
              "/* c2 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  // c1\n"
                 "           /* c2 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */\n"
              "// c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */\n"
                 "           // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1// c1\n"
              "// c2\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  // c1\n"
                 "           // c2\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 "           , arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1// c1\n"
              "// c2\n"
              "// c3\n"
              ", arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 "           , arg2);\n"},

    // after colon

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */ /* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */  /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */ /* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */  /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,// c\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  // c\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */ // c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */  // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "/* c */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           /* c */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "/* c1 */ /* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           /* c1 */  /* c2 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "/* c */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           /* c */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "/* c1 */ /* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           /* c1 */  /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "// c\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           // c\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "/* c1 */ // c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           /* c1 */  // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "/* c1 */\n"
              "/* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,\n"
              "// c1\n"
              "// c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           // c1\n"
                 "           // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */\n"
              "/* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */\n"
                 "           /* c2 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,// c1\n"
              "/* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  // c1\n"
                 "           /* c2 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */\n"
              "/* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,// c1\n"
              "/* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  // c1\n"
                 "           /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */\n"
              "// c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */\n"
                 "           // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,// c1\n"
              "// c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  // c1\n"
                 "           // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1,// c1\n"
              "// c2\n"
              "// c3\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */ /* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */  /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */ /* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */  /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,// c\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  // c\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */ // c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */  // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "/* c */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           /* c */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "/* c1 */ /* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           /* c1 */  /* c2 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "/* c */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           /* c */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "/* c1 */ /* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           /* c1 */  /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "// c\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           // c\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "/* c1 */ // c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           /* c1 */  // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "/* c1 */\n"
              "/* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,\n"
              "// c1\n"
              "// c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           // c1\n"
                 "           // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */\n"
              "/* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */\n"
                 "           /* c2 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,// c1\n"
              "/* c2 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  // c1\n"
                 "           /* c2 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */ arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */\n"
              "/* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,// c1\n"
              "/* c2 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  // c1\n"
                 "           /* c2 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */\n"
              "// c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */\n"
                 "           // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,// c1\n"
              "// c2\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  // c1\n"
                 "           // c2\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 "           arg2);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1,// c1\n"
              "// c2\n"
              "// c3\n"
              "arg2);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 "           arg2);\n"},

    // after last arg

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           arg2,  /* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1,\n"
                 "           arg2,  /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "/* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "/* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "           /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "           /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "           // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "           /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,\n"
              "// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,\n"
                 "           // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c1 */\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,// c1\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  // c1\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c1 */\n"
                 "           /* c2 */\n"
                 "/* c3 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,// c1\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  // c1\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c1 */\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg1, arg2,// c1\n"
              "// c2\n"
              "// c3\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg1, arg2,  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           arg2,  /* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1,\n"
                 "           arg2,  /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "/* c */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "/* c */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "/* c1 */ /* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "/* c1 */  /* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "/* c */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "           /* c */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "/* c1 */ /* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "           /* c1 */  /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "// c\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "           // c\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "/* c1 */ // c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "           /* c1 */  // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "           /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,\n"
              "// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,\n"
                 "           // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,// c1\n"
              "/* c2 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  // c1\n"
                 "/* c2 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */);\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
                 "           /* c2 */\n"
                 "/* c3 */);\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,// c1\n"
              "/* c2 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  // c1\n"
                 "           /* c2 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,// c1\n"
              "// c2\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  // c1\n"
                 "           // c2\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  /* c1 */\n"
                 "           /* c2 */\n"
                 "           /* c3 */\n"
                 ");\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg1, arg2,// c1\n"
              "// c2\n"
              "// c3\n"
              ");\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg1, arg2,  // c1\n"
                 "           // c2\n"
                 "           // c3\n"
                 ");\n"},

    // after ')', no args

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  // c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "/* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "/* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "/* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "/* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "// c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "/* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "/* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()\n"
              "// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()\n"
                 "// c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()// c1\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  // c1\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()// c1\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  // c1\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  // c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz()// c1\n"
              "// c2\n"
              "// c3\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz()  // c1\n"
                 "// c2\n"
                 "// c3\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  // c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "/* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "/* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "/* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "/* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "// c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "/* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "/* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()\n"
              "// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()\n"
                 "// c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()// c1\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  // c1\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()// c1\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  // c1\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  // c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ()// c1\n"
              "// c2\n"
              "// c3\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ()  // c1\n"
                 "// c2\n"
                 "// c3\n"
                 ";\n"},

    // after ')', with arg

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  // c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)\n"
                 "/* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)\n"
                 "/* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)\n"
                 "/* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)\n"
                 "/* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  // c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)\n"
                 "/* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)\n"
                 "/* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)\n"
              "// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)\n"
                 "// c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)// c1\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  // c1\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)// c1\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  // c1\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  // c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg)// c1\n"
              "// c2\n"
              "// c3\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg)  // c1\n"
                 "// c2\n"
                 "// c3\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  // c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "/* c */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "/* c */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "/* c1 */ /* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "/* c1 */  /* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "/* c */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "/* c */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "/* c1 */ /* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "/* c1 */  /* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "// c\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "// c\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "/* c1 */ // c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "/* c1 */  // c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "/* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)\n"
              "// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)\n"
                 "// c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)// c1\n"
              "/* c2 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  // c1\n"
                 "/* c2 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */;\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */;\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)// c1\n"
              "/* c2 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  // c1\n"
                 "/* c2 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)// c1\n"
              "// c2\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  // c1\n"
                 "// c2\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"
                 ";\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg)// c1\n"
              "// c2\n"
              "// c3\n"
              ";\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg)  // c1\n"
                 "// c2\n"
                 "// c3\n"
                 ";\n"},

    // after ';', no args

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "// c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "/* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "/* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();\n"
              "// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();\n"
                 "// c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();// c1 c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  // c1 c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  // c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz();// c1\n"
              "// c2\n"
              "// c3\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz();  // c1\n"
                 "// c2\n"
                 "// c3\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "// c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "/* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "/* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();\n"
              "// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();\n"
                 "// c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  // c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ();// c1\n"
              "// c2\n"
              "// c3\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ();  // c1\n"
                 "// c2\n"
                 "// c3\n"},

    // after ';', with arg

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "// c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "/* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "/* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);\n"
              "// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);\n"
                 "// c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);// c1 c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  // c1 c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  // c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz(arg);// c1\n"
              "// c2\n"
              "// c3\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz(arg);  // c1\n"
                 "// c2\n"
                 "// c3\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "// c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "/* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "/* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);\n"
              "// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);\n"
                 "// c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  // c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(arg);// c1\n"
              "// c2\n"
              "// c3\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(arg);  // c1\n"
                 "// c2\n"
                 "// c3\n"},

    // everywhere, no args

    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c */(/* c */)/* c */;/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */\n"
                 "    (  /* c */)  /* c */;  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */ /* c2 */(/* c1 */ /* c2 */)/* c1 */ /* c2 "
              "*/;/* c1 */ "
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  /* c2 */\n"
                 "    (  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 */  "
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c */\n"
              "(/* c */\n"
              ")/* c */\n"
              ";/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */\n"
                 "    (  /* c */\n"
                 "    )  /* c */\n"
                 ";  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */ /* c2 */\n"
              "(/* c1 */ /* c2 */\n"
              ")/* c1 */ /* c2 */\n"
              ";/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  /* c2 */\n"
                 "    (  /* c1 */  /* c2 */\n"
                 "    )  /* c1 */  /* c2 */\n"
                 ";  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz// c\n"
              "(// c\n"
              ")// c\n"
              ";// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c\n"
                 "    (  // c\n"
                 "    )  // c\n"
                 ";  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */ // c2\n"
              "(/* c1 */ // c2\n"
              ")/* c1 */ // c2\n"
              ";/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  // c2\n"
                 "    (  /* c1 */  // c2\n"
                 "    )  /* c1 */  // c2\n"
                 ";  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c */(\n"
              "/* c */)\n"
              "/* c */;\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c */ (\n"
                 "    /* c */)\n"
                 "/* c */;\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ /* c2 */(\n"
              "/* c1 */ /* c2 */)\n"
              "/* c1 */ /* c2 */;\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  /* c2 */ (\n"
                 "    /* c1 */  /* c2 */)\n"
                 "/* c1 */  /* c2 */;\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c */\n"
              "(\n"
              "/* c */\n"
              ")\n"
              "/* c */\n"
              ";\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c */\n"
                 "    (\n"
                 "        /* c */\n"
                 "    )\n"
                 "/* c */\n"
                 ";\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ /* c2 */\n"
              "(\n"
              "/* c1 */ /* c2 */\n"
              ")\n"
              "/* c1 */ /* c2 */\n"
              ";\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  /* c2 */\n"
                 "    (\n"
                 "        /* c1 */  /* c2 */\n"
                 "    )\n"
                 "/* c1 */  /* c2 */\n"
                 ";\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "// c\n"
              "(\n"
              "// c\n"
              ")\n"
              "// c\n"
              ";\n"
              "// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    // c\n"
                 "    (\n"
                 "        // c\n"
                 "    )\n"
                 "// c\n"
                 ";\n"
                 "// c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ // c2\n"
              "(\n"
              "/* c1 */ // c2\n"
              ")\n"
              "/* c1 */ // c2\n"
              ";\n"
              "/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  // c2\n"
                 "    (\n"
                 "        /* c1 */  // c2\n"
                 "    )\n"
                 "/* c1 */  // c2\n"
                 ";\n"
                 "/* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */\n"
              "/* c2 */(/* c1 */\n"
              "/* c2 */)/* c1 */\n"
              "/* c2 */;/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */ (  /* c1 */\n"
                 "    /* c2 */)  /* c1 */\n"
                 "/* c2 */;  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz// c1\n"
              "/* c2 */(// c1\n"
              "/* c2 */)// c1\n"
              "/* c2 */;// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    /* c2 */ (  // c1\n"
                 "    /* c2 */)  // c1\n"
                 "/* c2 */;  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */)/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */;/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */ (  /* c1 */\n"
                 "        /* c2 */\n"
                 "    /* c3 */)  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */;  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */\n"
              "/* c2 */\n"
              "(/* c1 */\n"
              "/* c2 */\n"
              ")/* c1 */\n"
              "/* c2 */\n"
              ";/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */\n"
                 "    (  /* c1 */\n"
                 "        /* c2 */\n"
                 "    )  /* c1 */\n"
                 "/* c2 */\n"
                 ";  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz// c1\n"
              "/* c2 */\n"
              "(// c1\n"
              "/* c2 */\n"
              ")// c1\n"
              "/* c2 */\n"
              ";// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    /* c2 */\n"
                 "    (  // c1\n"
                 "        /* c2 */\n"
                 "    )  // c1\n"
                 "/* c2 */\n"
                 ";  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */\n"
              "// c2\n"
              "(/* c1 */\n"
              "// c2\n"
              ")/* c1 */\n"
              "// c2\n"
              ";/* c1 */\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    // c2\n"
                 "    (  /* c1 */\n"
                 "        // c2\n"
                 "    )  /* c1 */\n"
                 "// c2\n"
                 ";  /* c1 */\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz// c1\n"
              "// c2\n"
              "(// c1\n"
              "// c2\n"
              ")// c1\n"
              "// c2\n"
              ";// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    // c2\n"
                 "    (  // c1\n"
                 "       // c2\n"
                 "    )  // c1\n"
                 "       // c2\n"
                 ";  // c1\n"
                 "   // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c */)/* c */;/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */)  /* c */;  /* c */\n"},
    {.input =
         "// verilog_syntax: parse-as-module-body\n"
         "`FOOBARBAZ(/* c1 */ /* c2 */)/* c1 */ /* c2 */;/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 "
                 "*/  /* c2 "
                 "*/\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c */\n"
              ")/* c */\n"
              ";/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */\n"
                 ")  /* c */\n"
                 ";  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ /* c2 */\n"
              ")/* c1 */ /* c2 */\n"
              ";/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
                 ")  /* c1 */  /* c2 */\n"
                 ";  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c\n"
              ")// c\n"
              ";// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c\n"
                 ")  // c\n"
                 ";  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ // c2\n"
              ")/* c1 */ // c2\n"
              ";/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  // c2\n"
                 ")  /* c1 */  // c2\n"
                 ";  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c */)\n"
              "/* c */;\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "/* c */)\n"
                 "/* c */;\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ /* c2 */)\n"
              "/* c1 */ /* c2 */;\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "/* c1 */  /* c2 */)\n"
                 "/* c1 */  /* c2 */;\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c */\n"
              ")\n"
              "/* c */\n"
              ";\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c */\n"
                 ")\n"
                 "/* c */\n"
                 ";\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ /* c2 */\n"
              ")\n"
              "/* c1 */ /* c2 */\n"
              ";\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  /* c2 */\n"
                 ")\n"
                 "/* c1 */  /* c2 */\n"
                 ";\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "// c\n"
              ")\n"
              "// c\n"
              ";\n"
              "// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    // c\n"
                 ")\n"
                 "// c\n"
                 ";\n"
                 "// c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ // c2\n"
              ")\n"
              "/* c1 */ // c2\n"
              ";\n"
              "/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  // c2\n"
                 ")\n"
                 "/* c1 */  // c2\n"
                 ";\n"
                 "/* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ")\n"
              "/* c1 */\n"
              "/* c2 */\n"
              ";\n"
              "/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */\n"
                 "    /* c2 */\n"
                 ")\n"
                 "/* c1 */\n"
                 "/* c2 */\n"
                 ";\n"
                 "/* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "// c1\n"
              "// c2\n"
              ")\n"
              "// c1\n"
              "// c2\n"
              ";\n"
              "// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    // c1\n"
                 "    // c2\n"
                 ")\n"
                 "// c1\n"
                 "// c2\n"
                 ";\n"
                 "// c1\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */)/* c1 */\n"
              "/* c2 */;/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "/* c2 */)  /* c1 */\n"
                 "/* c2 */;  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "/* c2 */)// c1\n"
              "/* c2 */;// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "/* c2 */)  // c1\n"
                 "/* c2 */;  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */)/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */;/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 "/* c3 */)  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */;  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              ")/* c1 */\n"
              "/* c2 */\n"
              ";/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 ")  /* c1 */\n"
                 "/* c2 */\n"
                 ";  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "/* c2 */\n"
              ")// c1\n"
              "/* c2 */\n"
              ";// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    /* c2 */\n"
                 ")  // c1\n"
                 "/* c2 */\n"
                 ";  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "// c2\n"
              ")/* c1 */\n"
              "// c2\n"
              ";/* c1 */\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    // c2\n"
                 ")  /* c1 */\n"
                 "// c2\n"
                 ";  /* c1 */\n"
                 "// c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "// c2\n"
              ")// c1\n"
              "// c2\n"
              ";// c1\n"
              "// c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    // c2\n"
                 ")  // c1\n"
                 "   // c2\n"
                 ";  // c1\n"
                 "   // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ")/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n"
              ";/* c1 */\n"
              "/* c2 */\n"
              "/* c3 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */\n"
                 "    /* c3 */\n"
                 ")  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"
                 ";  /* c1 */\n"
                 "/* c2 */\n"
                 "/* c3 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "// c2\n"
              "// c3\n"
              ")// c1\n"
              "// c2\n"
              "// c3\n"
              ";// c1\n"
              "// c2\n"
              "// c3\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
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

    {.input =
         "// verilog_syntax: parse-as-module-body\n"
         "$foobarbaz/* c */(/* c */arg1/* c */,/* c */arg2/* c */)/* c */;/* c "
         "*/\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */ (  /* c */\n"
                 "    arg1  /* c */,  /* c */\n"
                 "    arg2  /* c */)  /* c */;  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */ /* c2 */(/* c1 */ /* c2 */arg1/* c1 */ /* c2 "
              "*/,/* c1 "
              "*/ /* c2 */arg2/* c1 */ /* c2 */)/* c1 */ /* c2 */;/* c1 */ /* "
              "c2 */\n",
     .expected =
         "// verilog_syntax: parse-as-module-body\n"
         "$foobarbaz  /* c1 */  /* c2 */ (  /* c1 */  /* c2 */\n"
         "    arg1  /* c1 */  /* c2 */,  /* c1 */  /* c2 */\n"
         "    arg2  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 */  /* c2 "
         "*/\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c */\n"
              "(/* c */\n"
              "arg1/* c */\n"
              ",/* c */\n"
              "arg2/* c */\n"
              ")/* c */\n"
              ";/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c */\n"
                 "    (  /* c */\n"
                 "        arg1  /* c */\n"
                 "        ,  /* c */\n"
                 "        arg2  /* c */\n"
                 "    )  /* c */\n"
                 ";  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */ /* c2 */\n"
              "(/* c1 */ /* c2 */\n"
              "arg1/* c1 */ /* c2 */\n"
              ",/* c1 */ /* c2 */\n"
              "arg2/* c1 */ /* c2 */\n"
              ")/* c1 */ /* c2 */\n"
              ";/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  /* c2 */\n"
                 "    (  /* c1 */  /* c2 */\n"
                 "        arg1  /* c1 */  /* c2 */\n"
                 "        ,  /* c1 */  /* c2 */\n"
                 "        arg2  /* c1 */  /* c2 */\n"
                 "    )  /* c1 */  /* c2 */\n"
                 ";  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz// c\n"
              "(// c\n"
              "arg1// c\n"
              ",// c\n"
              "arg2// c\n"
              ")// c\n"
              ";// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c\n"
                 "    (  // c\n"
                 "        arg1  // c\n"
                 "        ,  // c\n"
                 "        arg2  // c\n"
                 "    )  // c\n"
                 ";  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */ // c2\n"
              "(/* c1 */ // c2\n"
              "arg1/* c1 */ // c2\n"
              ",/* c1 */ // c2\n"
              "arg2/* c1 */ // c2\n"
              ")/* c1 */ // c2\n"
              ";/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */  // c2\n"
                 "    (  /* c1 */  // c2\n"
                 "        arg1  /* c1 */  // c2\n"
                 "        ,  /* c1 */  // c2\n"
                 "        arg2  /* c1 */  // c2\n"
                 "    )  /* c1 */  // c2\n"
                 ";  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c */(\n"
              "/* c */arg1\n"
              "/* c */,\n"
              "/* c */arg2\n"
              "/* c */)\n"
              "/* c */;\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c */ (\n"
                 "        /* c */ arg1\n"
                 "        /* c */,\n"
                 "        /* c */ arg2\n"
                 "    /* c */)\n"
                 "/* c */;\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz\n"
              "/* c1 */ /* c2 */(\n"
              "/* c1 */ /* c2 */arg1\n"
              "/* c1 */ /* c2 */,\n"
              "/* c1 */ /* c2 */arg2\n"
              "/* c1 */ /* c2 */)\n"
              "/* c1 */ /* c2 */;\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz\n"
                 "    /* c1 */  /* c2 */ (\n"
                 "        /* c1 */  /* c2 */ arg1\n"
                 "        /* c1 */  /* c2 */,\n"
                 "        /* c1 */  /* c2 */ arg2\n"
                 "    /* c1 */  /* c2 */)\n"
                 "/* c1 */  /* c2 */;\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz/* c1 */\n"
              "/* c2 */(/* c1 */\n"
              "/* c2 */arg1/* c1 */\n"
              "/* c2 */,/* c1 */\n"
              "/* c2 */arg2/* c1 */\n"
              "/* c2 */)/* c1 */\n"
              "/* c2 */;/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  /* c1 */\n"
                 "    /* c2 */ (  /* c1 */\n"
                 "        /* c2 */ arg1  /* c1 */\n"
                 "        /* c2 */,  /* c1 */\n"
                 "        /* c2 */ arg2  /* c1 */\n"
                 "    /* c2 */)  /* c1 */\n"
                 "/* c2 */;  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "$foobarbaz// c1\n"
              "/* c2 */(// c1\n"
              "/* c2 */arg1// c1\n"
              "/* c2 */,// c1\n"
              "/* c2 */arg2// c1\n"
              "/* c2 */)// c1\n"
              "/* c2 */;// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "$foobarbaz  // c1\n"
                 "    /* c2 */ (  // c1\n"
                 "        /* c2 */ arg1  // c1\n"
                 "        /* c2 */,  // c1\n"
                 "        /* c2 */ arg2  // c1\n"
                 "    /* c2 */)  // c1\n"
                 "/* c2 */;  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input =
         "// verilog_syntax: parse-as-module-body\n"
         "`FOOBARBAZ(/* c */arg1/* c */,/* c */arg2/* c */)/* c */;/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */\n"
                 "    arg1  /* c */,  /* c */\n"
                 "    arg2  /* c */)  /* c */;  /* c */\n"},
    {.input =
         "// verilog_syntax: parse-as-module-body\n"
         "`FOOBARBAZ(/* c1 */ /* c2 */arg1/* c1 */ /* c2 */,/* c1 */ /* c2 "
         "*/arg2/* c1 */ /* c2 */)/* c1 */ /* c2 */;/* c1 */ /* c2 */\n",
     .expected =
         "// verilog_syntax: parse-as-module-body\n"
         "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
         "    arg1  /* c1 */  /* c2 */,  /* c1 */  /* c2 */\n"
         "    arg2  /* c1 */  /* c2 */)  /* c1 */  /* c2 */;  /* c1 */  /* c2 "
         "*/\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c */\n"
              "arg1/* c */\n"
              ",/* c */\n"
              "arg2/* c */\n"
              ")/* c */\n"
              ";/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c */\n"
                 "    arg1  /* c */\n"
                 "    ,  /* c */\n"
                 "    arg2  /* c */\n"
                 ")  /* c */\n"
                 ";  /* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ /* c2 */\n"
              "arg1/* c1 */ /* c2 */\n"
              ",/* c1 */ /* c2 */\n"
              "arg2/* c1 */ /* c2 */\n"
              ")/* c1 */ /* c2 */\n"
              ";/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  /* c2 */\n"
                 "    arg1  /* c1 */  /* c2 */\n"
                 "    ,  /* c1 */  /* c2 */\n"
                 "    arg2  /* c1 */  /* c2 */\n"
                 ")  /* c1 */  /* c2 */\n"
                 ";  /* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c\n"
              "arg1// c\n"
              ",// c\n"
              "arg2// c\n"
              ")// c\n"
              ";// c\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c\n"
                 "    arg1  // c\n"
                 "    ,  // c\n"
                 "    arg2  // c\n"
                 ")  // c\n"
                 ";  // c\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */ // c2\n"
              "arg1/* c1 */ // c2\n"
              ",/* c1 */ // c2\n"
              "arg2/* c1 */ // c2\n"
              ")/* c1 */ // c2\n"
              ";/* c1 */ // c2\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */  // c2\n"
                 "    arg1  /* c1 */  // c2\n"
                 "    ,  /* c1 */  // c2\n"
                 "    arg2  /* c1 */  // c2\n"
                 ")  /* c1 */  // c2\n"
                 ";  /* c1 */  // c2\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c */arg1\n"
              "/* c */,\n"
              "/* c */arg2\n"
              "/* c */)\n"
              "/* c */;\n"
              "/* c */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c */ arg1\n"
                 "    /* c */,\n"
                 "    /* c */ arg2\n"
                 "/* c */)\n"
                 "/* c */;\n"
                 "/* c */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(\n"
              "/* c1 */ /* c2 */arg1\n"
              "/* c1 */ /* c2 */,\n"
              "/* c1 */ /* c2 */arg2\n"
              "/* c1 */ /* c2 */)\n"
              "/* c1 */ /* c2 */;\n"
              "/* c1 */ /* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(\n"
                 "    /* c1 */  /* c2 */ arg1\n"
                 "    /* c1 */  /* c2 */,\n"
                 "    /* c1 */  /* c2 */ arg2\n"
                 "/* c1 */  /* c2 */)\n"
                 "/* c1 */  /* c2 */;\n"
                 "/* c1 */  /* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(/* c1 */\n"
              "/* c2 */arg1/* c1 */\n"
              "/* c2 */,/* c1 */\n"
              "/* c2 */arg2/* c1 */\n"
              "/* c2 */)/* c1 */\n"
              "/* c2 */;/* c1 */\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  /* c1 */\n"
                 "    /* c2 */ arg1  /* c1 */\n"
                 "    /* c2 */,  /* c1 */\n"
                 "    /* c2 */ arg2  /* c1 */\n"
                 "/* c2 */)  /* c1 */\n"
                 "/* c2 */;  /* c1 */\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
              "`FOOBARBAZ(// c1\n"
              "/* c2 */arg1// c1\n"
              "/* c2 */,// c1\n"
              "/* c2 */arg2// c1\n"
              "/* c2 */)// c1\n"
              "/* c2 */;// c1\n"
              "/* c2 */\n",
     .expected = "// verilog_syntax: parse-as-module-body\n"
                 "`FOOBARBAZ(  // c1\n"
                 "    /* c2 */ arg1  // c1\n"
                 "    /* c2 */,  // c1\n"
                 "    /* c2 */ arg2  // c1\n"
                 "/* c2 */)  // c1\n"
                 "/* c2 */;  // c1\n"
                 "/* c2 */\n"},
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "// verilog_syntax: parse-as-module-body\n"
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
     .expected = "// verilog_syntax: parse-as-module-body\n"
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
    {.input = "module indent();\n"
              "   reg     a;\n"
              "   reg [32:0] b;\n"
              "   wire    c;\n"
              "   wire    d = e ? kFoo : kBar;\n"
              "endmodule\n",
     .expected = "module indent ();\n"
                 "  reg         a;\n"
                 "  reg  [32:0] b;\n"
                 "  wire        c;\n"
                 "  wire        d = e ? kFoo : kBar;\n"
                 "endmodule\n"},

    {.input = "class C; T1 b; logic [$] a; T1 [$] c; endclass\n",
     .expected = "class C;\n"
                 "  T1        b;\n"
                 "  logic [$] a;\n"
                 "  T1    [$] c;\n"
                 "endclass\n"},
    {.input = "class C;\n"
              "  T1 b; //test\n"
              "  logic [$] a; //test\n"
              "  T1    [$] c; //test\n"
              "endclass\n",
     .expected = "class C;\n"
                 "  T1        b;  //test\n"
                 "  logic [$] a;  //test\n"
                 "  T1    [$] c;  //test\n"
                 "endclass\n"},
    {.input = "class C;\n"
              "  T1\n"
              "  b;\n"
              "  logic\n"
              "  [$]\n"
              "  a;\n"
              "  T1\n"
              "  [$]\n"
              "  c;\n"
              "endclass\n",
     .expected = "class C;\n"
                 "  T1        b;\n"
                 "  logic [$] a;\n"
                 "  T1    [$] c;\n"
                 "endclass\n"},
    {.input = "class C;\n"
              "  logic/*t*/ [0 : 1] /*t*/\n"
              "  a;/*t*/\n"
              "  T1/*t*/[0 : 1]/*t*/\n"
              "  c;/*t*/\n"
              "endclass\n",
     .expected = "class C;\n"
                 "  logic/*t*/ [0 : 1]  /*t*/ a;  /*t*/\n"
                 "  T1/*t*/    [0 : 1]  /*t*/ c;  /*t*/\n"
                 "endclass\n"},
    {.input = "always @(*/*t*/) begin\n"
              "end\n",
     .expected = "always @(*  /*t*/) begin\n"
                 "end\n"},
    {.input = "always @(/*t*/*) begin\n"
              "end\n",
     .expected = "always @(  /*t*/ *) begin\n"
                 "end\n"},
    {.input = "always @(/*t*/*/*t*/) begin\n"
              "end\n",
     .expected = "always @(  /*t*/ *  /*t*/) begin\n"
                 "end\n"},
    {.input = "always @(*) begin\n"
              "end\n",
     .expected = "always @(*) begin\n"
                 "end\n"},
    {.input = "always @(* ) begin\n"
              "end\n",
     .expected = "always @(*) begin\n"
                 "end\n"},
    {.input = "always @( *) begin\n"
              "end\n",
     .expected = "always @(*) begin\n"
                 "end\n"},
    {.input = "always @( * ) begin\n"
              "end\n",
     .expected = "always @(*) begin\n"
                 "end\n"},
    {.input = "always @(  /*t*/  *   /*t*/    ) begin\n"
              "end\n",
     .expected = "always @(  /*t*/ *  /*t*/) begin\n"
                 "end\n"},
    {
        // Don't touch verilog_format:off region #1538
        .input = R"(
module testcode;
  // verilog_format: off
  assign a = b
           & c;
  // verilog_format: on
      assign e = d;
endmodule
)",
        .expected = R"(
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
