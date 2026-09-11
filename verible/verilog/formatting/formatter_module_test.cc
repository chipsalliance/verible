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

static constexpr FormatterTestCase kModuleFormatterTestCases[] = {
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
    {"module t;\n"
     "    input bit i_bit;\n"
     "input   byte  i_byte ;\n"
     "input chandle i_chandle;\n"
     "input event i_event;\n"
     "input int i_int;\n"
     "input integer i_inte;\n"
     "input longint i_longint;\n"
     "input real i_real;\n"
     "input realtime i_realtime;\n"
     "input shortint i_shortint;\n"
     "input shortreal i_shortreal;\n"
     "input string i_string;\n"
     "input time i_time;\n"
     "     output bit o_bit;\n"
     "output    byte   o_byte  ;  \n"
     "output chandle o_chandle;\n"
     "output event o_event;\n"
     "output int o_int;\n"
     "output integer o_inte;\n"
     "output longint o_longint;\n"
     "output real o_real;\n"
     "output realtime o_realtime;\n"
     "output shortint o_shortint;\n"
     "output shortreal o_shortreal;\n"
     "output string o_string;\n"
     "output time o_time;\n"
     "endmodule\n",
     "module t;\n"
     "  input bit i_bit;\n"
     "  input byte i_byte;\n"
     "  input chandle i_chandle;\n"
     "  input event i_event;\n"
     "  input int i_int;\n"
     "  input integer i_inte;\n"
     "  input longint i_longint;\n"
     "  input real i_real;\n"
     "  input realtime i_realtime;\n"
     "  input shortint i_shortint;\n"
     "  input shortreal i_shortreal;\n"
     "  input string i_string;\n"
     "  input time i_time;\n"
     "  output bit o_bit;\n"
     "  output byte o_byte;\n"
     "  output chandle o_chandle;\n"
     "  output event o_event;\n"
     "  output int o_int;\n"
     "  output integer o_inte;\n"
     "  output longint o_longint;\n"
     "  output real o_real;\n"
     "  output realtime o_realtime;\n"
     "  output shortint o_shortint;\n"
     "  output shortreal o_shortreal;\n"
     "  output string o_string;\n"
     "  output time o_time;\n"
     "endmodule\n"},
    {"module t (\n"
     "    input bit i_bit,\n"
     "input     byte   i_byte  ,\n"
     "input chandle i_chandle,\n"
     "input event i_event,\n"
     "input int i_int,\n"
     "input integer i_inte,\n"
     "input longint i_longint,\n"
     "input real i_real,\n"
     "input realtime i_realtime,\n"
     "input shortint i_shortint,\n"
     "input shortreal i_shortreal,\n"
     "input string i_string,\n"
     "input time i_time,\n"
     "    output bit o_bit,\n"
     "output     byte   o_byte  ,  \n"
     "output chandle o_chandle,\n"
     "output event o_event,\n"
     "output int o_int,\n"
     "output integer o_inte,\n"
     "output longint o_longint,\n"
     "output real o_real,\n"
     "output realtime o_realtime,\n"
     "output shortint o_shortint,\n"
     "output shortreal o_shortreal,\n"
     "output string o_string,\n"
     "output time o_time);\n"
     "endmodule\n",
     "module t (\n"
     "    input  bit       i_bit,\n"
     "    input  byte      i_byte,\n"
     "    input  chandle   i_chandle,\n"
     "    input  event     i_event,\n"
     "    input  int       i_int,\n"
     "    input  integer   i_inte,\n"
     "    input  longint   i_longint,\n"
     "    input  real      i_real,\n"
     "    input  realtime  i_realtime,\n"
     "    input  shortint  i_shortint,\n"
     "    input  shortreal i_shortreal,\n"
     "    input  string    i_string,\n"
     "    input  time      i_time,\n"
     "    output bit       o_bit,\n"
     "    output byte      o_byte,\n"
     "    output chandle   o_chandle,\n"
     "    output event     o_event,\n"
     "    output int       o_int,\n"
     "    output integer   o_inte,\n"
     "    output longint   o_longint,\n"
     "    output real      o_real,\n"
     "    output realtime  o_realtime,\n"
     "    output shortint  o_shortint,\n"
     "    output shortreal o_shortreal,\n"
     "    output string    o_string,\n"
     "    output time      o_time\n"
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
    {"(* foo='{\"bar_.*\"} *)\n"  // funny attribute
     "module mattr;\n"
     "(* attr1=\"value1\" *)\n"  // attribute ignored
     "ex_input_pins_t ex_input_pins;\n"
     "(* attr2=\"value2\" *)\n"  // attribute ignored
     "ex_output_pins_t ex_output_pins;\n"
     "(* attr3=\"value3\" *)\n"  // attribute ignored
     "ex wrap_ex ( );\n"
     "endmodule\n",
     "(* foo='{\"bar_.*\"} *)\n"
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
    {"module foo ();\nif (1) begin\n$finish(\n"
     "      1    ); $finish    ();\n  end\nendmodule",
     "module foo ();\n"
     "  if (1) begin\n"
     "    $finish(1);\n"
     "    $finish();\n"
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
    {// Two consecutive EOL comments in kDPIImportItem
     "import \"DPI-C\" context function void foo(\n"
     "  input bit first,\n"
     "  // c3\n"
     "  // c3+\n"
     "  input bit second\n"
     ");\n",
     "import \"DPI-C\" context\n"
     "    function void foo(\n"
     "  input bit first,\n"
     "  // c3\n"
     "  // c3+\n"
     "  input bit second\n"
     ");\n"},
    {"export \"\" t;\n", "export \"\" t;\n"},
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
    {// module with system task call w or w/o parentheses
     "module m; initial begin #10 $display(\"foo\"); $display(\"bar\");"
     "end endmodule",
     "module m;\n"
     "  initial begin\n"
     "    #10 $display(\"foo\");\n"
     "    $display(\"bar\");\n"
     "  end\n"
     "endmodule\n"},
    {// module with system task call
     "module m; initial begin #10 $display; $display;"
     "end endmodule",
     "module m;\n"
     "  initial begin\n"
     "    #10 $display;\n"
     "    $display;\n"
     "  end\n"
     "endmodule\n"},

};

TEST(FormatterEndToEndTest, ModuleFormatterTestCases) {
  RunFormatterTestCases40(kModuleFormatterTestCases);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
