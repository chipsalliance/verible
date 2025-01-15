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

// Verilog parser unit tests

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "verible/common/parser/bison-parser-common.h"
#include "verible/common/parser/parser-test-util.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/parser-verifier.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/analysis/verilog-excerpt-parse.h"
#include "verible/verilog/parser/verilog-token-enum.h"
#include "verible/verilog/preprocessor/verilog-preprocess.h"

namespace verilog {

using ParserTestData = verible::TokenInfoTestData;

using ParserTestCaseArray = std::initializer_list<const char *>;

static constexpr VerilogPreprocess::Config kDefaultPreprocess;

// No syntax tree expected from these inputs.
static constexpr ParserTestCaseArray kEmptyTests = {
    "", "    ", "\t\t\t", "\n\n", "// comment\n", "/* comment */\n",
};

static constexpr ParserTestCaseArray kPreprocessorTests = {
    "`define TRUTH\n",        // definition without value
    "`define FOO BAR-1\n",    // definition with value
    "`include \"foo.svh\"\n"  // include directive
    "`include <foo.svh>\n"    // include directive
    "`ifndef SANITY\n"
    "`define SANITY\n"
    "`endif\n",
    "`include `EXPAND_TO_STRING\n"  // include directive
    "`include `EXPAND_TO_PATH  // path\n"
    "`define SANITY 1+1\n"
    "`ifdef INSANITY\n"
    "`undef INSANITY\n"  // undefine
    "`define INSANITY\n"
    "`endif\n",
    "`ifdef INSANITY\n"              // definition also in else clause
    "`define INSANITY // comment\n"  // definition with comment
    "`else\n"
    "`define SANITY 1\n"
    "`endif\n",
    "`define INCEPTION(a, b, c)\n",          // with parameters, no body
    "`define INCEPTION(a, b, c) (a*b-c)\n",  // with parameters, body
    "`define INCEPTION(a, b, c) \\\n"        // with line-conitnuation
    "  (a*b-c)\n",
    "`define INCEPTION(xyz) \\\n"  // definition that defines
    "  `define DEEPER (xyz)\n",
    "`define LONG_MACRO(\n"
    "    a, b, c)\n",  // parameters span multiple lines
    "`define LONG_MACRO(\n"
    "    a,\n"  // parameters span multiple lines without continuation
    "    b, c) text goes here\n",
    "`define LONG_MACRO(\n"
    "    a,\n"  // parameters span multiple lines without continuation
    "    b\n"
    ", c\n"
    ") \\\n"
    "more text c, b, a \\\n"
    "blah blah macro ends here\n",
    "`define LONG_MACRO(\n"
    "    a, b=2, c=42) \\\n"  // parameters with defaults
    "a + b /c +345\n",
    "`define LONG_MACRO(\n"
    "    a, b=\"(3,2)\", c=(3,2)) \\\n"  // parameters with defaults
    "a + b /c +345\n",
    // escaped identifier macro call argument
    "`FOO(\\BAR )\n",                //
    "`FOO(\\BAR\t)\n",               //
    "`FOO(\\BAR\n)\n",               //
    "`FOO(\\BAR.BAZ )\n",            //
    "`FOO(\\BAR , \\BAZ )\n",        //
    "`WE(`MUST(`GO(\\deeper )))\n",  //
    "`MACRO()\n",                    //
    "`MACRO(123badid)\n",  // call arg contains a lexical error, but remains
                           // unlexed
    "`MACRO(`)\n",   // call arg contains a lexical error, but remains unlexed
    "`MACRO(` )\n",  // call arg contains a lexical error, but remains unlexed
    "`MACRO(`DEEPER(`))\n",  // call arg contains a lexical error, but remains
                             // unlexed
    "`c(;d());\n"            // ";d()" remains unlexed
};

// Make sure line continuations, newlines and spaces get filtered out
static constexpr ParserTestCaseArray kLexerFilterTests = {
    "parameter \tfoo =\t\t0;",
    "parameter\n\nfoo =\n 0;",
    "parameter \\\nfoo =\\\n 0;",
    "//comment with line-continuation\\\n",
    ("//comment1 with line-continuation\\\n"
     "//comment2 with line-continuation\\\n"),
};

// Numbers are parsed as ([width], [signed]base, digits)
static constexpr ParserTestCaseArray kNumberTests = {
    "parameter foo = 0;",
    "parameter int foo = 0;",
    "parameter int foo = '0;",
    "parameter int foo = '1;",
    // binary
    "parameter int foo = 'b0;",
    "parameter int foo = 'b 0;",
    "parameter int foo = 1'b0;",
    "parameter int foo = 1'B0;",
    "parameter int foo = 1 'b 0;",
    "parameter int foo = 1'b1;",
    "parameter int foo = 1'bx;",
    "parameter int foo = 1 'b x;",
    "parameter int foo = 1'bz;",
    "parameter int foo = 4'bxxxx;",
    "parameter int foo = 4'bzzzz;",
    "parameter int foo = 4'sb1111;",
    "parameter int foo = 4'Sb0000;",
    "parameter int foo = 16'b`DIGITS;",
    "parameter int foo = 16'b`DIGITS();",
    "parameter int foo = 16'b`DIGITS(bar);",
    "parameter int foo = 16'b`DIGITS(bar, baz);",
    "parameter int foo = 16 'b `DIGITS;",
    "parameter int foo = 16 'b `DIGITS();",
    "parameter int foo = `WIDTH'b`DIGITS;",
    // decimal
    "parameter int foo = 'd0;",
    "parameter int foo = 'd 0;",
    "parameter int foo = 32'd1;",
    "parameter int foo = 32'D1;",
    "parameter int foo = 32 'd 1;",
    "parameter int foo = 32'sd1;",
    "parameter int foo = 32'Sd1;",
    "parameter int foo = 32'dx;",
    "parameter int foo = 32'dx_;",
    "parameter int foo = 32'dx__;",
    "parameter int foo = 32'dX;",
    "parameter int foo = 32'dz;",
    "parameter int foo = 32'dZ;",
    "parameter int foo = 32'd`DIGITS;",
    "parameter int foo = 32 'd `DIGITS;",
    "parameter int foo = 32'd`DIGITS();",
    "parameter int foo = 32'd`DIGITS(bar);",
    "parameter int foo = `WIDTH'd`DIGITS;",
    // octal
    "parameter int foo = 'o0;",
    "parameter int foo = 'o 0;",
    "parameter int foo = 32'o7;",
    "parameter int foo = 32'o7_7_7;",
    "parameter int foo = 32'O7;",
    "parameter int foo = 32 'o 7;",
    "parameter int foo = 32'so7;",
    "parameter int foo = 32'So7;",
    "parameter int foo = 32'oxxx;",
    "parameter int foo = 32'oXX;",
    "parameter int foo = 32'ozz;",
    "parameter int foo = 32'oZZ;",
    "parameter int foo = 32'o`DIGITS;",
    "parameter int foo = 32 'o `DIGITS;",
    "parameter int foo = 32'o`DIGITS();",
    "parameter int foo = 32'o`DIGITS(bar);",
    "parameter int foo = `WIDTH'o`DIGITS;",
    // hexadecimal
    "parameter int foo = 'h0;",
    "parameter int foo = 'h 0;",
    "parameter int foo = 32'h7;",
    "parameter int foo = 32'H7fFF;",
    "parameter int foo = 32'H_7f_FF_;",
    "parameter int foo = 32'hdeadbeef;",
    "parameter int foo = 32'hFEEDFACE;",
    "parameter int foo = 32 'h 7;",
    "parameter int foo = 32'sh7;",
    "parameter int foo = 32'Sh7;",
    "parameter int foo = 32'hxxx;",
    "parameter int foo = 32'hXX;",
    "parameter int foo = 32'hzz;",
    "parameter int foo = 32'hZZ;",
    "parameter int foo = 32'h`DIGITS;",
    "parameter int foo = 32 'h `DIGITS;",
    "parameter int foo = 32'h`DIGITS();",
    "parameter int foo = 32'h`DIGITS(bar);",
    "parameter int foo = `WIDTH'h`DIGITS;",
};

// classes are a System Verilog construct
static constexpr ParserTestCaseArray kClassTests = {
    "class semicolon_classy; ; ;;; ; ; ;endclass",
    "class Foo; endclass",
    "class static Foo; endclass",
    "class automatic Foo; endclass",
    "virtual class Foo; endclass",
    "virtual class automatic Foo; endclass",
    "class Foo extends Bar; endclass",
    "class Foo extends Package::Bar; endclass",
    "class Foo extends Bar #(x,y,z); endclass",
    "class Foo #(int N);\n"  // parameter without default value
    "endclass",
    "class Foo #(int N, P);\n"
    "endclass",
    "class Foo #(int N, int P);\n"
    "endclass",
    "class Foo #(int N=1, int P=2) extends Bar #(x,y,z);\n"
    "endclass",
    "class Foo #(int W=8, type Int=int) extends Bar #(x,y,z);\n"
    "endclass",
    "class Foo #(T=int);\n"  // default type
    "endclass",
    "class Foo #(type KeyType=int, Int=int);\n"  // default type
    "endclass",
    "class Foo #(IFType=virtual x_if);\n"  // interface type
    "endclass",
    "class Foo #(type IFType=virtual x_if);\n"  // interface type
    "endclass",
    "class Foo #(type IFType=virtual interface x_if);\n"
    "endclass",
    "class Foo extends Package::Bar #(x,y,z); endclass",
    "class Foo extends Package::Bar #(.v1(x),.v2(y)); endclass",
    "class Foo extends Package::Bar #(x,y,z); endclass",
    "class Foo extends Package::Bar #(.v1(x),.v2(y)); endclass",
    "class Foo implements Bar; endclass",
    "class Foo implements Bar, Blah, Baz; endclass",
    "class Foo implements Package::Bar; endclass",
    "class Foo implements Bar#(N); endclass",
    "class Foo implements Package::Bar#(1, 2); endclass",
    "class Foo extends Base implements Bar; endclass",
    "class Foo extends Base implements Pkg::Bar, Baz; endclass",
    "class Foo;\n"
    "integer size;\n"
    "function new (integer size);\n"
    "  begin\n"
    "    this.size = size;\n"
    "  end\n"
    "endfunction\n"
    "task print();\n"
    "  begin\n"
    "    $write(\"Hello, world!\");\n"
    "  end\n"
    "endtask\n"
    "endclass",
    "typedef class myclass_fwd;",
    "class zzxx;\n"
    "extern function void set_port(analysis_port #(1) ap);\n"
    "endclass",
    "class zzxy;\n"
    "extern function void set_port(dbg_pkg::analysis_port app);\n"
    "endclass",
    "class zzyyy;\n"
    "extern function void set_port(dbg_pkg::analysis_port #(1,N) apb);\n"
    "endclass",
    "class zzxx;\n"
    "extern function automatic void set_port(int ap);\n"
    "endclass",
    // import declarations
    "class foo;\n"
    "  import fedex_pkg::box;\n"
    "  import fedex_pkg::*;\n"
    "endclass",
    "virtual class foo extends bar;\n"
    "  import fedex_pkg::box;\n"
    "  import fedex_pkg::*;\n"
    "endclass",
    // macros as class items
    "class macros_as_class_item;\n"
    " `moobar()\n"
    " `zoobar(  )\n"
    " `zootar(\n)\n"
    "endclass",
    "class macros_as_class_item;\n"
    " `moobar(,)\n"
    " `zoobar(  ,  )\n"
    " `zootar(12,)\n"
    " `zoojar(,34)\n"
    "endclass",
    "class macros_as_class_item;\n"
    " `uvm_object_registry(myclass, \"class_name\")\n"
    "endclass",
    "class macros_as_class_item;\n"
    " `uvm_object_utils(stress_seq)\n"
    " `uvm_object_registry(myclass, \"class_name\")\n"
    " `uvm_sweets(dessert)\n"
    " `non_uvm_macro(apple, `banana, \"cherry\")\n"
    "endclass",
    "class macros_as_class_item;\n"
    " `uvm_object_utils_begin(foobar)\n"
    " `uvm_object_utils(blah)\n"
    " `uvm_object_utils_end\n"  // macro-id alone treated as call
    "endclass",
    "class macros_as_class_item;\n"
    " `uvm_object_utils_begin(foobar)\n"
    " `uvm_field_int(node, UVM_DEFAULT);\n"        // with semicolon
    " `uvm_field_int(foo::bar_t, UVM_DEFAULT);\n"  // with semicolon
    " `uvm_field_enum(offset, UVM_DEFAULT)\n"      // without semicolon
    " `uvm_object_utils_end\n"  // macro-id alone treated as call
    "endclass",
    "class macros_as_class_item;\n"
    " `uvm_field_utils_begin(my_class)\n"
    "   `uvm_field_int(blah1, flag1)\n"
    "   `uvm_field_real(blah2, flag2)\n"
    "   `uvm_field_enum(blah3, flag3)\n"
    "   `uvm_field_object(blah4, flag4)\n"
    "   `uvm_field_event(blah5, flag5)\n"
    "   `uvm_field_string(blah6, flag6)\n"
    "   `uvm_field_array_int(blah7, flag7)\n"
    "   `uvm_field_sarray_int(blah8, flag8)\n"
    "   `uvm_field_aa_int_string(blah9, flag9)\n"
    " `uvm_field_utils_end\n"  // macro-id alone treated as call
    "endclass",
    "class macros_as_class_item;\n"
    " `uvm_object_param_utils_begin(my_class)\n"
    "   `uvm_field_int(blah1, F1)\n"
    "   `uvm_field_real(blah2, F2)\n"
    "   `uvm_field_enum(blah3, F3)\n"
    " `uvm_object_utils_end\n"  // macro-id alone treated as call
    "endclass",
    "class macros_as_class_item;\n"
    " `uvm_component_utils(my_type)\n"
    " `uvm_component_utils_begin(my_type)\n"
    "   `uvm_field_object(blah1, F1)\n"
    "   `uvm_field_event(blah2, F2)\n"
    "   `uvm_field_string(blah3, F3)\n"
    " `uvm_component_utils_end\n"  // macro-id alone treated as call
    "endclass",
    "class macros_id_as_call;\n"
    " `uvm_new_func\n"
    " `uvm_new_func2  // comment\n"
    " `uvm_new_func3  /* comment */\n"
    "endclass",
    "class pp_as_class_item;\n"
    " `undef EVIL_MACRO\n"
    "endclass",
    // parameters and local parameters
    "class params_as_class_item;\n"
    "  parameter N = 2;\n"
    "  parameter reg P = '1;\n"
    "  localparam M = f(glb::arr[N]) + 1;\n"
    "endclass",
    "class params_as_class_item;\n"
    "  localparam M = {\"hello\", \"world\"}, X = \"spot\";\n"
    "  parameter int N = 2, P = Q(R), S = T[U];\n"
    "endclass",
    "class how_wide;\n"
    "  localparam Max_int = {$bits(int) - 1{1'b1}};\n"
    "endclass",
    "class how_wide #(type DT=int) extends uvm_sequence_item;\n"
    "  localparam Max_int = {$bits(DT) - 1{1'b1}};\n"
    "  localparam Min_int = {$bits(int) - $bits(DT){1'b1}};\n"
    "endclass",
    "class param_types_as_class_item;\n"
    "  parameter type AT;\n"
    "  parameter type BT = BrickType;\n"
    "  parameter type CT1 = Ctype1, CT2 = Ctype2;\n"
    "  localparam type GT = mypkg::GlueType, GT2;\n"
    "  localparam type HT1, HT2 = mypkg::ModuleType#(N+M);\n"
    "endclass",
    // event declaration
    "class event_calendar;\n"
    "  event birthday;\n"
    "  event first_date, anniversary;\n"
    "  event revolution[4:0], independence[2:0];\n"
    "endclass",
    // associative array declaration
    "class Driver;\n"
    "  Packet pNP [*];\n"
    "  Packet pNP1 [* ];\n"
    "  Packet pNP2 [ *];\n"
    "  Packet pNP3 [ * ];\n"
    "endclass",
    // class property declarations
    "class c;\n"
    "  foo bar;\n"
    "endclass\n",
    "class c;\n"
    "  const foo bar;\n"
    "endclass\n",
    "class c;\n"
    "  protected int count;\n"
    "endclass\n",
    "class c;\n"
    "  foo #(.baz) bar;\n"
    "endclass\n",
    "class c;\n"
    "  foo #(.baz(bah)) bar;\n"
    "endclass\n",
    "class c;\n"
    "  foo #(.baz) bar1, bar2;\n"
    "endclass\n",
    "class Driver;\n"
    "  data_type_or_module_type foo1;\n"
    "  data_type_or_module_type foo2 = 1'b1;\n"
    "  data_type_or_module_type foo3, foo4;\n"
    "  data_type_or_module_type foo5 = 5, foo6 = 6;\n"
    "endclass",
    "class fields_with_modifiers;\n"
    "  const data_type_or_module_type foo1 = 4'hf;\n"
    "  static data_type_or_module_type foo3, foo4;\n"
    "endclass",
    "class fields_with_modifiers;\n"
    "  const static data_type_or_module_type foo1 = 4'hf;\n"
    "  static const data_type_or_module_type foo3, foo4;\n"
    "endclass",
    // preprocessor balanced class items
    "class pp_class;\n"
    "`ifdef DEBUGGER\n"  // empty
    "`endif\n"
    "endclass",
    "class pp_class;\n"
    "`ifdef DEBUGGER\n"
    "`ifdef VERBOSE\n"  // nested, empty
    "`endif\n"
    "`endif\n"
    "endclass",
    "class pp_class;\n"
    "`ifndef DEBUGGER\n"  // `ifndef
    "`endif\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`endif\n"
    "  int router_size;\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"  // single item
    "`endif\n"
    "  int router_size;\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"  // multiple items
    "  string dest_name;\n"
    "`endif\n"
    "  int router_size;\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"
    "  string dest_name;\n"
    "`else\n"  // `else empty
    "`endif\n"
    "  int router_size;\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"
    "  string dest_name;\n"
    "`elsif LAZY\n"  // `elsif empty
    "`endif\n"
    "  int router_size;\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`elsif LAZY\n"  // `elsif with multiple items
    "  string source_name;\n"
    "  string dest_name;\n"
    "`endif\n"
    "  int router_size;\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`elsif BORED\n"
    "  string source_name;\n"
    "  string dest_name;\n"
    "`elsif LAZY\n"  // second `elsif
    "`endif\n"
    "  int router_size;\n"
    "endclass",
    "class pp_class;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`elsif BORED\n"
    "`else\n"  // `else with multiple items
    "  string source_name;\n"
    "  string dest_name;\n"
    "`endif\n"
    "  int router_size;\n"
    "endclass",
};

static constexpr ParserTestCaseArray kFunctionTests = {
    "function integer add;\n"
    "input a, b;\n"
    "begin\n"
    "  add = (a+b);\n"
    "end\n"
    "endfunction",
    "function integer mini_me;\n"
    "input a, b;\n"
    "mini_me = (a<b) ? a:b;\n"
    "endfunction",
    "function myclass::numtype mini_me;\n"
    "input a, b;\n"
    "mini_me = (a<b) ? a:b;\n"
    "endfunction",
    "function mypkg::myclass::numtype mini_me;\n"
    "input a, b;\n"
    "mini_me = (a<b) ? a:b;\n"
    "endfunction",
    "function myclass::numtype #(W) mini_me;\n"
    "input a, b;\n"
    "mini_me = (a<b) ? a:b;\n"
    "endfunction",
    "function void compare;\n"
    "input int a, b;\n"
    "output bit c;\n"
    "c = (a == b);\n"
    "endfunction",
    "function void vcompare;\n"
    "input int a, b;\n"
    "output bit [N:0] c;\n"
    "c = (a ^ b);\n"
    "endfunction",
    "function void nonsense;\n"
    "input int a[3:0], b[M:0];\n"
    "output bit [N:0] c [11:0];\n"
    "c = (a ^ b);\n"
    "endfunction",
    "function integer twiddle;\n"
    "input a, b;\n"
    "`MACRO(a, b);\n"
    "endfunction",
    "function integer twiddle;\n"
    "input a, b;\n"
    "`undef MACRO\n"
    "endfunction",
    // const ref formals
    "function integer reader(const ref rw);\n"
    "endfunction",
    "function void reader(\n"
    "  const ref in_value, ref out_value\n"
    ");\n"
    "endfunction",
    // preprocessor directives in ports
    "function void twiddle(\n"
    "`ifdef ASDF\n"
    "  string a,\n"
    "`else\n"
    "  int bb,\n"
    "`endif\n"
    "  bit cc\n"
    ");\n"
    "endfunction",
    "function void processor(\n"
    "  string a,\n"
    "`ifdef ASDF\n"
    "  bit cc\n"
    "`else\n"
    "  int bb\n"
    "`endif\n"
    ");\n"
    "endfunction",
    // local variables, various qualifiers
    "function void declare_stuff;\n"
    "int a_count;\n"
    "const int b_count;\n"
    "var int c_count;\n"
    "static foo::int_t d_count;\n"
    "automatic bool e_enabled;\n"
    "const static string incantation;\n"
    "static const string curse = \"dang nabbit\";\n"
    "const var automatic string password;\n"
    "endfunction",
    // let declaration
    "function void declare_stuff;\n"
    "let Max(a,b) = (a > b) ? a : b;\n"
    "endfunction",
    // with no function_items
    "function void twiddle;\n"
    "  `MACRO(a, b);\n"
    "endfunction",
    "function void twiddle;\n"
    "  if (enable_stats) begin\n"
    "    $display(\"stuff\");\n"
    "  end\n"
    "endfunction",
    // array return types
    "function integer[1:0] twiddle;\n"
    "input integer a, b;\n"
    "endfunction",
    "function integer[1:0][3:0] twiddle;\n"
    "input integer a, b;\n"
    "endfunction",
    "function string[2:0] twiddle;\n"
    "input bit b [4:0];\n"
    "endfunction",
    "function monster[n:0] twiddle;\n"
    "input integer a[n:0];\n"
    "endfunction",
    "function scope_name::type_name[n:0] twiddle;\n"
    "input integer a[n:0];\n"
    "endfunction",
    "function scope_name::type_name #(M) [n:0] twiddle;\n"
    "input integer a[n:0];\n"
    "endfunction",
    // streaming concatenation lvalue
    "function void unpack_id(int nums);\n"
    "{>>8 {foo, bar}} = nums;\n"
    "endfunction",
    "function void unpack_id(utils_pkg::bytestream_t bytes);\n"
    "{<< byte {this.reserved, this.id}} = bytes;\n"
    "endfunction",
    // concatenation lvalue
    "module foo;\n"
    "  initial begin\n"
    "    {A, B, C} = bar;\n"
    "  end\n"
    "endmodule\n",
    // nested concatenation lvalue
    "module foo;\n"
    "  initial begin\n"
    "    {{A, B}, {C, D}} = bar;\n"
    "  end\n"
    "endmodule\n",
    // assignment pattern lvalue
    "module foo;\n"
    "  initial begin\n"
    "    '{A, B, C} = bar;\n"
    "  end\n"
    "endmodule\n",
    // assignment pattern lvalue and rvalue
    "module foo;\n"
    "  initial begin\n"
    "    '{A, B, C} = '{D, E, F};\n"
    "  end\n"
    "endmodule\n",
    // nested assignment pattern lvalue
    "module foo;\n"
    "  initial begin\n"
    "    '{'{A, B}, '{C, D}} = bar;\n"
    "  end\n"
    "endmodule\n",
    // mixed nested lvalue
    "module foo;\n"
    "  initial begin\n"
    "    '{{A, B}, {C, D}} = bar1;\n"
    "    {'{E, F}, '{G, H}} = bar2;\n"
    "  end\n"
    "endmodule\n",
    // qualified expressions
    "function void scoper;\n"
    "  a = b::c;\n"
    "  d = $unit::c;\n"
    "  e = $unit::xx::yy + zz::ww;\n"
    "endfunction",
    // cast expressions
    "function void caster(int foo);\n"
    "  ret = ($bits(mux::info_t))'(ctrl_info);\n"
    "  rdata_address[$clog2(Aspect_ratio)-1:0] =\n"
    "      $unsigned(($clog2(Aspect_ratio))'(rndx));\n"
    "  credit = ($bits(credit))'(sq_ub::chunks_per_bank);\n"
    "endfunction",
    // various index expressions
    "function void dollar(utils_pkg::bytestream_t bytes);\n"
    "  return bar[num_bytes:$];\n"
    "endfunction",
    // builtin functions
    "function int optimize();\n"
    "  return max(a, b) / min(c, d);\n"
    "endfunction",
    "function num_pkg::float64_t tangent();\n"
    "  return sin(angle) / cos(angle);\n"
    "endfunction",
    // out-of-line method (class member function) definitions
    "function void my_class::method_name();\n"
    "  idle();\n"
    "endfunction",
    "function my_class::m_name();\n"  // implicit return type
    "endfunction : m_name",
    "function return_type my_class::method_name();\n"
    "  return to_sender;\n"
    "endfunction",
    "function qualified::return_type my_class::method_name();\n"
    "  return to_sender;\n"
    "endfunction",
    "function return_type[K:0] my_class::method_name();\n"
    "  return to_sender;\n"
    "endfunction",
    // out-of-line constructor definitions
    "function my_class::new;\n"
    "endfunction",
    "function my_class::new();\n"
    "endfunction",
    "function my_class::new;\n"
    "endfunction : new",
    "function my_class::nested::new;\n"
    "endfunction",
    "function foo::new (string name, uvm_component parent);\n"
    "  super.new(name, parent);\n"
    "endfunction : new",
    // calling array-locator methods
    "function void finder;\n"
    "  a0 = b.find;\n"
    "  a1 = b.find();\n"
    "  a2 = x.b.find() with (x < 1);\n"
    "  a3 = c[j].b.find(x) with (x != 0 && y > 2);\n"
    "  a4 = x.b.find with (x+y < 1);\n"
    "  a5 = x.b. find with (x.y < 1);\n"
    "endfunction",
    "function void finder;\n"
    "  a1 = b.find_index;\n"
    "  a2 = x.b.find_index with (x < 1);\n"
    "  a3 = x.b.find_index() with (x < 1);\n"
    "  a4 = d.c[j].find_index(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b.find_first with (x.y.z);\n"
    "  a1 = b.find_first();\n"
    "  a2 = x.b.find_first(item) with (item.count > 1);\n"
    "  a3 = d.c[j].find_first(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b.find_first_index with (x.y.z);\n"
    "  a1 = b.find_first_index();\n"
    "  a2 = x.b.find_first_index(item) with (item.count > 1);\n"
    "  a3 = d.c[j].find_first_index(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b.find_last with (x.y.z);\n"
    "  a1 = b.find_last();\n"
    "  a2 = x.b.find_last(item) with (item.count > 1);\n"
    "  a3 = d.c[j].find_last(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b.find_last_index with (x.y.z);\n"
    "  a1 = b.find_last_index();\n"
    "  a2 = x.b.find_last_index(item) with (item.count > 1);\n"
    "  a3 = d.c[j].find_last_index(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b. min with (x.y.z);\n"
    "  a1 = b. min();\n"
    "  a2 = x.b.min(item) with (item.count > 1);\n"
    "  a3 = d.c[j].min(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b. max with (x.y.z);\n"
    "  a1 = b. max();\n"
    "  a2 = x.b.max(item) with (item.count > 1);\n"
    "  a3 = d.c[j].max(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b. unique with (x.y.z);\n"
    "  a1 = b. unique();\n"
    "  a2 = x.b.unique(item) with (item.count > 1);\n"
    "  a3 = d.c[j].unique(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void finder;\n"
    "  a0 = b. unique_index with (x.y.z);\n"
    "  a1 = b. unique_index();\n"
    "  a2 = x.b.unique_index(item) with (item.count > 1);\n"
    "  a3 = d.c[j].unique_index(x) with (x != z && y > 2);\n"
    "endfunction",
    "function void sorter;\n"
    "  b. sort;\n"
    "  b. sort();\n"
    "  x.b.sort with (item.count);\n"
    "  d.c[j].sort(x) with (x + 2);\n"
    "endfunction",
    "function void rsorter;\n"
    "  b. rsort;\n"
    "  b. rsort();\n"
    "  x.b.rsort with (item.count);\n"
    "  d.c[j].rsort(x) with (x + 2);\n"
    "endfunction",
    "function void reverser;\n"
    "  b. reverse;\n"
    "  b. reverse();\n"
    "endfunction",
    "function void shuffler;\n"
    "  b. shuffle;\n"
    "  b. shuffle();\n"
    "endfunction",
    "function void summer;\n"
    "  a0 = b. sum;\n"
    "  a1 = b. sum();\n"
    "  a2 = x.b.sum with (item.count);\n"
    "  a3 = d.c[j].sum(x) with (x + 2);\n"
    "endfunction",
    "function void multiplier;\n"
    "  a0 = b. product;\n"
    "  a1 = b. product();\n"
    "  a2 = x.b.product with (item.count);\n"
    "  a3 = d.c[j].product(x) with (x - 2);\n"
    "endfunction",
    "function void ander;\n"
    "  a0 = b. and;\n"
    "  a1 = b. and();\n"
    "  a2 = x.b.and with (item.predicate);\n"
    "  a3 = d.c[j].and(x) with (~x);\n"
    "endfunction",
    "function void orer;\n"
    "  a0 = b. or;\n"
    "  a1 = b. or();\n"
    "  a2 = x.b.or with (item.predicate);\n"
    "  a3 = d.c[j].or(x) with (~x);\n"
    "endfunction",
    "function void xorer;\n"
    "  a0 = b. xor;\n"
    "  a1 = b. xor();\n"
    "  a2 = x.b.xor with (item.predicate);\n"
    "  a3 = d.c[j].xor(x) with (~x);\n"
    "endfunction",
    // testing for preprocessor-balanced statements
    "function void preprocess_statement_test();\n"
    "  a0 = b.x;\n"
    "`ifdef POINTLESS\n"  // empty `ifdef
    "`endif\n"
    "  a1 = b.x;\n"
    "endfunction",
    "function void preprocess_statement_test();\n"
    "  a0 = b.x;\n"
    "`ifdef POINTLESS\n"  // and empty `else
    "`else\n"
    "`endif\n"
    "  a1 = b.x;\n"
    "endfunction",
    "function void preprocess_statement_test();\n"
    "  a0 = b.x;\n"
    "`ifdef POINTLESS\n"
    "`elsif DUMB\n"  // empty `elsif's
    "`elsif DUMBER\n"
    "`else\n"
    "`endif\n"
    "  a1 = b.x;\n"
    "endfunction",
    "function void preprocess_statement_test();\n"
    "  a0 = b.x;\n"
    "`ifdef POINTLESS\n"
    "  b0 = f(x, y, z);\n"  // one statement in clause
    "`endif\n"
    "  a1 = b.x;\n"
    "endfunction",
    "function void preprocess_statement_test();\n"
    "  a0 = b.x;\n"
    "`ifdef POINTLESS\n"
    "  b0 = f(x, y, z);\n"  // multiple statements in clause
    "  b1 = g(y, x, z);\n"
    "`endif\n"
    "  a1 = b.x;\n"
    "endfunction",
    "function void preprocess_statement_test();\n"
    "  a0 = b.x;\n"
    "`ifdef POINTLESS\n"
    "  b0 = f(x, y, z);\n"  // multiple statements in clause
    "  b1 = g(y, x, z);\n"
    "`elsif DUMB\n"
    "  c0 = pp(x, y);\n"  // multiple statements in clause
    "  c1 = qq(y, x);\n"
    "`else\n"
    "  d0 = fg(\"error\");\n"  // multiple statements in clause
    "  d1 = gf(13);\n"
    "`endif\n"
    "  a1 = b.x;\n"
    "endfunction",
    "function net_type_decls;\n"
    "  nettype shortreal analog_wire;\n"
    "endfunction\n",
    "function net_type_decls;\n"
    "  nettype foo::bar[1:0] analog_wire with fire;\n"
    "endfunction\n",
};

static constexpr ParserTestCaseArray kTaskTests = {
    "task task_a;\n"
    "endtask",
    "task static task_a;\n"
    "endtask",
    "task automatic task_a;\n"
    "endtask",
    "task flask;\n"
    "input [7:0] intake;\n"
    "output [7:0] outtake [3:0];\n"
    "endtask",
    "task convertCtoF;\n"
    "input [7:0] temp_in;\n"
    "output [7:0] temp_out;\n"
    "begin\n"
    "  temp_out = ((9/5) *temp_in) + 32;\n"
    "end\n"
    "endtask",
    // interface task
    "task intf.task1();\n"
    "endtask",
    // package import declaration
    "task task_a;\n"
    "  import tb_pkg::*;\n"
    "endtask",
    "task task_b();\n"
    "  import tb_pkg::*;\n"
    "  w_cell cell_q[$];\n"
    "endtask",
    // various port types
    "task spindle;\n"
    "input string intake;\n"
    "output string [7:0] outtake [3:0];\n"
    "endtask",
    "task spindle;\n"
    "input spkg::StringN#(N) intake;\n"
    "output spkg::StringN#(K) [7:0] outtake [3:0];\n"
    "endtask",
    "task spindle;\n"
    "const ref StringType intake;\n"         // const ref
    "ref StringType [7:0] outtake [3:0];\n"  // ref
    "endtask",
    "task t(virtual foo_if vif);\n"
    "endtask\n",
    "task t(virtual foo_if#(12) vif);\n"
    "endtask\n",
    "task t(virtual foo_if#(.W(12)) vif);\n"
    "endtask\n",
    "task t(virtual interface foo_if vif);\n"
    "endtask\n",
    "task t(ref virtual foo_if vif);\n"
    "endtask\n",
    "task t(ref virtual foo_if vifs[]);\n"
    "endtask\n",
    "task t(ref virtual foo_if vifs[N]);\n"
    "endtask\n",
    "task t(ref virtual foo_if vifs[N:M]);\n"
    "endtask\n",
    "task t(ref virtual foo_if vifs[N:M][X:Y]);\n"
    "endtask\n",
    "task t(ref virtual interface foo_if#(P,Q,R) vifs[N:M][X:Y]);\n"
    "endtask\n",
    // macro
    "task stringer;\n"
    "  `uvm_error(`gtn, \"frownie =(\")\n"  // string with balance character
    "endtask",
    "task stringer;\n"
    "  `undef ERROR\n"  // preprocessor
    "endtask",
    // local variables, various qualifiers
    "task variable_soup;\n"
    "int a_count;\n"
    "const int b_count;\n"
    "var int c_count;\n"
    "static int d_count;\n"
    "automatic bool e_enabled;\n"
    "const static string incantation;\n"
    "static const string cheer = \"Go!\";\n"
    "const var automatic string password;\n"
    "endtask",
    // example referencing globals
    "reg [7:0] tempC;\n"
    "reg [7:0] tempF;\n"
    "task convertCtoF;\n"
    "begin\n"
    "  tempF = ((9/5) *tempC) + 32;\n"
    "end\n"
    "endtask",
    "task swap;\n"
    "inout a, b;\n"
    "reg temp;\n"
    "begin\n"
    "  temp = a;\n"
    "  a = b;\n"
    "  b = temp;\n"
    "end\n"
    "endtask",
    // queue reference
    "task queue_or_not_to_queue;\n"
    "  if(item_to_queue.count < request_queue[$].count) begin\n"
    "  end\n"
    "endtask",
    "task queue_or_not_to_queue;\n"
    "  x_fifo = x_fifo[$-Line_width/2:$];\n"
    "  t_fifo[line] = t_fifo[line][0:$-Line_width/2];\n"
    "endtask",
    // expect property statements
    "task great_expectations;\n"
    "  expect ( B |-> C );\n"
    "  expect ( C |-> A ) else sleep();\n"
    "endtask",
    // case statements
    "task mysterious_case;\n"
    "input a;\n"
    "output b;\n"
    "case (a)\n"
    "1: b = 2;\n"
    "2-a: b = 3;\n"
    "default: b = 4;\n"
    "endcase\n"
    "endtask",
    "task mysterious_casex;\n"
    "input a;\n"
    "output b;\n"
    "casex (a)\n"
    "1: b = 2;\n"
    "default: b = 4;\n"
    "endcase\n"
    "endtask",
    "task mysterious_casez;\n"
    "input a;\n"
    "output b;\n"
    "casez (a)\n"
    "1: b = 2;\n"
    "default: b = 4;\n"
    "endcase\n"
    "endtask",
    "task mysterious_case;\n"
    "case (a)\n"
    "1: b = 2;\n"
    "`ifdef MOO\n"
    "2-a: b = 3;\n"
    "`endif\n"
    "`include \"more_cases.sv\"\n"  // preprocessing
    "default: b = 4;\n"
    "endcase\n"
    "endtask",
    // case-inside statement
    "task inside_case;\n"
    "case (a) inside\n"
    "  6'b00_?1??: select_next = 2'd1;\n"
    "  6'b00_?01?: select_next = 2'd2;\n"
    "  6'b00_?001: select_next = 2'd3;\n"
    "default: select_next = select;\n"
    "endcase\n"
    "endtask",
    "task inside_case;\n"
    "case (a) inside\n"
    "  1,4,7: select_next = 2'd1;\n"
    "  2,5,8: select_next = 2'd2;\n"
    "  3,6,9: select_next = 2'd3;\n"
    "default: select_next = select;\n"
    "endcase\n"
    "endtask",
    "task inside_case;\n"
    "case (a) inside\n"
    "  [1:3],10,11: select_next = 2'd1;\n"
    "  [4:6],[12:14]: select_next = 2'd2;\n"
    "  [7:9]: select_next = 2'd3;\n"
    "default: select_next = select;\n"
    "endcase\n"
    "endtask",
    "task inside_case;\n"
    "case (a) inside\n"
    "`ifdef LOO\n"
    "  [1:3],10,11: select_next = 2'd1;\n"
    "`else\n"
    "  [4:6],[12:14]: select_next = 2'd2;\n"
    "`endif\n"
    "  [7:9]: select_next = 2'd3;\n"
    "default: select_next = select;\n"
    "endcase\n"
    "endtask",
    // randcase statement
    "task mysterious_randcase;\n"
    "input a;\n"
    "output b;\n"
    "randcase\n"
    "1: b = 2;\n"
    "1: b = 4;\n"
    "endcase\n"
    "endtask",
    "task mysterious_randcase;\n"
    "randcase\n"
    "1: b = 2;\n"
    "`ifdef NONDET\n"
    "1: b = 4;\n"
    "`endif\n"
    "endcase\n"
    "endtask",
    // case-matches statement (patterns)
    "task inside_case;\n"
    "case (a) matches\n"
    "  .FooBar: select_next = 4'd0;\n"
    "  123: select_next = 4'd1;\n"
    "  123 &&& x == y: select_next = 4'd5;\n"
    "  \"abc\": select_next = 4'd2;\n"
    "  '{123, \"xyz\", .*}: select_next = 4'd3;\n"    // pattern list
    "  '{X:123, Y:456, Z:.*}: select_next = 4'd4;\n"  // member pattern list
    "  .*: select_next = 4'd9;\n"
    "  default: select_next = select;\n"
    "endcase\n"
    "endtask",
    "task inside_case;\n"
    "case (a) matches\n"
    "  .FooBar: select_next = 4'd0;\n"
    "`ifndef STUFF /* stuff */\n"
    "  123: select_next = 4'd1;\n"
    "`endif // comment\n"
    "  123 &&& x == y: select_next = 4'd5;\n"
    "  .*: select_next = 4'd9;\n"
    "`include \"more_patterns.sv\"\n"
    "  default: select_next = select;\n"
    "endcase\n"
    "endtask",
    // conditional statement
    "task looper();\n"
    "if (a && b != c.d)\n"
    "  continue;\n"
    "else if (e < f[g])\n"
    "  break;\n"
    "else\n"
    "  return j/k;\n"
    "endtask",
    // dynamic array assignments
    "task it_is_dynamic;\n"
    "  bit[W:0] m_data[] = new[1];\n"
    "  integer mi_data[][K:0] = new[W](a+b);\n"
    "endtask",
    "task more_dynamic;\n"
    "  bit[W:0] m_data[] = new[1], m_data_2[] = new[2];\n"
    "  integer mi_data[][K:0][L:0] = new[K*L](a+b), b_data = '0;\n"
    "endtask",
    "task expression_copy;\n"
    "  match_cmd = new rsrc_wr_q[src][q_to_chk[i]][0];\n"  // reference
    "endtask",
    // assertion statements
    "task automatic with_interfaces;\n"
    "  virtual blah_if blah_if_inst;\n"
    "  virtual blah_if blah_if_inst[3:0];\n"
    "  virtual blah_if blah_if_inst1, blah_if_inst2;\n"
    "  virtual interface blah_if2 blah_if2_inst;\n"
    "  virtual blah_if3 #(N, M) blah_if3_inst;\n"
    "endtask",
    // copy (class-new)
    "task copy;\n"
    "  clone = new();\n"
    "endtask",
    "task copy;\n"
    "  clone = new(\"Name\", N+1);\n"  // positional constructor args
    "endtask",
    "task copy;\n"
    "  clone = new(.name(\"Name\"), .size(N));\n"  // named constructor args
    "endtask",
    "task copy(ref seqitem clone);\n"
    "  clone = new this;\n"  // new this
    "endtask",
    "task catcat;\n"
    "agent_pkg::req exp_pkt = new({get_full_name(), \".hello\"});\n"
    "endtask",
    "task catcat;\n"
    "agent_pkg::req exp_pkt = new(get_full_name(), \".hello\");\n"  // 2 args
    "endtask",
    // assertion statements
    "task assert_me;\n"
    "  assert(0);\n"
    "  assert(epic_fail) $display(\"FAIL.\");\n"
    "  assert final (wrong && wronger) $display(\"FAIL.\");\n"
    "  assert #0 (incorrect);\n"
    "endtask",
    "task assert_me;\n"
    "  assert(value1 ==? pattern);\n"  // wildcard equivalence
    "  assert(value2 !=? pattern);\n"  // wildcard equivalence
    "endtask",
    "task assume_me;\n"
    "  assume(counter == 1);\n"
    "  assume(we_are_good) $display(\"GOOD.\");\n"
    "  assume final (right || !wrong) $display(\"might makes right.\");\n"
    "  assume #0 (state_is_valid());\n"
    "endtask",
    "task cover_me;\n"
    "  cover(num_eggs == 1);\n"
    "  cover(we_are_good) $display(\"Yup, good.\");\n"
    "  cover final (right || !wrong) $display(\"are you covered?\");\n"
    "  cover #0 (state_is_valid());\n"
    "endtask",
    "task assert_me;\n"
    "  assert(good) else bad();\n"
    "  assert(great) celebrate(); else lament();\n"
    "  assert(better) begin\n"
    "    celebrate();\n"
    "  end else begin\n"
    "    weep();\n"
    "  end\n"
    "endtask",
    "task assume_me;\n"
    "  assume(good) else bad();\n"
    "  assume(great) celebrate(); else lament();\n"
    "  assume(better) begin\n"
    "    celebrate();\n"
    "  end else begin\n"
    "    weep();\n"
    "  end\n"
    "endtask",
    // member of method
    "task printer;\n"
    "r_field().parent().child(status).name().print(stdout);\n"
    "endtask",
    "task updater;\n"
    "write_q[i].r_field.parent().update(status, UVM_FRONTDOOR);\n"
    "endtask",
    // randomize
    "task magic8ball;\n"
    "  randomize();\n"
    "  randomize(dice);\n"
    "  deck.randomize();\n"
    "  deck.randomize;\n"
    "  coin.randomize(dice[k]);\n"
    "endtask",
    "task magic8ball;\n"
    "  randomize(dice[i]);\n"  // vendor extension allows indexed reference
    "  std::randomize(coin[j]);\n"
    "endtask",
    "task magic8ball;\n"
    "  if (!coin.randomize() with {\n"
    "    r_w > 2;\n"
    "    bar() == n_y;\n"
    "  }) begin\n"
    "    $payout();\n"
    "  end\n"
    "endtask",
    "task body_unique;\n"
    "value = std::randomize();\n"
    "success = std::randomize(interrupt_id) with {\n"
    "  unique {interrupt_id};\n"  // with unique_expression
    "};\n"
    "endtask",
    "task local_ref;\n"
    "if (!foo.randomize(interrupt_id) with {\n"
    "    a.count == local::count;})\n"  // with local class qualifier
    "  `uvm_error();\n"
    "endtask",
    "task local_ref;\n"
    "if (!foo.randomize with { a.count == xyz;})\n"
    "  `uvm_error(\"oops\");\n"
    "endtask",
    // out-of-line task member definitions
    "task class_name::high_roller;\n"
    "endtask",
    "task class_name::high_roller();\n"
    "  roll_dice();\n"
    "endtask",
    "task class_name::high_roller(input bit loaded);\n"
    "  if (loaded) begin\n"
    "    roll_twice();\n"
    "  end else begin\n"
    "    roll_dice();\n"
    "  end\n"
    "endtask",
    // delay statements
    "task waiter;\n"
    "  #done_timeout;\n"  // simple delay value
    "  `uvm_fatal(`gtn, \"Timeout\");\n"
    "endtask",
    "task waiter;\n"
    "  #cfg.done_timeout;\n"  // reference
    "  `uvm_fatal(`gtn, \"Timeout\");\n"
    "endtask",
    "task waiter;\n"
    "  #mypkg::done_timeout;\n"  // package-scoped reference
    "  `uvm_fatal(`gtn, \"Timeout\");\n"
    "endtask",
    // cycle delay statements
    "task waiter;\n"
    "  `uvm_info(\"blah\");\n"
    "  ## 10;\n"
    "  ##200 deck.randomize();\n"
    "  ##Duration;\n"
    "  `uvm_info(\"done\");\n"
    "endtask",
    "task schmoo3;\n"
    "begin\n"
    "  z0 <= y;\n"           // clocking_drive or nonblocking assignment
    "  z1 <= ##value yy;\n"  // clocking_drive with cycle_delay
    "  z2 <= ##(value) xx;\n"
    "  z3 <= ##50 yy;\n"
    "  z4.dfg[0] <= ##50 ww[1].xx;\n"
    "end\n"
    "endtask",
    "task net_type_decls;\n"
    "  nettype shortreal analog_wire;\n"
    "endtask\n",
    "task net_type_decls;\n"
    "  nettype foo::bar[1:0] analog_wire with fire;\n"
    "endtask\n",
    // event trigger and logical implication expressions '->'
    "task trigger_happy;\n"
    "  ->e1;\n"
    "endtask\n",
    "task trigger_happy;\n"
    "  ;\n"  // null
    "  ->e1;\n"
    "endtask\n",
    "task trigger_happy;\n"
    "  ->e1;\n"
    "  ->e2;\n"
    "endtask\n",
    "task trigger_happy;\n"
    "  ->e1;\n"
    "  g();\n"
    "  ->e2;\n"
    "endtask\n",
    "task logical_happy;\n"
    "  a = f->g;\n"
    "endtask\n",
    "task mixed_happy;\n"
    "  ->bb;\n"
    "  a = f->g;\n"
    "  ->cc;\n"
    "endtask\n",
    "task mixed_happy;\n"
    "  a = f->g;\n"
    "  ->bb;\n"
    "  p = q->r;\n"
    "endtask\n",
    "task mixed_happy;\n"
    "  if (x)\n"
    "    ->bb;\n"
    "endtask\n",
    "task mixed_happy;\n"
    "  for (int i=0; i<N; ++i)\n"
    "    ->bb;\n"
    "endtask\n",
};

static constexpr ParserTestCaseArray kModuleTests = {
    "module modular_thing;\n"
    "endmodule",
    "module semicolon_madness;;;;;\n"
    "endmodule",
    "module automatic modular_thing;\n"
    "endmodule",
    "module modular_thing();\n"
    "endmodule",
    "module `MODULE_NAME;\n"
    "endmodule",
    "module `MODULE_NAME1;\n"
    "endmodule : `MODULE_NAME1",
    "module `MODULE_NAME2;\n"
    "endmodule : `MODULE_NAME2\n",  // with newline
    "module `MODULE_NAME3\n"
    "  (input foo);\n"
    "endmodule",
    // K&R-style ports
    "module addf (a, b, ci, s, co);\n"
    "input a, b, ci;\n"
    "output s, co;\n"
    "always @(a, b, ci)\n"
    "begin\n"
    "  s = (a^b^ci);\n"
    "  co = (a&b)|(a&ci)|(b&ci);\n"
    "end\n"
    "endmodule",
    "module addf (a, b, ci, sum, co);\n"  // 'sum' not as keyword
    "input a, b, ci;\n"
    "output sum, co;\n"
    "always @(a, b, ci)\n"
    "begin\n"
    "  sum = (a^b^ci);\n"
    "  co = (a&b)|(a&ci)|(b&ci);\n"
    "end\n"
    "endmodule",
    "module asdf (a[N:0], o);\n"
    "input [N:0] a;\n"
    "output co;\n"
    "endmodule",
    "module portlogic (a[N:0], o);\n"
    "input logic [N:0] a;\n"
    "output logic co;\n"
    "endmodule",
    "module zoom (a, co);\n"
    "input bus_type a;\n"
    "output bus_type [3:0] co;\n"
    "endmodule",
    "module zoom (a, co);\n"
    "input somepkg::bus_type a;\n"
    "output somepkg::bus_type [3:0] co;\n"
    "endmodule",
    "module zoomzoom (a, co);\n"
    "input somepkg::bus_type #(2*W) a;\n"
    "output somepkg::bus_type #(W) [3:0] co;\n"
    "endmodule",
    "module nonansi_ports_unpacked_array(a, b, c);\n"
    "output a;\n"
    "input b;\n"
    "input c[2:0];\n"
    "endmodule",
    "module nonansi_ports_unpacked_array(a, b, c);\n"
    "output a, a2, a3;\n"
    "input b, b2[1:0], b3;\n"
    "input c[2:0], c2[N:0], c3[M:N];\n"
    "endmodule",
    "module nonansi_ports(a, b, c);\n"
    "endmodule",
    "module nonansi_ports(a[7:4], b[3:0]);\n"
    "endmodule",
    "module nonansi_ports(a[7:4][1:0], b[3:0][1:0]);\n"
    "endmodule",
    "module nonansi_ports(.a(), .b(), .c());\n"
    "endmodule",
    "module nonansi_ports(.a(a), .b(b), .c(c));\n"
    "endmodule",
    "module nonansi_ports({c, d}, {e, f});\n"
    "endmodule",
    "module nonansi_ports(.a({c, d}), .b({e, f}));\n"
    "endmodule",
    "module nonansi_ports({c[3:2], d}, {e, f[1:0]});\n"
    "endmodule",
    "module nonansi_ports(.x({c[3:2], d}), .y({e, f[1:0]}));\n"
    "endmodule",
    // full port declarations
    "module addf (\n"
    "input a, b, ci,\n"
    "output s, co);\n"
    "always @(a, b, ci)\n"
    "begin\n"
    "  s = (a^b^ci);\n"
    "  co = (a&b)|(a&ci)|(b&ci);\n"
    "end\n"
    "endmodule",
    "module addf (\n"
    "input a, b, ci,\n"
    "output sum, co);\n"  // 'sum' not as a keyword
    "always @(a, b, ci)\n"
    "begin\n"
    "  sum = (a^b^ci);\n"
    "  co = (a&b)|(a&ci)|(b&ci);\n"
    "end\n"
    "endmodule",
    "module zxc (\n"
    "input [3:0] b,\n"
    "output co);\n"
    "endmodule",
    "module qwert (\n"
    "intf::in_bus [3:0] b,\n"
    "intf::out_bus co);\n"
    "endmodule",
    "module foo (interface bar_if, interface baz_if);\n"  // interface port
    "endmodule",
    "module foo (interface .fgh bar_if);\n"  // interface port
    "endmodule",
    "module tryme (\n"
    "tri1 a,\n"
    "tri0 b);\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  input a,\n"
    "`ifdef PORT_B\n"
    "  input b,\n"
    "`endif  // PORT_B\n"
    "  input c);\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  input a,\n"
    "`ifdef PORT_B\n"
    "  input b,\n"
    "`else   // PORT_B\n"  // with else clause
    "  input b2,\n"
    "`endif  // PORT_B\n"
    "  input c);\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  input a,\n"
    "`ifdef PORT_B\n"
    "  input b\n"  // no trailing comma in both clauses
    "`else   // PORT_B\n"
    "  input c\n"          // no trailing comma in both clauses
    "`endif  // PORT_B\n"  // preprocessing directive last
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`ifdef PORT_B\n"  // preprocessing directive first
    "  input b1,\n"
    "  input b2\n"
    "`else\n"
    "  input c1,\n"
    "  input c2\n"
    "`endif\n"  // preprocessing directive last
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`ifdef BLAH\n"
    "`else\n"
    "`endif\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  output foo,\n"
    "`ifdef BLAH\n"
    "    bar = 1'b1\n"  // trailing assign, no comma
    "`else\n"
    "    bar = 1'b0\n"
    "`endif\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`ifdef BLAH\n"  // non-ANSI style ports
    "  a,\n"
    "`else\n"
    "  b,\n"
    "`endif\n"
    "  c\n"
    "  );\n"
    "input a;\n"
    "input b;\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  a,\n"
    "`ifdef BLAH\n"  // non-ANSI style ports
    "  b\n"          // no trailing comma
    "`else\n"
    "  c\n"
    "`endif\n"
    "  );\n"
    "input a;\n"
    "output c;\n"
    "endmodule",
    "`define BCD b,c,d,\n"
    "module preprocessor_pain (\n"
    "  a,\n"
    "  `BCD\n"  // macro-expanded ports
    "  e);\n"
    "endmodule",
    "`define BCD(b,c,d) x,b,c,d,y,\n"
    "module preprocessor_pain (\n"
    "  a,\n"
    "  `BCD(p,q,r)\n"  // macro-expanded ports
    "  e);\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  output foo,\n"
    "`ifdef BLAH\n"
    "    bar = 1'b1,\n"  // trailing assign, with comma
    "`else\n"
    "    bar = 1'b0,\n"
    "`endif\n"
    "    input clk\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  output foo,\n"
    "`ifdef BLAH\n"
    "    bar = 1'b1,\n"  // trailing assign, with comma
    "`elsif BLAH2\n"     // multiple `elseif clauses
    "    bar = 2'b0,\n"
    "`elsif BLAH3\n"
    "    bar = `INITBAR,\n"
    "`else\n"
    "    bar = 1'b0,\n"
    "`endif\n"
    "    input clk\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  output foo,\n"
    "`ifdef BLAH\n"  // all empty clauses
    "`elsif BLAH2\n"
    "`elsif BLAH3\n"
    "`else\n"
    "`endif\n"
    "    input clk\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`include \"BLAH.svh\"\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`include <BLUH.svh>\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`define FOO BAR\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`undef FOO\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  input foo,\n"  // with comma
    "`include \"BLAH.svh\"\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "  input foo\n"  // without comma
    "`include \"BLAH.svh\"\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`include \"BLAH.svh\"\n"
    "  , output bar\n"  // without comma
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`include \"BLAH.svh\"\n"
    "  , output bar\n"  // with comma
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`ifdef V1\n"
    "`include \"BLAH_V1.svh\"\n"
    "`else\n"
    "`include \"BLAH_V2.svh\"\n"
    "`endif\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`ifdef V1\n"
    "`include \"BLAH_V1.svh\"\n"
    "`elsif V2\n"
    "`include \"BLAH_V2.svh\"\n"
    "`else\n"
    "`include \"BLAH_V3.svh\"\n"
    "`endif\n"
    "  );\n"
    "endmodule",
    "module preprocessor_pain (\n"
    "`ifndef V1\n"
    "`define FOO\n"
    "`include \"BLAH_V1.svh\"\n"
    "`endif\n"
    "  );\n"
    "endmodule",
    "module clkgen (\n"
    "  output bit clk=0, input bit run_clock,\n"
    "  int clock_phase_a, int clock_phase_b,\n"  // int port
    "  int phase_init, bit edge_init);\n"
    "endmodule",
    "module foo (\n"
    "  input wire bar);\n"  // input wire port
    "endmodule",
    "module foo (\n"
    "  input wire bar = 1);\n"  // input wire port with trailing assign
    "endmodule",
    "module foo (\n"
    "  wire bar);\n"  // wire port, no direction
    "endmodule",
    "module foo (\n"
    "  wire bar = 1);\n"  // wire port, no direction, with trailing assign
    "endmodule",
    "module foo (\n"
    "  input real bar);\n"  // real port
    "endmodule",
    "module foo (\n"
    "  input var i,\n"  // var keyword
    "  output var o);\n"
    "endmodule",
    "module foo (\n"
    "  ref int o);\n"  // ref declaration
    "endmodule",
    "module foo (\n"
    "  ref var o);\n"  // ref declaration
    "endmodule",
    "module foo (\n"
    "  ref var int o);\n"  // ref declaration
    "endmodule",
    // port connections
    "module tryme;\n"
    "foo a;\n"
    "foo b();\n"
    "foo c(x, y, z);\n"
    "foo d(.x(x), .y(y), .w(z));\n"
    "foo e(x, a, .y(y), .w(z));\n"  // positional + named arguments
    "endmodule",
    "module tryme;\n"
    "foo c(sum, product, find);\n"  // built-in method names ok as identifiers
    "foo d(.sum(sum), .product(product), .find(find));\n"
    "foo e(.unique, .unique_index(), .and());\n"
    "endmodule",
    "module blank_ports;\n"
    "s_type xyz(,,,);\n"  // blank ports
    "bus_type abc(\n"
    "  sig_A,\n"
    "  /* blah */,\n"
    "  sig_C,\n"
    "  sig_D,\n"
    "  /* blah */,\n"
    "  /* blah */\n"
    ");\n"
    "endmodule",
    "module preprocessor_love;\n"
    "foo d(\n"
    "`ifdef CONNECT_X\n"
    "  .x(x),\n"
    "`endif\n"
    "  .y(y),\n"
    "  .w(z)\n"
    ");\n"
    "endmodule",
    "module preprocessor_love;\n"
    "foo d(\n"
    "  .x(x),\n"
    "  .y(y),\n"
    "`ifdef CONNECT_X\n"
    "  .w(z)\n"
    "`else\n"
    "  .w(1'b0)\n"
    "`endif\n"
    ");\n"
    "endmodule",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    `my_ports\n"
    "  );\n"
    "endmodule\n",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    `my_ports()\n"
    "  );\n"
    "endmodule\n",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    `my_ports  // with comment\n"
    "  );\n"
    "endmodule\n",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    `my_ports()  // with comment\n"
    "  );\n"
    "endmodule\n",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    .a(a),\n"
    "    `my_ports\n"
    "  );\n"
    "endmodule\n",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    .a(a),\n"
    "    `my_ports()\n"
    "  );\n"
    "endmodule\n",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    `my_ports,\n"  // with comma, macro is interpreted as an expression
    "    .b(b)\n"
    "  );\n"
    "endmodule\n",
    "module macro_ports;\n"
    "  foo bar(\n"
    "    `my_ports(),\n"  // with comma, macro is interpreted as an expression
    "    .b(b)\n"
    "  );\n"
    "endmodule\n",
#if 0
    // TODO(b/36237582): accept macro item in ports, without trailing comma
    "module macro_ports;\n"
    "  foo bar(\n"
    "    `my_ports()\n"
    "    .b(b)\n"
    "  );\n"
    "endmodule\n",
#endif
    "module cast_with_constant_functions;\n"
    "foo dut(\n"
    "  .bus_in({brn::Num_blocks{$bits(dbg::bus_t)'(0)}}),\n"
    "  .bus_mid({brn::Num_bits{$clog2(dbg::bus_t)'(1)}}),\n"
    "  .bus_out(out));\n"
    "endmodule",
    // keyword tests
    "module keyword_identifiers;\n"
    "reg branch;  // branch is a Verilog-AMS keyword\n"
    "input from;  // from is a Verilog-AMS keyword\n"
    "wire access;  // access is a Verilog-AMS keyword\n"
    "wire exclude;  // exclude is a Verilog-AMS keyword\n"
    "Timer timer;  // timer is a Verilog-AMS keyword\n"
    "High above;  // above is a Verilog-AMS keyword (but not below)\n"
    "Bee discrete;  // discrete is a Verilog-AMS keyword\n"
    "banana split;  // split is a Verilog-AMS keyword inside connect\n"
    "split hairs;  // split is a Verilog-AMS keyword inside connect\n"
    "merged traffic;  // merged is a Verilog-AMS keyword inside connect\n"
    "Blood sample;  // sample is used in SystemVerilog coverage_event\n"
    "connect four;  // connect is a Verilog-AMS keyword\n"
    "option option;  // option is a keyword only inside covergroups\n"
    "ddt pesticide;  // Verilog-A keyword\n"
    "ddx derivative;  // Verilog-A keyword\n"
    "idt current;  // Verilog-A keyword\n"
    "idtmod current;  // Verilog-A keyword\n"
    "table salt;  // table is a keyword inside primitives\n"
    "Blackjack table;\n"
    "swimming bool;  // bool is an Icarus Verilog extension\n"
    "endmodule",
    // net declarations
    "module m; wire #(100) x = y,z=k,foo; endmodule\n",
    "module m; wire #1.5 x = y; endmodule;\n"
    "module m; wire #(100) x = y; endmodule;\n"
    "module m; wire [1:0] #1 x = y; endmodule;\n"
    "module m; wire signed foo; endmodule\n",
    "module m; wire signed [7:0] foo; endmodule\n",
    "module m; wire unsigned foo; endmodule\n",
    "module m; wire unsigned [7:0][3:0] foo; endmodule\n",
    "module m(input wire signed foo); endmodule\n",
    "module m(input wire signed [1:0] foo); endmodule\n",
    "module m(input wire unsigned foo); endmodule\n",
    "module m; wire foo_t foo; endmodule\n",
    "module m(wire foo_t foo); endmodule\n",
    "module m; wire foo_t [1:0] foo [0:2]; endmodule\n",
    "module m(wire foo_t [1:0] foo [0:3]); endmodule\n",
    "module m; wire p_pkg::foo_t foo; endmodule\n",
    "module m(wire p_pkg::foo_t foo); endmodule\n",
    "module m; wire p_pkg::foo_t [2:0] foo [0:1]; endmodule\n",
    "module m(wire p_pkg::foo_t [2:0] foo [0:2]); endmodule\n",
    "module m; wire continuous; endmodule\n",  // Regression #672
    "module m; wire infinite; endmodule\n",    // infinite also a keyword
    "module m; wire slew; endmodule\n",        // Regression #753
    "interface _if; wire p_pkg::foo_t foo; endinterface\n",
    "interface _if; wire p_pkg::foo_t [1:0] foo [0:1]; endinterface\n",
    // system task calls
    "module caller;\n"
    "initial begin\n"
    "  $warning(\"dangerous.\");\n"
    "end\n"
    "endmodule",
    "module caller;\n"
    "generate\n"
    "  if (width > 2) begin : LBL\n"
    "    $display(\"Aloha.\");\n"
    "  end : LBL\n"
    "endgenerate\n"
    "endmodule",
    "module caller;\n"
    "generate\n"
    "  if (width > 2) begin : LBL\n"
    "    $info(\"Aloha.\");\n"
    "    $error(\"Panic[%m] %s.\", `STRINGIFY(__name), \"blah\");\n"
    "  end : LBL\n"
    "endgenerate\n"
    "endmodule",
    // direct assignment
    "module addf (\n"
    "input a, input b, input ci,\n"
    "output s, output co);\n"
    "assign s = (a^b^ci);\n"
    "assign co = (a&b)|(a&ci)|(b&ci);\n"
    "endmodule",
    "module foob;\n"
    "assign s = a, y[0] = b[1], z.z = c -jkl;\n"  // multiple assignments
    "endmodule",
    "module foob;\n"
    "assign `BIT_ASSIGN_MACRO(l1, r1)\n"  // as module item
    "endmodule",
    "module foob;\n"
    "assign `BIT_ASSIGN_MACRO(l2, r2);\n"  // with semicolon
    "endmodule",
    "module foob;\n"
    "initial begin\n"
    "assign `BIT_ASSIGN_MACRO(l1, r1)\n"  // as statement item
    "end\n"
    "endmodule",
    "module foob;\n"
    "initial begin\n"
    "assign `BIT_ASSIGN_MACRO(l2, r2);\n"  // with semicolon
    "end\n"
    "endmodule",
    "module mon;\n"
    "scoreboard #(.MAX(nf::sliced * nf::depth_c))\n"
    "  blah2blah (.ivld({rdv,\n"
    "    $past(rdv, 1, 1'b1, @(posedge clk))}));\n"  // $past, event control
    "endmodule",
    "module mon;\n"
    "scoreboard blah2blah (.ivld({rdv,\n"
    "    $past(rdv,, , @(posedge clk))}));\n"  // $past, empty arguments
    "endmodule",
    "module labeled_statements;\n"
    "initial begin\n"
    "  a = 0;\n"
    "  foo: b = 0;\n"  // with label
    "end\n"
    "endmodule",
    "module labeled_single_statements;\n"
    "always_comb\n"
    "  foo_implies_bar: assert #0 (valid);\n"
    "endmodule",
    "module labeled_single_statements;\n"
    "initial\n"
    "  foo_implies_bar: assert #0 (valid);\n"
    "endmodule",
    "module labeled_single_statements;\n"
    "final\n"
    "  foo_implies_bar: assert #0 (valid);\n"
    "endmodule",
    "module labeled_statements;\n"
    "always_comb begin\n"
    "  foo_implies_bar: assert #0 (valid);\n"  // with label
    "end\n"
    "endmodule",
    "module case_statements;\n"  // case statements
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd: w = z;\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements;\n"  // unique case statements
    "always_comb begin\n"
    "  unique case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd: w = z;\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements;\n"  // unique0 case statements
    "always_comb begin\n"
    "  unique0 case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd: w = z;\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements;\n"  // unique0 casex statements
    "always_comb begin\n"
    "  unique0 casex (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd: w = z;\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements;\n"  // unique0 casez statements
    "always_comb begin\n"
    "  unique0 casez (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd: w = z;\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements;\n"
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd: begin\n"  // statement block
    "      y = f(w);\n"
    "      w = z;\n"
    "    end\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements_with_preprocessing;\n"
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd:\n"
    "`ifdef ZZZ\n"
    "      w = z;\n"
    "`endif\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements_with_preprocessing;\n"
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd:\n"
    "`ifndef ZZZ\n"
    "      w = z;\n"
    "`endif\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements_with_preprocessing;\n"
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd:\n"
    "`ifdef ZZZ\n"
    "      w = z;\n"
    "`else\n"
    "      w = q;\n"
    "`endif\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements_with_preprocessing;\n"
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd:\n"
    "`ifdef ZZZ\n"
    "      w = z;\n"
    "`elsif YYY\n"
    "      w = q;\n"
    "`endif\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements_with_preprocessing;\n"
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd:\n"
    "`ifdef ZZZ\n"
    "      w = z;\n"
    "`elsif YYY\n"
    "      w = q;\n"
    "`elsif XXX\n"
    "      w = 1;\n"
    "`else\n"
    "      w = 2;\n"
    "`endif\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    "module case_statements_with_preprocessing;\n"
    "always_comb begin\n"
    "  case (blah.blah)\n"
    "    aaa,bbb: x = y;\n"
    "    ccc,ddd:\n"
    "`ifdef ZZZ\n"  // all empty clauses
    "`elsif YYY\n"
    "`elsif XXX\n"
    "`else\n"
    "`endif\n"
    "  endcase\n"
    "end\n"
    "endmodule",
    // default clocking
    "module clock_it;\n"
    "default clocking fastclk;\n"
    "endmodule",
    "module clock_it;\n"
    "if (clocked_mode)\n"
    "  default clocking fastclk;\n"
    "endmodule",
    // default disable
    "module diss;\n"
    "default disable iff should_be_disabled;\n"
    "default disable iff (x * y < 4);\n"
    "endmodule",
    "module diss;\n"
    "if (shutdown) begin\n"
    "  default disable iff z >= 9;\n"
    "  default disable iff (x || y);\n"
    "end\n"
    "endmodule",
    // always_comb
    "module comb_stuff;\n"
    "always_comb begin\n"
    "  unique if (d1 == nf::B) begin\n"  // unique-if
    "    data = b_data;\n"
    "  end\n"
    "  else if (d1 == nf::H) begin\n"
    "    data = h_data;\n"
    "  end\n"
    "  else begin\n"
    "    data = 1'b0;\n"
    "  end\n"
    "end\n"
    "endmodule",
    // assignment expressions
    "module rooter;\n"
    "initial begin\n"
    "  $root.xx = yy.zz;\n"  // $root
    "  xx.yy = $root.zz;\n"
    "end\n"
    "endmodule",
    "module yoonary;\n"
    "initial begin\n"
    "  x = ~ ~y;\n"
    "  z = - -w;\n"
    "  a = ~^b;\n"
    "  a = ^~b;\n"
    "  a = ~|b;\n"
    "  a = ~&b;\n"
    "  c = ^~~^&d;\n"
    "end\n"
    "endmodule",
    "module schmoo (\n"
    "input [7:0] wr, input [7:0] rd, input clk,\n"
    "output wptr, output rptr);\n"
    "always @(posedge clk)\n"
    "begin\n"
    "  wptr <= wr ? (wptr == (FDEPTH-1)) ? '0 : (wptr + 1) : wptr;\n"
    "  rptr <= rd ? (rptr == (FDEPTH-1)) ? '0 : (rptr + 1) : rptr;\n"
    "end\n"
    "endmodule",
    "module schmoo2 (\n"
    "input x, input y,\n"
    "output z1, output z2, output z3, output z4, output z5);\n"
    "always @*\n"
    "begin\n"
    "  z1 <= #value y;\n"
    "  z2 <= #(value) x;\n"
    "  z3 <= #50 y;\n"
    "  z4 <= #20ps x;\n"
    "  z5 <= #(20ps) y;\n"
    "end\n"
    "endmodule",
    "module assigner;\n"
    "  initial begin\n"
    "    n1 <= @(negedge k) r1;\n"  // nonblocking assign, edge
    "  end\n"
    "endmodule\n",
    "module assigner;\n"
    "  initial begin\n"
    "    n1 = @(negedge k) r1;\n"  // blocking assign, edge
    "  end\n"
    "endmodule\n",
    "module assigner;\n"
    "  initial begin\n"
    "    n1 <= @foo r1;\n"  // nonblocking assign, event control
    "  end\n"
    "endmodule\n",
    "module assigner;\n"
    "  initial begin\n"
    "    n2 = @bar r1;\n"  // blocking assign, event control
    "  end\n"
    "endmodule\n",
    "module assigner;\n"
    "  initial begin\n"
    "    n1 <= repeat(k) @foo r1;\n"  // nonblocking assign, repeat control
    "  end\n"
    "endmodule\n",
    "module assigner;\n"
    "  initial begin\n"
    "    n2 = repeat(k) @bar r1;\n"  // blocking assign, repeat control
    "  end\n"
    "endmodule\n",
    "module triggerer;\n"
    "  initial\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
    "module triggerer;\n"
    "  final\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
    "module triggerer;\n"
    "  always\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
    "module triggerer;\n"
    "  always_ff\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
    "module triggerer;\n"
    "  always_comb\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
    "module triggerer;\n"
    "  always_latch\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
#if 0
    // TODO(b/171362636): lexical context needs to understand event_control
    "module triggerer;\n"
    "  always @*\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
    "module triggerer;\n"
    "  always @(posedge clk)\n"
    "    ->a;\n"  // blocking event trigger
    "endmodule\n",
#endif
    "module triggerer;\n"
    "  always @(posedge clk) begin\n"
    "    ->a;\n"  // blocking event trigger
    "  end\n"
    "endmodule\n",
    "module triggerer;\n"
    "  always @* begin\n"
    "    ->a;\n"  // blocking event trigger
    "    x = 0;\n"
    "  end\n"
    "endmodule\n",
    "module triggerer;\n"
    "  always @* begin\n"
    "    ->a.b[1].c;\n"  // blocking event trigger
    "    x = 0;\n"
    "  end\n"
    "endmodule\n",
    "module triggerer;\n"
    "  always @* begin\n"
    "    ->>c;\n"  // nonblocking event trigger
    "  end\n"
    "endmodule\n",
    "module triggerer;\n"
    "  always @* begin\n"
    "    ->> #5 c;\n"  // nonblocking event trigger, delayed
    "  end\n"
    "endmodule\n",
    "module triggerer;\n"
    "  always @* begin\n"
    "    ->> @(posedge y) c;\n"  // nonblocking event trigger, edge
    "  end\n"
    "endmodule\n",
    "module triggerer;\n"
    "  always @* begin\n"
    "    ->> repeat(2) @foo c;\n"  // nonblocking event trigger, repeat
    "  end\n"
    "endmodule\n",
    // streaming concatenations with macros
    "module streaming_cats;\n"
    "assign s1 = {>>`BAR};\n"
    "assign s2 = {>>`BAR(foo)};\n"
    "assign s3 = {>>16 `BAR};\n"
    "assign s4 = {>>32 `BAR(foo)};\n"
    "assign s5 = {>>`WIDTH `BAR};\n"
    "assign s6 = {>>`WIDTH `BAR(foo)};\n"
    "endmodule",
    // assignment using logical implication expressions
    "module logical_implication;\n"
    "logic a, b, c;\n"
    "assign a = b -> c;\n"
    "endmodule",
    "module logical_implication;\n"
    "  initial\n"
    "    a = b -> c;\n"
    "endmodule",
    "module logical_implication;\n"
    "  initial begin\n"
    "    a = b -> c;\n"
    "  end\n"
    "endmodule",
    "module logical_implication;\n"
    "  initial\n"
    "    a = (b -> c);\n"
    "endmodule",
    "module logical_implication;\n"
    "  initial\n"
    "    a = (b -> c) & (d -> e);\n"
    "endmodule",
    "module logical_implication;\n"
    "  initial\n"
    "    {x, y} = {b -> c, d -> e};\n"
    "endmodule",
    "module logical_implication;\n"
    "  final\n"
    "    a = (b -> c) & (d -> e);\n"
    "endmodule",
    "module logical_implication;\n"
    "  final begin\n"
    "    a = (b -> c) & (d -> e);\n"
    "  end\n"
    "endmodule",
    "module logical_implication;\n"
    "  always @* begin\n"
    "    a <= b -> c;\n"
    "  end\n"
    "endmodule",
    "module logical_implication;\n"
    "  always @* begin\n"
    "    a <= b -> c || (f->g);\n"
    "  end\n"
    "endmodule",
    // if-else
    "module ifelsey ();\n"
    "  if (i==0) begin\n"
    "  end\n"
    "endmodule",
    "module ifelsey ();\n"
    "  if (i.f==0) begin\n"
    "  end else begin\n"
    "  end\n"
    "endmodule",
    "module ifelsey ();\n"
    "  if (func()) begin\n"
    "  end else begin\n"
    "  end\n"
    "endmodule",
    "module ifelsey ();\n"
    "  if (obj.func()) begin\n"
    "  end else begin\n"
    "  end\n"
    "endmodule",
    "module ifelsey ();\n"
    "  if (pkg::func()) begin\n"
    "  end else begin\n"
    "  end\n"
    "endmodule",
    // function calls
    "module funky ();\n"
    "initial begin\n"
    "  func2(1'b0);\n"
    "  pkg::func3(1'b0, \"joe\");\n"
    "  void'(obj.func(chicken, dinner));\n"
    "end\n"
    "endmodule",
    "module funky ();\n"
    "initial begin\n"
    "  func();\n"
    "  func2(.val(1'b0));\n"
    "  func3(.val(1'b0), .name(\"joe\"));\n"
    "  obj.func();\n"
    "  pkg::func();\n"
    "  void'(pkg::func());\n"
    "  void'(obj.func(chicken));\n"
    "end\n"
    "endmodule",
    "module funky ();\n"
    "initial begin\n"
    "  func1(0, .x());\n"
    "  func2(1'b0, 13, .narg1(1'b1));\n"
    "  pkg::func3(1'b0, \"joe\", .z(), .y(foo.bar));\n"
    "  void'(obj.func(chicken, dinner, .knife(spoon.handle)));\n"
    "end\n"
    "endmodule",
    "module funkier ();\n"
    "initial begin\n"
    "  cls #(Z)::func1(a, b, c);\n"
    "  pkg::cls #(Z)::func2(1, 2, 3);\n"
    "  cls #(virtual t_if)::func3(a, b, c);\n"  // interface parameter type
    "  cls #(virtual interface t_if.mp)::func4(a, b, c);\n"  // with modport
    "end\n"
    "endmodule",
    "module preprocessor_revenge ();\n"
    "initial begin\n"
    "  func1(\n"
    "`ifndef BLAH\n"  // preprocessor directives
    "    a,\n"
    "`endif  // BLAH\n"
    "    b,\n"
    "    .x()\n"
    ");\n"
    "end\n"
    "endmodule",
    "module preprocessor_revenge ();\n"
    "initial begin\n"
    "  func1(zzz, yyy,\n"
    "`ifndef BLAH\n"  // preprocessor directives
    "    a\n"
    "`else  // BLAH\n"
    "    b,\n"
    "    .x(ccc + 1)\n"
    "`endif  // BLAH\n"
    ");\n"
    "end\n"
    "endmodule",
    // special functions: randomize
    "module randomizer ();\n"
    "initial begin\n"
    "  randomize;\n"
    "  std::randomize;\n"
    "  void'(std::randomize);\n"
    "  void'(randomize(flt) with {flt[k:0] == '0;});\n"
    "  void'(randomize(null));\n"
    "  void'(randomize(x, y, z) with (b, c) {flt[k:0] == '0;});\n"
    "  void'(std::randomize with (b, c) {});\n"
    "  void'(std::randomize with (bb) {\n"
    "    c < z;\n"
    "    w + 2;\n"
    "  });\n"
    "end\n"
    "endmodule",
    "module randomizer ();\n"
    "initial begin\n"
    "  randval = std::randomize(dur) with {\n"
    "    dur >= mind;\n"
    "    dur <= maxd;\n"
    "    dur + c <= cfg.max_dur;\n"
    "  };\n"
    "end\n"
    "endmodule",
    "module randomizer ();\n"
    "initial begin\n"
    "  randval = std::randomize(dur) with {\n"
    "`ifdef CONDITION\n"
    "    dur >= mind;\n"
    "`else\n"
    "    dur <= maxd;\n"
    "`endif\n"
    "    dur <= maxd;\n"
    "    dur + c <= cfg.max_dur;\n"
    "  };\n"
    "end\n"
    "endmodule",
    "module randomizer ();\n"
    "initial begin\n"
    "  randval = std::randomize(del) with {\n"
    "    del >= mind && del <= maxd;\n"
    "    !(del inside {bad_dels});\n"
    "  };\n"
    "end\n"
    "endmodule",
    "module randomizer ();\n"
    "initial begin\n"
    "  randval = std::randomize(foo) with {\n"
    "    foo inside {a, b, c};\n"
    "    foo dist { a<2 };\n"
    "  };\n"
    "end\n"
    "endmodule",
    "module randomizer ();\n"
    "initial begin\n"
    "  randval = !std::randomize(foo) with {\n"
    "    if (foo.e == 0) foo.f == 0;\n"
    "    if (foo.e == 1) foo.f[j] != 1; else foo[k].f > g;\n"
    "  };\n"
    "end\n"
    "endmodule",
    "module randomizer ();\n"
    "initial begin\n"
    "  foo.randomize();\n"
    "end\n"
    "endmodule",
    // inside expression
    "module insider ();\n"
    "initial begin\n"
    "  ranges = {a, b, c};\n"
    "  lval = foo inside ranges;\n"  // grammar extension to LRM
    "end\n"
    "endmodule",
    "module insider ();\n"
    "initial begin\n"
    "  int slice[$] = '{3, 4, 5};\n"
    "  lval = ex inside {1, 2, slice};\n"
    "end\n"
    "endmodule",
    // task calls
    "module task_caller;\n"
    "initial begin\n"
    "  undefined_task();\n"
    "  undefined_task(\"hello\", \"world\");\n"
    "  undefined_task(.str(\"goodbye\"), .greet(\"world\"));\n"
    "end\n"
    "endmodule",
    /* FIXME:
     // seen this "pkg#(N)::func()" construct in our verilog source code,
     // but having difficulty supporting it without causing grammar conflicts.
     // This *looks* like a package parameter, but I haven't found an
     // official description.
    "module ifelsey ();\n"
    "  if (pkg #(42)::func()) begin\n"
    "  end else begin\n"
    "  end\n"
    "endmodule",
    */
    // for loops and conditionals
    "module addf ();\n"
    "generate\n"
    "  for (i=0;i<N;i++) begin\n"
    "  end\n"
    "endgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate\n"
    "  for (genvar i=0;i<N;i++) begin\n"  // genvar
    "  end\n"
    "endgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate\n"
    "  for (genvar i=0; ;i++) begin\n"  // no loop condition
    "  end\n"
    "endgenerate\n"
    "endmodule",
    "module loopit ();\n"
    "generate\n"
    "  for (i=0;i<N;) begin\n"  // no for-step
    "  end\n"
    "endgenerate\n"
    "endmodule",
    "module loopit ();\n"
    "initial begin\n"
    "  for (i=0;i<N;) begin\n"  // no for-step
    "  end\n"
    "end\n"
    "endmodule",
    "module loopit ();\n"
    "initial begin\n"
    "  for (i=0; ;i++) begin\n"  // no loop condition
    "  end\n"
    "end\n"
    "endmodule",
    "module semi ();\n"
    "generate\n"
    "  ;;;;;;\n"
    "endgenerate\n"
    "endmodule",
    "module semi ();\n"
    "generate\n"
    "  let max(a,b) = (a > b) ? a : b;\n"  // let declaration
    "endgenerate\n"
    "generate\n"
    "  begin\n"
    "    let max(a,b) = (a > b) ? a : b;\n"  // let declaration
    "  end\n"
    "endgenerate\n"
    "endmodule",
    "module shifter ();\n"
    "generate\n"
    "  for (i=0;i<N;i++,j--) begin\n"  // multiple step assignments
    "  end\n"
    "endgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate for (i=0;i<N;i++) begin : loop_name\n"
    "end\nendgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate for (i=0;i<N;i++) loop_name : begin\n"
    "end\nendgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate for (i=0;i<N;i++) begin : loop_name\n"
    "end : loop_name\nendgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate if (x < y) begin : cond_name\n"
    "end\nendgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate if (x < y) begin : cond_name\n"
    "end : cond_name\nendgenerate\n"
    "endmodule",
    "module addf ();\n"
    "generate\n"
    "  for (i=0;i<N;i+=1) begin : myloop\n"
    "  end\n"
    "  for (j=0;j<N;j+=1) begin : myloop2\n"
    "  end : myloop2\n"
    "  for (genvar k=0;k<N;++k) begin : myloop3\n"
    "  end : myloop3\n"
    "endgenerate\n"
    "endmodule",
    "module macmac ();\n"
    "generate if (x < y) begin : cond_name\n"
    "  logic upipe;\n"
    "  `FF(a, b, c, d)\n"  // macro call
    "  `undef FF\n"
    "end : cond_name\nendgenerate\n"
    "endmodule",
    "module wiry ();\n"
    "generate\n"
    "  if (need_more_wires) begin\n"
    "    wire happy_net;\n"
    "    wire nappy_net [3:0];\n"
    "    wire slappy_net = 1'b0;\n"
    "    wire zappy_net = 1'b1, yappy_net = 1'b0;\n"
    "  end\n"
    "endgenerate\n"
    "endmodule",
    // foreach
    "module oneforall;\n"
    "initial begin\n"
    "  foreach ( foo[i] )\n"
    "    func(foo[i]);\n"
    "end\n"
    "endmodule",
    "module oneforall;\n"
    "initial begin\n"
    "  foreach ( foo[i,j,k] )\n"
    "    func(foo[i,j,k]);\n"
    "end\n"
    "endmodule",
    "module oneforall;\n"
    "initial begin\n"
    "  foreach ( foo[,j,k] )\n"
    "    func(foo[j,k]);\n"
    "  foreach ( bar[] )\n"
    "    func();\n"
    "  foreach ( bar1[,] )\n"
    "    func();\n"
    "  foreach ( bar2[row,] )\n"
    "    func();\n"
    "  foreach ( bar3[,col] )\n"
    "    func();\n"
    "end\n"
    "endmodule",
    "module oneforall;\n"
    "initial begin\n"
    "  foreach ( this.foo[j] )\n"
    "    func(this.foo[j]);\n"
    "end\n"
    "endmodule",
    "module oneforall;\n"
    "initial begin\n"
    "  foreach ( super.bar[i][j,k] )\n"
    "    func(super.baz[i][j,k]);\n"
    "end\n"
    "endmodule",
    // wire decls
    "module foobar ();\n"
    "wire x0;\n"
    "wire x1, x2, x3;\n"
    "wire [N-1:0] x4;\n"
    "wire x5[2:0];\n"
    "wire [7:0] x6[2:0];\n"
    "wire sometype x7;\n"
    "wire sometype x8[2:0];\n"
    "wire sometype [7:0] x9;\n"
    "wire sometype [7:0] x10[2:0];\n"
    "endmodule",
    // fifo decls
    "module qoobar;\n"
    "QueueValue x0 [$];\n"
    "QueueValue y0 [$:N+1];\n"
    "endmodule",
    // intermediate value wires
    "module addf (\n"
    "input a, input b, input ci,\n"
    "output s, output co);\n"
    "wire x1, x2, x3;\n"
    "wire x4 = x1;\n"
    "assign x1 = a*b;\n"
    "assign x2 = a*ci;\n"
    "assign x3 = b*ci;\n"
    "assign s = a^b^ci;\n"
    "assign co = x1 +x2 +x3;\n"
    "endmodule",
    "module foobarf (\n"
    "input [1:0] a,\n"
    "input [1:0] b,\n"
    "output [1:0] c);\n"
    "wire [1:0] #delay d = a ^ b;\n"
    "assign #delay c = d;\n"
    "endmodule",
    // using logic primitives
    "module addf (\n"
    "input a, b, ci,\n"
    "output s, co);\n"
    "wire x1, x2, x3, p;\n"
    "and a0(a, b, x1);\n"
    "and a1(a, c1, x2);\n"
    "and a2(b, c1, x3);\n"
    "or o1(x1, x2, x3, co);\n"
    "xor y1(a, b, p);\n"
    "xor y2(ci, p, s);\n"
    "endmodule",
    // with localparam in parameter ports
    "module foo #(\n"
    "localparam bar = 2\n"
    ") (\n"
    "output reg zzz[bar-1:0]);\n"
    "endmodule",
    // with localparam in body
    "module rotator ();\n"
    "localparam foo = 2;\n"
    "localparam larry = 1, curly = 2, moe = 3;\n"
    "localparam bar [1:0] = '{1, 2};\n"
    "localparam [3:0] baz = 4'ha;\n"
    "localparam [1:0] ick [2:0] = '{'{0,1},'{1,2},'{2,3}};\n"
    "endmodule",
    // with parameter
    "module signer;\n"
    "parameter int M = 4;\n"
    "parameter int unsigned N = 5;\n"
    "parameter int signed O = 6;\n"
    "parameter int unsigned [M-1:0] P = {0,1,2,3};\n"
    "endmodule",
    "module paramtest #();\n"  // empty parameters
    "endmodule",
    "module paramtest #(int M);\n"  // without default value
    "endmodule",
    "module paramtest #(int M, N);\n"
    "endmodule",
    "module paramtest #(int M, int N);\n"
    "endmodule",
    "module paramtest #(int M=4);\n"
    "endmodule",
    "module paramtest #() ();\n"  // empty parameter, empty ports
    "endmodule",
    "module paramtest #(int M=4) ();\n"
    "endmodule",
    "module signerer #(\n"
    "parameter int M = 4,\n"
    "parameter int unsigned N = 5,\n"
    "parameter int signed O = 6,\n"
    "parameter int unsigned [M-1:0] P = {3,2,1,0}\n"
    ");\n"
    "endmodule",
    "module rotator ();\n"
    "parameter foo = 2;\n"
    "parameter reg [7:0] foo = 2;\n"
    "parameter larry = 1, curly = 2, moe = 3;\n"
    "parameter PType _larry_ = 0;\n"
    "parameter bar [1:0] = '{1, 2};\n"
    "parameter PType bar2 [1:0] = '{1, 2};\n"
    "parameter [3:0] baz = 4'ha;\n"
    "parameter [1:0] ick [2:0] = '{'{0,1},'{1,2},'{2,3}};\n"
    "parameter Ptype2 [3:0] bazzy = 4'ha;\n"
    "parameter Ptype2 [1:0] icky [2:0] = '{'{0,1},'{1,2},'{2,3}};\n"
    "endmodule",
    "module rotator;\n"
    "parameter foo = `WWW 'hfeedf00d;\n"  // macro-id as width
    "endmodule",
    // type parameters
    "module rotator ();\n"
    "localparam type Foo;\n"
    "localparam type Goo = Zoo#(V, W, X);\n"
    "localparam type Koo, Loo, Moo;\n"
    "parameter type Woo;\n"
    "parameter type Yoo = MMM::TPP, Xoo = JJJ::KKK::LLL;\n"
    "parameter type Zoo = Zoo#(V, W, X)::BusType;\n"
    "endmodule",
    // with parameter (keyword and type optional)
    "module rotator #(parameter width=8) (\n"
    "input clk,\n"
    "output reg [width-1:0] out);\n"
    "always @(posedge clk)\n"
    "  out <= {out[0], out[width-1:1]};\n"
    "endmodule",
    "module rotator #(width=8) (\n"
    "input clk,\n"
    "output reg [width-1:0] out);\n"
    "endmodule",
    "module rotator #(integer width=8) (\n"
    "input clk,\n"
    "output reg [width-1:0] out);\n"
    "endmodule",
    "module rotator #(parameter integer width=8) (\n"
    "input clk,\n"
    "output reg [width-1:0] out);\n"
    "endmodule",
    // with parameters
    "module rotator #(\n"
    "parameter width=8, dir=0\n"
    ") (\n"
    "input clk,\n"
    "output reg [width-1:0] out);\n"
    "always @(posedge clk)\n"
    "  if (dir) out <= {out[0], out[width-1:1]};\n"
    "  else out <= {out[width-2:0], out[width-1]};\n"
    "endmodule",
    // with user-defined type parameter
    "module biggerinter #(parameter SomeEnumType mode=DefaultMode) (\n"
    "input clk,\n"
    "output reg out);\n"
    "endmodule",
    "module biggerinter\n"
    "  #(parameter yourpkg::EnumType mode=DefaultMode) (\n"
    "  input clk,\n"
    "  output reg out);\n"
    "endmodule",
    // type parameters
    "module typer #(\n"
    "  parameter type Mtype\n"
    ") ();\n"
    "endmodule",
    "module typer #(\n"
    "  parameter type Mtype = X::Y,\n"
    "  parameter type Qtype = X::ZZ #(NA, NB),\n"
    "  parameter type Ztype = integer\n"
    ") ();\n"
    "endmodule",
    "module typer #(\n"
    "  type Mtype = X::Y,\n"
    "  type Qtype = ZZ #(.NA(A), .NB(B)),\n"
    "  type Ztype = bit\n"
    ") ();\n"
    "endmodule",
    "module typer #(\n"
    "  parameter type Mtype = type(Xtype)\n"
    ") ();\n"
    "endmodule",
    // type references
    "module type_reffer;\n"
    "real a = 4.76;\n"
    "var type(a) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "wreal a;\n"
    "wreal b = 4.76;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "real a = 4.76;\n"
    "real b = 0.74;\n"
    "var type(a+b) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "type(a) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "type(a::b) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "type(f()) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "type(f(x)) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "type(f(x, y)) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "type(a::b()) c;\n"
    "endmodule\n",
    "module type_reffer;\n"
    "type(type(a)) c;\n"
    "endmodule\n",
    "module preprocessor_in_parameters #(\n"
    "  parameter int M = 4\n"
    "`ifdef MORE_PARAMS\n"  // preprocessor directive
    "  , parameter int N = 8\n"
    "`endif  // MORE_PARAMS\n"
    ") ();\n"
    "endmodule",
    "module preprocessor_in_parameters #(\n"
    "`ifdef MORE_PARAMS\n"  // preprocessor directive
    "  parameter int M = 4,\n"
    "`endif  // MORE_PARAMS\n"
    "  parameter int N = 8\n"
    ") ();\n"
    "endmodule",
    "module preprocessor_in_parameters #(\n"
    "`ifdef MORE_PARAMS\n"  // preprocessor directive
    "  parameter int M = 4,\n"
    "  parameter int N = 8\n"
    "`endif  // MORE_PARAMS\n"
    ") ();\n"
    "endmodule",
    // passing subinstance parameter
    "module rotator #(parameter width=8) (\n"
    "input clk,\n"
    "output reg [width-1:0] out);\n"
    "foo #(.WIDTH(width)) bar(clk, out);\n"
    "endmodule",
    "module spammer;\n"
    "foo #(.clk) bar;\n"
    "foo #(bit) bar0();\n"
    "foo #(.ValType(integer)) bar1();\n"
    "foo #(.ValType(virtual t_if)) barif1();\n"  // interface type
    "foo #(virtual interface t_if) barif2();\n"  // interface type
    "foo #(logic[N:0][k:0], integer[1:0]) bar2();\n"
    "foo #(.ValType(logic[N:0][k:0]), .Val2Type(bit[2:0])) bar3();\n"
    "foo #($, $) bar4();\n"
    "foo #(.XYZ($), .ABC($)) bar5();\n"
    "endmodule",
    "module spammer;\n"
    "foo #(`VALUE1,\n"  // macro-id expression
    "      `VALUE2,\n"  // macro-id expression
    "      `VALUE3\n"   // macro-id expression (MacroGenericItem)
    ") bar_x0();\n"
    "endmodule",
    "module splammer;\n"
    "foo #(`FUNC(a, 1),\n"  // macro-call expression
    "      `FUNC(b, 2),\n"  // macro-call expression
    "      `FUNC(c, 4)\n"   // macro-call expression (MacroGenericItem)
    ") bar_y0();\n"
    "endmodule",
    "module parameter_conditioner;\n"
    "foo #(.A(1),\n"
    "`ifdef HAVE_B\n"  // preprocessing directive
    "      .B(2),\n"
    "`endif\n"
    "      .C(8))\n"
    "  bar_y0();\n"
    "endmodule",
    "module parameter_conditioner;\n"
    "foo #(\n"
    "`ifdef HAVE_B\n"  // preprocessing directive
    "      .B(2),\n"
    "      .C(8)\n"
    "`endif\n"
    ") bar_y0();\n"
    "endmodule",
    // package::type
    "module foobar (\n"
    "mypackage::type1 ins,\n"
    "mypackage::type2 outs,\n"
    "mypackage::type2 [3:0] more_outs\n"
    ");\n"
    "endmodule",
    "module goobar (\n"
    "mypackage::myclass::type1 ins,\n"  // nested class type
    "mypackage::myclass::type2 outs\n"
    ");\n"
    "endmodule",
    "module hoobar (\n"
    "mypackage::type1 #(1,2) ins,\n"  // parameterized type
    "mypackage::type2 outs\n"
    ");\n"
    "module goobar (\n"
    "mypackage::myclass #(4)::type1 ins, ctrls,\n"  // template class member
    "mypackage::myclass::type2 outs\n"
    ");\n"
    "endmodule\n"
    "endmodule",
    "module foobar (\n"
    "input mypackage::type1 ins,\n"
    "output mypackage::type2 outs,\n"
    "output mypackage::type2 [N-1:0] more_outs\n"
    ");\n"
    "endmodule",
    // interface.modport
    "module barfoo (\n"
    "myinterface.mymodport1 ins,\n"
    "myinterface.mymodport2 [N-1:0] outs\n"
    ");\n"
    "endmodule",
    // import export
    "import thatpkg::thing;\n"
    "module import_tax;\n"
    "endmodule",
    "module import_tax\n"
    "  import thatpkg::thing;\n"
    "();\n"  // empty ports
    "endmodule",
    "module import_tax\n"
    "  import thatpkg::thing;\n"
    "#() ();\n"  // empty parameters and empty ports
    "endmodule",
    "module import_tax\n"
    "  import thatpkg::*;\n"  // wildcard
    "();\n"
    "endmodule",
    "module import_tax\n"
    "  import thispkg::blah, thatpkg::*;\n"  // multiple imports
    "();\n"
    "endmodule",
    "module import_tax\n"
    "  import thispkg::blah;\n"  // separate imports
    "  import thatpkg::*;\n"
    "();\n"
    "endmodule",
    "module import_tax\n"
    "  import thispkg::blah, baz::*;\n"
    "  import thatpkg::*, another_one::foobar;\n"
    "#(int p = 4) (input clk);\n"
    "endmodule",
    "import thatpkg::*;\n"
    "module import_tax;\n"
    "endmodule",
    "module barzoo ();\n"
    "import pkg::ident;\n"
    "export \"name\" task rabbit;\n"
    "endmodule",
    "import \"DPI\" function bar;\n",
    "import \"DPI-C\" function bar;\n",
    "import \"DPI-C\" foo = function bar;\n",
    "import \"DPI-C\" foo = function void bar;\n",
    "import \"DPI-C\" foo = function void bar();\n",
    "import \"DPI-C\" foo = function void bar(input chandle ch);\n",
    // using escaped identifier as alias:
    "import \"DPI-C\" \\foo = function void bar(input chandle ch);\n",
    "import \"DPI-C\" `foo = function void bar(input chandle ch);\n",  // macro
    "import \"DPI-C\" foo = function longint unsigned bar(input chandle "
    "ch);\n",
    "module zoobar ();\n"
    "import \"DPI-C\" function void fizzbuzz (\n"
    "  input in val, output int nothing);\n"
    "endmodule",
    "module woobar ();\n"
    "import \"DPI-C\" context function void fizzbuzz (\n"
    "  input in val, output int nothing);\n"
    "endmodule",
    "module zoofar ();\n"
    "export \"DPI-C\" function twiddle;\n"
    "endmodule",
    "module zoofar ();\n"
    "export \"DPI-C\" function void twiddle;\n"
    "endmodule",
    "module toofar ();\n"
    "export \"DPI-C\" task swiddle;\n"
    "endmodule",
    // attributes (ignored)
    "(* war=peace, freedom=slavery, ignorance=strength *)\n"
    "module moofar;\n"
    "(*\n"
    "lexer ignores this for now\n"
    "*)\n"
    "(*******)\n"
    "(**)\n"
    "endmodule",
    // macro calls
    "module moofar;\n"
    "  `DFF(c, d, q)\n"  // as a module_item or block_item
    "endmodule",
    "module moofar;\n"
    "  `ASSERT (True == False)\n"
    "  `ASSERT (Ignorance == Strength);\n"  // with semicolon
    "  initial begin end\n"
    "  `COVER (your, a__)\n"
    "endmodule",
    "module moofar;\n"
    "initial `WAIT(for_it)\n"
    "initial begin\n"
    "  `SLEEP(duration)\n"   // as a statement
    "  `SLEEP(duration);\n"  // with semicolon
    "end\n"
    "endmodule",
    "module moofar;\n"
    "initial begin\n"
    "  xyz = `SLEEP(duration);\n"  // as an expression
    "  abc[`INDEX(2)] = jkl;\n"    // as an index
    "end\n"
    "endmodule",
    // event control
    "module moofar;\n"
    "always @ (*) begin end\n"
    "always @ (*) begin end\n"
    "endmodule",
    "module noofar;\n"
    "always @ doo.xyz begin end\n"
    "always @ (foo.xyz) begin end\n"
    "endmodule",
    "module poofar;\n"
    "always @ (xyz, bar.bq) begin end\n"
    "endmodule",
    "module qoofar;\n"
    "always @ (posedge xyz, negedge bar.bq) begin end\n"
    "endmodule",
    "module qoofar;\n"
    "always @ (posedge xyz iff bar.bq) begin end\n"  // trailing iff
    "endmodule",
    "module qoofar;\n"
    "always_ff @(posedge(clk) iff rst_clk == 1'b0) begin\n"
    "end\n"
    "endmodule",
    // assignment patterns
    "module assignment_test1;\n"
    "initial begin\n"
    "  xx = \'{a, b, c};\n"
    "  xy = \'{4{a}};\n"
    "  xz = \'{4{a, b}};\n"
    "end\n"
    "endmodule",
    "module assignment_test2;\n"
    "initial begin\n"
    "  x1 = \'{default:0};\n"
    "  x[2] = \'{int:1};\n"
    "  x3 = \'{byte:127,bit:1};\n"
    "  x4 = \'{bool:1};\n"
    "  x5 = \'{bit:0};\n"
    "end\n"
    "endmodule",
    "module assignment_test3;\n"
    "initial begin\n"
    "  xx = byte\'{a, b, c};\n"
    "end\n"
    "endmodule",
    // struct literals
    "module struct_literal;\n"
    "always_comb begin\n"
    "  x = t_foo\'{bar: 0, boo: 1};\n"
    "end\n"
    "endmodule\n",

    // assertion items
    "module assertion_items;\n"
    "assert property ( A + B <= C );\n"
    "triangle1: assert property ( A + B >= C );\n"
    "assert property ( A + C <= B ) do_something();\n"
    "assume property ( C + B <= A ) do_something_else();\n"
    "assume property ( C + B <= A ) /* do nothing */;\n"
    "rect1: cover property ( D + B <= A ) do_something2();\n"
    "restrict property ( D <= E );\n"
    "endmodule",
    "module assumption_items;\n"
    "assert property ( A + B <= C ) else give_up();\n"  // with else clause
    "triangle1: assert property ( A + B >= C ) else give_up();\n"
    "endmodule",
    "module cover_sequences;\n"
    "cover sequence ( F );\n"
    "cover sequence ( @(posedge gg) F );\n"
    "cover sequence ( disable iff (kill_this) sequence_expr_H );\n"
    "cover sequence ( @x.y disable iff (kill_that) sequence_expr_J );\n"
    "endmodule",
    // temporal property expressions
    "module props;\n"
    "  assert property (nexttime x > y);\n"
    "  assert property (nexttime [11] x > z);\n"
    "  assert property (s_nexttime w < y);\n"
    "  assert property (s_nexttime [12] w < z);\n"
    "endmodule",
    "module props;\n"
    "  assert property (always x > y);\n"
    "  assert property (always [11:$] x > z);\n"
    "  assert property (s_always [12:15] w < z);\n"
    "endmodule",
    "module props;\n"
    "  assert property (eventually [8:$] z);\n"
    "  assert property (s_eventually y);\n"
    "  assert property (s_eventually [9:14] w);\n"
    "endmodule",
    "module props;\n"
    "  assert property (s_eventually always y);\n"
    "  assert property (always s_eventually z);\n"
    "endmodule",
    // bind directives
    "module bind_me;\n"
    "bind dut.rtl.mm ib_if ib_if_inst ();\n"
    "bind dut.rtl.ff tc_c tc_c_inst (.clk(clk_tc), .*);\n"
    "endmodule",
    // encrypted contents
    "module secrets;\n"
    "`protected\n"
    "(*&^%$#1aALkc@!!@#$%^&*(a8sayasd\n"
    "87y&*H*!@9FbnASDLCZ)HYgyGYh@#BH)72361^&*\n"
    "`endprotected\n"
    "endmodule",
    "module secrets;\n"
    "//pragma protect begin_protected\n"
    "(*&^%$#1aALkc@!!@#$%^&*(a8sayasd\n"
    "87y&*H*!@9FbnASDLCZ)HYgyGYh@#BH)72361^&*\n"
    "//pragma protect end_protected\n"
    "endmodule",
    // timescale directives
    "`timescale 1ns / 1ns\n"
    "module modular_thing;\n"
    "endmodule",
    "`timescale `DEFAULT_TIMESCALE\n"  // unexpanded macro
    "module modular_thing;\n"
    "endmodule",
    "`ifdef FOO\n"
    "`timescale 1ns / 1ns\n"
    "`else\n"
    "`timescale 100 ps / 100 ps\n"  // with spaces before unit
    "`endif\n"
    "module modular_thing;\n"
    "endmodule",
    "`timescale 100 ps / 100 ps\n"  // with spaces before unit
    // celldefine directives
    "`celldefine\n"
    "module modmod;\n"
    "endmodule\n"
    "`endcelldefine\n"
    "`celldefine\n"
    "module modmod2;\n"
    "endmodule\n"
    "`endcelldefine\n",
    "`resetall\n"
    "`pragma black magic\n"
    "module startover;\n"
    "endmodule",
    "`unconnected_drive pull0\n"
    "module startover;\n"
    "endmodule\n"
    "`nounconnected_drive",
    "`default_decay_time 99\n"
    "`default_trireg_strength 64\n"
    "module blank;\n"
    "endmodule",
    "`default_nettype wire\n"
    "module blank;\n"
    "endmodule",
    "`default_nettype none\n"
    "module blank;\n"
    "endmodule\n"
    "`default_nettype tri\n"
    "module blank;\n"
    "endmodule",
    "`default_nettype trireg\n"
    "module blank;\n"
    "endmodule",
    "`default_nettype supply0\n"
    "`default_nettype supply1\n"
    "`default_nettype tri0\n"
    "module blank;\n"
    "endmodule",
    "`suppress_faults\n"
    "`nosuppress_faults\n"
    "module blank;\n"
    "endmodule",
    "`enable_portfaults\n"
    "`disable_portfaults\n"
    "module blank;\n"
    "endmodule",
    "`delay_mode_distributed\n"
    "`delay_mode_path\n"
    "`delay_mode_unit\n"
    "`delay_mode_zero\n"
    "module blank;\n"
    "endmodule",
    "`begin_keywords \"1364-2005\"\n"
    "module blank;\n"
    "endmodule\n"
    "`end_keywords",
    "`uselib lib=foo libext=.vv\n"
    "module blank;\n"
    "endmodule",
    "module blank;\n"
    "`protect\n"
    "wire secret;\n"
    "`endprotect\n"
    "endmodule",
    // specify block
    "module specify_recmem_tests;\n"
    "specify\n"
    "  $recrem (posedge ASYNC_R, posedge CE1_TCLK,\n"
    "           `REC_TIME, `REM_TIME);\n"
    "endspecify\n"
    "endmodule",
    "module specify_recmem_tests;\n"
    "specify\n"
    "  $recrem (negedge ASYNC_R, posedge CE0_TCLK &&& (ZZZ==1'b1),\n"
    "           `REC_TIME, `REM_TIME);\n"
    "endspecify\n"
    "endmodule",
    "module specify_recmem_tests;\n"
    "specify\n"
    "  $recrem (edge ASYNC_R, edge CE1_TCLK,\n"
    "           `REC_TIME, `REM_TIME);\n"
    "endspecify\n"
    "endmodule",
    "module specify_recmem_tests;\n"
    "specify\n"
    "  $recrem (ASYNC_R, CE1_TCLK &&& (YYY==1'b0),\n"
    "           `REC_TIME, `REM_TIME);\n"
    "endspecify\n"
    "endmodule",
    "module specify_recmem_tests;\n"  // with edge specifiers
    "specify\n"
    "  $recrem (edge ASYNC_R, edge [ 01, 0x ] CE1_TCLK &&& (YYY==1'b0),\n"
    "           `REC_TIME, `REM_TIME);\n"
    "endspecify\n"
    "endmodule",
    "module specparams_test;\n"
    "specify\n"
    "  specparam PATHPULSE$ = ( 0, 0.001 );\n"
    "  specparam tCD = (`CM_ACCESS);\n"
    "`ifdef SQUASHING\n"
    "  specparam tHOLD = (`CM_ACCESS);\n"
    "`else\n"
    "  specparam tHOLD = (`CM_RETAIN);\n"
    "`endif\n"
    "endspecify\n"
    "endmodule",
    // preprocessor balanced module items
    "module pp_mod;\n"
    "`ifdef MONEY\n"
    "`endif  // MONEY\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifdef MONEY\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifndef MONEY\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifndef MONEY\n"
    "`else   // MONEY\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifndef MONEY\n"
    "`elsif  BROKE\n"
    "`else   // MONEY\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifdef MONEY\n"
    "`elsif  BROKE\n"
    "`elsif  BROKER\n"
    "`else   // MONEY\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifdef MONEY\n"
    "wire sender;\n"
    "wire [2:0] receiver;\n"
    "`elsif  BROKE\n"
    "`elsif  BROKER\n"
    "`else   // MONEY\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifdef MONEY\n"
    "`elsif  BROKE\n"
    "wire sender;\n"
    "wire [2:0] receiver;\n"
    "`else   // MONEY\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifdef MONEY\n"
    "`elsif  BROKE\n"
    "`else   // MONEY\n"
    "wire sender;\n"
    "wire [2:0] receiver;\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    "module pp_mod;\n"
    "wire transfer;\n"
    "`ifdef MONEY\n"
    "`else   // MONEY\n"
    "wire sender;\n"
    "wire [2:0] receiver;\n"
    "`endif  // MONEY\n"
    "assign receiver = transfer;\n"
    "endmodule",
    // preprocessor balanced generate items
    "module balancer_generator;\n"
    "generate\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`define BAR FOO\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`undef BAR\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"  // empty `ifdef
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"  // empty `ifdef
    "`define BARRY\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "  clocker clkr;\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"  // two items
    "  clocker clkr;\n"
    "  clicker clkr;\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifndef BAR\n"  // empty `ifndef
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifndef BAR\n"
    "  clicker clkr;\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifndef BAR\n"  // two items
    "  clicker clkr;\n"
    "  clacker clkr;\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`else\n"  // empty `else clause
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifndef BAR\n"
    "`else\n"  // empty `else clause
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`elsif BLARG\n"  // empty `elsif clause
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`elsif BLARG\n"  // empty `elsif clause
    "`elsif ZERG\n"   // second `elsif
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifndef BAR\n"
    "`elsif BLARG\n"  // empty `elsif clause
    "`elsif ZERG\n"   // second `elsif
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`elsif BLARG\n"  // `elsif clause
    "  wire frame;\n"
    "  wire line;\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`elsif BLARG\n"  // `elsif clause
    "  wire frame;\n"
    "  wire line;\n"
    "`else\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`elsif BLARG\n"  // `elsif clause
    "`else\n"
    "  wire frame;\n"
    "  wire line;\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`elsif BLARG\n"
    "`undef DEF\n"
    "`else\n"
    "  wire frame;\n"
    "  wire line;\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`ifdef NESTED\n"  // nested `ifdef
    "  wire frame;\n"
    "  wire line;\n"
    "`endif\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    "module balancer_generator;\n"
    "generate\n"
    "`ifdef BAR\n"
    "`elsif BLARG\n"
    "`else\n"
    "`ifdef NESTED\n"  // nested `ifdef
    "  wire frame;\n"
    "  wire line;\n"
    "`endif\n"
    "`endif\n"
    "endgenerate\n"
    "endmodule",
    // Test for +: and -: operators in declarations:
    "module gen #(parameter width = 3) ();\n"
    "  wire   [base +: width] x;\n"
    "  wire   [base -: width] y;\n"
    "  wire   [base : width]  z;\n"
    "endmodule",
    "module gen #(parameter width = 3)\n"
    "            (out, in1, in2);\n"
    "  output [0  +: width] out;\n"
    "  input  [2  -: width] in1, in2;\n"
    "endmodule",
    "module gen #(parameter width = 3) (\n"
    "    output [0  +: width] out,\n"
    "    input  [2  -: width] in1, in2\n"
    ");\n"
    "endmodule",
    // begin-end blocks at module level are not LRM-valid,
    // but are diagnosed by the linter:
    "module foobar;\n"
    "  begin\n"
    "  end\n"
    "endmodule",
    "module foobar;\n"
    "  begin : block\n"
    "  end : block\n"
    "endmodule",
    "module foobar;\n"
    "  begin\n"
    "    wire crossed;\n"
    "  end\n"
    "endmodule",
    "module foobar;\n"
    "  begin begin\n"
    "  end end\n"
    "endmodule",
};

static constexpr ParserTestCaseArray kModuleInstanceTests = {
    "module tryme;\n"
    "logic lol;\n"           // is a data_declaration
    "wire money;\n"          // is a net_declaration
    "interconnect floyd;\n"  // is a TK_interconnect
    "endmodule",
    "module tryme;\n"
    "foo a;\n"  // looks like data_declaration
    "endmodule",
    "module tryme;\n"
    "foo b();\n"
    "endmodule",
    "module tryme;\n"
    "foo c(x, y, z);\n"
    "endmodule",
    "module tryme;\n"
    "foo d(.x(x), .y(y), .w(z));\n"
    "endmodule",
    // multiple instances in one declaration, comma-separated
    "module tryme;\n"
    "foo b(),\n"
    "    c();\n"
    "endmodule",
    "module macro_as_type;\n"
    "`foo b();\n"
    "endmodule",
    "module macro_as_instance;\n"
    "foo `bar();\n"
    "endmodule",
    "module macro_as_instance;\n"
    "foo `bar ();\n"
    "endmodule",
    "module macro_as_instance;\n"
    "foo `bar (\n"
    ");\n"
    "endmodule",
    "module macro_as_instance;\n"
    "foo `bar [1:0] ();\n"
    "endmodule",
    "module macro_as_instance;\n"
    "foo `bar(x, y, z);\n"  // positional ports as macro args
    "endmodule",
    "module macro_as_instance;\n"
    "foo `bar(.x(x), .y(y), .z(z));\n"  // named ports as macro args
    "endmodule",
    "module macro_as_instance;\n"
    "foo `bar1(), `bar2();\n"  // multiple macro instances
    "endmodule",
};

static constexpr ParserTestCaseArray kInterfaceTests = {
    "interface Face;\n"
    "endinterface",
    "interface Face;\n"
    "endinterface : Face",
    "interface automatic Face;\n"
    "endinterface",
    "interface Bus;\n"
    "logic [7:0] Addr, Data;\n"
    "logic RWn;\n"
    "endinterface",
    "interface Bus;\n"
    "input logic clk;\n"
    "ref logic [7:0] Addr, Data;\n"
    "ref logic RWn;\n"
    "endinterface",
    "interface foo_if;\n"
    "logic [7:0] Addr, Data;\n"
    "modport logic_if(output addr, output data);\n"
    "modport test_if(input addr, input data);\n"
    "endinterface",
    "interface harness_if\n"
    "  #(string name = \"anon\");\n"  // string parameter
    "endinterface",
    "interface blah_if(\n"
    "  input bit clk,\n"
    "  output bit run_clock,\n"
    "  edge_init = 0,\n"  // initial value
    "  rst);\n"
    "endinterface",
    "interface state_if #(parameter W=2)\n"
    "  (input wire clk, inout wire state);\n"
    "endinterface",
    // modport declarations
    "interface foo_if;\n"
    "  modport a(input x);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(input x);\n"
    "  modport b(output z);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(input x, y, z);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(input x), b(output y);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(input x, y, output w, z);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport b(clocking c);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(import x);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(import x, y);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(export x);\n"
    "endinterface : foo_if",
    "interface foo_if;\n"
    "  modport a(export x, y);\n"
    "endinterface : foo_if",
    R"(//second modport port is task
interface conduit_if;
  logic something;
  modport dut(output something, export task task1());
  modport tb (input something, import task task1());
endinterface : conduit_if
)",
    // TODO(b/115654078): module declarations with preprocessing directives

