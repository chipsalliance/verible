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

#include "absl/log/log.h"
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
        .input = "  parameter  int   foo=0 ;",
        .expected = "parameter int foo = 0;\n",
    },
    {
        .input = "  parameter  int   foo=bar [ 0 ] ;",  // index expression
        .expected = "parameter int foo = bar[0];\n",
    },
    {
        .input =
            "  parameter  int   foo=bar [ a+b ] ;",  // binary inside index expr
        .expected =
            "parameter int foo = bar[a+b];\n",  // allowed to be 0 spaces
                                                // (preserved)
    },
    {
        .input = "  parameter  int   foo=bar [ a+ b ] ;",  // binary inside
                                                           // index expr
        .expected =
            "parameter int foo = bar[a+b];\n",  // allowed to be 0 spaces
                                                // (symmetrized)
    },
    {
        .input =
            "  parameter  int   foo=bar [ a +b ] ;",    // compact binary inside
        .expected = "parameter int foo = bar[a+b];\n",  // index expression
    },
    {
        .input =
            "  parameter  int   foo=bar [ a  +b ] ;",   // compact binary inside
        .expected = "parameter int foo = bar[a+b];\n",  // index expression
    },
    {
        // with line continuations
        .input = "  parameter  \\\nint   \\\nfoo=a+ \\\nb ;",
        .expected = "parameter\\\n    int\\\n    foo = a +\\\n    b;\n",
        // TODO(fangism): should text following a line continuation hang-indent?
    },
    // unary prefix expressions
    {
        .input = "  parameter  int   foo=- 1 ;",
        .expected = "parameter int foo = -1;\n",
    },
    {
        .input = "  parameter  int   foo=+ 7 ;",
        .expected = "parameter int foo = +7;\n",
    },
    {
        .input = "  parameter  int   foo=- J ;",
        .expected = "parameter int foo = -J;\n",
    },
    {
        .input = "  parameter  int   foo=- ( y ) ;",
        .expected = "parameter int foo = -(y);\n",
    },
    {
        .input = "  parameter  int   foo=- ( z*y ) ;",
        .expected = "parameter int foo = -(z * y);\n",
    },
    {
        .input = "  parameter  int   foo=-  z*- y  ;",
        .expected = "parameter int foo = -z * -y;\n",
    },
    {
        .input = "  parameter  int   foo=( - 2 ) ;",  //
        .expected = "parameter int foo = (-2);\n",
    },
    {
        .input = "  parameter  int   foo=$bar(-  z,- y ) ;",
        .expected = "parameter int foo = $bar(-z, -y);\n",
    },
    {.input = "  parameter int a=b&~(c<<d);",
     .expected = "parameter int a = b & ~(c << d);\n"},
    {.input = "  parameter int a=~~~~b;",
     .expected = "parameter int a = ~~~~b;\n"},
    {.input = "  parameter int a = ~ ~ ~ ~ b;",
     .expected = "parameter int a = ~~~~b;\n"},
    {.input = "  parameter int a   =   ~--b;",
     .expected = "parameter int a = ~--b;\n"},
    {.input = "  parameter int a   =   ~ --b;",
     .expected = "parameter int a = ~--b;\n"},
    {.input = "  parameter int a = ~ ++ b;",
     .expected = "parameter int a = ~++b;\n"},
    {.input = "  parameter int a=--b- --c;",
     .expected = "parameter int a = --b - --c;\n"},
    // ^~ and ~^ are bitwise nor, but ^ ~ isn't
    {.input = "  parameter int a=b^~(c<<d);",
     .expected = "parameter int a = b ^~ (c << d);\n"},
    {.input = "  parameter int a=b~^(c<<d);",
     .expected = "parameter int a = b ~^ (c << d);\n"},
    {.input = "  parameter int a=b^ ~ (c<<d);",
     .expected = "parameter int a = b ^ ~(c << d);\n"},
    {.input = "  parameter int a=b ^ ~(c<<d);",
     .expected = "parameter int a = b ^ ~(c << d);\n"},

    {.input = "  parameter int a=b^~{c};",
     .expected = "parameter int a = b ^~ {c};\n"},
    {.input = "  parameter int a=b~^{c};",
     .expected = "parameter int a = b ~^ {c};\n"},
    {.input = "  parameter int a=b^ ~ {c};",
     .expected = "parameter int a = b ^ ~{c};\n"},
    {.input = "  parameter int a=b ^ ~{c};",
     .expected = "parameter int a = b ^ ~{c};\n"},

    {.input = "  parameter int a={a}^{b};",
     .expected = "parameter int a = {a} ^ {b};\n"},
    {.input = "  parameter int a={b}^(c);",
     .expected = "parameter int a = {b} ^ (c);\n"},
    {.input = "  parameter int a=b[0]^ {c};",
     .expected = "parameter int a = b[0] ^ {c};\n"},
    {.input = "  parameter int a={c}^a[b];",
     .expected = "parameter int a = {c} ^ a[b];\n"},
    {.input = "  parameter int a=(c)^{a[b]};",
     .expected = "parameter int a = (c) ^ {a[b]};\n"},

    {.input = "  parameter int a={^{a,^b},c};",
     .expected = "parameter int a = {^{a, ^b}, c};\n"},
    {.input = "  parameter int a=(a)^(^d[e]^{c});",
     .expected = "parameter int a = (a) ^ (^d[e] ^ {c});\n"},
    {.input = "  parameter int a=(a)^(^d[e]^f[g]);",
     .expected = "parameter int a = (a) ^ (^d[e] ^ f[g]);\n"},
    {.input = "  parameter int a=(b^(c^(d^e)));",
     .expected = "parameter int a = (b ^ (c ^ (d ^ e)));\n"},
    {.input = "  parameter int a={b^{c^{d^e}}};",
     .expected = "parameter int a = {b ^ {c ^ {d ^ e}}};\n"},
    {.input = "  parameter int a={b^{c[d^e]}};",
     .expected = "parameter int a = {b ^ {c[d^e]}};\n"},  // allow 0 spaces
                                                          // inside "[d^e]"
    {.input = "  parameter int a={(b^c),(d^^e)};",
     .expected = "parameter int a = {(b ^ c), (d ^ ^e)};\n"},

    {.input = "  parameter int a={(b[x]^{c[y]})};",
     .expected = "parameter int a = {(b[x] ^ {c[y]})};\n"},
    {.input = "  parameter int a={d^^e[f] ^ (g)};",
     .expected = "parameter int a = {d ^ ^e[f] ^ (g)};\n"},

    // ~| is unary reduction NOR, |~ and | ~ aren't
    {.input = "  parameter int a=b| ~(c<<d);",
     .expected = "parameter int a = b | ~(c << d);\n"},
    {.input = "  parameter int a=b|~(c<<d);",
     .expected = "parameter int a = b | ~(c << d);\n"},
    {.input = "  parameter int a=b| ~| ( c<<d);",
     .expected = "parameter int a = b | ~|(c << d);\n"},
    {.input = "  parameter int a=b| ~| ~| ( c<<d);",
     .expected = "parameter int a = b | ~|~|(c << d);\n"},
    {.input = "  parameter int a=b| ~~~( c<<d);",
     .expected = "parameter int a = b | ~~~(c << d);\n"},
    {
        .input = "  parameter  int   foo=- - 1 ;",  // double negative
        .expected = "parameter int foo = - -1;\n",
    },
    {
        .input = "  parameter  int   ternary=1?2:3;",
        .expected = "parameter int ternary = 1 ? 2 : 3;\n",
    },
    {
        .input = "  parameter  int   ternary=a?b:c;",
        .expected = "parameter int ternary = a ? b : c;\n",
    },
    {
        .input = "  parameter  int   ternary=\"a\"?\"b\":\"c\";",
        .expected = "parameter int ternary = \"a\" ? \"b\" : \"c\";\n",
    },
    {
        .input = "  parameter  int   t=`\"a`\"?`\"b`\":`\"c`\";",
        .expected = "parameter int t = `\"a`\" ? `\"b`\" : `\"c`\";\n",
    },
    {
        .input = "  parameter  int   ternary=(a)?(b):(c);",
        .expected = "parameter int ternary = (a) ? (b) : (c);\n",
    },
    {
        .input = "  parameter  int   ternary={a}?{b}:{c};",
        .expected = "parameter int ternary = {a} ? {b} : {c};\n",
    },
    {
        .input =
            "  parameter  int   long_ternary=cond?long_option_t:long_option_f;",
        .expected = "parameter int long_ternary = cond ?\n"
                    "    long_option_t : long_option_f;\n",
    },
    {
        .input =
            "  parameter  int   break_two=cond\n"
            "? "
            "a_really_long_option_number_one:a_really_long_option_number_two;",
        .expected = "parameter int break_two = cond ?\n"
                    "    a_really_long_option_number_one :\n"
                    "    a_really_long_option_number_two;\n",
    },
    {
        .input = "  assign   ternary=1?2:3;",
        .expected = "assign ternary = 1 ? 2 : 3;\n",
    },
    {
        .input = "  assign   ternary=a?b:c;",
        .expected = "assign ternary = a ? b : c;\n",
    },
    {
        .input = "  assign   ternary={a}?{b}:{c};",
        .expected = "assign ternary = {a} ? {b} : {c};\n",
    },
    {
        .input =
            "  assign   break_two=cond\n"
            "? "
            "a_really_long_option_number_one:a_really_long_option_number_two;",
        .expected = "assign break_two = cond ?\n"
                    "    a_really_long_option_number_one :\n"
                    "    a_really_long_option_number_two;\n",
    },
    {
        .input = "assign prefetch_d     =\n"
                 "lookup_grant_ic0 ? (lookup_addr_aligned + ADDR) :\n"
                 "                   addr_i;",
        .expected = "assign prefetch_d = lookup_grant_ic0 ?\n"
                    "    (lookup_addr_aligned + ADDR) :\n"
                    "    addr_i;\n",
    },
    {
        .input = "assign prefetch_d     =\n"
                 "lookup_grant_ic0 ? (lookup_addr + 1) :\n"
                 "                   addr_i;",
        .expected = "assign prefetch_d = lookup_grant_ic0 ?\n"
                    "    (lookup_addr + 1) : addr_i;\n",
    },
    {
        .input = "module test;\n"
                 " assign next = // EOL\n"
                 "  foo ? '0 :\n"
                 "  cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next =  // EOL\n"
                    "      foo ? '0 : cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo // EOL\n"
                 "  ? '0 :\n"
                 "  cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo  // EOL\n"
                    "      ? '0 : cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? // EOL\n"
                 "  '0 :\n"
                 "  cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ?  // EOL\n"
                    "      '0 : cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? '0 // EOL\n"
                 "  :\n"
                 "  cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ? '0  // EOL\n"
                    "      : cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? '0 : // EOL\n"
                 "  cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ? '0 :  // EOL\n"
                    "      cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? '0 : // EOL\n"
                 "  bar ? '1 : '0;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ? '0 :  // EOL\n"
                    "      bar ? '1 : '0;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? '0 : // EOL\n"
                 "  bar // EOL2\n"
                 " ? '1 : '0;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ? '0 :  // EOL\n"
                    "      bar  // EOL2\n"
                    "      ? '1 : '0;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? '0 : // EOL\n"
                 "  bar ? // EOL2\n"
                 "  '1 : '0;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ? '0 :  // EOL\n"
                    "      bar ?  // EOL2\n"
                    "      '1 : '0;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? '0 : // EOL\n"
                 "  bar ? '1 // EOL2\n"
                 "  : '0;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ? '0 :  // EOL\n"
                    "      bar ? '1  // EOL2\n"
                    "      : '0;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? '0 : // EOL\n"
                 "  bar ? '1 : // EOL2\n"
                 "  '0;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ? '0 :  // EOL\n"
                    "      bar ? '1 :  // EOL2\n"
                    "      '0;\n"
                    "endmodule\n",
    },
    {
        .input = "assign prefetch_d     =\n"
                 "lookup_ic0 ? // EOL\n"
                 " (lookup_addr + 1) :// BOO\n"
                 "                   addr_i;",
        .expected = "assign prefetch_d = lookup_ic0 ?  // EOL\n"
                    "    (lookup_addr + 1) :  // BOO\n"
                    "    addr_i;\n",
    },
    {
        .input = "module test;\n"
                 " assign next = (foo) ? '0          : // clear \n"
                 "           (bar) ? cnt + 1'b1  : // count \n"
                 "                   cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = (foo) ? '0 :  // clear \n"
                    "      (bar) ? cnt + 1'b1 :  // count \n"
                    "      cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = // FOO\n"
                 "  (foo) ? '0          : // clear \n"
                 "           (bar) ? cnt + 1'b1  : // count \n"
                 "                   cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next =  // FOO\n"
                    "      (foo) ? '0 :  // clear \n"
                    "      (bar) ? cnt + 1'b1 :  // count \n"
                    "      cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? a_really_long_identifier : // EOL\n"
                 "  cnt;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ?\n"
                    "      a_really_long_identifier :  // EOL\n"
                    "      cnt;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? a_really_long_identifier : // EOL\n"
                 "  another_really_long_identifier;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ?\n"
                    "      a_really_long_identifier :  // EOL\n"
                    "      another_really_long_identifier;\n"
                    "endmodule\n",
    },
    {
        .input = "module test;\n"
                 " assign next = foo ? a_really_long_identifier : "
                 "another_really_long_identifier;\n"
                 "endmodule\n",
        .expected = "module test;\n"
                    "  assign next = foo ?\n"
                    "      a_really_long_identifier :\n"
                    "      another_really_long_identifier;\n"
                    "endmodule\n",
    },
    {
        .input = "assign m = check                ? {10'b0, foo} :\n"
                 "           (bar && (baz == '0)) ? hello        :\n"
                 "           world                ? temp1        : temp2;\n",
        .expected = "assign m = check ? {10'b0, foo} :\n"
                    "    (bar && (baz == '0)) ? hello :\n"
                    "    world ? temp1 : temp2;\n",
    },
    {
        .input = "assign {a, b} = !(c == d) ? {1'b0, e} :\n"
                 "                ((e == f) && g) ?\n"
                 "                {1'b0, f} : (h) ?\n"
                 "                {1'b0, e} - 1'b1 :\n"
                 "                {1'b0, e} + 1'b1;\n",
        .expected = "assign {a, b} = !(c == d) ? {1'b0, e} :\n"
                    "    ((e == f) && g) ? {1'b0, f} : (h) ?\n"
                    "    {1'b0, e} - 1'b1 : {1'b0, e} + 1'b1;\n",
    },
    {
        .input = "assign {aaaaaaaaaa, bbbbbbbbb} = {1'b0, "
                 "cccccccccccccccccc[15:0]} +\n"
                 "                                 {1'b0, "
                 "ddddddddddddddddd[15:0]};\n",
        .expected = "assign {aaaaaaaaaa, bbbbbbbbb} =\n"
                    "    {1'b0, cccccccccccccccccc[15:0]} +\n"
                    "    {1'b0, ddddddddddddddddd[15:0]};\n",
    },
    {
        .input =
            "covergroup a(string b);\n"
            "foobar: cross foo, bar {"
            "ignore_bins baz = binsof(qux) intersect {1, 2, 3, 4, 5, 6, 7};"
            "}\n"
            "endgroup : a\n",
        .expected = "covergroup a(string b);\n"
                    "  foobar: cross foo, bar{\n"
                    "    ignore_bins baz =\n"
                    "        binsof (qux) intersect {\n"
                    "      1, 2, 3, 4, 5, 6, 7\n"
                    "    };\n"
                    "  }\n"
                    "endgroup : a\n",
    },
    {
        .input = "assign {aa, bb} = {1'b0, cc} + {1'b0, dd};\n",
        .expected = "assign {aa, bb} = {1'b0, cc} +\n"
                    "    {1'b0, dd};\n",
    },

    // streaming operators
    {
        .input = "   parameter  int  b={ >>   { a } } ;",
        .expected = "parameter int b = {>>{a}};\n",
    },
    {
        .input = "   parameter  int  b={ >>   { a , b,  c } } ;",
        .expected = "parameter int b = {>>{a, b, c}};\n",
    },
    {
        .input = "   parameter  int  b={ >> 4  { a } } ;",
        .expected = "parameter int b = {>>4{a}};\n",
    },
    {
        .input = "   parameter  int  b={ >> byte  { a } } ;",
        .expected = "parameter int b = {>>byte{a}};\n",
    },
    {
        .input = "   parameter  int  b={ >> my_type_t  { a } } ;",
        .expected = "parameter int b = {>>my_type_t{a}};\n",
    },
    {
        .input = "   parameter  int  b={ >> `GET_TYPE  { a } } ;",
        .expected = "parameter int b = {>>`GET_TYPE{a}};\n",
    },
    {
        .input = "   parameter  int  b={ >> 4  {{ >> 2 { a }  }} } ;",
        .expected = "parameter int b = {>>4{{>>2{a}}}};\n",
    },
    {
        .input = "   parameter  int  b={ <<   { a } } ;",
        .expected = "parameter int b = {<<{a}};\n",
    },
    {
        .input = "   parameter  int  b={ <<   { a , b,  c } } ;",
        .expected = "parameter int b = {<<{a, b, c}};\n",
    },
    {
        .input = "   parameter  int  b={ << 4  { a } } ;",
        .expected = "parameter int b = {<<4{a}};\n",
    },
    {
        .input = "   parameter  int  b={ << byte  { a } } ;",
        .expected = "parameter int b = {<<byte{a}};\n",
    },
    {
        .input = "   parameter  int  b={ << my_type_t  { a } } ;",
        .expected = "parameter int b = {<<my_type_t{a}};\n",
    },
    {
        .input = "   parameter  int  b={ << `GET_TYPE  { a } } ;",
        .expected = "parameter int b = {<<`GET_TYPE{a}};\n",
    },
    {
        .input = "   parameter  int  b={ << 4  {{ << 2 { a }  }} } ;",
        .expected = "parameter int b = {<<4{{<<2{a}}}};\n",
    },

    // basic module test cases
    {.input = "module foo;endmodule:foo\n",
     .expected = "module foo;\n"
                 "endmodule : foo\n"},
    {.input = "module\nfoo\n;\nendmodule\n:\nfoo\n",
     .expected = "module foo;\n"
                 "endmodule : foo\n"},
    {.input = "module\tfoo\t;\tendmodule\t:\tfoo",
     .expected = "module foo;\n"
                 "endmodule : foo\n"},
    {.input = "module foo;     // foo\n"
              "endmodule:foo\n",
     .expected = "module foo;  // foo\n"
                 "endmodule : foo\n"},
    {.input = "module foo;/* foo */endmodule:foo\n",
     .expected = "module foo;  /* foo */\n"
                 "endmodule : foo\n"},
    {.input = "module pm #(\n"
              "//comment\n"
              ") (wire ww);\n"
              "endmodule\n",
     .expected = "module pm #(\n"
                 "    //comment\n"  // comment indented
                 ") (\n"
                 "    wire ww\n"
                 ");\n"
                 "endmodule\n"},
    {.input = "module pm ( ) ;\n"  // empty ports list
              "endmodule\n",
     .expected = "module pm ();\n"
                 "endmodule\n"},
    {.input = "module pm #(\n"
              "//comment\n"
              ") ( );\n"
              "endmodule\n",
     .expected = "module pm #(\n"
                 "    //comment\n"  // comment indented
                 ") ();\n"          // (); grouped together
                 "endmodule\n"},
    {.input = "`ifdef FOO\n"
              "    `ifndef BAR\n"
              "    `endif\n"
              "`endif\n",
     .expected = "`ifdef FOO\n"
                 "`ifndef BAR\n"
                 "`endif\n"
                 "`endif\n"},
    {.input = "module foo(\n"
              "       `include \"ports.svh\"\n"
              "         ) ; endmodule\n",
     .expected = "module foo (\n"
                 "    `include \"ports.svh\"\n"
                 ");\n"
                 "endmodule\n"},
    {.input = "module foo(\n"
              "       `define FOO\n"
              "`undef\tFOO\n"
              "         ) ; endmodule\n",
     .expected = "module foo (\n"
                 "    `define FOO\n"
                 "    `undef FOO\n"
                 ");\n"
                 "endmodule\n"},
    {.input = "module foo(  input x  , output y ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  x,\n"  // aligned
                 "    output y\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(\n"
              "// comment\n"
              "  input x  , output y ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    // comment\n"
                 "    input  x,\n"  // aligned
                 "    output y\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input[2:0]x  , output y [3:0] ) ;endmodule:foo\n",
     // each port item should be on its own line
     .expected = "module foo (\n"
                 "    input  [2:0] x,\n"  // aligned
                 "    output       y[3:0]\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire x  , output reg yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x,\n"  // aligned
                 "    output reg  yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire x  ,//c1\n"
              "output reg yyy //c2\n"
              " ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x,   //c1\n"  // aligned
                 "    output reg  yyy  //c2\n"  // aligned
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire x  ,/* c1 */\n"
              "output reg yyy /* c2 */\n"
              " ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x,   /* c1 */\n"  // aligned
                 "    output reg  yyy  /* c2 */\n"  // aligned
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(\n"
              "// comment\n"
              "input wire x  ,//c1\n"
              "output reg yyy //c2\n"
              " ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    // comment\n"
                 "    input  wire x,   //c1\n"  // aligned
                 "    output reg  yyy  //c2\n"  // aligned
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(\n"
              "/* comment */\n"
              "input wire x  ,/* c1 */\n"
              "output reg yyy /* c2 */\n"
              " ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    /* comment */\n"
                 "    input  wire x,   /* c1 */\n"  // aligned
                 "    output reg  yyy  /* c2 */\n"  // aligned
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire x  ,/* c1\n"
              "c2\n"
              "c3 */\n"
              "output reg yyy /* c4 */\n"
              " ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x,   /* c1\n"
                 "c2\n"
                 "c3 */\n"  // TODO: align multiline comments
                 "    output reg  yyy  /* c4 */\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire x  ,/* c1 */\n"
              "output reg yyy,\n"
              "output z // c2\n"
              " ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x,    /* c1 */\n"  // aligned
                 "    output reg  yyy,\n"
                 "    output      z     // c2\n"  // aligned
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module m(input logic [4:0] foo,  // comment\n"
              "input logic bar // comment\n"
              " ) ;endmodule:m\n",
     .expected = "module m (\n"
                 "    input logic [4:0] foo,  // comment\n"  // aligned
                 "    input logic       bar   // comment\n"  // aligned
                 ");\n"
                 "endmodule : m\n"},
    {.input = "module foo(  input wire x  , output yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x,\n"  // aligned
                 "    output      yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   x  , output reg yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input      x,\n"  // aligned
                 "    output reg yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   x  , output reg[a:b]yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input            x,\n"  // aligned
                 "    output reg [a:b] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(  input   [a:b]x  , output reg  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input      [a:b] x,\n"  // aligned
                 "    output reg       yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   x  , "
              "  output logic  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input        x,\n"  // aligned
                 "    output logic yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   [a:c]x  , "
              "  output logic[a-b: c]  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input        [  a:c] x,\n"  // aligned
                 "    output logic [a-b:c] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   [a:c]x  , "
              "  output logic[a - b: c]  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input        [    a:c] x,\n"  // aligned
                 "    output logic [a - b:c] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   [a:c]x  , input zzz ,"
              "  output logic[a - b: c]  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input        [    a:c] x,\n"    // aligned []'s
                 "    input                  zzz,\n"  // aligned ids
                 "    output logic [a - b:c] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   [a:b]x  , "
              "  output reg[e: f]  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input      [a:b] x,\n"  // aligned
                 "    output reg [e:f] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   tri[aa: bb]x  , "
              "  output reg[e: f]  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  tri [aa:bb] x,\n"  // aligned
                 "    output reg [  e:f] yy\n"  // TODO(b/70310743): align ':'
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input   [a:b][c:d]x  , "
              "  output reg[e: f]  yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input      [a:b][c:d] x,\n"  // aligned
                 "    output reg [e:f]      yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(  input wire x  [j:k], output reg yy ) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x [j:k],\n"  // aligned
                 "    output reg  yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(  input wire x  , output reg yy [j:k]) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x,\n"  // aligned
                 "    output reg  yy[j:k]\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire x  [p:q], output reg yy [j:k]) "
              ";endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x [p:q],\n"  // aligned
                 "    output reg  yy[j:k]\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire x  [p:q][r:s], output reg yy [j:k]) "
              ";endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire x [p:q][r:s],\n"  // aligned
                 "    output reg  yy[j:k]\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(  input wire x  [p:q][rr:ss], output reg yy [jj:kk][m:n]) "
         ";endmodule:foo\n",
     .expected = "module foo (\n"
                 // TODO(b/70310743): align :'s
                 "    input  wire x [  p:q][rr:ss],\n"
                 "    output reg  yy[jj:kk][  m:n]\n"  // aligned
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire   [p:q]x, output reg yy [j:k]) "
              ";endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire [p:q] x,\n"  // aligned
                 "    output reg        yy[j:k]\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input wire  x [p:q], output reg[j:k]yy) "
              ";endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire       x [p:q],\n"  // aligned
                 "    output reg  [j:k] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(  input pkg::bar_t  x , output reg  yy) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  pkg::bar_t x,\n"  // aligned
                 "    output reg        yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(  input wire  x , output pkg::bar_t  yy) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  wire       x,\n"  // aligned
                 "    output pkg::bar_t yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input pkg::bar_t#(1)  x , output reg  yy) "
              ";endmodule:foo\n",
     .expected = "module foo (\n"  // with parameterized port type
                 "    input  pkg::bar_t#(1) x,\n"
                 "    output reg            yy\n"  // aligned
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input signed x , output reg  yy) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  signed x,\n"  // aligned
                 "    output reg    yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(  input signed x , output reg [m:n] yy) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  signed       x,\n"  // aligned
                 "    output reg    [m:n] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input int signed x , output reg [m:n] yy) "
              ";endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  int signed       x,\n"  // aligned
                 "    output reg        [m:n] yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(  input signed x , output pkg::bar_t  yy) "
              ";endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input  signed     x,\n"  // aligned
                 "    output pkg::bar_t yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module somefunction ("
              "logic clk, int   a, int b);endmodule",
     .expected = "module somefunction (\n"
                 "    logic clk,\n"  // direction missing
                 "    int   a,\n"    // direction missing
                 "    int   b\n"     // direction missing
                 ");\n"
                 "endmodule\n"},
    {.input = "module somefunction ("
              "logic clk, input int   a, int b);endmodule",
     .expected = "module somefunction (\n"
                 "          logic clk,\n"  // direction missing
                 "    input int   a,\n"
                 "          int   b\n"  // direction missing
                 ");\n"
                 "endmodule\n"},
    {.input = "module somefunction ("
              "input logic clk, input int   a, int b);endmodule",
     .expected = "module somefunction (\n"
                 "    input logic clk,\n"
                 "    input int   a,\n"
                 "          int   b\n"  // direction missing
                 ");\n"
                 "endmodule\n"},
    {.input = "module somefunction ("
              "input clk, input int   a, int b);endmodule",
     .expected = "module somefunction (\n"
                 "    input     clk,\n"  // type missing
                 "    input int a,\n"
                 "          int b\n"
                 ");\n"
                 "endmodule\n"},
    {.input = "module somefunction ("
              "input logic clk, input a, int b);endmodule",
     .expected = "module somefunction (\n"
                 "    input logic clk,\n"
                 "    input       a,\n"  // type missing
                 "          int   b\n"   // direction missing
                 ");\n"
                 "endmodule\n"},
    {.input = "module t;\n"
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
     .expected = "module t;\n"
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
    {.input = "module t (\n"
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
     .expected = "module t (\n"
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
    {.input = "module m;foo bar(.baz({larry, moe, curly}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.baz({larry, moe, curly}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.baz({larry,// expand this\n"
              "moe, curly}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (\n"
                 "      .baz({\n"
                 "        larry,  // expand this\n"
                 "        moe,\n"
                 "        curly\n"
                 "      })\n"
                 "  );\n"
                 "endmodule\n"},
    {.input = "parameter priv_reg_t impl_csr[] = {\n"
              "// Machine mode mode CSR\n"
              "MVENDORID, //\n"
              "MARCHID,   //\n"
              "DSCRATCH0, //\n"
              "DSCRATCH1  //\n"
              "};",
     .expected = "parameter priv_reg_t impl_csr[] = {\n"
                 "  // Machine mode mode CSR\n"
                 "  MVENDORID,  //\n"
                 "  MARCHID,  //\n"
                 "  DSCRATCH0,  //\n"
                 "  DSCRATCH1  //\n"
                 "};\n"},
    {.input = "parameter priv_reg_t impl_csr[] = {\n"
              "// Expand elements\n"
              "MVENDORID,\n"
              "MARCHID,\n"
              "DSCRATCH0,\n"
              "DSCRATCH1\n"
              "};",
     .expected = "parameter priv_reg_t impl_csr[] = {\n"
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
    {.input = "module foo(\n"
              "//c1\n"
              "input wire x , \n"
              "//c2\n"
              "output reg  yy\n"
              "//c3\n"
              ") ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    //c1\n"
                 "    input  wire x,\n"  // aligned, ignoring comments
                 "    //c2\n"
                 "    output reg  yy\n"
                 "    //c3\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(\n"
              "//c1\n"
              "input wire x , \n"
              "//c2a\n"  // longer comment
              "//c2b\n"
              "output reg  yy\n"
              "//c3\n"
              ") ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    //c1\n"
                 "    input  wire x,\n"  // aligned, ignoring comments
                 "    //c2a\n"  // note: separated by 2 lines of comments
                 "    //c2b\n"
                 "    output reg  yy\n"
                 "    //c3\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(\n"
              "`ifdef   FOO\n"
              "input wire x , \n"
              " `else\n"
              "output reg  yy\n"
              " `endif\n"
              ") ;endmodule:foo\n",
     .expected =
         "module foo (\n"
         "`ifdef FOO\n"
         "    input  wire x,\n"  // aligned, ignoring preprocessor conditionals
         "`else\n"
         "    output reg  yy\n"
         "`endif\n"
         ");\n"
         "endmodule : foo\n"},
    {.input = "module foo(\n"
              "input w , \n"
              "`define   FOO BAR\n"
              "input wire x , \n"
              " `include  \"stuff.svh\"\n"
              "output reg  yy\n"
              " `undef    FOO\n"
              "output zz\n"
              ") ;endmodule:foo\n",
     .expected =
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
    {.input =
         "module foo(\n"
         "input wire x , \n  \n"  // blank line, separating alignment groups
         "output reg  yy\n"
         ") ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input wire x,\n"  // not aligned, due to blank line
                                        // separating groups
                 "\n"
                 "    output reg yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo(\n"
         "input wire x1 [r:s],\n"
         "input [p:q] x2 , \n  \n"  // blank line, separating alignment groups
         "output reg  [jj:kk]yy1,\n"
         "output pkg::barr_t [mm:nn] yy2\n"
         ") ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input wire       x1[r:s],\n"  // aligned in this group,
                                                    // but not across
                                                    // groups
                 "    input      [p:q] x2,\n"
                 "\n"
                 "    output reg         [jj:kk] yy1,\n"
                 "    output pkg::barr_t [mm:nn] yy2\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo(\n"  // same as previous, with comments
              " //c1\n"
              "input wire x1 [r:s],\n"
              "input [p:q] x2 , \n"
              " //c2\n\n"  // blank line, separating alignment groups
              " //c3\n"
              "output reg  [jj:kk]yy1,\n"
              " //c4\n"
              "output pkg::barr_t [mm:nn] yy2\n"
              ") ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    //c1\n"
                 "    input wire       x1[r:s],\n"  // aligned in this group,
                                                    // but not across
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
     .input = "class sample;"
              "bit a;;"
              "bit b;"
              "endclass",
     .expected = "class sample;\n"
                 "  bit a;\n"
                 "  ;\n"
                 "  bit b;\n"
                 "endclass\n"},
    {.input = "class sample;"
              "bit a;;"
              "endclass",
     .expected = "class sample;\n"
                 "  bit a;\n"
                 "  ;\n"
                 "endclass\n"},
    {.input = "class sample;"
              "bit a;"
              "bit b;;"
              "endclass",
     .expected = "class sample;\n"
                 "  bit a;\n"
                 "  bit b;\n"
                 "  ;\n"
                 "endclass\n"},

    {// aligning here just barely fits in the 40col limit
     .input = "module foo(  input int signed x [a:b],"
              "output reg [mm:nn] yy) ;endmodule:foo\n",
     .expected =
         "module foo (\n"
         // ---------------40col----------------->
         "    input  int signed         x [a:b],\n"  // aligned, still fits
         "    output reg        [mm:nn] yy\n"
         ");\n"
         "endmodule : foo\n"},
    {// when aligning would result in exceeding column limit, don't align for
     // now
     .input = "module foo(  input int signed x [aa:bb],"
              "output reg [mm:nn] yy) ;endmodule:foo\n",
     .expected =
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
     .input = "module foo(  input int signed x [aa:bb],"
              "output reg [mm:nn] yyy) ;endmodule:foo\n",
     .expected =
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
     .input = "module foo(  input int signed x [a:b],//c\n"
              "output reg [m:n] yy) ;endmodule:foo\n",
     .expected =
         "module foo (\n"
         // ---------------40col---------------->
         //   input  int signed       x [a:b],  //c\n"  // over limit, by
         //   comment
         //   output reg        [m:n] yy\n"
         "    input int signed x[a:b],  //c\n"  // aligned would be 42 columns
         "    output reg [m:n] yy\n"
         ");\n"
         "endmodule : foo\n"},
    {// aligning interfaces in port headers like types
     // TODO(b/161181877): flush interface port type left (multi-column)
     .input = "module foo(  input clk , inter.face yy) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input            clk,\n"  // aligned
                 "          inter.face yy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {// aligning interfaces in port headers like types
     // TODO(b/161181877): flush interface port type left (multi-column)
     .input = "module foo(  input wire   clk , inter.face yy) ;endmodule:foo\n",
     .expected = "module foo (\n"
                 "    input wire       clk,\n"  // aligned
                 "          inter.face yy\n"
                 ");\n"
                 "endmodule : foo\n"},

    // module local variable/net declaration alignment test cases
    {.input = "module m;\n"
              "logic a;\n"
              "bit b;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic a;\n"
                 "  bit   b;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic a;\n"
              "bit b;\n"
              "initial e=f;\n"  // separates alignment groups
              "wire c;\n"
              "bit d;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic a;\n"
                 "  bit   b;\n"
                 "  initial e = f;\n"  // separates alignment groups
                 "  wire c;\n"
                 "  bit  d;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "// hello a\n"
              "logic a;\n"
              "// hello b\n"
              "bit b;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  // hello a\n"
                 "  logic a;\n"
                 "  // hello b\n"
                 "  bit   b;\n"  // aligned across comments
                 "endmodule\n"},
    {.input = "module m;\n"
              "// hello a\n"
              "logic a;\n"
              "\n"  // extra blank line
              "// hello b\n"
              "bit b;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  // hello a\n"
                 "  logic a;\n"
                 "\n"              // extra blank line
                 "  // hello b\n"  // aligned across blank lines
                 "  bit   b;\n"    // aligned across comments
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic [x:y]a;\n"  // packed dimensions
              "bit b;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic [x:y] a;\n"
                 "  bit         b;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic a;\n"
              "bit [pp:qq]b;\n"  // packed dimensions
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic         a;\n"
                 "  bit   [pp:qq] b;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic [x:y]a;\n"  // packed dimensions
              "bit [pp:qq]b;\n"  // packed dimensions
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic [  x:y] a;\n"
                 "  bit   [pp:qq] b;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic [x:y]a;\n"         // packed dimensions
              "wire [pp:qq] [e:f]b;\n"  // packed dimensions, 2D
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic [  x:y]      a;\n"
                 "  wire  [pp:qq][e:f] b;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic a [x:y];\n"  // unpacked dimensions
              "bit bbb;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic a   [x:y];\n"
                 "  bit   bbb;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic aaa ;\n"
              "wire w [yy:zz];\n"  // unpacked dimensions
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic aaa;\n"
                 "  wire  w   [yy:zz];\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic aaa [s:t] ;\n"  // unpacked dimensions
              "wire w [yy:zz];\n"    // unpacked dimensions
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic aaa[  s:t];\n"
                 "  wire  w  [yy:zz];\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic aaa [s:t] ;\n"     // unpacked dimensions
              "wire w [yy:zz][u:v];\n"  // unpacked dimensions, 2D
              "endmodule\n",
     .expected =
         "module m;\n"
         "  logic aaa[  s:t];\n"
         "  wire  w  [yy:zz] [u:v];\n"
         // TODO(b/165323560): unwanted space between unpacked dimensions of 'w'
         "endmodule\n"},
    {.input = "module m;\n"
              "qqq::rrr s;\n"     // user-defined type
              "wire [pp:qq]w;\n"  // packed dimensions
              "endmodule\n",
     .expected = "module m;\n"
                 "  qqq::rrr         s;\n"
                 "  wire     [pp:qq] w;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "qqq#(rr) s;\n"     // parameterized type
              "wire [pp:qq]w;\n"  // packed dimensions
              "endmodule\n",
     .expected = "module m;\n"
                 "  qqq #(rr)         s;\n"
                 "  wire      [pp:qq] w;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic a;\n"
              "bit b;\n"
              "my_module  my_inst( );\n"  // module instance separates alignment
                                          // groups
              "wire c;\n"
              "bit d;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic a;\n"  // these two are aligned
                 "  bit   b;\n"
                 "  my_module my_inst ();\n"  // module instance separates
                                              // alignment groups
                 "  wire c;\n"                // these two are aligned
                 "  bit  d;\n"
                 "endmodule\n"},
    {.input = "module m;\n"
              "logic aaa = expr1;\n"
              "bit b = expr2;\n"
              "endmodule\n",
     .expected = "module m;\n"
                 "  logic aaa = expr1;\n"
                 "  bit   b = expr2;\n"  // no alignment at '=' yet
                 "endmodule\n"},
    {.input = "(* foo='{\"bar_.*\"} *)\n"  // funny attribute
              "module mattr;\n"
              "(* attr1=\"value1\" *)\n"  // attribute ignored
              "ex_input_pins_t ex_input_pins;\n"
              "(* attr2=\"value2\" *)\n"  // attribute ignored
              "ex_output_pins_t ex_output_pins;\n"
              "(* attr3=\"value3\" *)\n"  // attribute ignored
              "ex wrap_ex ( );\n"
              "endmodule\n",
     .expected = "(* foo='{\"bar_.*\"} *)\n"
                 "module mattr;\n"
                 "  (* attr1=\"value1\" *)\n"           // indented
                 "  ex_input_pins_t  ex_input_pins;\n"  // aligned
                 "  (* attr2=\"value2\" *)\n"           // indented
                 "  ex_output_pins_t ex_output_pins;\n"
                 "  (* attr3=\"value3\" *)\n"  // indented
                 "  ex wrap_ex ();\n"
                 "endmodule\n"},
    {.input = "module mattr;\n"
              "ex_input_pins_t ex_input_pins;\n"
              "ex_output_pins_t ex_output_pins;\n"
              "(* package_definition=\"ex_pkg\" *)\n"  // attribute ignored
              "ex wrap_ex (\n"
              ".clk(ex_input_pins.clk),\n"
              ".rst(ex_input_pins.rst),\n"
              ".in(ex_input_pins.in)\n"
              ");\n"
              "endmodule\n",
     .expected = "module mattr;\n"
                 "  ex_input_pins_t  ex_input_pins;\n"  // aligned
                 "  ex_output_pins_t ex_output_pins;\n"
                 "  (* package_definition=\"ex_pkg\" *)\n"  // indented
                 "  ex wrap_ex (\n"
                 "      .clk(ex_input_pins.clk),\n"
                 "      .rst(ex_input_pins.rst),\n"
                 "      .in (ex_input_pins.in)\n"  // aligned
                 "  );\n"
                 "endmodule\n"},
    {.input = "module test;\n"
              "bind entropy_src tlul_assert #(.EndpointType(\"Device\"))\n"
              "tlul_assert_device (.clk_i, .rst_ni, .h2d(tl_i), .d2h(tl_o));\n"
              "endmodule\n",
     .expected = "module test;\n"
                 "  bind entropy_src tlul_assert #(\n"
                 "      .EndpointType(\"Device\")\n"
                 "  ) tlul_assert_device (\n"
                 "      .clk_i,\n"
                 "      .rst_ni,\n"
                 "      .h2d(tl_i),\n"
                 "      .d2h(tl_o)\n"
                 "  );\n"
                 "endmodule\n"},
    {.input = "module test;\n"
              "bind entropy_src tlul_assert #(.EndpointType(\"Device\"))\n"
              "tlul_assert_device (.clk_i, .rst_ni,\n\n .h2d(tl_i),\n\n "
              ".d2h(tl_o));\n"
              "endmodule\n",
     .expected = "module test;\n"
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
    {.input = "bind expand_me long_name #(.W(W_CONST), .D(D_CONST)) "
              "instaaance_name ("
              ".in(iiiiiiiin),\n\n .out(ooooooout),\n .clk(ccccccclk),\n\n"
              ".in1234 (in),\n //c1\n .out1234(out),\n .clk1234(clk),);",
     .expected = "bind expand_me long_name #(\n"
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
    {.input = "initial // clock generation\n begin\n clk = 0;\n forever begin\n"
              "#4ns clk = !clk;\n end\n end\n",
     .expected = "initial  // clock generation\n"
                 "  begin\n"
                 "    clk = 0;\n"
                 "    forever begin\n"
                 "      #4ns clk = !clk;\n"
                 "    end\n"
                 "  end\n"},
    {.input = "module foo #(int x,int y) ;endmodule:foo\n",  // parameters
     .expected = "module foo #(\n"
                 "    int x,\n"
                 "    int y\n"
                 ");\n"  // each parameter on its own line
                 "endmodule : foo\n"},
    {.input = "module foo #(int x)(input y) ;endmodule:foo\n",
     // parameter and port
     .expected =
         "module foo #(\n"
         "    int x\n"
         ") (\n"
         "    input y\n"
         ");\n"  // each paramater and port item should be on its own line
         "endmodule : foo\n"},
    {.input = "module foo #(parameter int x,parameter int y) ;endmodule:foo\n",
     // parameters don't fit (also should be on its own line)
     .expected = "module foo #(\n"
                 "    parameter int x,\n"
                 "    parameter int y\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input =
         "module foo #(parameter int xxxx,parameter int yyyy) ;endmodule:foo\n",
     // parameters don't fit
     .expected = "module foo #(\n"
                 "    parameter int xxxx,\n"
                 "    parameter int yyyy\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo #(parameter int x = $clog2  (N) ,parameter int y ) "
              ";endmodule:foo\n",
     // parameters don't fit
     .expected = "module foo #(\n"
                 "    parameter int x = $clog2(N),\n"  // no space after $clog2
                 "    parameter int y\n"
                 ");\n"
                 "endmodule : foo\n"},
    {.input = "module foo #(//comment\n"
              "parameter bar =1,\n"
              "localparam baz =2"
              ") ();"
              "endmodule",
     .expected = "module foo #(  //comment\n"
                 "    parameter  bar = 1,\n"
                 "    localparam baz = 2\n"
                 ") ();\n"
                 "endmodule\n"},
    {.input = "module foo #("
              "parameter  bar =1,//comment\n"
              "localparam baz =2"
              ") ();"
              "endmodule",
     .expected = "module foo #(\n"
                 "    parameter  bar = 1,  //comment\n"
                 "    localparam baz = 2\n"
                 ") ();\n"
                 "endmodule\n"},
    {.input = "module foo #("
              "parameter  bar =1,"
              "localparam baz =2//comment\n"
              ") ();"
              "endmodule",
     .expected = "module foo #(\n"
                 "    parameter  bar = 1,\n"
                 "    localparam baz = 2   //comment\n"
                 ") ();\n"
                 "endmodule\n"},
    {.input = "module foo #("
              "parameter  bar =1//comment\n"
              ",localparam baz =2\n"
              ") ();"
              "endmodule",
     .expected = "module foo #(\n"
                 "      parameter  bar = 1  //comment\n"
                 "    , localparam baz = 2\n"
                 ") ();\n"
                 "endmodule\n"},
    {.input = "module foo;"
              // fit in one line
              "parameter int i = '{\n"
              "1,\n"
              "2,\n"
              "3\n"
              "};\n"
              "endmodule",
     .expected = "module foo;\n"
                 "  parameter int i = '{1, 2, 3};\n"
                 "endmodule\n"},
    {.input = "module foo;"
              // too long for one line, expand
              "localparam logic [63:0] RC[24] = '{\n"
              "64'h 1,\n"
              "64'h 2,\n"
              "64'h 3\n"
              "};\n"
              "endmodule",
     .expected = "module foo;\n"
                 "  localparam logic [63:0] RC[24] = '{\n"
                 "      64'h1,\n"
                 "      64'h2,\n"
                 "      64'h3\n"
                 "  };\n"
                 "endmodule\n"},
    {.input = "module foo;"
              "parameter int i = '{\n"
              // force expansion
              "1, //\n"
              "2,\n"
              "3\n"
              "};\n"
              "endmodule",
     .expected = "module foo;\n"
                 "  parameter int i = '{\n"
                 "      1,  //\n"
                 "      2,\n"
                 "      3\n"
                 "  };\n"
                 "endmodule\n"},
    {.input = "module foo;"
              "localparam logic [63:0] RC[24] = '{\n"
              "64'h 0000_0000_0000_0001, // 0\n"
              "64'h 0000_0000_0000_8082, // 1\n"
              "64'h 8000_0000_8000_8008 // 23\n"
              "};\n"
              "endmodule",
     .expected = "module foo;\n"
                 "  localparam logic [63:0] RC[24] = '{\n"
                 "      64'h0000_0000_0000_0001,  // 0\n"
                 "      64'h0000_0000_0000_8082,  // 1\n"
                 "      64'h8000_0000_8000_8008  // 23\n"
                 "  };\n"
                 "endmodule\n"},
    {.input = "module foo;"
              // nest two patterns
              "parameter logic [11:0] i = '{\n"
              "'{1,2,3},\n"
              "'{1,2,3}\n"
              "};\n"
              "endmodule",
     .expected = "module foo;\n"
                 "  parameter logic [11:0] i = '{\n"
                 "      '{1, 2, 3},\n"
                 "      '{1, 2, 3}\n"
                 "  };\n"
                 "endmodule\n"},
    {.input = "module foo;"
              // nest two patterns, expand interior
              "parameter logic [11:0] i = '{\n"
              "'{1, //\n"
              " 2,3},\n"
              "'{1,2,3}\n"
              "};\n"
              "endmodule",
     .expected = "module foo;\n"
                 "  parameter logic [11:0] i = '{\n"
                 "      '{\n"
                 "          1,  //\n"
                 "          2,\n"
                 "          3\n"
                 "      },\n"
                 "      '{1, 2, 3}\n"
                 "  };\n"
                 "endmodule\n"},
    {.input = "module foo;"
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
     .expected = "module foo;\n"
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
    {.input = "module foo;"
              // nest three patterns
              "parameter logic [11:0] i = '{\n"
              "'{'{1,2,3},4}\n"
              "};\n"
              "endmodule",
     .expected = "module foo;\n"
                 "  parameter logic [11:0] i = '{\n"
                 "      '{'{1, 2, 3}, 4}\n"
                 "  };\n"
                 "endmodule\n"},
    {.input = "module foo;"
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
     .expected = "module foo;\n"
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
    {.input = "module    top;"
              "foo#(  \"test\"  ) foo(  );"
              "bar#(  \"test\"  ,5) bar(  );"
              "endmodule\n",
     .expected =
         "module top;\n"
         "  foo #(\"test\") foo ();\n"  // module instantiation, string arg
         "  bar #(\"test\", 5) bar ();\n"
         "endmodule\n"},
    {.input = "module    top;"
              "foo#(  `\"test`\"  ) foo(  );"
              "bar#(  `\"test`\"  ,5) bar(  );"
              "endmodule\n",
     .expected = "module top;\n"
                 "  foo #(`\"test`\") foo ();\n"  // module instantiation, eval
                                                  // string arg
                 "  bar #(`\"test`\", 5) bar ();\n"
                 "endmodule\n"},
    {.input = "`ifdef FOO\n"
              "  module bar;endmodule\n"
              "`endif\n",
     .expected = "`ifdef FOO\n"
                 "module bar;\n"
                 "endmodule\n"
                 "`endif\n"},
    {.input = "`ifdef FOO\n"
              "  module bar;endmodule\n"
              "`else module baz;endmodule\n"
              "`endif\n",
     .expected = "`ifdef FOO\n"
                 "module bar;\n"
                 "endmodule\n"
                 "`else\n"
                 "module baz;\n"
                 "endmodule\n"
                 "`endif\n"},
    {.input = "`ifdef FOO\n"
              "  module bar;endmodule\n"
              "`else /* glue me */ module baz;endmodule\n"
              "`endif\n",
     .expected = "`ifdef FOO\n"
                 "module bar;\n"
                 "endmodule\n"
                 "`else  /* glue me */\n"
                 "module baz;\n"
                 "endmodule\n"
                 "`endif\n"},
    {.input = "`ifdef FOO\n"
              "  module bar;endmodule\n"
              "`else// different unit\n"
              "  module baz;endmodule\n"
              "`endif\n",
     .expected = "`ifdef FOO\n"
                 "module bar;\n"
                 "endmodule\n"
                 "`else  // different unit\n"
                 "module baz;\n"
                 "endmodule\n"
                 "`endif\n"},

    // unary: + - !  ~ & | ^  ~& ~| ~^ ^~
    {.input = "module m;foo bar(.x(-{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(-{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(!{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(!{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(~{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(~{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(&{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(&{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(|{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(|{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(^{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(^{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(~&{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(~&{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(~|{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(~|{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(~^{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(~^{a, b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(^~{a,b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(^~{a, b}));\n"
                 "endmodule\n"},

    // binary: + - * / % & | ^ ^~ ~^ && ||
    {.input = "module m;foo bar(.x(a+b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a + b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a-b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a - b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a*b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a * b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a/b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a / b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a%b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a % b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a&b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a & b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a|b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a | b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a^b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a ^ b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a^~b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a ^~ b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a~^b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a ~^ b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a&&b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a && b));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a||b));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a || b));\n"
                 "endmodule\n"},

    // {a} op {b}
    {.input = "module m;foo bar(.x({a}+{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} + {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}-{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} - {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}*{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} * {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}/{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} / {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}%{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} % {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}&{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} & {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}|{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} | {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}^{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} ^ {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}^~{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} ^~ {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}~^{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} ~^ {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}&&{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} && {b}));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x({a}||{b}));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x({a} || {b}));\n"
                 "endmodule\n"},

    // (a) op (b)
    {.input = "module m;foo bar(.x((a)+(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) + (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)-(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) - (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)*(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) * (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)/(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) / (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)%(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) % (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)&(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) & (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)|(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) | (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)^(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) ^ (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)^~(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) ^~ (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)~^(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) ~^ (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)&&(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) && (b)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a)||(b)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a) || (b)));\n"
                 "endmodule\n"},

    // a[b] op c
    {.input = "module m;foo bar(.x(a[b]+c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] + c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]-c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] - c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]*c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] * c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]/c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] / c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]%c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] % c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]&c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] & c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]|c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] | c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]^c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] ^ c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]^~c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] ^~ c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]~^c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] ~^ c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]&&c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] && c));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b]||c));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] || c));\n"
                 "endmodule\n"},

    // misc
    {.input = "module m;foo bar(.x(a[1:0]^b[2:1]));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[1:0] ^ b[2:1]));\n"
                 "endmodule\n"},

    {.input = "module m;foo bar(.x(a[b] | b[c]));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] | b[c]));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x(a[b] & b[c]));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x(a[b] & b[c]));\n"
                 "endmodule\n"},

    {.input = "module m;foo bar(.x((a^c)^(b^ ~c)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a ^ c) ^ (b ^ ~c)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a^c)^(b^~c)));endmodule",
     .expected = "module m;\n"
                 "  foo bar (.x((a ^ c) ^ (b ^~ c)));\n"
                 "endmodule\n"},
    {.input = "module m;foo bar(.x((a^{c,d})^(b^^{c,d})));endmodule",
     .expected = "module m;\n"
                 "  foo bar (\n"
                 "      .x((a ^ {c, d}) ^ (b ^ ^{c, d}))\n"
                 "  );\n"
                 "endmodule\n"},

    {// module items mixed with preprocessor conditionals and comments
     .input = "    module foo;\n"
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
     .expected = "module foo;\n"
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
    {.input = "  module bar;wire foo;reg bear;endmodule\n",
     .expected = "module bar;\n"
                 "  wire foo;\n"
                 "  reg  bear;\n"  // aligned
                 "endmodule\n"},
    {.input = " module bar;initial\nbegin a<=b . c ; end endmodule\n",
     .expected = "module bar;\n"
                 "  initial begin\n"
                 "    a <= b.c;\n"
                 "  end\n"
                 "endmodule\n"},
    {.input = "module foo ();\nif (1) begin\n$finish(\n"
              "      1    ); $finish    ();\n  end\nendmodule",
     .expected = "module foo ();\n"
                 "  if (1) begin\n"
                 "    $finish(1);\n"
                 "    $finish();\n"
                 "  end\n"
                 "endmodule\n"},
    {.input =
         "  module bar;for(genvar i = 0 ; i<N ; ++ i  ) begin end endmodule\n",
     .expected = "module bar;\n"
                 "  for (genvar i = 0; i < N; ++i) begin\n"
                 "  end\n"
                 "endmodule\n"},
    {.input = "  module bar;for(genvar i = 0 ; i!=N ; i ++  ) begin "
              "foo f;end endmodule\n",
     .expected = "module bar;\n"
                 "  for (genvar i = 0; i != N; i++) begin\n"
                 "    foo f;\n"
                 "  end\n"
                 "endmodule\n"},
    {
        .input = "module block_generate;\n"
                 "`ASSERT(blah)\n"
                 "generate endgenerate endmodule\n",
        .expected = "module block_generate;\n"
                    "  `ASSERT(blah)\n"
                    "  generate\n"
                    "  endgenerate\n"
                    "endmodule\n",
    },
    {
        .input = "module conditional_generate;\n"
                 "if(foo)  ; \t"  // null action
                 "endmodule\n",
        .expected = "module conditional_generate;\n"
                    "  if (foo);\n"
                    "endmodule\n",
    },
    {
        .input = "module conditional_generate;\n"
                 "if(foo[a*b+c])  ; \t"  // null action
                 "endmodule\n",
        .expected =
            "module conditional_generate;\n"
            "  if (foo[a*b+c]);\n"  // allow compact expressions inside []
            "endmodule\n",
    },
    {
        .input = "module conditional_generate;\n"
                 "if(foo)begin\n"
                 "`ASSERT()\n"
                 "`COVER()\n"
                 " end\n"
                 "endmodule\n",
        .expected = "module conditional_generate;\n"
                    "  if (foo) begin\n"
                    "    `ASSERT()\n"
                    "    `COVER()\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module conditional_generate;\n"
                 "`ASSERT()\n"
                 "if(foo)begin\n"
                 " end\n"
                 "`COVER()\n"
                 "endmodule\n",
        .expected = "module conditional_generate;\n"
                    "  `ASSERT()\n"
                    "  if (foo) begin\n"
                    "  end\n"
                    "  `COVER()\n"
                    "endmodule\n",
    },
    {
        .input = "module conditional_generate;\n"
                 "if(foo)begin\n"
                 "           // comment1\n"
                 " // comment2\n"
                 " end\n"
                 "endmodule\n",
        .expected = "module conditional_generate;\n"
                    "  if (foo) begin\n"
                    "    // comment1\n"
                    "    // comment2\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;"
                 "for(genvar i=0; ;)\n; "   // null generate statement
                 "for(genvar j=0 ;; )\n; "  // null generate statement
                 "endmodule",
        .expected = "module m;\n"
                    "  for (genvar i = 0;;);\n"
                    "  for (genvar j = 0;;);\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;"
                 "for (genvar f = 0; f < N; f++) begin "
                 "assign x = y; assign y = z;"
                 "end endmodule",
        .expected = "module m;\n"
                    "  for (genvar f = 0; f < N; f++) begin\n"
                    "    assign x = y;\n"
                    "    assign y = z;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        // standalone genvar statement
        .input = "module m ;"
                 "genvar f;"
                 "for(f=0; f<N; f ++ )begin "
                 "end endmodule",
        .expected = "module m;\n"
                    "  genvar f;\n"
                    "  for (f = 0; f < N; f++) begin\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        // multiple arguments to genvar statement
        .input = "module m ;"
                 "genvar f, g;"
                 "for(f=0; f<N; f ++ )begin "
                 "end for(g=N; g>0; g -- )begin "
                 "end endmodule",
        .expected = "module m;\n"
                    "  genvar f, g;\n"
                    "  for (f = 0; f < N; f++) begin\n"
                    "  end\n"
                    "  for (g = N; g > 0; g--) begin\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        // multiple genvar statements
        .input = "module m ;"
                 "genvar f;"
                 "genvar g;"
                 "for(f=0; f<N; f ++ )begin "
                 "end for(g=N; g>0; g -- )begin "
                 "end endmodule",
        .expected = "module m;\n"
                    "  genvar f;\n"
                    "  genvar g;\n"
                    "  for (f = 0; f < N; f++) begin\n"
                    "  end\n"
                    "  for (g = N; g > 0; g--) begin\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module event_control ;"
                 "always@ ( posedge   clk )z<=y;"
                 "endmodule\n",
        .expected = "module event_control;\n"
                    "  always @(posedge clk) z <= y;\n"
                    "endmodule\n",
    },
    {
        .input = "module always_if ;"
                 "always@ ( posedge   clk ) if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @(posedge clk)\n"  // doesn't fit
                    "    if (expr)\n"
                    "      z <= y;\n"
                    "endmodule\n",
    },
    {
        .input = "module always_if ;"
                 "always@*  if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @* if (expr) z <= y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else ;"
                 "always@*  if (expr) z<=y; else g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"  // fits
                    "    else g <= 0;\n"       // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else_if;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if_else ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
                 "endmodule\n",
        .expected = "module always_if_else_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "    else h <= 1;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(b,  c)"
                 "  for (;;)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(b, c) for (;;) s = y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  for (i=0;i<k;++i)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    for (i = 0; i < k; ++i)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  repeat (jj+kk)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    repeat (jj + kk)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  foreach(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    foreach (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  while(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    while (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  do s=y;while(jj[kk]);\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    do\n"
                    "      s = y;\n"
                    "    while (jj[kk]);\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)  \n"
                 "  case(jj)\tS:s = y;endcase\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    case (jj)\n"
                    "      S: s = y;\n"
                    "    endcase\n"
                    "endmodule\n",
    },

    {
        .input = "module always_if ;"
                 "always@ ( posedge   clk ) if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @(posedge clk)\n"  // doesn't fit
                    "    if (expr)\n"
                    "      z <= y;\n"
                    "endmodule\n",
    },
    {
        .input = "module always_if ;"
                 "always@*  if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @* if (expr) z <= y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else ;"
                 "always@*  if (expr) z<=y; else g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"  // fits
                    "    else g <= 0;\n"       // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else_if;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if_else ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
                 "endmodule\n",
        .expected = "module always_if_else_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "    else h <= 1;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(b,  c)"
                 "  for (;;)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(b, c) for (;;) s = y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  for (i=0;i<k;++i)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    for (i = 0; i < k; ++i)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  repeat (jj+kk)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    repeat (jj + kk)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  foreach(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    foreach (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  while(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    while (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  do s=y;while(jj[kk]);\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    do\n"
                    "      s = y;\n"
                    "    while (jj[kk]);\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)  \n"
                 "  case(jj)\tS:s = y;endcase\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    case (jj)\n"
                    "      S: s = y;\n"
                    "    endcase\n"
                    "endmodule\n",
    },

    {
        .input = "module always_if ;"
                 "always@ ( posedge   clk ) if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @(posedge clk)\n"  // doesn't fit
                    "    if (expr)\n"
                    "      z <= y;\n"
                    "endmodule\n",
    },
    {
        .input = "module always_if ;"
                 "always@*  if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @* if (expr) z <= y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else ;"
                 "always@*  if (expr) z<=y; else g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"  // fits
                    "    else g <= 0;\n"       // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else_if;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if_else ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
                 "endmodule\n",
        .expected = "module always_if_else_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "    else h <= 1;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(b,  c)"
                 "  for (;;)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(b, c) for (;;) s = y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  for (i=0;i<k;++i)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    for (i = 0; i < k; ++i)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  repeat (jj+kk)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    repeat (jj + kk)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  foreach(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    foreach (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  while(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    while (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  do s=y;while(jj[kk]);\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    do\n"
                    "      s = y;\n"
                    "    while (jj[kk]);\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)  \n"
                 "  case(jj)\tS:s = y;endcase\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    case (jj)\n"
                    "      S: s = y;\n"
                    "    endcase\n"
                    "endmodule\n",
    },

    {
        .input = "module always_if ;"
                 "always@ ( posedge   clk ) if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @(posedge clk)\n"  // doesn't fit
                    "    if (expr)\n"
                    "      z <= y;\n"
                    "endmodule\n",
    },
    {
        .input = "module always_if ;"
                 "always@*  if (expr) z<=y;"
                 "endmodule\n",
        .expected = "module always_if;\n"
                    "  always @* if (expr) z <= y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else ;"
                 "always@*  if (expr) z<=y; else g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"  // fits
                    "    else g <= 0;\n"       // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;"
                 "endmodule\n",
        .expected = "module always_if_else_if;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module \talways_if_else_if_else ;"
                 "always@*  if (expr) z<=y; else if (w) g<=0;else h<=1;"
                 "endmodule\n",
        .expected = "module always_if_else_if_else;\n"
                    "  always @*\n"
                    "    if (expr) z <= y;\n"    // fits
                    "    else if (w) g <= 0;\n"  // fits
                    "    else h <= 1;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(b,  c)"
                 "  for (;;)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(b, c) for (;;) s = y;\n"  // fits
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  for (i=0;i<k;++i)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    for (i = 0; i < k; ++i)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  repeat (jj+kk)\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    repeat (jj + kk)\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  foreach(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    foreach (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  while(jj[kk])\ts = y;"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    while (jj[kk])\n"
                    "      s = y;\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)"
                 "  do s=y;while(jj[kk]);\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    do\n"
                    "      s = y;\n"
                    "    while (jj[kk]);\n"
                    "endmodule\n",
    },
    {
        .input = "module m;\n"
                 "always @(posedge clk)  \n"
                 "  case(jj)\tS:s = y;endcase\t"
                 "endmodule",
        .expected = "module m;\n"
                    "  always @(posedge clk)\n"
                    "    case (jj)\n"
                    "      S: s = y;\n"
                    "    endcase\n"
                    "endmodule\n",
    },

    {
        // begin/end with labels
        .input = "module m ;initial  begin:yyy\tend:yyy endmodule",
        .expected = "module m;\n"
                    "  initial begin : yyy\n"
                    "  end : yyy\n"
                    "endmodule\n",
    },
    {
        // conditional generate begin/end with labels
        .input = "module m ;if\n( 1)  begin:yyy\tend:yyy endmodule",
        .expected = "module m;\n"
                    "  if (1) begin : yyy\n"
                    "  end : yyy\n"
                    "endmodule\n",
    },
    {
        // begin/end with labels, nested
        .input = "module m ;initial  begin:yyy if(1)begin:zzz "
                 "end:zzz\tend:yyy endmodule",
        .expected = "module m;\n"
                    "  initial begin : yyy\n"
                    "    if (1) begin : zzz\n"
                    "    end : zzz\n"
                    "  end : yyy\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;initial  begin #  1 x<=y ;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    #1 x <= y;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;initial  begin x<=y ;  y<=z;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    x <= y;\n"
                    "    y <= z;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input =
            "module m ;initial  begin # 10 x<=y ;  # 20  y<=z;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    #10 x <= y;\n"
                    "    #20 y <= z;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        // prefix increment/decrement as statement after a ')'
        .input = "module m ;function automatic void f;"
                 "if(a==8'ha0)++result; if(b==8'ha0)--result;"
                 "i++; j--; c[i]++;"
                 "endfunction endmodule",
        .expected = "module m;\n"
                    "  function automatic void f;\n"
                    "    if (a == 8'ha0) ++result;\n"
                    "    if (b == 8'ha0) --result;\n"
                    "    i++;\n"
                    "    j--;\n"
                    "    c[i]++;\n"
                    "  endfunction\n"
                    "endmodule\n",
    },
    {
        .input = "module t;initial #2 ->e1; initial #3 ->>e1[3]; "
                 "initial begin ->a; ->b; ->>c; ->>d; end endmodule",
        .expected = "module t;\n"
                    "  initial #2 ->e1;\n"
                    "  initial #3 ->>e1[3];\n"
                    "  initial begin\n"
                    "    ->a;\n"
                    "    ->b;\n"
                    "    ->>c;\n"
                    "    ->>d;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        // qualified variables
        .input = "module m ;initial  begin automatic int a; "
                 " static byte s=0;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    automatic int a;\n"
                    "    static byte   s = 0;\n"  // aligned
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;initial  begin automatic int a,b; "
                 " static byte s,t;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    automatic int a, b;\n"
                    "    static byte s, t;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;initial  begin   static byte a=1,b=0;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    static byte a = 1, b = 0;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;initial  begin   const int a=0;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    const int a = 0;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input =
            "module m ;initial  begin automatic   const int a=0;end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    automatic const int a = 0;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m ;initial  begin const  var automatic  int a=0;end "
                 "endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    const var automatic int a = 0;\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input =
            "module m ;initial  begin static byte s  ={<<{a}};end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    static byte s = {<<{a}};\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input =
            "module m ;initial  begin static int s  ={>>4{a}};end endmodule",
        .expected = "module m;\n"
                    "  initial begin\n"
                    "    static int s = {>>4{a}};\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m; final  assert   (expr ) ;endmodule",
        .expected = "module m;\n"
                    "  final assert (expr);\n"
                    "endmodule\n",
    },
    {
        .input = "module m; final  begin\tassert   (expr ) ;end  endmodule",
        .expected = "module m;\n"
                    "  final begin\n"
                    "    assert (expr);\n"
                    "  end\n"
                    "endmodule\n",
    },
    {
        .input = "module m; final  assume   (expr ) ;endmodule",
        .expected = "module m;\n"
                    "  final assume (expr);\n"
                    "endmodule\n",
    },
    {
        .input = "module m; final  cover   (expr ) ;endmodule",
        .expected = "module m;\n"
                    "  final cover (expr);\n"
                    "endmodule\n",
    },
    {
        // two consecutive clocking declarations in modules
        .input = " module mcd ; "
                 "clocking   cb @( posedge clk);\t\tendclocking "
                 "clocking cb2   @ (posedge  clk\n); endclocking endmodule",
        .expected = "module mcd;\n"
                    "  clocking cb @(posedge clk);\n"
                    "  endclocking\n"
                    "  clocking cb2 @(posedge clk);\n"
                    "  endclocking\n"
                    "endmodule\n",
    },
    {
        // two consecutive clocking declarations in modules, with end labels
        .input =
            " module mcd ; "
            "clocking   cb @( posedge clk);\t\tendclocking:  cb "
            "clocking cb2   @ (posedge  clk\n); endclocking   :cb2 endmodule",
        .expected = "module mcd;\n"
                    "  clocking cb @(posedge clk);\n"
                    "  endclocking : cb\n"
                    "  clocking cb2 @(posedge clk);\n"
                    "  endclocking : cb2\n"
                    "endmodule\n",
    },
    {
        // clocking declarations with ports in modules
        .input =
            " module mcd ; "
            "clocking cb   @ (posedge  clk\n); input a; output b; endclocking "
            "endmodule",
        .expected = "module mcd;\n"
                    "  clocking cb @(posedge clk);\n"
                    "    input a;\n"
                    "    output b;\n"
                    "  endclocking\n"
                    "endmodule\n",
    },
    {
        // DPI import declarations in modules
        .input = "module mdi;"
                 "import   \"DPI-C\" function  int add(\n) ;"
                 "import \"DPI-C\"\t\tfunction int\nsleep( input int secs );"
                 "import \"DPI-C\"\t\tfunction int\nwake( input int secs, "
                 "output bit "
                 "[2:0] z);"
                 "endmodule",
        .expected = "module mdi;\n"
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
        .input = "module m;"
                 "export \"DPI-C\" function get;"
                 "export \"DPI-C\" function mhpmcounter_get;\n"
                 "export \"DPI-C\"\t\tfunction int\nwake( input int secs, "
                 "output bit "
                 "[2:0] z);"
                 "endmodule",
        .expected =
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
     .input = "import \"DPI-C\" context function void foo(\n"
              "  input bit first,\n"
              "  // c3\n"
              "  // c3+\n"
              "  input bit second\n"
              ");\n",
     .expected = "import \"DPI-C\" context\n"
                 "    function void foo(\n"
                 "  input bit first,\n"
                 "  // c3\n"
                 "  // c3+\n"
                 "  input bit second\n"
                 ");\n"},
    {.input = "export \"\" t;\n", .expected = "export \"\" t;\n"},
    {.input = "import \"DPI-C\" context function void func(input bit impl_i,"
              "input bit op_i,"
              "input bit [5:0] mode_i,"
              "input bit [3:0][31:0] iv_i,"
              "input bit [2:0] key_len_i,"
              "input bit [7:0][31:0] key_i,"
              "input bit [7:0] data_i[],"
              "output bit [7:0] data_o[]);",
     .expected = "import \"DPI-C\" context\n"
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
     .input =
         "module m; initial begin #10 $display(\"foo\"); $display(\"bar\");"
         "end endmodule",
     .expected = "module m;\n"
                 "  initial begin\n"
                 "    #10 $display(\"foo\");\n"
                 "    $display(\"bar\");\n"
                 "  end\n"
                 "endmodule\n"},
    {// module with system task call
     .input = "module m; initial begin #10 $display; $display;"
              "end endmodule",
     .expected = "module m;\n"
                 "  initial begin\n"
                 "    #10 $display;\n"
                 "    $display;\n"
                 "  end\n"
                 "endmodule\n"},

};

TEST(FormatterEndToEndTest, ModuleFormatterTestCases) {
  RunFormatterTestCases40(kModuleFormatterTestCases);
}

TEST(FormatterEndToEndTest, TernaryInsideSubscriptExpression_issue2597) {
  static constexpr FormatterTestCase kTestCases[] = {
      // Outside subscript
      {.input = "module foo ();\n"
                "assign a = b > 1'h0 ? 1'h0 : c;\n"
                "endmodule\n",

       .expected = "module foo ();\n"
                   "  assign a = b > 1'h0 ? 1'h0 : c;\n"
                   "endmodule\n"},

      // Inside subscript.
      {.input = "module foo ();\n"
                "assign a = some_array[b > 1'h0 ? 1'h0 : c];\n"
                "endmodule\n",

       .expected = "module foo ();\n"
                   "  assign a = some_array[b > 1'h0 ? 1'h0 : c];\n"
                   "endmodule\n"},
  };

  FormatStyle style;
  style.indentation_spaces = 2;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
