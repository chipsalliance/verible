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

static constexpr FormatterTestCase kClassPackageFormatterTestCases[] = {
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
    // task with macro call
    {"module m1;\ntask automatic t1();\n"
     "t2(`R1(1)+ 8);endtask : t1\nendmodule",
     "module m1;\n"
     "  task automatic t1();\n"
     "    t2(`R1(1) + 8);\n"
     "  endtask : t1\n"
     "endmodule\n"},

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

    // interface class test cases
    {"interface class Foo;\nendclass\n",
     "interface class Foo;\n"
     "endclass\n"},
    {"interface   class   Foo  ;  endclass\n",
     "interface class Foo;\n"
     "endclass\n"},
    {"interface class Foo extends Bar , Baz ;\nendclass\n",
     "interface class Foo extends Bar, Baz;\n"
     "endclass\n"},
    {"interface class Foo;\n  pure   virtual   task   foo (  ) ; \nendclass\n",
     "interface class Foo;\n"
     "  pure virtual task foo();\n"
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
    // By default (class_parameter_space == false), no space before '#'
    {"typedef dv_base_env_cov #(.CFG_T(tl_agent_env_cfg)) tl_agent_env_cov;\n",
     "typedef dv_base_env_cov#(\n"
     "    .CFG_T(tl_agent_env_cfg)\n"
     ") tl_agent_env_cov;\n"},
    {"typedef dv_base_env_cov#(.CFG_T(tl_agent_env_cfg)) tl_agent_env_cov;\n",
     "typedef dv_base_env_cov#(\n"
     "    .CFG_T(tl_agent_env_cfg)\n"
     ") tl_agent_env_cov;\n"},
    // single short parameter stays on one line
    {"typedef my_class #(.P(P)) my_class_t;\n",
     "typedef my_class#(.P(P)) my_class_t;\n"},

    // let declarations each stay on their own line
    {"module t;\n"
     "let OFF = 4;\n"
     "let UNIQUE = 32;\n"
     "let PP(a) = 30 + a;\n"
     "endmodule\n",
     "module t;\n"
     "  let OFF = 4;\n"
     "  let UNIQUE = 32;\n"
     "  let PP(a) = 30 + a;\n"
     "endmodule\n"},

    // let declarations each stay on their own line
    {"module t;\n"
     "let OFF = 4;\n"
     "let UNIQUE = 32;\n"
     "let PP(a) = 30 + a;\n"
     "endmodule\n",
     "module t;\n"
     "  let OFF = 4;\n"
     "  let UNIQUE = 32;\n"
     "  let PP(a) = 30 + a;\n"
     "endmodule\n"},

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
    {"package fedex;\n"
     "  import \"asdf\" context function void bar(\n"
     "    input bit              [2:1] aaaa,   // EOL COMMENT\n"
     "                                          // another\n"
     "    input bit foo\n"
     "  );\n"
     "endpackage\n",
     "package fedex;\n"
     "  import \"asdf\" context\n"
     "      function void bar(\n"
     "    input\n        bit [2:1] aaaa,  // EOL COMMENT\n"
     "                         // another\n"
     "    input bit foo\n"
     "  );\n"
     "endpackage\n"},

};

TEST(FormatterEndToEndTest, ClassPackageFormatterTestCases) {
  RunFormatterTestCases40(kClassPackageFormatterTestCases);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