    // clocking declarations
    "interface state_if #(parameter W=2)\n"
    "  (input wire clk, inout wire state);\n"
    "clocking mst @(posedge clk);\n"
    "  output state = ~foo;\n"
    "endclocking\n"
    "clocking pss @(posedge clk);\n"
    "  input state;\n"
    "endclocking : pss\n"
    "endinterface",
    "interface state_if #(parameter W=2)\n"
    "  (input wire clk, inout wire state);\n"
    "clocking @(posedge clk);\n"  // anonymous clocking block
    "  output state = ~foo;\n"
    "endclocking\n"
    "clocking @(negedge clk);\n"  // anonymous clocking block
    "  input state;\n"
    "endclocking : pss\n"
    "endinterface",
    "interface state_if\n"
    "  (input wire clk, inout wire state);\n"
    "clocking mst @clk;\n"
    "  output state;\n"
    "endclocking\n"
    "default clocking pss @(a, b, c);\n"
    "  input state;\n"
    "endclocking\n"
    "endinterface",
    "interface state_if\n"
    "  (input wire clk, inout wire state);\n"
    "clocking @clk;\n"  // anonymous clocking block
    "  output state;\n"
    "endclocking\n"
    "default clocking @(a, b, c);\n"  // anonymous clocking block
    "  input state;\n"
    "endclocking\n"
    "endinterface",
    "interface state_if\n"
    "  (input wire clk, inout wire state);\n"
    "global clocking mst @clk;\n"
    "endclocking\n"
    "default clocking pss @(edge ac);\n"
    "endclocking\n"
    "endinterface",
    "interface state_if\n"
    "  (input wire clk, inout wire state);\n"
    "global clocking @clk;\n"  // anonymous clocking block
    "endclocking\n"
    "default clocking @(edge ac);\n"  // anonymous clocking block
    "endclocking\n"
    "endinterface",
    "interface clocker(input logic clk);\n"
    "logic rst_n;\n"
    "clocking tb @(posedge clk);\n"
    "  default input #1step output negedge;\n"
    "endclocking\n"
    "endinterface",
    // interface port types
    "interface blah_if(interface clk_if, data_if);\n"
    "endinterface",
    "interface blah_if(interface.foo clk_if, data_if);\n"
    "endinterface",
};

