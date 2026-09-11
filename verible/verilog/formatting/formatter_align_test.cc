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
#include "verible/common/formatting/align.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter-test-utils.h"
#include "verible/verilog/formatting/formatter.h"

namespace verilog {
namespace formatter {
namespace {

using verible::AlignmentPolicy;

static constexpr FormatterTestCase kAlignFormatterTestCases[] = {
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
    {"typedef union soft packed {\n"
     "bit [3:0] first; bit [31:0] second; generic_type_name_t third;\n"
     "} type_t;",
     "typedef union soft packed {\n"
     "  bit [3:0]           first;\n"
     "  bit [31:0]          second;\n"
     "  generic_type_name_t third;\n"
     "} type_t;\n"},
    {"typedef union soft {\n"
     "bit [3:0] first; bit [31:0] second; generic_type_name_t third;\n"
     "} type_t;",
     "typedef union soft {\n"
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
    {"struct {logic test1; // c\n"
     "logic test2;} test3;\n",
     "struct {\n"
     "  logic test1;  // c\n"
     "  logic test2;\n"
     "} test3;\n"},
    {"struct {\n"
     "  /* t */ logic test1; /* t */\n"
     "/* t */ logic test2; }test3;\n",
     "struct {\n"
     "  /* t */ logic test1;  /* t */\n"
     "  /* t */ logic test2;\n"
     "} test3;\n"},
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
    // user defined type alignment
    {
        "module foo();\n"
        "  logic [5:0][5:0] net_c;\n"
        " messy_type_name [1:0] net_e;\n"
        "endmodule\n",
        "module foo ();\n"
        "  logic           [5:0][5:0] net_c;\n"
        "  messy_type_name [1:0]      net_e;\n"
        "endmodule\n",
    },
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
    {"module foo\n"
     "#(\n"
     "  parameter type baz_t = struct packed { `BAZ(); }\n"
     ")\n"
     "();\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter type\n"
     "        baz_t = struct packed {\n"
     "      `BAZ();\n"
     "    }\n"
     ") ();\n"
     "endmodule\n"},
    // The same as the previous test case, but without a semicolon at the macro
    // call
    {"module foo\n"
     "#(\n"
     "  parameter type baz_t = struct packed { `BAZ() }\n"
     ")\n"
     "();\n"
     "endmodule\n",
     "module foo #(\n"
     "    parameter type\n"
     "        baz_t = struct packed {\n"
     "      `BAZ()\n"
     "    }\n"
     ") ();\n"
     "endmodule\n"},
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

};

TEST(FormatterEndToEndTest, AlignFormatterTestCases) {
  RunFormatterTestCases40(kAlignFormatterTestCases);
}

TEST(FormatterEndToEndTest, AlignmentGroupBoundaryNone) {
  // Default behavior: separator comments and blank lines do NOT break groups.
  static constexpr FormatterTestCase kTestCases[] = {
      {// Separator comment does not break alignment group
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ============\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo    = 1'b1;\n"
       "  assign baar   = 1'b0;\n"
       "  // ============\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.alignment_group_boundary = AlignmentGroupBoundary::kNone;
  style.assignment_statement_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, AlignmentGroupBoundarySeparatorComments) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Separator comment breaks alignment group
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ============\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "  // ============\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Dashes separator also breaks
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ------------\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "  // ------------\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Captioned divider (text between separator runs) also breaks
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ------ section heading ------\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "  // ------ section heading ------\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Leading-only divider run with trailing caption text
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ==== Registers\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "  // ==== Registers\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// No space after // also works
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "//============\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "  //============\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Slash separator (/////) also works
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "//////////\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "  //////////\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Regular comment does NOT break alignment
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// Title text\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo    = 1'b1;\n"
       "  assign baar   = 1'b0;\n"
       "  // Title text\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Multi-line comment block with separators
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ============\n"
       "// Title\n"
       "// ============\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "  // ============\n"
       "  // Title\n"
       "  // ============\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Short repeated body (3 chars) does NOT break
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ---\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo    = 1'b1;\n"
       "  assign baar   = 1'b0;\n"
       "  // ---\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Declarations also respect separator comments
       "module m;\n"
       "logic       a;\n"
       "logic [7:0] b;\n"
       "// ============\n"
       "logic c_long_name;\n"
       "logic d;\n"
       "endmodule\n",
       "module m;\n"
       "  logic       a;\n"
       "  logic [7:0] b;\n"
       "  // ============\n"
       "  logic c_long_name;\n"
       "  logic d;\n"
       "endmodule\n"},
      {// Statements in always blocks
       "module m;\n"
       "always_comb begin\n"
       "aaaaa = b;\n"
       "c     = 1'b0;\n"
       "// ============\n"
       "dd = eee;\n"
       "ffffff = g;\n"
       "end\n"
       "endmodule\n",
       "module m;\n"
       "  always_comb begin\n"
       "    aaaaa = b;\n"
       "    c     = 1'b0;\n"
       "    // ============\n"
       "    dd     = eee;\n"
       "    ffffff = g;\n"
       "  end\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.alignment_group_boundary = AlignmentGroupBoundary::kSeparatorComments;
  style.assignment_statement_alignment = AlignmentPolicy::kAlign;
  style.module_net_variable_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, AlignmentGroupBoundaryBlankLines) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Blank line breaks alignment group
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// No blank line: single alignment group
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo    = 1'b1;\n"
       "  assign baar   = 1'b0;\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Single item before blank line (not enough for alignment)
       "module m;\n"
       "assign foo = 1'b1;\n"
       "\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo = 1'b1;\n"
       "\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
      {// Separator comment does NOT break (only blank lines)
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "// ============\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo    = 1'b1;\n"
       "  assign baar   = 1'b0;\n"
       "  // ============\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.alignment_group_boundary = AlignmentGroupBoundary::kBlankLines;
  style.assignment_statement_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest,
     AlignmentGroupBoundaryBlankLinesAndSeparatorComments) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Both blank line and separator break groups
       "module m;\n"
       "assign foo  = 1'b1;\n"
       "assign baar = 1'b0;\n"
       "\n"
       "assign baaaaz = 1'b1;\n"
       "assign c      = 1'b0;\n"
       "// ============\n"
       "assign dd  = 1'b1;\n"
       "assign eee = 1'b0;\n"
       "endmodule\n",
       "module m;\n"
       "  assign foo  = 1'b1;\n"
       "  assign baar = 1'b0;\n"
       "\n"
       "  assign baaaaz = 1'b1;\n"
       "  assign c      = 1'b0;\n"
       "  // ============\n"
       "  assign dd  = 1'b1;\n"
       "  assign eee = 1'b0;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.alignment_group_boundary =
      AlignmentGroupBoundary::kBlankLinesAndSeparatorComments;
  style.assignment_statement_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify kAlign behavior for body-level param/localparam declarations
// in module and package bodies.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentBasics) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// localparam alignment in module body
       "module m;\n"
       "localparam foo = 4'b0000;\n"
       "localparam barr = 4'b0010;\n"
       "localparam baaaaz = 4'b0111;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 4'b0000;\n"
       "  localparam barr   = 4'b0010;\n"
       "  localparam baaaaz = 4'b0111;\n"
       "endmodule\n"},
      {// localparam alignment in package body
       "package p;\n"
       "localparam foo = 4'b0000;\n"
       "localparam barr = 4'b0010;\n"
       "localparam baaaaz = 4'b0111;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam foo    = 4'b0000;\n"
       "  localparam barr   = 4'b0010;\n"
       "  localparam baaaaz = 4'b0111;\n"
       "endpackage\n"},
      {// parameter alignment in module body
       "module m;\n"
       "parameter int foo = 1;\n"
       "parameter int barr = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int foo  = 1;\n"
       "  parameter int barr = 2;\n"
       "endmodule\n"},
      {// parameter alignment in package body
       "package p;\n"
       "parameter int foo = 1;\n"
       "parameter int barrrr = 2;\n"
       "endpackage\n",
       "package p;\n"
       "  parameter int foo    = 1;\n"
       "  parameter int barrrr = 2;\n"
       "endpackage\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify kFlushLeft behavior: body-level param/localparam declarations
// are not aligned.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentFlushLeft) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// module body: parameters are not aligned
       "module m;\n"
       "localparam foo = 4'b0000;\n"
       "localparam barr = 4'b0010;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo = 4'b0000;\n"
       "  localparam barr = 4'b0010;\n"
       "endmodule\n"},
      {// package context: flush-left
       "package p;\n"
       "localparam foo = 4'b0000;\n"
       "localparam barr = 4'b0010;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam foo = 4'b0000;\n"
       "  localparam barr = 4'b0010;\n"
       "endpackage\n"},
      {// generate context: flush-left
       "module m;\n"
       "generate\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "endgenerate\n"
       "endmodule\n",
       "module m;\n"
       "  generate\n"
       "    localparam foo = 1;\n"
       "    localparam barr = 2;\n"
       "  endgenerate\n"
       "endmodule\n"},
      {// parameter in module body: flush-left
       "module m;\n"
       "parameter int W = 8;\n"
       "parameter int HHHH = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int W = 8;\n"
       "  parameter int HHHH = 2;\n"
       "endmodule\n"},
      {// mixed parameter and localparam: flush-left, no alignment
       "module m;\n"
       "parameter int W = 8;\n"
       "localparam L = 4;\n"
       "parameter int H = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int W = 8;\n"
       "  localparam L = 4;\n"
       "  parameter int H = 2;\n"
       "endmodule\n"},
      {// interface context: flush-left
       "interface my_if;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "endinterface\n",
       "interface my_if;\n"
       "  localparam foo = 1;\n"
       "  localparam barr = 2;\n"
       "endinterface\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kFlushLeft;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify kPreserve behavior: body-level param/localparam declarations
// maintain their existing spacing.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentPreserve) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// module body: existing spacing is kept
       "module m;\n"
       "localparam foo = 4'b0000;\n"
       "localparam barr  = 4'b0010;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo = 4'b0000;\n"
       "  localparam barr  = 4'b0010;\n"
       "endmodule\n"},
      {// package: existing flush-left spacing preserved
       "package p;\n"
       "localparam foo = 4'b0000;\n"
       "localparam barr = 4'b0010;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam foo = 4'b0000;\n"
       "  localparam barr = 4'b0010;\n"
       "endpackage\n"},
      {// generate: existing aligned spacing preserved
       "module m;\n"
       "generate\n"
       "localparam foo  = 1;\n"
       "localparam barr = 2;\n"
       "endgenerate\n"
       "endmodule\n",
       "module m;\n"
       "  generate\n"
       "    localparam foo  = 1;\n"
       "    localparam barr = 2;\n"
       "  endgenerate\n"
       "endmodule\n"},
      {// parameter flush-left preserved
       "module m;\n"
       "parameter int W = 8;\n"
       "parameter int HHHH = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int W = 8;\n"
       "  parameter int HHHH = 2;\n"
       "endmodule\n"},
      {// parameter pre-aligned preserved
       "module m;\n"
       "parameter int W    = 8;\n"
       "parameter int HHHH = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int W    = 8;\n"
       "  parameter int HHHH = 2;\n"
       "endmodule\n"},
      {// interface pre-aligned preserved
       "interface my_if;\n"
       "localparam foo  = 1;\n"
       "localparam barr = 2;\n"
       "endinterface\n",
       "interface my_if;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "endinterface\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kPreserve;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify kAlign behavior for param/localparam declarations across
// interface, generate, and module body contexts, including mixed
// param/localparam and packed dimension alignment.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentContexts) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// interface body localparam alignment
       "interface my_if;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "endinterface\n",
       "interface my_if;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "endinterface\n"},
      {// interface body parameter alignment
       "interface my_if;\n"
       "parameter int X = 1;\n"
       "parameter int Y_LONG = 2;\n"
       "endinterface\n",
       "interface my_if;\n"
       "  parameter int X      = 1;\n"
       "  parameter int Y_LONG = 2;\n"
       "endinterface\n"},
      {// generate block localparam alignment
       "module m;\n"
       "generate\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "endgenerate\n"
       "endmodule\n",
       "module m;\n"
       "  generate\n"
       "    localparam foo  = 1;\n"
       "    localparam barr = 2;\n"
       "  endgenerate\n"
       "endmodule\n"},
      {// generate block parameter alignment
       "module m;\n"
       "generate\n"
       "parameter int A = 1;\n"
       "parameter int BB = 2;\n"
       "endgenerate\n"
       "endmodule\n",
       "module m;\n"
       "  generate\n"
       "    parameter int A  = 1;\n"
       "    parameter int BB = 2;\n"
       "  endgenerate\n"
       "endmodule\n"},
      {// mixed param and net declarations form separate alignment groups
       "module m;\n"
       "localparam X = 1;\n"
       "localparam YYY = 2;\n"
       "logic clk;\n"
       "logic rst_n;\n"
       "localparam ZZ = 3;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam X   = 1;\n"
       "  localparam YYY = 2;\n"
       "  logic clk;\n"
       "  logic rst_n;\n"
       "  localparam ZZ = 3;\n"
       "endmodule\n"},
      {// mixed parameter and localparam in same block
       "module m;\n"
       "parameter int W = 8;\n"
       "localparam L = 4;\n"
       "parameter int H = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter  int W = 8;\n"
       "  localparam     L = 4;\n"
       "  parameter  int H = 2;\n"
       "endmodule\n"},
      {// packed dimensions are aligned
       "module m;\n"
       "parameter bit [7:0] X = 0;\n"
       "parameter bit [31:0] YYYY = 1;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter bit [ 7:0] X    = 0;\n"
       "  parameter bit [31:0] YYYY = 1;\n"
       "endmodule\n"},
      {// multi-identifier comma-separated params are not split (no-crash)
       "module m;\n"
       "parameter int a=1, b=2, ccc=3;\n"
       "localparam d=4, e=5;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter  int a = 1, b = 2, ccc = 3;\n"
       "  localparam     d = 4, e = 5;\n"
       "endmodule\n"},
      {// parameter type declarations do not crash
       "module m;\n"
       "parameter type T = int;\n"
       "parameter type TT_LONG = bit;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter type T = int;\n"
       "  parameter type TT_LONG = bit;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify kInferUserIntent behavior: body-level param/localparam
// declarations infer alignment intent from existing spacing.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentInferUserIntent) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// flush-left localparams with small spacing diff: infer aligns
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "endmodule\n"},
      {// pre-aligned localparams: infer preserves alignment
       "module m;\n"
       "localparam foo    = 1;\n"
       "localparam barr   = 2;\n"
       "localparam baaaaz = 3;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  localparam barr   = 2;\n"
       "  localparam baaaaz = 3;\n"
       "endmodule\n"},
      {// flush-left params in package with small spacing diff: infer aligns
       "package p;\n"
       "localparam X = 1;\n"
       "localparam YY = 2;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam X  = 1;\n"
       "  localparam YY = 2;\n"
       "endpackage\n"},
      {// mixed param/localparam flush-left: infer keeps flush-left
       "module m;\n"
       "parameter int W = 8;\n"
       "localparam L = 4;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int W = 8;\n"
       "  localparam L = 4;\n"
       "endmodule\n"},
      {// interface flush-left with small diff: infer aligns
       "interface my_if;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "endinterface\n",
       "interface my_if;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "endinterface\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kInferUserIntent;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify boundary behavior for param declarations with
// kBlankLinesAndSeparatorComments: both blank lines and separator comments
// break alignment groups.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentBoundary) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// separator comment breaks param alignment group
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "// ============\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "  // ============\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
      {// blank line breaks param alignment group
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
      {// no separator: single alignment group (all aligned together)
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  localparam barr   = 2;\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
      {// regular comment does NOT break alignment group
       "module m;\n"
       "localparam foo = 1;\n"
       "// regular comment\n"
       "localparam barr = 2;\n"
       "localparam baaaaz = 1;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  // regular comment\n"
       "  localparam barr   = 2;\n"
       "  localparam baaaaz = 1;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  style.alignment_group_boundary =
      AlignmentGroupBoundary::kBlankLinesAndSeparatorComments;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify edge cases: unpacked dimensions, signed/unsigned, implicit types,
// complex types, single declarations, deeply nested generate blocks.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentEdgeCases) {
  // Use column_limit = 80 to avoid unintended line wrapping inside
  // deeply nested generate blocks (generate-if, generate-for), which
  // would interfere with verifying alignment behavior.
  static constexpr FormatterTestCase kTestCases[] = {
      {// unpacked dimension alignment
       "module m;\n"
       "parameter bit X [7:0] = 0;\n"
       "parameter bit YYYY [15:0] = 1;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter bit X   [ 7:0] = 0;\n"
       "  parameter bit YYYY[15:0] = 1;\n"
       "endmodule\n"},
      {// param declarations with only type and default value (no idim)
       "module m;\n"
       "parameter int X = 0;\n"
       "parameter int YYYY = 0;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int X    = 0;\n"
       "  parameter int YYYY = 0;\n"
       "endmodule\n"},
      {// signed modifier with packed dimensions
       "module m;\n"
       "parameter bit signed [7:0] X = 0;\n"
       "parameter bit signed [31:0] YYYY = 1;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter bit signed [ 7:0] X    = 0;\n"
       "  parameter bit signed [31:0] YYYY = 1;\n"
       "endmodule\n"},
      {// param with complex type (struct/typedef) — does not crash
       "module m;\n"
       "parameter foo_t X = foo_t'(0);\n"
       "parameter foo_t Y_LONG = foo_t'(0);\n"
       "endmodule\n",
       "module m;\n"
       "  parameter foo_t X      = foo_t'(0);\n"
       "  parameter foo_t Y_LONG = foo_t'(0);\n"
       "endmodule\n"},
      {// single param declaration (not enough for alignment group)
       "module m;\n"
       "localparam X = 1;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam X = 1;\n"
       "endmodule\n"},
      {// params inside generate-if block
       "module m;\n"
       "generate\n"
       "if (1) begin : blk\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "end\n"
       "endgenerate\n"
       "endmodule\n",
       "module m;\n"
       "  generate\n"
       "    if (1) begin : blk\n"
       "      localparam foo  = 1;\n"
       "      localparam barr = 2;\n"
       "    end\n"
       "  endgenerate\n"
       "endmodule\n"},
      {// unsigned keyword with packed dimensions (complement to signed)
       "module m;\n"
       "parameter bit unsigned [7:0] X = 0;\n"
       "parameter bit unsigned [31:0] YYYY = 1;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter bit unsigned [ 7:0] X    = 0;\n"
       "  parameter bit unsigned [31:0] YYYY = 1;\n"
       "endmodule\n"},
      {// parameter with implicit type (no explicit type keyword)
       "module m;\n"
       "parameter X = 32;\n"
       "parameter Y_LONG = 64;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter X      = 32;\n"
       "  parameter Y_LONG = 64;\n"
       "endmodule\n"},
      {// params inside generate-for block
       "module m;\n"
       "generate\n"
       "for (i = 0; i < 2; i++) begin : blk\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "end\n"
       "endgenerate\n"
       "endmodule\n",
       "module m;\n"
       "  generate\n"
       "    for (i = 0; i < 2; i++) begin : blk\n"
       "      localparam foo  = 1;\n"
       "      localparam barr = 2;\n"
       "    end\n"
       "  endgenerate\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 80;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify parameter declarations inside generate-case blocks are aligned.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentGenerateCase) {
  // Params inside generate-case need wider column limit to avoid wrapping the
  // case label / begin block line.
  static constexpr FormatterTestCase kTestCases[] = {
      {// params inside generate-case block
       "module m #(P = 0);\n"
       "generate\n"
       "case (P)\n"
       "0: begin : blk\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "end\n"
       "endcase\n"
       "endgenerate\n"
       "endmodule\n",
       "module m #(\n"
       "    P = 0\n"
       ");\n"
       "  generate\n"
       "    case (P)\n"
       "      0: begin : blk\n"
       "        localparam foo  = 1;\n"
       "        localparam barr = 2;\n"
       "      end\n"
       "    endcase\n"
       "  endgenerate\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 80;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, FormalAndBodyParamAlignmentIndependence) {
  // Verify that --formal_parameters_alignment and
  // --parameter_declaration_alignment act independently: setting formal params
  // to kFlushLeft should not affect body-level param alignment, and vice versa.
  static constexpr FormatterTestCase kTestCases[] = {
      {// formal flush-left + body align: only body params align
       "module m #(\n"
       "int W = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo    = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
      {// formal align + body flush-left: only formal params align
       "module m #(\n"
       "int W = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W    = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
      {// both flush-left: no alignment anywhere
       "module m #(\n"
       "int W = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
      {// both align: both formal and body params aligned
       "module m #(\n"
       "int W = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W    = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo    = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
  };
  // First case: formal kFlushLeft, body kAlign
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
    style.formal_parameters_alignment = AlignmentPolicy::kFlushLeft;
    const auto &tc = kTestCases[0];
    VLOG(1) << "code-to-format:\n" << tc.input << "<EOF>";
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // Second case: formal kAlign, body kFlushLeft
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kFlushLeft;
    style.formal_parameters_alignment = AlignmentPolicy::kAlign;
    const auto &tc = kTestCases[1];
    VLOG(1) << "code-to-format:\n" << tc.input << "<EOF>";
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // Third case: both kFlushLeft
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kFlushLeft;
    style.formal_parameters_alignment = AlignmentPolicy::kFlushLeft;
    const auto &tc = kTestCases[2];
    VLOG(1) << "code-to-format:\n" << tc.input << "<EOF>";
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // Fourth case: both kAlign
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
    style.formal_parameters_alignment = AlignmentPolicy::kAlign;
    const auto &tc = kTestCases[3];
    VLOG(1) << "code-to-format:\n" << tc.input << "<EOF>";
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
}

TEST(FormatterEndToEndTest, ClassBodyParamNotAffected) {
  // Verify that class body parameter/localparam declarations are NOT affected
  // by --parameter_declaration_alignment, which targets only module/generate/
  // package/interface body-level params.
  static constexpr FormatterTestCase kTestCases[] = {
      {// class body params are not affected
       "class c;\n"
       "parameter int X = 1;\n"
       "parameter int YYYY = 2;\n"
       "localparam foo = 3;\n"
       "localparam barrrr = 4;\n"
       "endclass\n",
       "class c;\n"
       "  parameter int X = 1;\n"
       "  parameter int YYYY = 2;\n"
       "  localparam foo = 3;\n"
       "  localparam barrrr = 4;\n"
       "endclass\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, PackageBodyVariableAlignment) {
  // Verify that moving kPackageItemList to kTabularAlignment in tree-unwrapper
  // enables net/variable declaration alignment in package bodies via
  // module_net_variable_alignment.
  static constexpr FormatterTestCase kTestCases[] = {
      {// net/variable declaration alignment in package body
       "package p;\n"
       "logic [7:0] a;\n"
       "logic [31:0] bb;\n"
       "endpackage\n",
       "package p;\n"
       "  logic [ 7:0] a;\n"
       "  logic [31:0] bb;\n"
       "endpackage\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.module_net_variable_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, ParamDeclarationAlignmentApplyToAll) {
  // Verify that ApplyToAllAlignmentPolicies affects
  // parameter_declaration_alignment for all four alignment policies.
  static constexpr FormatterTestCase kTestCases[] = {
      {// kAlign aligns identifiers and = signs across declarations
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barrrr = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  localparam barrrr = 2;\n"
       "endmodule\n"},
      {// kFlushLeft: declarations remain flush-left
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barrrr = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo = 1;\n"
       "  localparam barrrr = 2;\n"
       "endmodule\n"},
      {// kPreserve: existing spacing is kept
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barrrr  = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo = 1;\n"
       "  localparam barrrr  = 2;\n"
       "endmodule\n"},
      {// kInferUserIntent: large identifier width difference (3 cols)
       // suggests flush-left intent, no alignment.
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barrrr = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo = 1;\n"
       "  localparam barrrr = 2;\n"
       "endmodule\n"},
  };
  // kAlign
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.ApplyToAllAlignmentPolicies(AlignmentPolicy::kAlign);
    const auto &tc = kTestCases[0];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // kFlushLeft
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.ApplyToAllAlignmentPolicies(AlignmentPolicy::kFlushLeft);
    const auto &tc = kTestCases[1];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // kPreserve
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.ApplyToAllAlignmentPolicies(AlignmentPolicy::kPreserve);
    const auto &tc = kTestCases[2];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // kInferUserIntent
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.ApplyToAllAlignmentPolicies(AlignmentPolicy::kInferUserIntent);
    const auto &tc = kTestCases[3];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
}

// Verify that alignment_group_boundary=kBlankLines treats only blank lines
// (not separator comments) as boundary breaks for param declarations.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentBoundaryBlankLines) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Only blank line breaks group; separator comment does not
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
      {// Separator comment does NOT break group with kBlankLines
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "// ============\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  localparam barr   = 2;\n"
       "  // ============\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  style.alignment_group_boundary = AlignmentGroupBoundary::kBlankLines;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify that alignment_group_boundary=kNone keeps all param declarations
// in a single alignment group regardless of blank lines or comments.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentBoundaryNone) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Blank line does NOT break group with kNone
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  localparam barr   = 2;\n"
       "\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
      {// Separator comment does NOT break group with kNone
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "// ============\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  localparam barr   = 2;\n"
       "  // ============\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  style.alignment_group_boundary = AlignmentGroupBoundary::kNone;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify that alignment_group_boundary=kSeparatorComments treats only
// separator comments (not blank lines) as boundary breaks for param
// declarations.
TEST(FormatterEndToEndTest,
     ParamDeclarationAlignmentBoundarySeparatorComments) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// Separator comment breaks alignment group
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "// ============\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "  // ============\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
      {// Blank line does NOT break group with kSeparatorComments
       "module m;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endmodule\n",
       "module m;\n"
       "  localparam foo    = 1;\n"
       "  localparam barr   = 2;\n"
       "\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  style.alignment_group_boundary = AlignmentGroupBoundary::kSeparatorComments;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify that class body parameter/localparam declarations are NOT affected
// by --parameter_declaration_alignment for the kFlushLeft, kPreserve,
// and kInferUserIntent policies (kAlign is covered in
// ClassBodyParamNotAffected).
TEST(FormatterEndToEndTest, ClassBodyParamNotAffectedOtherPolicies) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// kFlushLeft: class body params remain flush-left
       "class c;\n"
       "parameter int X = 1;\n"
       "parameter int YYYY = 2;\n"
       "localparam foo = 3;\n"
       "localparam barrrr = 4;\n"
       "endclass\n",
       "class c;\n"
       "  parameter int X = 1;\n"
       "  parameter int YYYY = 2;\n"
       "  localparam foo = 3;\n"
       "  localparam barrrr = 4;\n"
       "endclass\n"},
      {// kPreserve: parameter_declaration_alignment does not apply to class
       // bodies; whitespace is normalized by default formatting.
       "class c;\n"
       "parameter int X     = 1;\n"
       "parameter int YYYY  = 2;\n"
       "endclass\n",
       "class c;\n"
       "  parameter int X = 1;\n"
       "  parameter int YYYY = 2;\n"
       "endclass\n"},
      {// kInferUserIntent: class body params infer flush-left
       "class c;\n"
       "parameter int X = 1;\n"
       "parameter int YYYY = 2;\n"
       "endclass\n",
       "class c;\n"
       "  parameter int X = 1;\n"
       "  parameter int YYYY = 2;\n"
       "endclass\n"},
  };
  // kFlushLeft
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kFlushLeft;
    const auto &tc = kTestCases[0];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // kPreserve
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kPreserve;
    const auto &tc = kTestCases[1];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // kInferUserIntent
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kInferUserIntent;
    const auto &tc = kTestCases[2];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
}

// Verify formal_parameters_alignment and parameter_declaration_alignment
// independence for kPreserve and kInferUserIntent combinations.
TEST(FormatterEndToEndTest,
     FormalAndBodyParamAlignmentIndependenceOtherPolicies) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// formal kPreserve + body kAlign: only body params aligned
       "module m #(\n"
       "int W = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo    = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
      {// formal kAlign + body kPreserve: only formal params aligned
       "module m #(\n"
       "int W = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo   = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W    = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo   = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
      {// formal kInferUserIntent + body kAlign: only body params aligned
       "module m #(\n"
       "int W = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo    = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
  };
  // formal kPreserve + body kAlign
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
    style.formal_parameters_alignment = AlignmentPolicy::kPreserve;
    const auto &tc = kTestCases[0];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // formal kAlign + body kPreserve
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kPreserve;
    style.formal_parameters_alignment = AlignmentPolicy::kAlign;
    const auto &tc = kTestCases[1];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
  // formal kInferUserIntent + body kAlign
  {
    FormatStyle style;
    style.column_limit = 40;
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
    style.formal_parameters_alignment = AlignmentPolicy::kInferUserIntent;
    const auto &tc = kTestCases[2];
    std::ostringstream stream;
    const auto status = FormatVerilog(tc.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), tc.expected) << "code:\n" << tc.input;
  }
}

// Verify parameter declarations with string, real, and function-call default
// value types to exercise the ColumnSchemaScanner on non-standard type tokens.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentTypeVariants) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// parameter real type
       "module m;\n"
       "parameter real X = 1.0;\n"
       "parameter real Y_LONG = 2.0;\n"
       "endmodule\n",
       "module m;\n"
       "  parameter real X      = 1.0;\n"
       "  parameter real Y_LONG = 2.0;\n"
       "endmodule\n"},
      {// parameter string type
       "module m;\n"
       "parameter string s = \"hello\";\n"
       "parameter string long_s = \"world\";\n"
       "endmodule\n",
       "module m;\n"
       "  parameter string s      = \"hello\";\n"
       "  parameter string long_s = \"world\";\n"
       "endmodule\n"},
      {// parameter with function call in default value
       "module m;\n"
       "parameter int W = func(1, 2);\n"
       "parameter int H_LONG = other(3);\n"
       "endmodule\n",
       "module m;\n"
       "  parameter int W      = func(1, 2);\n"
       "  parameter int H_LONG = other(3);\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify parameter_declaration_alignment kInferUserIntent in package bodies.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentPackageInfer) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// flush-left params with small diff: infer aligns
       "package p;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "endpackage\n"},
      {// pre-aligned params: infer preserves
       "package p;\n"
       "localparam foo    = 1;\n"
       "localparam barr   = 2;\n"
       "localparam baaaaz = 3;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam foo    = 1;\n"
       "  localparam barr   = 2;\n"
       "  localparam baaaaz = 3;\n"
       "endpackage\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kInferUserIntent;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify parameter_declaration_alignment boundary behavior in package bodies.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentPackageBoundary) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// blank line breaks alignment group in package
       "package p;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endpackage\n"},
      {// separator comment breaks alignment group in package
       "package p;\n"
       "localparam foo = 1;\n"
       "localparam barr = 2;\n"
       "// ============\n"
       "localparam baaaaz = 1;\n"
       "localparam c = 2;\n"
       "endpackage\n",
       "package p;\n"
       "  localparam foo  = 1;\n"
       "  localparam barr = 2;\n"
       "  // ============\n"
       "  localparam baaaaz = 1;\n"
       "  localparam c      = 2;\n"
       "endpackage\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  style.alignment_group_boundary =
      AlignmentGroupBoundary::kBlankLinesAndSeparatorComments;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify the most common default scenario: both formal_parameters_alignment
// and parameter_declaration_alignment set to kInferUserIntent.
TEST(FormatterEndToEndTest, BothParamAlignmentsInferUserIntent) {
  static constexpr FormatterTestCase kTestCases[] = {
      {// both flush-left with small diff: infer aligns both formal and body
       "module m #(\n"
       "int W = 2,\n"
       "int LL = 4\n"
       ");\n"
       "localparam foo = 0;\n"
       "localparam barr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W  = 2,\n"
       "    int LL = 4\n"
       ");\n"
       "  localparam foo  = 0;\n"
       "  localparam barr = 0;\n"
       "endmodule\n"},
      {// both infer: pre-aligned formal, pre-aligned body preserved
       "module m #(\n"
       "int W    = 2,\n"
       "int LLLL = 4\n"
       ");\n"
       "localparam foo    = 0;\n"
       "localparam barrrr = 0;\n"
       "endmodule\n",
       "module m #(\n"
       "    int W    = 2,\n"
       "    int LLLL = 4\n"
       ");\n"
       "  localparam foo    = 0;\n"
       "  localparam barrrr = 0;\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kInferUserIntent;
  style.formal_parameters_alignment = AlignmentPolicy::kInferUserIntent;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Verify that a large multi-line comment block between parameter
// declarations does not crash the formatter.  The tree-unwrapper may
// produce a partition structure where the comment tokens are absorbed
// into the following parameter partition, which the alignment code
// must handle gracefully by skipping alignment for that row.
TEST(FormatterEndToEndTest, ParamDeclarationAlignmentCommentBlockNoCrash) {
  // These inputs contain large multi-line comment blocks between
  // parameter/localparam declarations.  The formatter must not crash
  // (SIGABRT).  Output correctness is secondary; the formatter may
  // fall back to preserving the original input when the partition
  // structure prevents safe alignment.
  const char *kInputs[] = {
      "module m;\n"
      "parameter int W = 8;\n"
      "// Multi-line comment block\n"
      "// that spans across\n"
      "//\n"
      "// several lines\n"
      "// of explanatory text\n"
      "//\n"
      "// It contains enough\n"
      "// lines to trigger\n"
      "// the partition structure\n"
      "// edge case where\n"
      "// comment tokens are\n"
      "// absorbed into the\n"
      "// following parameter\n"
      "// partition.\n"
      "//\n"
      "localparam int H = 2;\n"
      "endmodule\n",
      "package p;\n"
      "parameter int X = 1;\n"
      "// Long comment block\n"
      "// with many lines\n"
      "//\n"
      "// of text\n"
      "//\n"
      "localparam int Y = 2;\n"
      "endpackage\n",
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.parameter_declaration_alignment = AlignmentPolicy::kAlign;
  for (const char *input : kInputs) {
    VLOG(1) << "code-to-format:\n" << input << "<EOF>";
    std::ostringstream stream;
    const auto status = FormatVerilog(input, "<filename>", style, stream);
    // The primary requirement is that the formatter does not crash
    // (SIGABRT).  Gtest will report failure if the process aborts.
    // The partition structure may cause output format differences;
    // the important thing is the formatter handled it gracefully.
  }
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