static constexpr ParserTestCaseArray kTypedefTests = {
    "typedef i_am_a_type_really;",
    "typedef reg[3:0] quartet;",
    "typedef reg quartet[3:0];",
    "typedef reg[1:0] quartet[1:0];",
    "typedef enum { RED, GREEN, BLUE } colors;",
    "typedef union { int i; bool b; } bint;",
    "typedef struct { int i; bool b; } mystruct;",
    "typedef struct { int i, j, k; bool b, c, d; } mystruct;",
    "typedef some_other_type myalias;",
    "typedef data_t my_array_t [k:0][j:0];",
    "typedef data_t my_array_t [ * ];",  // need spaces for now
    "typedef data_t my_array_t [bit];",
    "typedef data_t my_array_t [bit[31:0]];",
    "typedef data_t my_ar_t [bit[31:0][k:0]][bit[j:0][l:0]];",
    "typedef some_package::some_type myalias;",
    "typedef struct packed {\n"
    "  [4:0] some_member;\n"
    "} mystruct_t;",
    "typedef struct packed {\n"
    "  logic [4:0] some_member;\n"
    "} mystruct_t;",
    "typedef struct packed {\n"
    "  apkg::type_member #(N, M) [P:0] some_member;\n"
    "} mystruct_t;",
    "typedef struct {\n"
    "  rand bit i;\n"
    "  randc integer b[k:0];\n"
    "} randstruct;",
    "typedef enum logic {\n"
    "  Global = 4'h2,\n"
    "  Local = 4'h3\n"
    "} myenum_fwd;",
    "typedef enum logic[3:0] {\n"
    "  Global = 4'h2,\n"
    "  Local = 4'h3\n"
    "} myenum_fwd;",
    "typedef enum bit[3:0][7:0] {\n"
    "  Global = 4'h2,\n"
    "  Local = 4'h3\n"
    "} myenum_fwd;",
    "typedef enum uvec8_t {\n"
    "  Global = 4'h2,\n"
    "  Local = 4'h3\n"
    "} myenum_fwd;",
    "typedef enum yourpkg::num_t {\n"
    "  Global = 4'h2,\n"
    "  Local = 4'h3\n"
    "} myenum_fwd;",
    "typedef struct {\n"
    "  int sample;\n"
    "  int tile;\n"
    "} tuple_t;",
    // typedef with preprocessor directives
    "typedef struct packed {\n"
    "  reg  [A-1:0] addr;\n"
    "  reg [D-1:0] data;\n"
    "`ifndef FOO\n"
    "  reg  [E-1:0] ecc;\n"
    "`endif //  `ifndef FOO\n"
    "  reg   [M-1:0] mask;\n"
    "  reg         parity;\n"
    "} req_t;",
    "typedef enum {\n"
    "`ifdef TWO\n"
    "  Global = 4'h2,\n"
    "`else\n"
    "  Global = 4'h1,\n"
    "`endif\n"
    "  Local = 4'h3\n"
    "} myenum_fwd;",
    "typedef enum {\n"
    "  Global = 4'h2,\n"
    "`ifdef TWO\n"
    "  Local = 4'h2\n"
    "`else\n"
    "  Local = 4'h1\n"
    "`endif\n"
    "} myenum_fwd;",
};

// typedefs as forward declarations
static constexpr ParserTestCaseArray kStructTests = {
    "typedef struct mystruct_fwd;",
    // anonymous structs:
    "struct { int i; bit b; } foo;",
    "struct packed { int i; bit b; } foo;",
    "struct packed signed { int i; bit b; } foo;",
    "struct packed unsigned { int i; bit b; } foo;",
    "struct { `BAZ() } foo;",
    "struct { `BAZ(); } foo;",
    "struct { `BAZ()\n } foo;",
};

static constexpr ParserTestCaseArray kEnumTests = {
    "typedef enum myenum_fwd;",
};

static constexpr ParserTestCaseArray kUnionTests = {
    "typedef union myunion_fwd;",
    // anonymous unions:
    "union { int i; bit b; } foo;",
    "union packed { int i; bit b; } foo;",
    "union packed signed { int i; bit b; } foo;",
    "union packed unsigned { int i; bit b; } foo;",
    "union tagged { int i; bit b; } foo;",
    "union tagged packed { int i; bit b; } foo;",
    "union tagged packed signed { int i; bit b; } foo;",
    "union tagged packed unsigned { int i; bit b; } foo;",
};

// TODO(fangism): implement and test ENUM_CONSTANT

// packages
static constexpr ParserTestCaseArray kPackageTests = {
    "package mypkg;\n"
    "endpackage",
    "package automatic mypkg;\n"
    "endpackage",
    "package static mypkg;\n"
    "endpackage",
    "package mypkg;\n"
    "parameter size = 4;\n"
    "endpackage",
    "package mypkg;\n"
    "typedef class myclass;\n"
    "endpackage",
    "package mypkg;\n"
    "task sleep;\n"
    "endtask\n"
    "endpackage",
    "package mypkg;\n"
    "class myclass;\n"
    "endclass\n"
    "endpackage",
    "package semicolons_galore;\n"
    ";;;;;\n"
    "endpackage",
    // top-level parameters
    "localparam real foo = 789.456;\n",
    "localparam shortreal foo = 123.456;\n",
    "localparam realtime foo = 123.456ns;\n",
    "package foos;\n"
    "  localparam real foo = 789.456;\n"
    "  localparam shortreal foo = 123.456;\n"
    "  localparam realtime foo = 123.456ns;\n"
    "endpackage : foos\n",
    // `macro call
    "`include \"stuff.svh\"\n"
    "`expand_stuff()\n"
    "`expand_with_semi(name);\n"  // with semicolon
    "`expand_more(name)\n"
    "package macro_call_as_package_item;\n"
    "endpackage",
    // preprocessor
    "package mypkg;\n"
    "`undef FOBAR\n"
    "endpackage",
    // let declarations
    "let War = Peace;\n"
    "package mypkg;\n"
    "endpackage",
    "package mypkg;\n"
    "let War = Peace;\n"
    "endpackage",
    "package mypkg;\n"
    "let Three() = One + Two;\n"
    "endpackage",
    "package mypkg;\n"
    "let Max(a,b) = (a > b) ? a : b;\n"
    "endpackage",
    "package mypkg;\n"
    "let Max(a,b=1) = (a > b) ? a : b;\n"
    "endpackage",
    "package mypkg;\n"
    "let Max(untyped a, bit b=1) = (a > b) ? a : b;\n"
    "endpackage",
    // global variables
    "int foobar;\n"
    "static electricity zap;\n"  // variable lifetime
    "automatic transmission vehicle;\n"
    "package mypkg;\n"
    "endpackage",
    "virtual a_if b_if;\n",        // global virtual interface declaration
    "virtual a_if b_if, c_if;\n",  // global virtual interface declaration
    "package p;\n"
    "  virtual a_if b_if;\n"  // global virtual interface declaration
    "endpackage : p\n",
    "package p;\n"
    "  virtual a_if b_if, d_if;\n"
    "endpackage : p\n",
    "package p;\n"
    "  uint [x:y] g = 2;\n"  // user-defined type, packed dimensions
    "endpackage\n",
    "package p;\n"
    "  uint [x][y] g = 2;\n"  // user-defined type, packed dimensions
    "endpackage\n",
    "package p;\n"
    "  uint [x:y] g[z] = 2;\n"  // user-defined type, packed+unpacked dimensions
    "endpackage\n",
    // import directives
    "package foo;\n"
    "import $unit::skynet;\n"
    "import $unit::*;\n"
    "endpackage",
    "package important;\n"
    "import your_pkg::foo;\n"
    "import her_pkg::*;\n"
    "import secret_pkg::item1, secret_pkg::item2, spkg::bar;\n"
    "endpackage",
    "import \"DPI-C\" function void fizzbuzz (\n"
    "  input in val, output int nothing);\n"
    "export \"DPI-C\" function twiddle;\n"
    "package nothing_here;\n"
    "endpackage",
    // export directives
    "package foo;\n"
    "export bar::baz;\n"
    "endpackage",
    "package foo;\n"
    "export bar::*;\n"
    "endpackage",
    "package foo;\n"
    "export *::*;\n"
    "endpackage",
    "package foo;\n"
    "export bar::xx, baz::yy, goo::*, zoo::zz;\n"
    "endpackage",
    "package foo;\n"
    "import bar::*;\n"
    "export bar::*;\n"
    "endpackage",
    // parameter declarations
    "package foo;\n"
    "parameter reg[BITS:0] MR0 = '0;\n"
    "endpackage",
    // bind directive tests (just piggy-backing on package tests)
    "bind scope_x type_y z (.*);\n"
    "package bindme;\nendpackage\n",
    "bind scope_x type_y z1(.*), z2(.*);\n"
    "package bindme;\nendpackage\n",
    "bind module_scope : inst_x type_y inst_z(.*);\n"
    "package bindme;\nendpackage\n",
    "bind module_scope : inst_x type_y::a inst_z(.*);\n"
    "package bindme;\nendpackage\n",
    "bind module_scope : inst_x type_y::a#(4,3) inst_z(.*);\n"
    "package bindme;\nendpackage\n",
    "bind module_scope : inst_x1, inst_x2 type_y inst_z(.*);\n"
    "package bindme;\nendpackage\n",
    "bind module_scope : inst_x1.www, inst_x2.zz[0] type_y inst_z(.*);\n"
    "package bindme;\nendpackage\n",
    "bind module_scope : inst_x1.www, inst_x2.zz[0] type_y z1(.*), z2(.*);\n"
    "package bindme;\nendpackage\n",
    // preprocessor balanced package items
    "package pp_pkg;\n"
    "`ifdef DEBUGGER\n"  // empty
    "`endif\n"
    "endpackage",
    "package pp_pkg;\n"
    "`ifdef DEBUGGER\n"
    "`ifdef VERBOSE\n"  // nested, empty
    "`endif\n"
    "`endif\n"
    "endpackage",
    "package pp_pkg;\n"
    "`ifndef DEBUGGER\n"  // `ifndef
    "`endif\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"  // single item
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"  // multiple items
    "  string dest_name;\n"
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"
    "  string dest_name;\n"
    "`else\n"  // `else empty
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "  string source_name;\n"
    "  string dest_name;\n"
    "`elsif LAZY\n"  // `elsif empty
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`elsif LAZY\n"  // `elsif with multiple items
    "  string source_name;\n"
    "  string dest_name;\n"
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`elsif BORED\n"
    "  string source_name;\n"
    "  string dest_name;\n"
    "`elsif LAZY\n"  // second `elsif
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    "package pp_pkg;\n"
    "  int num_packets;\n"
    "`ifdef DEBUGGER\n"
    "`elsif BORED\n"
    "`else\n"  // `else with multiple items
    "  string source_name;\n"
    "  string dest_name;\n"
    "`endif\n"
    "  int router_size;\n"
    "endpackage",
    // net_type_declarations
    "nettype shortreal foo_wire;\n",
    "nettype real foo_wire;\n",
    "nettype real foo_wire with bar;\n",
    "nettype real foo_wire with bar::baz;\n",
    "nettype logic[3:0] foo_wire with bar;\n",
    "package p;\n"
    "  nettype real foo_wire;\n"
    "endpackage\n",
    "package p;\n"
    "  nettype shortreal[2] foo_wire with fire;\n"
    "endpackage\n",
    "package p;\n"
    "  nettype foo_pkg::bar_t baz_wire;\n"
    "endpackage\n",
    "package p;\n"
    "  nettype foo#(x,y,z)::bar_t baz_wire;\n"
    "endpackage\n",
    "package p;\n"
    "  nettype foo#(x,y,z)::bar_t[2:0] baz_wire;\n"
    "endpackage\n",
    "package p;\n"
    "  nettype foo#(x,y,z)::bar_t baz_wire with quux;\n"
    "endpackage\n",
};

static constexpr ParserTestCaseArray kDescriptionTests = {
    // preprocessor balanced top-level description items
    "`ifdef DEBUGGER\n"  // empty
    "`endif\n",
    "`ifdef DEBUGGER\n"
    "`ifdef VERBOSE\n"  // nested, empty
    "`else\n"
    "`endif\n"
    "`endif\n",
    "`ifndef DEBUGGER\n"  // `ifndef
    "`endif\n",
    "`ifdef DEBUGGER\n"
    "`else\n"  // `else with multiple items
    "`endif\n",
    "`ifdef DEBUGGER\n"
    "`elsif BORED\n"
    "`else\n"  // `else with multiple items
    "`endif\n",
    "`ifdef DEBUGGER\n"
    "`elsif BORED\n"
    "`elsif MORE_BORED\n"
    "`else\n"  // `else with multiple items
    "`endif\n",
    "`ifdef DEBUGGER\n"  // module declaration (not a package item)
    "module mymod;\n"
    "endmodule\n"
    "`endif\n",
    "`ifdef DEBUGGER\n"
    "module mymod;\n"
    "endmodule\n"
    "module mymod_different;\n"  // multiple module declarations
    "endmodule\n"
    "`endif\n",
    "`ifdef DEBUGGER\n"  // package declaration (not a package item)
    "package mypkg;\n"
    "endpackage\n"
    "`endif\n",
    "`ifdef FPGA\n"
    "`ifdef DEBUGGER\n"  // nested `ifdef
    "module mymod;\n"
    "endmodule\n"
    "`endif\n"
    "`endif\n",
    "`ifdef FPGA\n"
    "`ifndef DEBUGGER\n"  // `ifndef
    "interface myinterface;\n"
    "endinterface\n"
    "`endif\n"
    "`endif\n",
    "`ifdef DEBUGGER\n"
    "`MACRO(stuff, morestuff)\n"
    "`endif\n",
    "`ifdef DEBUGGER\n"
    "`MACRO(stuff, morestuff)\n"
    "`SCHMACRO()\n"  // multiple macros
    "`endif\n",
    "package foo_pkg;\n"
    "endpackage\n"
    "`ifdef F00\n"
    "`MACRO(stuff)\n"
    "`endif\n",
    "package foo_pkg;\n"
    "endpackage\n"
    "`ifdef F00\n"
    "`MACRO(stuff)\n"
    "`endif\n"
    "module foo_mod;\n"
    "endmodule\n",
    "`ifdef ASIC\n"
    "module module_asic;\n"
    "endmodule\n"
    "`else  // ASIC\n"
    "module module_fpga;\n"
    "endmodule\n"
    "`endif  // ASIC\n",
    "`ifdef ASIC\n"
    "module module_asic;\n"
    "endmodule\n"
    "`elsif FPGA  // ASIC\n"
    "module module_fpga;\n"
    "endmodule\n"
    "`endif  // ASIC\n",
    "`ifdef ASIC_OR_FPGA\n"
    "module module_asic;\n"
    "endmodule\n"
    "module module_fpga;\n"
    "endmodule\n"
    "`else\n"
    "`endif\n",
    "`ifndef ASIC_OR_FPGA\n"
    "`else\n"
    "module module_asic;\n"
    "endmodule\n"
    "module module_fpga;\n"
    "endmodule\n"
    "`endif\n",
};

static constexpr ParserTestCaseArray kSequenceTests = {
    "sequence myseq;\n"
    "  a != b\n"
    "endsequence\n",
    "sequence myseq;\n"
    "  a != b\n"
    "endsequence : myseq\n",  // end label
    "sequence myseq;\n"
    "  a != b;\n"  // semicolon
    "endsequence\n",
    "sequence myseq();\n"  // empty port list
    "  a >= b\n"
    "endsequence\n",
    "sequence myseq(\n"
    "  type_a a,\n"  // non-empty port list
    "  xpkg::type_g g,\n"
    "  sequence bb,\n"
    "  untyped cc[2:0],\n"
    "  int fgh = '1,\n"
    "  local type_b b,\n"
    "  local inout type_z z,\n"
    "  local input pkgf::type_y y,\n"
    "  local output pkgf::type_g#(H) gg\n"
    ");\n"
    "  a == b\n"
    "endsequence\n",
    "sequence myseq();\n"
    "  logic [3:0] idx;\n"  // assertion variable declaration
    "  a >= b\n"
    "endsequence\n",
    "sequence myseq();\n"
    "  var logic [3:0] idx;\n"  // assertion variable declaration
    "  a >= b\n"
    "endsequence\n",
    "sequence myseq();\n"
    "  logic idx = '0;\n"  // with assignment
    "  a >= b\n"
    "endsequence\n",
    "sequence myseq();\n"
    "  logic idx = '0, bar = 1;\n"  // with assignments
    "  a >= b\n"
    "endsequence\n",
};

static constexpr ParserTestCaseArray kPropertyTests = {
    "property ready_follows_valid_bounded_P;\n"
    "  @(posedge clk) disable iff (reset)\n"
    "  valid |-> eventually [0:Bound] (ready);\n"
    "endproperty",
    "property myprop;\n"
    "a < b\n"
    "endproperty\n",
    "property myprop;\n"
    "a < b;\n"
    "endproperty\n",
    "property myprop;\n"
    "a < b\n"
    "endproperty : myprop\n",
    "property myprop(\n"
    "  type_a a,\n"
    "  xpkg::type_g g,\n"
    "  sequence b,\n"
    "  untyped c,\n"
    "  property d\n"
    ");\n"
    "a < b\n"
    "endproperty\n",
    "property myprop(int a);\n"
    "  valid |-> eventually [1:2] a (ready)\n"
    "endproperty\n",
    "property myprop(\n"
    "  local int a = foo,\n"
    "  local input bit b,\n"
    "  int c[2:0]\n"
    ");\n"
    "a > b\n"
    "endproperty\n",
    "property myprop(\n"
    "  local type_a a = foo,\n"
    "  local input type_b b = 1,\n"
    "  type_c c[2:0]\n"
    ");\n"
    "a > b\n"
    "endproperty\n",
    "property myprop(\n"
    "  foo_a a,\n"
    "  foo_b b\n"
    ");\n"
    "var bool c;\n"
    "var bool d, e;\n"
    "var bool f=2, g=a+b;\n"
    "a > b\n"
    "endproperty\n",
    "property myprop;\n"
    "##1 selected |-> a != 0;\n"
    "endproperty\n",
    "property myprop;\n"
    "##1 selected |-> $myfunc((a & ~b) != 0);\n"
    "endproperty\n",
    "property myprop;\n"
    "##1 selected |-> a(j(k)) == b[c[d]];\n"
    "endproperty\n",
    "property myprop;\n"
    "identifier[xyz] |-> a && b;\n"
    "endproperty\n",
    "property myprop;\n"
    "identifier[xyz] |-> a[a(f, g, h)] && b;\n"
    "endproperty\n",
    "property myprop;\n"
    "identifier[xyz] |-> a[a(f, g, h)] && b;\n"
    "endproperty\n",
    "property myprop;\n"
    "(xx |=> !xx);\n"  // extra parens
    "endproperty\n",
    "property myprop;\n"
    "##1 $fell(s_idle) |-> $past($rose(sched_z));\n"
    "endproperty\n",
    "property myprop;\n"
    "##1 xx |-> (yy || zz);\n"
    "endproperty\n",
    "property myprop;\n"
    "(##1 xx |-> (yy || zz));\n"  // extra parens
    "endproperty\n",
    "property myprop;\n"
    "yy ##1 ww |-> $past(xx);\n"
    "endproperty\n",
    "property myprop;\n"
    "##1 xx |-> (yy || zz)\n"
    "and yy ##1 ww |-> $past(xx)\n"
    "and xx |=> !xx;\n"
    "endproperty\n",
    "property myprop;\n"
    "  zp_s |-> s_cos == $past(z_cn) ##1 !zp_s;\n"
    "endproperty\n",
    "property myprop;\n"
    "  ssz |=> !ssz && !ssi && ssg ##1 !ssg;\n"
    "endproperty\n",
    "property myprop;\n"
    "  !zp |=> $stable(sch) && $stable(sco);"
    "endproperty\n",
    "property myprop;\n"
    "  ##2 g0 && !abr |-> $past(davs != 0, 2);\n"
    "endproperty\n",
    "property myprop;\n"
    "  logic [3:0] idx;\n"
    "  1\n"  // property_spec (expression) required
    "endproperty\n",
    "property myprop;\n"
    "  logic [3:0] idx;\n"
    "  z ##0 y |=> strong(a ##1 b);\n"
    "endproperty\n",
    "property myprop;\n"
    "  logic [3:0] idx;\n"
    "  X && (Y != 4)\n"
    "  ##0\n"
    "  (a & ~b) == 0\n"
    "  |=>\n"
    "  strong(f && g\n"
    "         ##1 !b && w[r] && $onehot(f)\n"
    "         ##0 p || q throughout g [->1]\n"
    "         ##1 g_x);\n"
    "endproperty\n",
    "property myprop;\n"
    "  if (w) y |=> strong(a ##1 b);\n"
    "endproperty\n",
    "property myprop;\n"
    "  if (w) y else qq;\n"
    "endproperty\n",
    "property myprop;\n"
    "  if (w) y else qq |=> j or k;\n"
    "endproperty\n",
    "property myprop_seq_match;\n"
    "  (a, b = c);\n"
    "endproperty\n",
    "property myprop_seq_match;\n"
    "  (a, b = c);\n"
    "endproperty\n",
    "property myprop_seq_match;\n"
    "  (a && d ##0 q == r, b = c[4:0]);\n"
    "endproperty\n",
    "property myprop_seq_match;\n"
    "  (a, b = c) |-> weak(j);\n"
    "endproperty\n",
    // Test for disambiguation of logical implication -> operator.
    "property run_thing(start_seq, end_state);\n"
    "start_seq\n"
    "|->\n"
    "(response_valid -> yyy)\n"  // -> is logical implication
    "##1\n"
    "end_state;\n"
    "endproperty",
    "property p_P;\n"
    "  pkg::foo_t foo;\n"  // without 'var'
    "  a - 1\n"
    "endproperty\n",
    "property p_P;\n"
    "  var pkg::foo_t foo;\n"  // with 'var'
    "  a - 1\n"
    "endproperty\n",
    "property p_P;\n"
    "  pkg::foo_t foo = bar;\n"  // with assignment
    "  a\n"
    "endproperty\n",
    "property p_P;\n"
    "  pkg::foo_t foo = bar, x = y;\n"  // with assignments
    "  a\n"
    "endproperty\n",
};

static constexpr ParserTestCaseArray kModuleMemberTests = {
    "module mymodule;\n"
    "task subtask;\n"
    "endtask\n"
    "endmodule",
    "module m();\n"
    "  task intf.task1();\n"
    "  endtask\n"
    "endmodule",
    "module mymodule;\n"
    "function integer subroutine;\n"
    "  input a;\n"
    "  subroutine = a+42;\n"
    "endfunction\n"
    "endmodule",
    "module mymodule;\n"
    "function integer[j:k][k:l] subroutine;\n"
    "  input a;\n"
    "  subroutine = a+42;\n"
    "endfunction\n"
    "endmodule",
    "module mymodule;\n"
    "function automatic integer subroutine;\n"
    "  input a;\n"
    "  subroutine = a+42;\n"
    "endfunction\n"
    "endmodule",
    "module mymodule;\n"
    "function bclass::inttype #(16) subroutine;\n"
    "  input a;\n"
    "  subroutine = a+42;\n"
    "endfunction\n"
    "endmodule",
    "module mymodule;\n"
    "function automatic bclass::inttype #(16) subroutine;\n"
    "  input foo::bar #(N) a;\n"
    "  input b;\n"
    "  subroutine = a+41;\n"
    "endfunction\n"
    "endmodule",
    "module mymodule;\n"
    "class myclass;\n"
    "endclass\n"
    "endmodule",
    "module mymodule;\n"
    "module innermod;\n"
    "endmodule\n"
    "endmodule",
    "module mymodule;\n"
    "class myclass;\n"
    "task subtask;\n"
    "endtask\n"
    "endclass\n"
    "endmodule",
    "module mymodule;\n"
    "module innermod;\n"
    "class myclass;\n"
    "task subtask;\n"
    "endtask\n"
    "endclass\n"
    "endmodule\n"
    "endmodule",
    "module mymodule;\n"
    "property myprop;\n"
    "##1 selected |-> a != 0;\n"
    "endproperty\n"
    "endmodule",
    // covergroup tests
    "module mymodule;\n"
    "covergroup err;\n"
    "  fff : coverpoint xx;\n"
    "endgroup\n"
    "endmodule",
    "module mymodule;\n"
    "covergroup err @ (posedge clk);\n"  // event control
    "  c_e : coverpoint ((a == b) && c);\n"
    "endgroup\n"
    "endmodule",
    "module cover_this;\n"
    "covergroup asdf;\n"
    "  c_e : coverpoint ccc;\n"
    "endgroup : asdf\n"  // with end label
    "endmodule",
    "module cover_this;\n"
    "covergroup asdf;\n"
    "  c_e : coverpoint (a == b) && c;\n"
    "endgroup\n"
    "endmodule",
    "module cover_this;\n"
    "covergroup qweasdf;\n"
    "  option.name = \"foobar\";\n"  // with option
    "  cp_e : coverpoint c.d;\n"
    "endgroup\n"
    "endmodule",
    "module cover_this;\n"
    "covergroup qweasdf;\n"
    "  option.name = $sformat(\"%s.world\", hello);\n"  // with option
    "  cp_e : coverpoint c.d;\n"
    "endgroup\n"
    "endmodule",
    "module cover_this;\n"
    "covergroup qweasdf;\n"
    "`ifdef BARFOO\n"
    "  option.name = \"foobar\";\n"  // preprocessor directive
    "`else\n"
    "  option.date = \"tomorrow\";\n"
    "`endif\n"
    "  cp_e : coverpoint c.d;\n"
    "endgroup\n"
    "endmodule",
    "module cover_this;\n"
    "covergroup qweasdf;\n"
    "  option.name = \"foobar\";\n"  // preprocessor directive
    "`ifdef BARFOO\n"
    "  cp_d : coverpoint c.e;\n"
    "`else\n"
    "  cp_e : coverpoint c.d;\n"
    "`endif\n"
    "endgroup\n"
    "endmodule",
    "module cover_this;\n"
    "covergroup qweasdf @ (posedge (x && y.z));\n"
    "  l1 : coverpoint c.d[0] == ns::state1;\n"
    "  l2 : coverpoint c.d[0] == ns::state2;\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings @ (p_coverage);\n"
    "  coverpoint __disable;\n"  // anonymous coverpoint
    "  bar : coverpoint __enable;\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  cross dbi, mask;\n"  // cover_cross
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  _name : cross dbi, mask, mask2;\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  cross_c : cross a, b { }\n"  // empty cross body
    "endgroup\n"
    "endmodule",
    "class foo;\n"
    "covergroup ram_cg;\n"
    "  e_d_c_cp : cross ec.r_inputs, ec.r_outputs\n"  // hierarchical refs
    "    iff(ec.e_type != UNDEF_ERROR);\n"
    "  endgroup\n"
    "endclass",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  _name : cross dbi, mask {\n"
    "    bins garbage = binsof(foo);\n"
    "    ignore_bins g2 = binsof(other_stuff);\n"
    "    illegal_bins g3 = binsof(not_mine);\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  _name : cross dbi, mask {\n"
    "    bins garbage = binsof(foo) || binsof(bar) iff (a-b);\n"
    "    bins lottery = binsof(win) iff ((x));\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  _name : cross dbi, mask {\n"
    "    bins nodm = binsof(win) intersect {0};\n"  // intersect range list
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  _name : cross dbi, mask {\n"
    "    bins nodm = binsof(win) intersect {0} && binsof(zz) intersect {1};\n"
    "    bins nod2 = !binsof(win) intersect {1} || !binsof(zz) intersect {2};\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  _name : cross dbi, mask {\n"
    "    function int foo(int bar);\n"  // function declaration
    "      return bar;\n"
    "    endfunction\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  coverpoint cfgpsr {\n"
    "    bins legal = {[0:12]};\n"
    "    illegal_bins illegal = {4,6};\n"
    "    illegal_bins illegal2 = default;\n"  // default
    "    ignore_bins ignored = {4,6,8,[44:46]};\n"
    "    bins enabled = {[1:$]};\n"
    "    bins other[2] = {[3:6]};\n"  // array decl
    "    bins another[] = {[2:7]};\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup settings;\n"
    "  coverpoint cfgpsr {\n"
    "    bins          en = 4'b0000;\n"
    "    wildcard bins b0 = 4'b???1;\n"  // wildcard
    "    wildcard bins b1 = 4'b??1?;\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup init_this @ (posedge clk);\n"
    "  coverpoint refresh iff (sync == 1'b1) {\n"  // trailing iff
    "    bins          nsr = {4'b0000};\n"
    "    wildcard bins nr1 = {4'b???1} iff (NR > K);\n"  // trailing iff
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup sample_me with function sample ();\n"
    "  coverpoint __disable;\n"
    "endgroup\n"
    "endmodule",
    "module cover_that;\n"
    "covergroup sample_me (bit x, bit y)\n"
    "    with function sample (bit w, bit z);\n"
    "  coverpoint __disable;\n"
    "endgroup\n"
    "endmodule",
    "module module_with_property;\n"
    "property myprop_seq_match;\n"
    "  (a, b = c) |-> weak(j);\n"
    "endproperty\n"
    "endmodule",
    "module module_with_sequence;\n"
    "sequence S_data_matches(valid, obs_data, exp_data);\n"
    "  1 ##[1:$] valid && (obs_data == exp_data);\n"
    "endsequence\n"
    "endmodule",
    // testing preprocessor-balanced coverpoint items (bins_or_options)
    "module cover_that_pp;\n"
    "covergroup settings;\n"
    "  cp : coverpoint cfgpsr {\n"
    "`ifdef GOO\n"
    "`endif\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that_pp;\n"
    "covergroup settings;\n"
    "  cp : coverpoint cfgpsr {\n"
    "`ifndef GOO\n"
    "`endif\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that_pp;\n"
    "covergroup settings;\n"
    "  cp : coverpoint cfgpsr {\n"
    "`ifdef GOO\n"
    "`else\n"
    "`endif\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that_pp;\n"
    "covergroup settings;\n"
    "  cp : coverpoint cfgpsr {\n"
    "`ifdef GOO\n"
    "`elsif TAR\n"
    "`endif\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that_pp;\n"
    "covergroup settings;\n"
    "  cp : coverpoint cfgpsr {\n"
    "`ifdef GOO\n"
    "`elsif TAR\n"
    "`else\n"
    "`endif\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "module cover_that_pp;\n"
    "covergroup settings;\n"
    "  cp : coverpoint cfgpsr {\n"
    "`ifdef GOO\n"
    "    wildcard bins b0 = {4'b???1};\n"
    "    wildcard bins b1 = {4'b??1?};\n"
    "`elsif TAR\n"
    "    wildcard bins b0 = {4'b??11};\n"
    "    wildcard bins b1 = {4'b11??};\n"
    "`else\n"
    "    wildcard bins b0 = {4'b??00};\n"
    "    wildcard bins b1 = {4'b00??};\n"
    "`endif\n"
    "  }\n"
    "endgroup\n"
    "endmodule",
    "covergroup cg() with function\n"
    "  sample(blah blah);\n"
    "  option.bar = 0;\n"
    "endgroup",
    "covergroup cg() with function\n"
    "  sample(blah blah);\n"
    "  `MACRO()\n"  // macro as item
    "endgroup",
    "covergroup cg() with function\n"
    "  sample(blah blah);\n"
    "  `MACRO();\n"  // macro as item
    "endgroup",
    "covergroup cg;\n"
    "  cp : coverpoint foo {\n"
    "    `MACRO();\n"  // macro as item
    "  }\n"
    "endgroup",
};

static constexpr ParserTestCaseArray kClassMemberTests = {
    // member tasks
    "class myclass;\n"
    "task subtask;\n"
    "endtask\n"
    "endclass",
    "class c;\n"
    "  task intf.task1();\n"
    "  endtask\n"
    "endclass",
    "class myclass;\n"
    "extern task subtask(arg_type arg);\n"
    "endclass",
    "class myclass;\n"
    "extern task automatic subtask(arg_type arg);\n"
    "endclass",
    "class myclass;\n"
    "extern virtual task subtask(arg_type arg);\n"
    "endclass",
    "class myclass;\n"
    "pure virtual task pure_task1;\n"
    "pure virtual task pure_task2(arg_type arg);\n"
    "endclass",
    "class myclass;\n"
    "extern protected task subtask(arg_type arg);\n"
    "endclass",
    "class myclass;\n"
    "extern virtual protected task subtask(arg_type arg);\n"
    "endclass",
    "class myclass;\n"
    "extern protected virtual task subtask(arg_type arg);\n"
    "endclass",
    "class myclass;\n"
    "extern local static task subtask(arg_type arg);\n"
    "endclass",
    // nested classes
    "class outerclass;\n"
    "  class innerclass;\n"
    "    class reallyinnerclass;\n"
    "      task subtask;\n"
    "      endtask\n"
    "    endclass\n"
    "  endclass\n"
    "endclass",
    // field variables
    "class myclass;\n"
    "  int buzz_count;\n"
    "endclass",
    "class semaphore;\n"
    "  local chandle p_handle;\n"  // chandle type
    "endclass",
    "class protected_stuff;\n"
    "  protected int count;\n"
    "  protected const int countess = `SSS;\n"
    "  protected var int counter = 0;\n"
    "  protected const var int counted = 1;\n"
    "  protected const myclass::msg_t null_msg = {1'b1, 1'b0};\n"
    "endclass",
    "class c;\n"
    "  uint [x:y] g = 2;\n"  // user-defined type, packed dimensions
    "endclass\n",
    "class c;\n"
    "  uint [x][y] g = 2;\n"  // user-defined type, packed dimensions
    "endclass\n",
    "class c;\n"
    "  uint [x:y] g[y:g] = 2;\n"  // user-defined type, packed+unpacked
                                  // dimensions
    "endclass\n",
    "class c;\n"
    "  foo_pkg::uint [x:y] g = 2;\n"  // user-defined type, packed dimensions
    "endclass\n",
    "class c;\n"
    "  bar#(foo)::uint [x:y] g = 2;\n"  // user-defined type, packed dimensions
    "endclass\n",
    // member functions
    "class myclass;\n"
    "function integer subroutine;\n"
    "  input a;\n"
    "  subroutine = a+42;\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "extern function void subroutine;\n"
    "endclass",
    "class myclass;\n"
    "extern function automatic void subroutine;\n"
    "endclass",
    "class myclass;\n"
    "extern function yourpkg::classy::xtype #(p,q) subr;\n"
    "endclass",
    "class myclass;\n"
    "extern function void subroutine(input bool x);\n"
    "endclass",
    "class myclass;\n"
    "extern function void subr(bool x[N]);\n"
    "endclass",
    "class myclass;\n"
    "extern function void subr(ducktype #(3) x);\n"
    "endclass",
    "class myclass;\n"
    "extern function sometype #(N+1) subr(ducktype #(3) x);\n"
    "endclass",
    "class myclass;\n"
    "extern function sometype #(N+1)[N:0]"
    "subr(ducktype #(3) x);\n"
    "endclass",
    "class myclass;\n"
    "extern function void subr(mypkg::foo y[M]);\n"
    "endclass",
    "class myclass;\n"
    "extern function void subr(mypkg::foo #(4) x[N]);\n"
    "endclass",
    "class myclass;\n"
    "extern virtual function integer subroutine;\n"
    "endclass",
    "class myclass;\n"
    "pure virtual function integer subroutine;\n"
    "pure virtual function integer compute(int a, bit b);\n"
    "endclass",
    "class myclass;\n"
    "virtual function void starter(uvm_phase phase);\n"
    "  report_server new_server = new;\n"  // initialized to class_new
    "endfunction : starter\n"
    "endclass",
    "class myclass;\n"
    "function void shifter;\n"
    "  for ( ; shft_idx < n_bits;\n"
    "       shft_idx++) begin\n"  // no initializations
    "  end\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "function void shifter;\n"
    "  for ( ; shft_idx < n_bits; ) begin\n"  // no for-step
    "  end\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "function void shifter;\n"
    "  for (shft_idx=0, c=1'b1; shft_idx < n_bits;\n"
    "       shft_idx++) begin\n"  // multiple initializations
    "    dout = {dout} << 1;\n"
    "  end\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "function void shifter;\n"
    "  for (int shft_idx=0, bit c=1'b1; shft_idx < n_bits;\n"
    "       shft_idx++) begin\n"  // multiple declarations
    "    dout = {dout} << 1;\n"
    "  end\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "function void shifter;\n"
    "  for (var int shft_idx=1, bit c=1'b0; shft_idx < n_bits;\n"
    "       shft_idx++) begin\n"  // multiple declarations, with 'var'
    "    dout = {dout} << 1;\n"
    "  end\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "function void shifter;\n"
    "  for (int shft_idx=0; shft_idx < n_bits;\n"
    "       shft_idx++, data.width--) begin\n"  // multiple step assignments
    "    dout = {dout} << 1;\n"
    "  end\n"
    "endfunction\n"
    "endclass",
    // should 'virtual' be enclosed?
    "class myclass;\n"
    "virtual function integer subroutine;\n"
    "  input a;\n"
    "  subroutine = a+42;\n"
    "endfunction\n"
    "endclass",
    // virtual interface return type
    "class myclass;\n"
    "function virtual cmd_array_if subroutine();\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "virtual function virtual cmd_array_if subroutine();\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "virtual function virtual interface\n"
    "    cmd_array_if subroutine();\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "virtual function virtual array_if.modport_x subroutine();\n"
    "endfunction\n"
    "endclass",
    // constructor
    "class constructible;\n"
    "function new;\n"
    "endfunction\n"
    "endclass",
    "class constructible;\n"
    "function new ();\n"
    "endfunction\n"
    "endclass",
    "class constructible;\n"
    "function new ();\n"
    "endfunction : new\n"  // end label
    "endclass",
    "class constructible;\n"
    "function new (string name, virtual time_if vif);\n"
    "  this.name = name;\n"
    "endfunction\n"
    "endclass",
    "class constructible;\n"
    "function new (foo::bar name,\n"
    "    virtual interface time_if vif,\n"
    "    baz#(M,N)::foo bar, bit [K:0] b);\n"
    "  this.name = name;\n"
    "endfunction\n"
    "endclass",
    "class constructible;\n"
    "extern function new;\n"
    "endclass",
    "class constructible;\n"
    "extern function new();\n"
    "endclass",
    "class constructible;\n"
    "extern function new(string name, int count);\n"
    "endclass",
    // typedef
    "class fun_with_typedef_members;\n"
    "typedef struct { int i; bool b; } mystruct;\n"
    "typedef enum { RED, GREEN, BLUE } colors;\n"
    "typedef virtual blah_if harness_if;\n"
    "typedef virtual interface blah_if harness_if;\n"
    "typedef virtual blah_if#(N) foo_if;\n"
    "typedef virtual blah_if#(N).modport_id foo_if;\n"
    "endclass",
    // interface
    "class myclass;\n"
    "virtual splinterface grinterface;\n"
    "virtual interface foo_if bar_if;\n"
    "endclass",
    "class myclass;\n"
    "virtual splinterface grinterface, winterface;\n"
    "virtual interface foo_if bar_if, baz_if;\n"
    "endclass",
    "class myclass;\n"
    "virtual hinterface.some_mod_port winterface;\n"
    "virtual interface foo_if #(J,K) bar_if, baz_if;\n"
    "virtual disinterface#(.N(N)).some_mod_port blinterface;\n"
    "endclass",
    // foreach
    "class myclass;\n"
    "function void apply_all();\n"
    "  foreach (foo[i]) begin\n"
    "    y = apply_this(foo[i]);\n"
    "  end\n"
    "endfunction\n"
    "endclass",
    "class myclass;\n"
    "function apkg::num_t apply_all();\n"
    "  foreach (this.foo[i]) begin\n"
    "    y = {y, this.foo[i]};\n"
    "    z = {z, super.bar[i]};\n"
    "  end\n"
    "endfunction\n"
    "endclass",
    // constraints
    "class myclass extends uvm_object;\n"
    "constraint size_c {\n"
    "  A + B <= C;\n"
    "}\n"
    "endclass",
    "class myclass extends uvm_object;\n"
    "constraint size_c {\n"
    "  A + B <= C;\n"
    "};\n"  // extra ';' here is interpreted as a null class_item
    "endclass",
    "class myclass extends uvm_object;\n"
    "static constraint size_c {\n"
    "  soft A inside {[1:10]};\n"
    "  A + B <= C;\n"
    "}\n"
    "endclass",
    "class myclass extends uvm_object;\n"
    "constraint size_c {\n"
    "  soft A inside {[1:10]};\n"
    "  A dist { 1 := 10, [2:9] :/ 80, 10 := 10 };\n"  // distribution
    "  A + B <= C;\n"
    "}\n"
    "endclass",
    "class foo_class extends bar;\n"
    "constraint gain_constraint {\n"
    "  A dist {1 := 90, 0 := 10 };\n"
    "  if (c_g[A:B] == 0) c_g[K:0] == 0;\n"      // if
    "  if (c_g[A:B] == 1) { c_g[K:0] == 1; }\n"  // if block
    "  c_g[A:B] != '1;\n"
    "}\n"
    "endclass",
    "class myclass extends uvm_object;\n"
    "constraint solve_this {\n"
    "  solve x before y;\n"
    "  solve x.z before y.x;\n"
    "  solve x.z, f, g before q, r, y.x;\n"
    "  solve x.z[2], f[1], g before q, r[4], y[3].x;\n"
    "}\n"
    "endclass",
    // net_type_declarations
    "class c;\n"
    "  nettype real foo_wire;\n"
    "endclass\n",
    "class c;\n"
    "  nettype shortreal[2] foo_wire with fire;\n"
    "endclass\n",
    "class c;\n"
    "  nettype foo_pkg::bar_t baz_wire;\n"
    "endclass\n",
    "class c;\n"
    "  nettype foo#(x,y,z)::bar_t baz_wire;\n"
    "endclass\n",
    "class c;\n"
    "  nettype foo#(x,y,z)::bar_t[2:0] baz_wire;\n"
    "endclass\n",
    "class c;\n"
    "  nettype foo#(x,y,z)::bar_t baz_wire with quux;\n"
    "endclass\n",
};

static constexpr ParserTestCaseArray kInterfaceClassTests = {
    "interface class base_ic;\n"
    "endclass",
    "interface class base_ic;\n"
    "endclass : base_ic",  // end label
    "interface class base_ic #(int N = 8, type T = string);\n"
    "endclass",
    "interface class base_ic extends basebase;\n"  // inheritance
    "endclass",
    "interface class base_ic extends base1, base2, base3;\n"  // multiple
    "endclass",
    "interface class base_ic #(int N = 8) extends pkg1::base1, "
    "base2#(N);\n"
    "endclass",
    "interface class base_ic;\n"
    "typedef int[3:0] quartet;\n"
    "typedef string string_type;\n"
    "endclass",
    "interface class base_ic;\n"
    "typedef struct { int i; bool b; } mystruct;\n"
    "typedef enum { RED, GREEN, BLUE } colors;\n"
    "typedef virtual blah_if harness_if;\n"
    "typedef some_class#(3, 2, 1) car_type;\n"
    "endclass",
    "interface class base_ic;\n"
    "parameter N = 2;\n"
    "parameter type T = bit;\n"
    "localparam M = f(bhg::arr[N]) -1;\n"
    "endclass",
    "interface class base_ic;\n"
    "pure virtual task pure_task1;\n"
    "pure virtual task pure_task2(arg_type arg);\n"
    "endclass",
    "interface class base_ic;\n"
    "pure virtual function void pure_task1;\n"
    "pure virtual function string concatenator(string arg);\n"
    "endclass",
    "interface class base_ic;\n"
    "parameter N = 2;\n"
    "typedef some_class#(N) car_type;\n"
    "pure virtual task pure_task1;\n"
    "pure virtual function void pure_task1;\n"
    "endclass",
};

static constexpr ParserTestCaseArray kConstraintTests = {
    "constraint solve_this {\n"
    "  solve x.z before y.x;\n"
    "}",
    "constraint myclass::solve_this {\n"  // out-of-line definition
    "  solve x.z before y.x;\n"
    "}",
    "constraint smooth_operator {\n"
    "  !(v0.op inside {[V_rt:V_rec]} &&\n"  // inside has precedence over &&
    "    v1.op inside {[V_rt:V_rec]});\n"
    "}",
    "constraint solve_this {\n"
    "  !present <-> length == 0;\n"  // <-> operator
    "}",
    "constraint equiv_this {\n"
    "  solve present before length;\n"
    "  length inside {[0:1024]};\n"
    "  !present <-> length == 0;\n"
    "}",
    "constraint order_c {\n"
    "  foreach (producer_h[i]) {\n"  // foreach
    "    producer_h[i] != 0;\n"
    "  }\n"
    "}",
    "constraint order_c {\n"
    "  solve WTN_h before loop_nest.mem_size;\n"
    "  foreach (producer_h[i]) {\n"  // foreach
    "    solve WTN_h before producer_h[i].syncFlagLocation;\n"
    "  }\n"  // solve inside foreach is an extension
    "}",
    // constraint_blocks with preprocessor conditionals
    "constraint solve_this {\n"
    "`ifdef BOO\n"
    "`endif\n"
    "}",
    "constraint solve_this {\n"
    "`ifdef BOO\n"
    "  solve x.z before y.x;\n"
    "`endif\n"
    "}",
    "constraint solve_that {\n"
    "`ifdef BOO\n"
    "  solve z before y;\n"
    "  solve x before z;\n"
    "`endif\n"
    "}",
    "constraint solve_that {\n"
    "`ifdef BOO\n"
    "`else\n"
    "  solve z before y;\n"
    "  solve x before z;\n"
    "`endif\n"
    "}",
    "constraint solve_that {\n"
    "`ifdef BOO\n"
    "  solve z before y;\n"
    "  solve x before z;\n"
    "`else\n"
    "`endif\n"
    "}",
    "constraint solve_that {\n"
    "`ifndef BOO\n"
    "  solve z before y;\n"
    "`elsif FAR\n"
    "`elsif FAR\n"
    "`elsif AWAY\n"
    "  solve x before z;\n"
    "`endif\n"
    "}",
    /* TODO(fangism): allow empty constraint_set
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "  }\n"  // empty constraint_set
    "}",
    */
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "    foo[i] == 0;\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifdef DEFY\n"
    "`endif\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifndef DEFY\n"
    "    foo[i].foo == 0;\n"
    "`endif\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifdef DEFY\n"
    "    foo[i] > 0;\n"
    "    foo[i] < 4;\n"
    "`endif\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifdef DEFY\n"
    "`else\n"
    "    foo[i] > 0;\n"
    "    foo[i] < 4;\n"
    "`endif\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifdef DEFY\n"
    "    foo[i] > 0;\n"
    "    foo[i] < 4;\n"
    "`else\n"
    "`endif\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifdef DEFY\n"
    "    foo[i] > 0;\n"
    "`elsif DENY\n"
    "    foo[i] < 4;\n"
    "`endif\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifdef DEFY\n"
    "    foo[i] > 0;\n"
    "`elsif DENY\n"
    "    foo[i] < 4;\n"
    "`else\n"
    "    foo[i] == 2;\n"
    "`endif\n"
    "  }\n"
    "}",
    "constraint solve_that {\n"
    "  foreach (foo[i]) {\n"
    "`ifdef DEFY\n"
    "    foo[i] > 0;\n"
    "`elsif DENY\n"
    "    foo[i] < 4;\n"
    "`elsif DECRY\n"
    "    foo[i] < 3;\n"
    "`else\n"
    "    foo[i] == 2;\n"
    "`endif\n"
    "  }\n"
    "}",
};

static constexpr ParserTestCaseArray kConfigTests = {
    "config cfg;\n"
    "  design foo.bar;\n"  // library-qualified cell-id
    "endconfig",
    "config cfg;\n"
    "  design foo.bar;\n"
    "endconfig : cfg",  // with end label
    "config cfg;\n"
    "  design foo bar;\n"  // with multiple cell-ids
    "endconfig",
    "config cfg;\n"
    "  design baz foo.bar;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  default liblist larry moe curly;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  instance top.x.y liblist grouch zeppo;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  instance top.x.y use oscar;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  instance top.x.y use oscar : config;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  instance top.x.y use bert.ernie;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz liblist grouch zeppo;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz use red;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz use red.blue;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz use red.blue : config;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  instance top.x.y use bert.ernie;\n"  // multiple config_rule_statement
    "  cell y.zz use red.blue : config;\n"
    "endconfig",
    // various use_clauses with parameters...
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz use .red(blue) : config;\n"  // named parameter
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz use .red(blue), .one(11) : config;\n"  // named parameters
    "endconfig",
    /* TODO(b/124600414): cell-id followed by named parameters
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz use yellow .red(blue) : config;\n"  // named parameter
    "endconfig",
    */
    // preprocess-balanced config rule statements, empty clauses
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "`else\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "`else\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "`elsif ZZZ\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "`elsif ZZZ\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "`elsif ZZZ\n"
    "`else\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "`elsif ZZZ\n"
    "`else\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "`elsif ZZZ\n"
    "`elsif WWW\n"
    "`else\n"
    "`endif\n"
    "endconfig",
    // preprocess-balanced config rule statements, non-empty clauses
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`else\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`else\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`elsif ZZZ\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`elsif ZZZ\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifdef YYY\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`elsif ZZZ\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`else\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`elsif ZZZ\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`else\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`endif\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "`ifndef YYY\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`elsif ZZZ\n"
    "  instance top.x use a.b;\n"
    "  cell y.zz use red;\n"
    "`elsif WWW\n"
    "  cell y.zz use red;\n"
    "  instance top.x use a.b;\n"
    "`else\n"
    "  cell y.zz use red;\n"
    "`endif\n"
    "endconfig",
};

static constexpr ParserTestCaseArray kPrimitiveTests = {
    "primitive ape (output out, input in1, input in2);\n"
    "  table\n"
    "    0:0:0;\n"  // sequential entry
    "  endtable\n"
    "endprimitive",
    "primitive ape (output out, input in1, input in2);\n"
    "  table\n"
    "`ifdef QWERTY\n"
    "    0:0:0;\n"  // sequential entry
    "`endif\n"
    "  endtable\n"
    "endprimitive",
    "primitive ape (out, in1, in2);\n"
    "  output out;\n"
    "  input in1;\n"
    "  input in2;\n"
    "  table\n"
    "    0:0:0;\n"
    "  endtable\n"
    "endprimitive",
    "primitive ape (out, in1, in2);\n"
    "  output out;\n"
    "  input in1;\n"
    "  input in2;\n"
    "  table\n"
    "`include \"prim1.v\"\n"
    "`include \"prim2.v\"\n"
    "  endtable\n"
    "endprimitive",
    /* TODO(b/38347353): mis-lexed input value
    "primitive ape (output out, input in1, input in2);\n"
    "  table\n"
    "    101:X;\n"  // combinational entry
    "  endtable\n"
    "endprimitive",
    */
    "primitive ape (output out, input in1, input in2);\n"
    "  table\n"
    "    1 0 1:X;\n"  // combinational entry
    "  endtable\n"
    "endprimitive",
    "primitive ape (output out, input in1, input in2);\n"
    "  table\n"
    "`ifndef ASDF\n"
    "    1 0 1:X;\n"  // combinational entry
    "`else\n"
    "    1 0 1:1;\n"  // combinational entry
    "`endif\n"
    "  endtable\n"
    "endprimitive",
    "primitive ZZSRDFF1e(Q,D,CK,SET,RST);\n"
    "  input D,CK,SET,RST;\n"
    "  output Q;\n"
    "  reg Q;\n"
    "  table\n"
    "  // D  CK  SET RST : curr :  Q\n"
    "  `ifdef TTTT\n"
    "     0   r   0   0  :  ?   :  0 ;\n"
    "     1   r   0   0  :  ?   :  1 ;\n"
    "     ?   f   0   0  :  ?   :  - ;\n"
    "  `else\n"
    "     0   p   0   0  :  ?   :  0 ;\n"
    "     1   p   0   0  :  ?   :  1 ;\n"
    "     ?   n   0   0  :  ?   :  - ;\n"
    "  `endif\n"
    "     *   ?   0   0  :  ?   :  - ;\n"
    "     ?   ?   0   1  :  ?   :  0 ;\n"
    "     ?   ?   1   0  :  ?   :  1 ;\n"
    "  endtable\n"
    "endprimitive",
};

static constexpr ParserTestCaseArray kDisciplineTests = {
    "discipline d;\n"
    "enddiscipline\n",
    "discipline d \n"  // no ';'
    "enddiscipline\n",
    "discipline d;\n"
    "  domain discrete;\n"
    "enddiscipline\n",
    "discipline d;\n"
    "  domain continuous;\n"
    "enddiscipline\n",
    "discipline d;\n"
    "  potential pot;\n"
    "enddiscipline\n",
    "discipline d;\n"
    "  flow joe;\n"
    "enddiscipline\n",
};

static constexpr ParserTestCaseArray kMultiBlockTests = {
    // check for correct scope switching around timescale and module
    "module modular_thing1;\n"
    "endmodule\n"
    "`timescale 10 ps / 10 ps\n"  // with spaces before unit
    "module modular_thing2;\n"
    "endmodule",
    "function void foo;\n"
    "endfunction\n"
    "task snoozer;\n"
    "endtask\n"
    "function void zoo;\n"
    "endfunction\n"
    "task floozer;\n"
    "endtask\n",
    "class foo;\n"
    "endclass\n"
    "module bar;\n"
    "endmodule\n"
    "class baz;\n"
    "endclass\n"
    "interface ick;\n"
    "endinterface\n",
    "module bar;\n"
    "  function void zoo;\n"
    "  endfunction\n"
    "task loozer;\n"
    "  endtask\n"
    "endmodule\n"
    "class scar;\n"
    "  function void zoo;\n"
    "  endfunction\n"
    "task loozer;\n"
    "  endtask\n"
    "endclass\n"};

static constexpr ParserTestCaseArray kRandSequenceTests = {
    // randsequence inside initial block
    "module foo();\n"
    "initial begin\n"
    "  randsequence(main)\n"
    "    main: A;\n"
    "    A: {$display(\"1\");};\n"
    "  endsequence\n"
    "end\n"
    "endmodule\n",
    // randsequence inside task
    "task body();\n"
    "  randsequence(main)\n"
    "    main: A B;\n"
    "    A: {$display(\"1\");};\n"
    "    B: {$display(\"2\");};\n"
    "  endsequence\n"
    "endtask\n",
    // randsequence inside function
    "function int body();\n"
    "  int [$] q;\n"
    "  randsequence(main)\n"
    "    main: A B;\n"
    "  endsequence\n"
    "endfunction\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: A C | B;\n"  // testing '|'
    "    A: {$display(\"1\");};\n"
    "    B: {$display(\"2\");};\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: A B;\n"
    "    A: {func_1();};\n"  // different statement types
    "    B: {g = 4;};\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: rand join foo bar;\n"  // rand join, without value
    "    foo: A B C;\n"
    "    bar: D E;\n"
    "    A: {task_0();};\n"
    "    B: {task_1();};\n"
    "    C: {task_2();};\n"
    "    D: {task_3();};\n"
    "    E: {task_4();};\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: rand join(0.0) foo bar;\n"  // rand join with value
    "    foo: A B;\n"
    "    bar: D E;\n"
    "    A: {task_0();};\n"
    "    B: {task_1();};\n"
    "    D: {task_3();};\n"
    "    E: {task_4();};\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: rand join(1.0) foo bar;\n"  // rand join with value
    "    foo: B C;\n"
    "    bar: E F;\n"
    "    B: {task_1();};\n"
    "    C: {task_2();};\n"
    "    E: {task_4();};\n"
    "    F: {task_5();};\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: foo bar := 3;\n"  // production with random weight
    "    foo: {task_1();};\n"
    "    bar: {task_2();};\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: foo := 2 {}\n"    // optional code block
    "        | bar := (1+4);\n"  // production with random weights
    "    foo: {task_1();};\n"
    "    bar: {task_2();};\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: if (depth < 2) push;\n"  // conditional
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: if (depth < 2) push else pop;\n"  // conditional, with else
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence()\n"
    "    select: case (device & 7)\n"  // case statement
    "      0: NETWORK;\n"
    "    endcase;\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence()\n"
    "    select: case (device & 7)\n"
    "      1,2: MEMORY;\n"    // multiple case item expressions
    "      default: DISK;\n"  // default item
    "    endcase;\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence()\n"
    "    select: case (device & 7)\n"
    "      1: MEMORY;\n"
    "      2: DISK;\n"
    "      default DISK;\n"  // default item (no colon)
    "    endcase;\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: repeat ($urandom_range(2, 6)) push;\n"  // loop, random
    "    push: repeat (3) fall;\n"                     // loop, constant
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: { if (depth > 2) break; else return; };\n"  // break/return
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: gen(\"push\");\n"  // parameterized production
    "    gen(string s = \"x\"): { $display(s); };\n"
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    void main: value op value;\n"                // void type
    "    bit [3:0] value: { return $urandom(); };\n"  // array type
    "    string op: add := 5 { return \"+\"; }\n"     // string type
    "             | sub := 3 { return \"-\"; };\n"
    "    foobar [3:0] value: { };\n"  // user-defined return type
    "  endsequence\n"
    "endtask\n",
    "task body();\n"
    "  randsequence(main)\n"
    "    main: gen(2);\n"
    "    int gen(int s): { return s + 1; };\n"  // parameterized, with return
    "  endsequence\n"
    "endtask\n",
};

static constexpr ParserTestCaseArray kNetAliasTests = {
    "module byte_swap (inout wire [31:0] A, inout wire [31:0] B);\n"
    "  alias {A[7:0],A[15:8],A[23:16],A[31:24]} = B;\n"
    "endmodule\n",

    "module byte_rip (inout wire [31:0] W, inout wire [7:0] LSB, MSB);\n"
    "  alias W[7:0] = LSB;\n"
    "  alias W[31:24] = MSB;\n"
    "endmodule\n",

    "module lib1_dff(Reset, Clk, Data, Q, Q_Bar);\n"
    "endmodule\n"
    "module lib2_dff(reset, clock, data, q, qbar);\n"
    "endmodule\n"
    "module lib3_dff(RST, CLK, D, Q, Q_);\n"
    "endmodule\n"
    "module my_dff(rst, clk, d, q, q_bar);\n"
    "  input rst, clk, d;\n"
    "  output q, q_bar;\n"
    "  alias rst = Reset = reset = RST;\n"
    "  alias clk = Clk = clock = CLK;\n"
    "  alias d = Data = data = D;\n"
    "  alias q = Q;\n"
    "  alias Q_ = q_bar = Q_Bar = qbar;\n"
    "endmodule\n",

    "module foo;\n"
    "  logic a, b, c, d, e, f, g, h;\n"
    "  alias a = b = c = d = e = f = g = h;\n"
    "endmodule;\n",
};

// These tests target LRM Ch. 33
static constexpr ParserTestCaseArray kLibraryTests = {
    ";\n",
    "library foolib bar;\n",
    "library foolib *.bar;\n",
    "library foolib ?.bar;\n",
    "library foolib /bar;\n",
    "library foolib bar/;\n",
    "library foolib ../bar;\n",
    "library foolib ../bar/;\n",
    "library foolib */bar;\n",
    "library foolib b/a/r;\n",
    "library foolib /b/a/r/;\n",
    "library foolib b/a/r/*.v;\n",
    "library foolib b/a/r/*.vg;\n",
    "library foolib *.v,*.vg;\n",
    "library  foolib  *.v , *.vg ; \n",
    "library foolib foo/*.v -incdir bar; \n",
    "library foolib foo/*.v -incdir bar, baz; \n",
    "library foolib bar1;\n"
    "library foolib bar2;\n",
    "include foo/*.bar;\n",
    "include /foo/*.bar;\n",
    "include /foo/bar/;\n",
    "include foo/*.bar;\n"
    "include bar/*.foo;\n",
    // most of config_declaration syntax already covered in kConfigTests
    "config cfg;\n"
    "  design foo.bar;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  instance top.x.y use bert.ernie;\n"
    "endconfig",
    "config cfg;\n"
    "  design foo.bar baz;\n"
    "  cell y.zz liblist grouch zeppo;\n"
    "endconfig",
    // mixed, all valid library_description elements
    "library foolib foo/lib/*.v;\n"
    "include foo/inc/;\n"
    "config cfg;\n"
    "  design foo.bar;\n"
    "endconfig\n",
};

// In the positions where we specify a token via {enum, string} or single
// character like 'x', is where we expect an error to be found.
// All other string literals are only used for concatenating the test case's
// text to be analyzed.
static const std::initializer_list<ParserTestData> kInvalidCodeTests = {
    {"function ",
     {TK_module, "module"},
     "\n"
     "end\n"},
    {"class ",
     {TK_DecNumber, "123"},
     ";\n"
     "endclass\n"},
    {{TK_StringLiteral, "\"class\""}, " foo;\n"},
    // cannot mix positional and named parameter arguments:
    {"class foo;\n"
     "fizz #(buzz, ",
     '.',  // expect error on '.'
     "pop(1)) bar;\n"
     "endclass\n"},
    // cannot mix positional and named parameter arguments:
    {"class foo;\n"
     "fizz #(.buzz(\"bzz\"), ",
     {TK_DecNumber, "1"},
     ") bar;\n"
     "endclass\n"},
    {"module foo;",  // missing endmodule
     {verible::TK_EOF, ""}},
    {"module foo;\n",  // missing endmodule
     {verible::TK_EOF, ""}},
    {"module foo(aaa, ",
     ')',  // trailing comma, unexpected )
     ";\nendmodule\n"},
    {"module foo(",
     ',',  // unexpected ,
     "bbb);\nendmodule\n"},
    {"module foo #(int aaa=7, ",
     ')',  // trailing comma, unexpected )
     ";\nendmodule\n"},
    {"module foo #(",
     ',',  // unexpected ,
     " int bbb);\nendmodule\n"},
    {"function void bar(",
     ',',  // blank before comma
     " int a);\nendfunction\n"},
    {"function void bar(int a,",
     ')',  // trailing comma, unexpected )
     ";\nendfunction\n"},
    {"enum foo{bar,",
     '}',  // trailing comma, unexpected }
     " baz;\n"},
    {"enum foo{",
     ',',  // blank before comma
     " bar} baz;\n"},
    // preprocessor imbalance
    {"function void foobar;\n",
     {PP_else, "`else"},  // no matching `ifdef/`ifndef
     "\n"
     "  return 2;\n"
     "endfunction\n"},
    {"function void foobar;\n",
     {PP_elsif, "`elsif"},  // no matching `ifdef/`ifndef
     "\n"
     "  return 2;\n"
     "endfunction\n"},
    {"function void foobar;\n",
     {PP_endif, "`endif"},  // no matching `ifdef/`ifndef
     "\n"
     "  return 2;\n"
     "endfunction\n"},
    {"function void foobar;\n"
     "`ifndef FOOBAR\n"
     "  return 0;\n"
     "`endif\n",
     {PP_endif, "`endif"},  // unbalanced `endif
     "\n"
     "  return 2;\n"
     "endfunction\n"},
    {"module unbalanced(\n",
     {PP_else, "`else"},  // no matching `ifdef/`ifndef
     " // blah\n"
     "input x)\n"
     "endmodule\n"},
    {"module unbalanced(\n",
     {PP_elsif, "`elsif"},  // no matching `ifdef/`ifndef
     " BLAH\n"
     "input x)\n"
     "endmodule\n"},
    {"module unbalanced(\n"
     "`ifdef SIM\n"
     "input x",
     ')',  // close ) before balanced `ifdef
     "\n"
     "`else\n"
     "input y)\n"
     "`endif\n"
     ";\n"
     "endmodule\n"},
    {"module unbalanced;\n"
     "generate\n",
     {PP_else, "`else"},  // no matching `ifdef/`ifndef
     " // blah\n"
     "endgenerate\n"
     "endmodule\n"},
    {"module unbalanced;\n"
     "generate\n",
     {PP_elsif, "`elsif"},  // no matching `ifdef/`ifndef
     " BLAH\n"
     "endgenerate\n"
     "endmodule\n"},
    {"module unbalanced;\n"
     "generate\n",
     {PP_endif, "`endif"},  // no matching `ifdef/`ifndef
     "\n"
     "endgenerate\n"
     "endmodule\n"},
    {"class imbalanced;\n"
     "int foo;\n",
     {PP_else, "`else"},  // unmatched `else
     "\n"
     "endclass"},
    {"class imbalanced;\n"
     "int foo;\n",
     {PP_elsif, "`elsif"},  // unmatched `elsif
     " FOO\n"
     "endclass"},
    {"class imbalanced;\n"
     "int foo;\n",
     {PP_endif, "`endif"},
     "\n"  // unmatched
     "endclass"},
    {"module unbalanced;\n",
     {PP_elsif, "`elsif"},  // no matching `ifdef/`ifndef
     " BLAH\n"
     "endmodule\n"},
    {"module unbalanced;\n",
     {PP_else, "`else"},  // no matching `ifdef/`ifndef
     "  // blah\n"
     "endmodule\n"},
    {"module unbalanced;\n",
     {PP_endif, "`endif"},  // no matching `ifdef/`ifndef
     "  // blah\n"
     "endmodule\n"},
    {"module unbalanced;\n",
     {PP_endif, "`endif"},  // no matching `ifdef/`ifndef
     "  // blah\n"
     "endmodule\n"},
    {"class imbalanced;\n"
     "int foo;\n",
     {PP_else, "`else"},  // unmatched `else
     "\n"
     "endclass"},
    {"class imbalanced;\n"
     "int foo;\n",
     {PP_elsif, "`elsif"},
     " FOO\n"  // unmatched
     "endclass"},
    {"class imbalanced;\n"
     "int foo;\n",
     {PP_endif, "`endif"},  // unmatched `endif
     "\n"
     "endclass"},
    {"package imbalanced;\n"
     "int foo;\n",
     {PP_else, "`else"},  // unmatched `else
     "\n"
     "endpackage"},
    {"package imbalanced;\n"
     "int foo;\n",
     {PP_elsif, "`elsif"},  // unmatched `elsif
     " FOO\n"
     "endpackage"},
    {"package imbalanced;\n"
     "int foo;\n",
     {PP_endif, "`endif"},  // unmatched `endif
     "\n"
     "endpackage"},
    {"task bar;\n"
     "@e foo:\n"
     "`baz\n",
     {verible::TK_EOF, ""}},  // missing endtask before EOF
    {"`define F",             // missing newline
     {verible::TK_EOF, ""}},
    // invalid DPI import
    {"import \"DPI-C\" foo", /* rejected */ ';'},
    {"task t;\n"
     "for (uint32_t fn",
     /* = initializer expression required here */ ';',
     " fn < a; fn++) begin\n"
     "end\n"
     "endtask\n"},
    // unbalanced `endif
    {{PP_endif, "`endif"}},
    {{PP_endif, "`endif"}, "\n"},
    // The following tests are valid library map syntax (LRM Ch. 33),
    // but invalid for the rest of SystemVerilog:
    {{TK_library, "library"}, " foo bar;\n"},
    {{TK_include, "include"}, " foo/bar/*.v;\n"},
    // fuzzer-discovered cases: (these may have crashed at one point in history)
    {"`g((\\x\" `g(::\"\n"
     "),",
     {verible::TK_EOF, ""}},
    // members may only be unqualified (unlike C++)
    {"function int bad;\n"
     "  return x.y",
     {TK_SCOPE_RES, "::"},
     "z;\n"
     "endfunction\n"},
    {{TK_endprimitive, "endprimitive"}},
    {"//www\n", {TK_endprimitive, "endprimitive"}},
    {"module m;\n"
     "  if ((",
     {')', ")"},  // empty paren is invalid expression
     "foo());"
     "\n"
     "endmodule\n"},
    {"[i()",  // unexpected EOF
     {verible::TK_EOF, ""}},
    {"[int'",  // unexpected EOF
     {verible::TK_EOF, ""}},
};

using verible::LeafTag;
// All enums in the following section refer to enum class NodeEnum.
#define NodeTag(tag) verible::NodeTag(NodeEnum::tag)

// These test cases check that error-recovery produces partially formed
// syntax trees that can still be analyzed.
static const verible::ErrorRecoveryTestCase kErrorRecoveryTests[] = {
    // module_item error tests
    {"module foo;\n"
     "wire 123;\n"  // rejects '123'
     "endmodule\n",
     {NodeTag(kModuleDeclaration)}},
    {"module foo;\n"
     "wire abc;\n"
     "wire 123;\n"  // rejects '123'
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kNetDeclaration)}},
    {"module foo;\n"
     "wire 123;\n"  // rejects '123'
     "wire abc;\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kNetDeclaration)}},
    {"module foo;\n"
     "wire wire;\n"  // rejects second 'wire'
     "initial begin\n"
     "  x <= y;\n"
     "end\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kInitialStatement)}},
    {"module foo;\n"
     "`ifdef ERROR\n"  // inside conditional block
     "wire 123;\n"     // rejects '123'
     "`endif  // ERROR\n"
     "wire abc;\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kNetDeclaration)}},
    // generate_item error tests
    {"module foo;\n"
     "generate\n"
     "  123 foo;\n"  // rejects '123'
     "endgenerate\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kGenerateRegion), NodeTag(kGenerateItemList)}},
    {"module foo;\n"
     "generate\n"
     "  class class;\n"  // rejects 'class'
     "endgenerate\n"
     "wire xx;\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kNetDeclaration)}},
    {"module foo;\n"
     "wire x;\n"
     "generate\n"
     "if (a & b) begin\n"
     "  123 b(q);\n"  // rejects '123'
     "end\n"
     "endgenerate\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kGenerateRegion), NodeTag(kGenerateItemList),
      NodeTag(kConditionalGenerateConstruct), NodeTag(kGenerateIfClause),
      NodeTag(kGenerateIfBody)}},
    // function/task statement_item error tests
    {"function void foobar(int a, int b);\n"
     "  8 9;\n"  // expression error
     "  return a -b;\n"
     "endfunction\n",
     {NodeTag(kFunctionDeclaration), NodeTag(kBlockItemStatementList),
      NodeTag(kJumpStatement)}},
    {"task automatic barfoo;\n"
     "  $display(\"moo\");;\n"
     "  5+5;\n"  // statement error
     "  c <= d;\n"
     "endtask\n",
     {NodeTag(kTaskDeclaration), NodeTag(kStatementList),
      NodeTag(kNonblockingAssignmentStatement)}},
    {"module bar;\n"
     "task automatic barfoo;\n"
     "  $display(\"moo\");;\n"
     "  5+4;\n"  // statement error
     "  c <= d;\n"
     "endtask\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kTaskDeclaration), NodeTag(kStatementList),
      NodeTag(kNonblockingAssignmentStatement)}},
    // class_item errors
    {"class asdf;\n"
     "wire foo;\n"  // error on 'wire'
     "task blah;\n"
     "endtask\n"
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kTaskDeclaration)}},
    {"class asdf;\n"
     "foo var;\n"  // error on 'var'
     "function automatic void blah;\n"
     "endfunction\n"
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kFunctionDeclaration)}},
    {"class asdf;\n"
     "`ifdef VARRRR\n"  // error inside conditional block
     "foo var;\n"       // error on 'var'
     "`endif\n"
     "function automatic void blah;\n"
     "endfunction\n"
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kFunctionDeclaration)}},
    // package_item errors
    {"package fedex;\n"
     "11 22 33;\n"
     "class qwer;\n"
     "endclass\n"
     "endpackage\n",
     {NodeTag(kPackageDeclaration), NodeTag(kPackageItemList),
      NodeTag(kClassDeclaration)}},
    {"package fedex;\n"
     "`ifndef FAIL_INSIDE\n"
     "11 22 33;\n"  // error inside conditional block
     "`endif\n"
     "class qwer;\n"
     "endclass\n"
     "endpackage\n",
     {NodeTag(kPackageDeclaration), NodeTag(kPackageItemList),
      NodeTag(kClassDeclaration)}},
    {"11 22 33;\n"
     "class qwer;\n"
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassHeader),
      LeafTag(SymbolIdentifier)}},
    {"task t;\n"
     "if (;c());\n"  // error on the first ';'
     "endtask\n",
     {NodeTag(kTaskDeclaration), NodeTag(kStatementList),
      NodeTag(kConditionalStatement), NodeTag(kIfClause), NodeTag(kIfHeader)}},
    {"module m;\n"
     "if (a+);\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kConditionalGenerateConstruct), NodeTag(kGenerateIfClause),
      NodeTag(kGenerateIfHeader)}},
    {"class c;\n"
     "  `BAD function new();\n"  // real problem here is the macro,
     // but 'function' keyword gets rejected because it is only then that
     // there is conclusively an error.
     // From error-recovery, this entire function/constructor declaration will
     // be dropped.
     "  endfunction\n"
     // recovered from here onward
     "  int count;\n"  // this data declaration will be recovered and saved
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kDataDeclaration)}},
    {"class c;\n"
     "  `BAD task rabbit();\n"  // real problem here is the macro,
     // but 'task' keyword gets rejected because it is only then that
     // there is conclusively an error.
     // From error-recovery, this entire function/constructor declaration will
     // be dropped.
     "  endtask\n"
     // recovered from here onward
     "  int count;\n"  // this data declaration will be recovered and saved
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kDataDeclaration)}},
    {"class c;\n"
     "  `BAD task rabbit();\n"  // real problem here is the macro
     "  endtask\n"
     // recovered from here onward
     "    static function bit r();\n"
     "    if (m == null) m = new();\n"
     "    uvm_resource#(T)::m_set_converter(m_singleton);\n"
     "  endfunction\n"
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kFunctionDeclaration), NodeTag(kBlockItemStatementList),
      NodeTag(kConditionalStatement), NodeTag(kIfClause)}},
    {"class c;\n"
     "  `BAD covergroup cg;\n"  // real problem here is the macro
     "  endgroup\n"             // this covergroup will be lost
     // recovered from here onward
     "  int count;\n"  // this data declaration will be recovered and saved
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kDataDeclaration)}},
    {"class c;\n"
     "  covergroup cg;\n"
     "    cp: coverpoint foo.bar {\n"
     "      123;\n"  // syntax error here
     // recovered from here onward
     "    }\n"
     "  endgroup\n"
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kCovergroupDeclaration)}},
    {"class c;\n"
     "  covergroup cg;\n"
     "    cp: coverpoint foo.bar {\n"
     "      --\n"  // syntax error here
     // recovered from here onward
     "    }\n"
     "  endgroup\n"
     "endclass\n",
     {NodeTag(kClassDeclaration), NodeTag(kClassItems),
      NodeTag(kCovergroupDeclaration)}},
    {"module m;\n"
     "  foo;\n"  // invalid syntax, recover from here
     "  wire w;\n"
     "endmodule\n",
     {NodeTag(kModuleDeclaration), NodeTag(kModuleItemList),
      NodeTag(kNetDeclaration)}},
};
#undef NodeTag

namespace {

// Tests that valid code is accepted.
static void TestVerilogParser(const ParserTestCaseArray &data) {
  int i = 0;
  for (const auto &code : data) {
    verible::TestParserAcceptValid<VerilogAnalyzer>(code, i);
    ++i;
  }
}

static void TestVerilogLibraryParser(const ParserTestCaseArray &data) {
  int i = 0;
  for (const auto &code : data) {
    VLOG(1) << "test_data[" << i << "] = '" << code << "'\n";
    // TODO(fangism): refactor TestParserAcceptValid to accept a lambda
    const auto analyzer =
        AnalyzeVerilogLibraryMap(code, "<<inline-test>>", kDefaultPreprocess);
    const absl::Status status = ABSL_DIE_IF_NULL(analyzer)->ParseStatus();
    if (!status.ok()) {
      // Print more detailed error message.
      const auto &rejected_tokens = analyzer->GetRejectedTokens();
      if (!rejected_tokens.empty()) {
        EXPECT_TRUE(status.ok())
            << "Rejected valid code:\n"
            << code << "\nRejected token: " << rejected_tokens[0].token_info;
      } else {
        EXPECT_TRUE(status.ok()) << "Rejected valid code:\n" << code;
      }
    }
    EXPECT_TRUE(analyzer->SyntaxTree().get()) << "Missing tree on code:\n"
                                              << code;
    ++i;
  }
}

// Tests that invalid code is rejected.
static void TestVerilogParserReject(
    std::initializer_list<ParserTestData> data) {
  int i = 0;
  for (const auto &test_case : data) {
    verible::TestParserRejectInvalid<VerilogAnalyzer>(test_case, i);
    ++i;
  }
}

template <int N>
static void TestVerilogParserErrorRecovery(
    const verible::ErrorRecoveryTestCase (&data)[N]) {
  int i = 0;
  for (const auto &test : data) {
    verible::TestParserErrorRecovered<VerilogAnalyzer>(test, i);
    ++i;
  }
}

static void TestVerilogParserMatchAll(const ParserTestCaseArray &data) {
  int i = 0;
  for (const auto &code : data) {
    verible::TestParserAllMatched<VerilogAnalyzer>(code, i);
    ++i;
  }
}

static void TestVerilogLibraryParserMatchAll(const ParserTestCaseArray &data) {
  using verible::Symbol;
  int i = 0;
  for (const auto &code : data) {
    VLOG(1) << "test_data[" << i << "] = '" << code << "'\n";

    // TODO(fangism): refactor TestParserAllMatched to accept a lambda
    const auto analyzer =
        AnalyzeVerilogLibraryMap(code, "<<inline-test>>", kDefaultPreprocess);
    const absl::Status status = ABSL_DIE_IF_NULL(analyzer)->ParseStatus();
    EXPECT_TRUE(status.ok())
        << status.message()
        << "\nRejected: " << analyzer->GetRejectedTokens().front().token_info;

    const Symbol *tree_ptr = analyzer->SyntaxTree().get();
    EXPECT_NE(tree_ptr, nullptr) << "Missing syntax tree with input:\n" << code;
    if (tree_ptr == nullptr) return;  // Already failed, abort this test case.
    const Symbol &root = *tree_ptr;

    verible::ParserVerifier verifier(root,
                                     analyzer->Data().GetTokenStreamView());
    const auto unmatched = verifier.Verify();

    EXPECT_EQ(unmatched.size(), 0)
        << "On code:\n"
        << code << "\nFirst unmatched token: " << unmatched.front();
    ++i;
  }
}

// Tests on valid code.
TEST(VerilogParserTest, LexerFilter) { TestVerilogParser(kLexerFilterTests); }
TEST(VerilogParserTest, Empty) { TestVerilogParser(kEmptyTests); }
TEST(VerilogParserTest, Preprocessor) { TestVerilogParser(kPreprocessorTests); }
TEST(VerilogParserTest, Numbers) { TestVerilogParser(kNumberTests); }
TEST(VerilogParserTest, Classes) { TestVerilogParser(kClassTests); }
TEST(VerilogParserTest, Functions) { TestVerilogParser(kFunctionTests); }
TEST(VerilogParserTest, Tasks) { TestVerilogParser(kTaskTests); }
TEST(VerilogParserTest, Modules) { TestVerilogParser(kModuleTests); }
TEST(VerilogParserTest, ModuleInstances) {
  TestVerilogParser(kModuleInstanceTests);
}
TEST(VerilogParserTest, Interfaces) { TestVerilogParser(kInterfaceTests); }
TEST(VerilogParserTest, Typedefs) { TestVerilogParser(kTypedefTests); }
TEST(VerilogParserTest, Structs) { TestVerilogParser(kStructTests); }
TEST(VerilogParserTest, Unions) { TestVerilogParser(kUnionTests); }
TEST(VerilogParserTest, Enums) { TestVerilogParser(kEnumTests); }
TEST(VerilogParserTest, Packages) { TestVerilogParser(kPackageTests); }
TEST(VerilogParserTest, Description) { TestVerilogParser(kDescriptionTests); }
TEST(VerilogParserTest, Constraints) { TestVerilogParser(kConstraintTests); }
TEST(VerilogParserTest, Properties) { TestVerilogParser(kPropertyTests); }
TEST(VerilogParserTest, Sequences) { TestVerilogParser(kSequenceTests); }
TEST(VerilogParserTest, ModuleMembers) {
  TestVerilogParser(kModuleMemberTests);
}
TEST(VerilogParserTest, ClassMembers) { TestVerilogParser(kClassMemberTests); }
TEST(VerilogParserTest, InterfaceClass) {
  TestVerilogParser(kInterfaceClassTests);
}
TEST(VerilogParserTest, Configs) { TestVerilogParser(kConfigTests); }
TEST(VerilogParserTest, Primitives) { TestVerilogParser(kPrimitiveTests); }
TEST(VerilogParserTest, Disciplines) { TestVerilogParser(kDisciplineTests); }
TEST(VerilogParserTest, MultiBlockTests) {
  TestVerilogParser(kMultiBlockTests);
}
TEST(VerilogParserTest, RandSequenceTests) {
  TestVerilogParser(kRandSequenceTests);
}
TEST(VerilogParserTest, Aliases) { TestVerilogParser(kNetAliasTests); }
TEST(VerilogParserTest, Library) { TestVerilogLibraryParser(kLibraryTests); }

// Tests on invalid code.
TEST(VerilogParserTest, InvalidCode) {
  TestVerilogParserReject(kInvalidCodeTests);
}

// Tests on invalid code.
TEST(VerilogParserTest, InvalidCodePerformance) {
  const char piece1[] = "bar\n";
  const auto piece2 = absl::StrCat("//foo", std::string(500000, '\0'));
  const std::initializer_list<ParserTestData> kLocalInvalidCodeTests = {
      {piece1, {verible::TK_EOF, piece2}},
  };
  TestVerilogParserReject(kLocalInvalidCodeTests);
}

// Tests for parser recovery from syntax errors.
TEST(VerilogParserTest, ErrorRecovery) {
  TestVerilogParserErrorRecovery(kErrorRecoveryTests);
}

// Test parser's internal stack reallocation.
TEST(VerilogParserTest, InternalStackRealloc) {
  // Construct an input that will grow the symbol stack.
  std::string code("module foo;\ninitial\n");
  constexpr int depth = YYINITDEPTH * 8;
  for (int i = 0; i < depth; ++i) {
    code += "begin\n";
  }
  for (int i = 0; i < depth; ++i) {
    code += "end\n";
  }
  code += "endmodule\n";
  VerilogAnalyzer analyzer(code, "<<inline-text>>");
  auto status = analyzer.Analyze();
  EXPECT_TRUE(status.ok()) << "Unexpected failure on code: " << code;
  const size_t max_stack_size = analyzer.MaxUsedStackSize();
  EXPECT_LE(depth, max_stack_size);
}

// Tests that Tokenize() properly sets the range of the EOF token.
TEST(VerilogParserTest, TokenizeTerimnatesEOFRange) {
  constexpr std::string_view kCode[] = {
      "",       "\t",       "\n",          "\n\n",
      "module", "module\n", "module foo;", "module foo;\n",
  };
  for (const auto code : kCode) {
    VerilogAnalyzer analyzer(code, "<<inline-text>>");
    auto status = analyzer.Tokenize();
    EXPECT_TRUE(status.ok());
    const auto &tokens = analyzer.Data().TokenStream();
    ASSERT_FALSE(tokens.empty());
    const auto &eof_token = tokens.back();
    EXPECT_EQ(eof_token.token_enum(), verible::TK_EOF);
    const auto eof_text = eof_token.text();
    const auto &contents = analyzer.Data().Contents();
    EXPECT_EQ(eof_text.begin(), contents.end());
    EXPECT_EQ(eof_text.end(), contents.end());
  }
}

// Tests to ensure valid code is completely parsed into tree
// Failing these tests mean that some tokens were dropped and not
// incorporated into the tree.
TEST(VerilogParserTestMatchAll, Empty) {
  TestVerilogParserMatchAll(kEmptyTests);
}

TEST(VerilogParserTestMatchAll, Preprocessor) {
  TestVerilogParserMatchAll(kPreprocessorTests);
}

TEST(VerilogParserTestMatchAll, LexerFilter) {
  TestVerilogParserMatchAll(kLexerFilterTests);
}

TEST(VerilogParserTestMatchAll, Numbers) {
  TestVerilogParserMatchAll(kNumberTests);
}

TEST(VerilogParserTestMatchAll, Classes) {
  TestVerilogParserMatchAll(kClassTests);
}

TEST(VerilogParserTestMatchAll, Functions) {
  TestVerilogParserMatchAll(kFunctionTests);
}

TEST(VerilogParserTestMatchAll, Tasks) {
  TestVerilogParserMatchAll(kTaskTests);
}

TEST(VerilogParserTestMatchAll, Modules) {
  TestVerilogParserMatchAll(kModuleTests);
}

TEST(VerilogParserTestMatchAll, Interfaces) {
  TestVerilogParserMatchAll(kInterfaceTests);
}

TEST(VerilogParserTestMatchAll, Typedefs) {
  TestVerilogParserMatchAll(kTypedefTests);
}

TEST(VerilogParserTestMatchAll, Structs) {
  TestVerilogParserMatchAll(kStructTests);
}

TEST(VerilogParserTestMatchAll, Enums) {
  TestVerilogParserMatchAll(kEnumTests);
}

TEST(VerilogParserTestMatchAll, Unions) {
  TestVerilogParserMatchAll(kUnionTests);
}

TEST(VerilogParserTestMatchAll, Packages) {
  TestVerilogParserMatchAll(kPackageTests);
}

TEST(VerilogParserTestMatchAll, Description) {
  TestVerilogParserMatchAll(kDescriptionTests);
}

TEST(VerilogParserTestMatchAll, Sequences) {
  TestVerilogParserMatchAll(kSequenceTests);
}

TEST(VerilogParserTestMatchAll, Property) {
  TestVerilogParserMatchAll(kPropertyTests);
}

TEST(VerilogParserTestMatchAll, ModuleMembers) {
  TestVerilogParserMatchAll(kModuleMemberTests);
}

TEST(VerilogParserTestMatchAll, ClassMembers) {
  TestVerilogParserMatchAll(kClassMemberTests);
}

TEST(VerilogParserTestMatchAll, InterfaceClasses) {
  TestVerilogParserMatchAll(kInterfaceClassTests);
}

TEST(VerilogParserTestMatchAll, Constraints) {
  TestVerilogParserMatchAll(kConstraintTests);
}

TEST(VerilogParserTestMatchAll, Configs) {
  TestVerilogParserMatchAll(kConfigTests);
}

TEST(VerilogParserTestMatchAll, Primitives) {
  TestVerilogParserMatchAll(kPrimitiveTests);
}

TEST(VerilogParserTestMatchAll, Disciplines) {
  TestVerilogParserMatchAll(kDisciplineTests);
}

TEST(VerilogParserTestMatchAll, MultiBlock) {
  TestVerilogParserMatchAll(kMultiBlockTests);
}

TEST(VerilogParserTestMatchAll, RandSequence) {
  TestVerilogParserMatchAll(kRandSequenceTests);
}

TEST(VerilogParserTestMatchAll, NetAlias) {
  TestVerilogParserMatchAll(kNetAliasTests);
}

TEST(VerilogParserTestMatchAll, Library) {
  TestVerilogLibraryParserMatchAll(kLibraryTests);
}

}  // namespace

}  // namespace verilog
