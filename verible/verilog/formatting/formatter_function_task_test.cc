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

static constexpr FormatterTestCase kFunctionTaskFormatterTestCases[] = {
    // function test cases
    {.input = "function f ;endfunction",
     .expected = "function f;\nendfunction\n"},
    {.input = "function f ;endfunction:   f",
     .expected = "function f;\nendfunction : f\n"},
    {.input = "function f ( );endfunction",
     .expected = "function f();\nendfunction\n"},
    {.input = "function f (input bit x);endfunction",
     .expected = "function f(input bit x);\nendfunction\n"},
    {.input = "function f (input bit x,logic y );endfunction",
     .expected = "function f(input bit x, logic y);\nendfunction\n"},
    {.input = "function f;\n// statement comment\nendfunction\n",
     .expected = "function f;\n"
                 "  // statement comment\n"  // indented
                 "endfunction\n"},
    {.input = "function f();\n// statement comment\nendfunction\n",
     .expected = "function f();\n"
                 "  // statement comment\n"  // indented
                 "endfunction\n"},
    {.input = "function f(input int x);\n"
              "// statement comment\n"
              "f=x;\n"
              "// statement comment\n"
              "endfunction\n",
     .expected = "function f(input int x);\n"
                 "  // statement comment\n"  // indented
                 "  f = x;\n"
                 "  // statement comment\n"  // indented
                 "endfunction\n"},
    {// line breaks around assignments
     .input = "function f;a=b;c+=d;endfunction",
     .expected = "function f;\n"
                 "  a = b;\n"
                 "  c += d;\n"
                 "endfunction\n"},
    {.input = "function f;a&=b;c=d;endfunction",
     .expected = "function f;\n"
                 "  a &= b;\n"
                 "  c = d;\n"
                 "endfunction\n"},
    {.input = "function f;a<<=b;c=b;d>>>=b;endfunction",
     .expected = "function f;\n"
                 "  a <<= b;\n"
                 "  c = b;\n"
                 "  d >>>= b;\n"
                 "endfunction\n"},
    {// port declaration exceeds line length limit
     .input = "function f (loooong_type if_it_fits_I_sits);endfunction",
     .expected = "function f(\n"
                 "    loooong_type if_it_fits_I_sits);\n"
                 "endfunction\n"},
    {.input = "function\nvoid\tspace;a=( b+c )\n;endfunction   :space\n",
     .expected = "function void space;\n"
                 "  a = (b + c);\n"
                 "endfunction : space\n"},
    {.input = "function\nvoid\twarranty;return  to_sender\n;endfunction   "
              ":warranty\n",
     .expected = "function void warranty;\n"
                 "  return to_sender;\n"
                 "endfunction : warranty\n"},
    {// if statement that fits on one line
     .input = "function if_i_fits_i_sits;"
              "if(x)y=x;"
              "endfunction",
     .expected = "function if_i_fits_i_sits;\n"
                 "  if (x) y = x;\n"
                 "endfunction\n"},
    {// for loop
     .input = "function\nvoid\twarranty;for(j=0; j<k; --k)begin "
              "++j\n;end endfunction   :warranty\n",
     .expected = "function void warranty;\n"
                 "  for (j = 0; j < k; --k) begin\n"
                 "    ++j;\n"
                 "  end\n"
                 "endfunction : warranty\n"},
    {// for loop that needs wrapping
     .input =
         "function\nvoid\twarranty;for(jjjjj=0; jjjjj<kkkkk; --kkkkk)begin "
         "++j\n;end endfunction   :warranty\n",
     .expected = "function void warranty;\n"
                 "  for (\n"
                 "      jjjjj = 0; jjjjj < kkkkk; --kkkkk\n"
                 "  ) begin\n"
                 "    ++j;\n"
                 "  end\n"
                 "endfunction : warranty\n"},
    {// for loop that needs more wrapping
     .input = "function\nvoid\twarranty;"
              "for(jjjjjjjj=0; jjjjjjjj<kkkkkkkk; --kkkkkkkk)begin "
              "++j\n;end endfunction   :warranty\n",
     .expected = "function void warranty;\n"
                 "  for (\n"
                 "      jjjjjjjj = 0;\n"
                 "      jjjjjjjj < kkkkkkkk;\n"
                 "      --kkkkkkkk\n"
                 "  ) begin\n"
                 "    ++j;\n"
                 "  end\n"
                 "endfunction : warranty\n"},
    {// for loop that fits on one line
     .input = "function loop_fits;"
              "for(x=0;x<N;++x) y=x;"
              "endfunction",
     .expected = "function loop_fits;\n"
                 "  for (x = 0; x < N; ++x) y = x;\n"
                 "endfunction\n"},
    {// for loop that would fit on one line, but is force-split with //comment
     .input = "function loop_fits;"
              "for(x=0;x<N;++x) //\n y=x;"
              "endfunction",
     .expected = "function loop_fits;\n"
                 "  for (x = 0; x < N; ++x)  //\n"
                 "    y = x;\n"
                 "endfunction\n"},
    {// for loop with function call in initializer
     .input = "function  void looper(); "
              "for (int i=f(n); i>=0; i -- ) begin end "
              "endfunction",
     .expected = "function void looper();\n"
                 "  for (int i = f(n); i >= 0; i--) begin\n"
                 "  end\n"
                 "endfunction\n"},
    {// for loop with function call in condition
     .input = "function  void looper(); "
              "for (int i=0; i<f(m); i -- ) begin end "
              "endfunction",
     .expected = "function void looper();\n"
                 "  for (int i = 0; i < f(m); i--) begin\n"
                 "  end\n"
                 "endfunction\n"},
    {// for loop with an attribute instance in the initializer.
     // Regression: this used to abort with a CHECK failure while reshaping
     // the kForSpec partitions when an attribute appears in the header.
     .input = "module m; initial for(int i=0(* a *);i<4;i++) x=i; endmodule",
     .expected = "module m;\n"
                 "  initial\n"
                 "    for (int i = 0 (* a *); i < 4; i++)\n"
                 "      x = i;\n"
                 "endmodule\n"},
    {// forever loop
     .input = "function\nvoid\tforevah;forever  begin "
              "++k\n;end endfunction\n",
     .expected = "function void forevah;\n"
                 "  forever begin\n"
                 "    ++k;\n"
                 "  end\n"
                 "endfunction\n"},
    {// forever loop
     .input = "function\nvoid\tforevah;forever  "
              "++k\n;endfunction\n",
     .expected = "function void forevah;\n"
                 "  forever ++k;\n"
                 "endfunction\n"},
    {// forever loop, forced break
     .input = "function\nvoid\tforevah;forever     //\n"
              "++k\n;endfunction\n",
     .expected = "function void forevah;\n"
                 "  forever  //\n"
                 "    ++k;\n"
                 "endfunction\n"},
    {// repeat loop
     .input = "function\nvoid\tpete;repeat(3)  begin "
              "++k\n;end endfunction\n",
     .expected = "function void pete;\n"
                 "  repeat (3) begin\n"
                 "    ++k;\n"
                 "  end\n"
                 "endfunction\n"},
    {// repeat loop
     .input = "function\nvoid\tpete;repeat(3)  "
              "++k\n;endfunction\n",
     .expected = "function void pete;\n"
                 "  repeat (3) ++k;\n"
                 "endfunction\n"},
    {// repeat loop, forced break
     .input = "function\nvoid\tpete;repeat(3)//\n"
              "++k\n;endfunction\n",
     .expected = "function void pete;\n"
                 "  repeat (3)  //\n"
                 "    ++k;\n"
                 "endfunction\n"},
    {// while loop
     .input = "function\nvoid\twily;while( coyote )  begin "
              "++super_genius\n;end endfunction\n",
     .expected = "function void wily;\n"
                 "  while (coyote) begin\n"
                 "    ++super_genius;\n"
                 "  end\n"
                 "endfunction\n"},
    {// while loop
     .input = "function\nvoid\twily;while( coyote )  "
              "++ super_genius\n;   endfunction\n",
     .expected = "function void wily;\n"
                 "  while (coyote) ++super_genius;\n"
                 "endfunction\n"},
    {// while loop, forced break
     .input = "function\nvoid\twily;while( coyote ) //\n "
              "++ super_genius\n;   endfunction\n",
     .expected = "function void wily;\n"
                 "  while (coyote)  //\n"
                 "    ++super_genius;\n"
                 "endfunction\n"},
    {// do-while loop
     .input = "function\nvoid\tdonot;do  begin "
              "++s\n;end  while( z);endfunction\n",
     .expected = "function void donot;\n"
                 "  do begin\n"
                 "    ++s;\n"
                 "  end while (z);\n"
                 "endfunction\n"},
    {// do-while loop, single statement
     .input = "function\nvoid\tdonot;do  "
              "++s\n;  while( z);endfunction\n",
     .expected = "function void donot;\n"
                 "  do ++s; while (z);\n"
                 "endfunction\n"},
    {// do-while loop, single statement, forced break
     .input = "function\nvoid\tdonot;do  "
              "++s\n;//\n  while( z);endfunction\n",
     .expected = "function void donot;\n"
                 "  do\n"
                 "    ++s;  //\n"
                 "  while (z);\n"
                 "endfunction\n"},
    {// foreach loop
     .input = "function\nvoid\tforeacher;foreach( m [n] )  begin "
              "++m\n;end endfunction\n",
     .expected = "function void foreacher;\n"
                 "  foreach (m[n]) begin\n"
                 "    ++m;\n"
                 "  end\n"
                 "endfunction\n"},
    {// spaces in condition expression
     .input = "function f; return {a}? {b} :{ c };endfunction",
     .expected = "function f;\n"
                 "  return {a} ? {b} : {c};\n"
                 "endfunction\n"},
    {.input = "task t;endtask",
     .expected = "task t;\n"
                 "endtask\n"},
    {.input = "task t (   );endtask",
     .expected = "task t();\n"
                 "endtask\n"},
    {.input = "task t (input    bit   drill   ) ;endtask",
     .expected = "task t(input bit drill);\n"
                 "endtask\n"},
    {.input = "task t; ## 100 ;endtask",
     .expected = "task t;\n"
                 "  ##100;\n"
                 "endtask\n"},
    {.input = "task t; ## (1+1) ;endtask",  // delay expression
     .expected = "task t;\n"
                 "  ##(1 + 1);\n"
                 "endtask\n"},
    {.input = "task t; ## delay_value ;endtask",
     .expected = "task t;\n"
                 "  ##delay_value;\n"
                 "endtask\n"},
    {.input = "task t; ## `DELAY_VALUE ;endtask",
     .expected = "task t;\n"
                 "  ##`DELAY_VALUE;\n"
                 "endtask\n"},
    {.input = "task t;\n"
              "`uvm_error( foo,bar);\n"
              "endtask\n",
     .expected = "task t;\n"
                 "  `uvm_error(foo, bar);\n"
                 "endtask\n"},
    {.input = "task t;\n"
              "`uvm_error(foo,bar)\n"
              ";\n"  // null statement
              "endtask\n",
     .expected = "task t;\n"
                 "  `uvm_error(foo, bar)\n"
                 "  ;\n"
                 "endtask\n"},
    {.input = "task t;\n"
              "if(expr)begin\t\n"
              "`uvm_error(foo,bar);\n"
              "end\n"
              "endtask\n",
     .expected = "task t;\n"
                 "  if (expr) begin\n"
                 "    `uvm_error(foo, bar);\n"
                 "  end\n"
                 "endtask\n"},
    {.input = "task\nrabbit;$kill(the,\nrabbit)\n;endtask:  rabbit\n",
     .expected = "task rabbit;\n"
                 "  $kill(the, rabbit);\n"
                 "endtask : rabbit\n"},
    {.input = "function  int foo( );if( a )a+=1 ; endfunction",
     .expected = "function int foo();\n"
                 "  if (a) a += 1;\n"
                 "endfunction\n"},
    {.input = "function  void foo( );foo=`MACRO(b,c) ; endfunction",
     .expected = "function void foo();\n"
                 "  foo = `MACRO(b, c);\n"
                 "endfunction\n"},
    {.input = "module foo;if    \t  (bar)begin assign a=1; end endmodule",
     .expected = "module foo;\n"
                 "  if (bar) begin\n"
                 "    assign a = 1;\n"
                 "  end\n"
                 "endmodule\n"},
    {.input = "module proc_cont_assigner;\n"
              "always begin\n"
              "assign x1 =   y1;\n"
              "deassign   x2 ;\n"
              "force x3=y3;\n"
              "release   x4 ;\n"
              "end\n"
              "endmodule\n",
     .expected = "module proc_cont_assigner;\n"
                 "  always begin\n"
                 "    assign x1 = y1;\n"
                 "    deassign x2;\n"
                 "    force x3 = y3;\n"
                 "    release x4;\n"
                 "  end\n"
                 "endmodule\n"},
    {.input = "module g_test(  );\n"
              "\tinitial begin:main_test \t"
              "for(int i=0;i<k;i++)begin "
              "case(i )\n"
              " 6'd0  :release in[0];  \n"
              "   endcase  "
              " \t\tend \t"
              "\t end:main_test\n"
              "endmodule:g_test\n",
     .expected = "module g_test ();\n"
                 "  initial begin : main_test\n"
                 "    for (int i = 0; i < k; i++) begin\n"
                 "      case (i)\n"
                 "        6'd0: release in[0];\n"
                 "      endcase\n"
                 "    end\n"
                 "  end : main_test\n"
                 "endmodule : g_test\n"},
    {// conditional generate (case)
     .input = "module mc; case(s)a : bb c ; d : ee f; endcase endmodule",
     .expected = "module mc;\n"
                 "  case (s)\n"
                 "    a: bb c;\n"
                 "    d: ee f;\n"
                 "  endcase\n"
                 "endmodule\n"},
    {// conditional generate (case), with comments
     .input =
         "module mc; case(s)\n//comment a\na:bb  c;\n//comment b\n endcase "
         "endmodule",
     .expected = "module mc;\n"
                 "  case (s)\n"
                 "    //comment a\n"  // indented to case-item level
                 "    a: bb c;\n"
                 "    //comment b\n"  // indented to case-item level
                 "  endcase\n"
                 "endmodule\n"},

    {// "default:", not "default :"
     .input = "function f; case (x) default: x=y; endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x)\n"
                 "    default: x = y;\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// default with null statement: "default: ;", not "default :;"
     .input = "function f; case (x) default :; endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x)\n"
                 "    default: ;\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// case statement
     .input = "function f; case (x) State0 : a=b; State1 : begin a=b; end "
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x)\n"
                 "    State0: a = b;\n"
                 "    State1: begin\n"
                 "      a = b;\n"
                 "    end\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// case statement, interleaved with comments
     .input = "function f; case (x) \n//c1\nState0 : a=b;//c2\n//c3\n State1 : "
              "a=b;//c4\n//c5\n "
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x)\n"
                 "    //c1\n"
                 "    State0: a = b;  //c2\n"
                 "    //c3\n"
                 "    State1: a = b;  //c4\n"
                 "    //c5\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// case inside statement, comments
     .input = "function f; case (x)inside \n//comment\n"
              "[0:1]:x=y; \n"
              "    //comment\n"
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x) inside\n"
                 "    //comment\n"
                 "    [0 : 1]: x = y;\n"
                 "    //comment\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// case inside statement
     .input =
         "function f; case (x)inside k1 : return b; k2 : begin return b; end "
         "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x) inside\n"
                 "    k1: return b;\n"
                 "    k2: begin\n"
                 "      return b;\n"
                 "    end\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// case inside statement, with ranges
     .input = "function f; case (x) inside[a:b] : return b; [c:d] : return b; "
              "default :return z;"
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x) inside\n"
                 "    [a : b]: return b;\n"
                 "    [c : d]: return b;\n"
                 "    default: return z;\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// case pattern statement
     .input = "function f;"
              "case (y) matches "
              ".foo   : return 0;"
              ".*\t: return 1;"
              "endcase "
              "case (z) matches "
              ".foo\t\t: return 0;"
              ".*   : return 1;"
              "endcase "
              "endfunction",
     .expected = "function f;\n"
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
     .input = "function f; case (x)k1 : if( b )break; default :return 2;"
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x)\n"
                 "    k1:      if (b) break;\n"  // aligned
                 "    default: return 2;\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// keep short default items on one line
     .input = "function f; case (x)k1 :break; default :if( c )return 2;"
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x)\n"
                 "    k1:      break;\n"  // aligned
                 "    default: if (c) return 2;\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// keep short case inside items on one line
     .input = "function f; case (x)inside k1 : if( b )return c; k2 : return a;"
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  case (x) inside\n"
                 "    k1: if (b) return c;\n"
                 "    k2: return a;\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// keep short case pattern items on one line
     .input = "function f;"
              "case (y) matches "
              ".foo   :if( n )return 0;"
              ".*\t: return 1;"
              "endcase "
              "endfunction",
     .expected = "function f;\n"
                 "  case (y) matches\n"
                 "    .foo: if (n) return 0;\n"
                 "    .*: return 1;\n"
                 "  endcase\n"
                 "endfunction\n"},
    {// randcase
     .input = "function f; randcase k1 : return c; k2 : return a;"
              "endcase endfunction\n",
     .expected = "function f;\n"
                 "  randcase\n"
                 "    k1: return c;\n"
                 "    k2: return a;\n"
                 "  endcase\n"
                 "endfunction\n"},

    // This tests checks for not breaking around hierarchy operators.
    {.input = "function\nvoid\twarranty;"
              "foo.bar = fancyfunction(aaaaaaaa.bbbbbbb,"
              "    ccccccccc.ddddddddd) ;"
              "endfunction   :warranty\n",
     .expected = "function void warranty;\n"
                 "  foo.bar = fancyfunction(\n"
                 "      aaaaaaaa.bbbbbbb,\n"
                 "      ccccccccc.ddddddddd\n"
                 "  );\n"
                 "endfunction : warranty\n"},

    // Group of tests testing partitioning of arguments inside function calls
    {// function with function call inside if statement header
     .input = "function foo;if(aa(bb,cc));endfunction\n",
     .expected = "function foo;\n"
                 "  if (aa(bb, cc));\n"
                 "endfunction\n"},
    {// function with function call inside if statement header and with
     // begin-end block
     .input = "function foo;if (aa(bb,cc,dd,ee))begin end endfunction\n",
     .expected = "function foo;\n"
                 "  if (aa(bb, cc, dd, ee)) begin\n"
                 "  end\n"
                 "endfunction\n"},
    {// function with kMethodCallExtension inside if statement header and with
     // begin-end block
     .input = "function foo;if (aa.bb(cc,dd,ee))begin end endfunction\n",
     .expected = "function foo;\n"
                 "  if (aa.bb(cc, dd, ee)) begin\n"
                 "  end\n"
                 "endfunction\n"},
    {// nested kMethodCallExtension calls - one level
     .input = "function foo;aa.bb(cc.dd(a1), ee.ff(a2));endfunction\n",
     .expected = "function foo;\n"
                 "  aa.bb(cc.dd(a1), ee.ff(a2));\n"
                 "endfunction\n"},
    {// nested kMethodCallExtension calls - two level
     .input = "function foo;aa.bb(cc.dd(a1.b1(a2), b1), ee.ff(c1, "
              "d1));endfunction\n",
     .expected = "function foo;\n"
                 "  aa.bb(cc.dd(a1.b1(a2), b1), ee.ff(\n"
                 "        c1, d1));\n"
                 "endfunction\n"},

    {// simple initial statement with function call
     .input = "module m;initial aa(bb,cc,dd,ee);endmodule\n",
     .expected = "module m;\n"
                 "  initial aa(bb, cc, dd, ee);\n"
                 "endmodule\n"},
    {// expressions and function calls inside if-statement headers
     .input =
         "module m;initial begin if(aa(bb)==cc(dd))a=b;if (xx()) b = a;end "
         "endmodule\n",
     .expected = "module m;\n"
                 "  initial begin\n"
                 "    if (aa(bb) == cc(dd)) a = b;\n"
                 "    if (xx()) b = a;\n"
                 "  end\n"
                 "endmodule\n"},
    {// fuction with two arguments inside if-statement headers
     .input = "module\nm;initial\nbegin\nif(aa(bb,cc))x=y;end\nendmodule\n",
     .expected = "module m;\n"
                 "  initial begin\n"
                 "    if (aa(bb, cc)) x = y;\n"
                 "  end\n"
                 "endmodule\n"},
    {// kMethodCallExtension inside if-statement headers
     .input = "module m;initial begin if (aa.bb(cc)) x = y;end endmodule",
     .expected = "module m;\n"
                 "  initial begin\n"
                 "    if (aa.bb(cc)) x = y;\n"
                 "  end\n"
                 "endmodule\n"},
    {// initial statement with object method call
     .input = "module m; initial a.b(a,b,c); endmodule\n",
     .expected = "module m;\n"
                 "  initial a.b(a, b, c);\n"
                 "endmodule\n"},
    {// initial statement with method call on indexed object
     .input = "module m; initial a[i].b(a,b,c); endmodule\n",
     .expected = "module m;\n"
                 "  initial a[i].b(a, b, c);\n"
                 "endmodule\n"},
    {// initial statement with method call on function returned object
     .input = "module m; initial a(d,e,f).b(a,b,c); endmodule\n",
     .expected = "module m;\n"
                 "  initial a(d, e, f).b(a, b, c);\n"
                 "endmodule\n"},
    {// initial statement with indexed access to function returned object
     .input = "module m; initial a(a,b,c)[i]; endmodule\n",
     .expected = "module m;\n"
                 "  initial a(a, b, c) [i];\n"
                 "endmodule\n"},
    {// method call with no arguments on an object
     .input = "module m; initial foo.bar();endmodule\n",
     .expected = "module m;\n"
                 "  initial foo.bar();\n"
                 "endmodule\n"},
    {// method call with one argument on an object
     .input = "module m; initial foo.bar(aa);endmodule\n",
     .expected = "module m;\n"
                 "  initial foo.bar(aa);\n"
                 "endmodule\n"},
    {// method call with two arguments on an object
     .input = "module m; initial foo.bar(aa,bb);endmodule\n",
     .expected = "module m;\n"
                 "  initial foo.bar(aa, bb);\n"
                 "endmodule\n"},
    {// method call with three arguments on an object
     .input = "module m; initial foo.bar(aa,bb,cc);endmodule\n",
     .expected = "module m;\n"
                 "  initial foo.bar(aa, bb, cc);\n"
                 "endmodule\n"},

    {
        // This tests for if-statements with null statements
        .input = "function foo;"
                 "if (zz) ; "
                 "if (yy) ; "
                 "endfunction",
        .expected = "function foo;\n"
                    "  if (zz);\n"
                    "  if (yy);\n"
                    "endfunction\n",
    },

    {
        // This tests for if-statements starting on their own line.
        .input = "function foo;"
                 "if (zz) begin "
                 "return 0;"
                 "end "
                 "if (yy) begin "
                 "return 1;"
                 "end "
                 "endfunction",
        .expected = "function foo;\n"
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
        .input = "function foo;"
                 "if (zz) return 0;"
                 "if (yy) return 1;"
                 "endfunction",
        .expected = "function foo;\n"
                    "  if (zz) return 0;\n"
                    "  if (yy) return 1;\n"
                    "endfunction\n",
    },

    {
        // This tests for if-statement mixed with plain statements
        .input = "function foo;"
                 "a=b;"
                 "if (zz) return 0;"
                 "c=d;"
                 "endfunction",
        .expected = "function foo;\n"
                    "  a = b;\n"
                    "  if (zz) return 0;\n"
                    "  c = d;\n"
                    "endfunction\n",
    },

    {
        // This tests for if-statement with forced break mixed with others
        .input = "function foo;"
                 "a=b;"
                 "if (zz)//\n return 0;"
                 "c=d;"
                 "endfunction",
        .expected = "function foo;\n"
                    "  a = b;\n"
                    "  if (zz)  //\n"
                    "    return 0;\n"
                    "  c = d;\n"
                    "endfunction\n",
    },
    {
        .input = "function t;"
                 "if (r == t)"
                 "a.b(c);"
                 "endfunction",
        .expected = "function t;\n"
                    "  if (r == t) a.b(c);\n"
                    "endfunction\n",
    },

    {// This tests for for-statement with forced break mixed with others
     .input = "function f;"
              "x=y;"
              "for (int i=0; i<S*IPS; i++) #1ps a += $urandom();"
              "return 2;"
              "endfunction",
     .expected =
         "function f;\n"
         "  x = y;\n"
         "  for (int i = 0; i < S * IPS; i++)\n"  // doesn't fit, so indents
         "    #1ps a += $urandom();\n"
         "  return 2;\n"
         "endfunction\n"},

    {
        // This tests for-statements with null statements
        .input = "function foo;"
                 "for(;;)  ;\t"
                 "for(;;)  ;\t"
                 "endfunction",
        .expected = "function foo;\n"
                    "  for (;;);\n"
                    "  for (;;);\n"
                    "endfunction\n",
    },

    {
        // This tests for if-else-statements with null statements
        .input = "function foo;"
                 "if (zz) ;  else  ;"
                 "if (yy) ;   else   ;"
                 "endfunction",
        .expected = "function foo;\n"
                    "  if (zz);\n"
                    "  else;\n"
                    "  if (yy);\n"
                    "  else;\n"
                    "endfunction\n",
    },

    {
        // This tests for end-else-begin.
        .input = "function foo;"
                 "if (zz) begin "
                 "return 0;"
                 "end "
                 "else "
                 "begin "
                 "return 1;"
                 "end "
                 "endfunction",
        .expected = "function foo;\n"
                    "  if (zz) begin\n"
                    "    return 0;\n"
                    "  end else begin\n"
                    "    return 1;\n"
                    "  end\n"
                    "endfunction\n",
    },
    {
        // This tests for end-else-if
        .input = "function foo;"
                 "if (zz) begin "
                 "return 0;"
                 "end "
                 "else "
                 "if(yy)begin "
                 "return 1;"
                 "end "
                 "endfunction",
        .expected = "function foo;\n"
                    "  if (zz) begin\n"
                    "    return 0;\n"
                    "  end else if (yy) begin\n"
                    "    return 1;\n"
                    "  end\n"
                    "endfunction\n",
    },
    {
        // This tests labeled end-else-if
        .input = "function foo;"
                 "if (zz) begin : label1 "
                 "return 0;"
                 "end : label1 "
                 "else if (yy) begin : label2 "
                 "return 1;"
                 "end : label2 "
                 "endfunction",
        .expected = "function foo;\n"
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
        .input = "function foo;"
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
        .expected = "function foo;\n"
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
        .input = "function r ;"
                 "if ( ! randomize (bar )) begin    end "
                 "if ( ! obj.randomize (bar )) begin    end "
                 "endfunction",
        .expected = "function r;\n"
                    "  if (!randomize(bar)) begin\n"
                    "  end\n"
                    "  if (!obj.randomize(bar)) begin\n"
                    "  end\n"
                    "endfunction\n",
    },
    {
        // randomize-with call, with comments
        .input = "function f;"
                 "s = std::randomize() with {\n"
                 "// comment1\n"
                 "a == e;\n"
                 "// comment2\n"
                 "};"
                 "endfunction\n",
        .expected = "function f;\n"
                    "  s = std::randomize() with {\n"
                    "    // comment1\n"
                    "    a == e;\n"
                    "    // comment2\n"
                    "  };\n"
                    "endfunction\n",
    },
    {
        // randomize-with call, with comments, one joined
        .input = "function f;"
                 "s = std::randomize() with {\n"
                 "// comment1\n"
                 "a == e;// comment2\n"
                 "};"
                 "endfunction\n",
        .expected = "function f;\n"
                    "  s = std::randomize() with {\n"
                    "    // comment1\n"
                    "    a == e;  // comment2\n"
                    "  };\n"
                    "endfunction\n",
    },
    {
        // randomize-with call, with comment, and conditional
        .input = "function f;"
                 "s = std::randomize() with {\n"
                 "// comment\n"
                 "a == e;"
                 "if (x) {"
                 "a;"
                 "}"
                 "};"
                 "endfunction\n",
        .expected = "function f;\n"
                    "  s = std::randomize() with {\n"
                    "    // comment\n"
                    "    a == e;\n"
                    "    if (x) {a;}\n"  // TODO(fangism): consider expanding
                    "  };\n"
                    "endfunction\n",
    },

    // module declaration test cases
    {.input = "   module       foo  ;     endmodule\n",
     .expected = "module foo;\n"
                 "endmodule\n"},
    {.input = "   module       foo   (    )   ;     endmodule\n",
     .expected = "module foo ();\n"
                 "endmodule\n"},
    {.input = "   module       foo   (  .x (  x) );     endmodule\n",
     .expected = "module foo (\n"
                 "    .x(x)\n"
                 ");\n"
                 "endmodule\n"},
    {.input = "   module       foo   (  .x (  x)  \n,\n . y "
              "  ( \ny) );     endmodule\n",
     .expected = "module foo (\n"
                 "    .x(x),\n"
                 "    .y(y)\n"
                 ");\n"
                 "endmodule\n"},

    // module instantiation test cases
    {.input = "  module foo   ; bar bq();endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq ();\n"  // single instance
                 "endmodule\n"},
    {.input = "  module foo   ; bar bq(), bq2(  );endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (), bq2 ();\n"  // multiple instances, still fitting
                                           // on one line
                 "endmodule\n"},
    {.input = "module foo; bar #(.N(N)) bq (.bus(bus));endmodule\n",
     // instance parameter and port fits on line
     .expected = "module foo;\n"
                 "  bar #(.N(N)) bq (.bus(bus));\n"
                 "endmodule\n"},
    {.input = "module foo; bar #(.N(N),.M(M)) bq ();endmodule\n",  // two named
                                                                   // params
     .expected = "module foo;\n"
                 "  bar #(\n"
                 "      .N(N),\n"
                 "      .M(M)\n"
                 "  ) bq ();\n"
                 "endmodule\n"},
    {.input = "module foo; bar #(//comment\n.N(N),.M(M)) bq ();endmodule\n",
     .expected = "module foo;\n"
                 "  bar #(  //comment\n"  // EOL comment before first param
                 "      .N(N),\n"
                 "      .M(M)\n"
                 "  ) bq ();\n"
                 "endmodule\n"},
    {.input = "module foo; bar #(.N(N),//comment\n.M(M)) bq ();endmodule\n",
     .expected = "module foo;\n"
                 "  bar #(\n"
                 "      .N(N),  //comment\n"  // EOL comment after first param
                 "      .M(M)\n"
                 "  ) bq ();\n"
                 "endmodule\n"},
    {.input = "module foo; bar #(.N(N),.M(M)//comment\n) bq ();endmodule\n",
     .expected = "module foo;\n"
                 "  bar #(\n"
                 "      .N(N),\n"
                 "      .M(M)   //comment\n"  // EOL comment after last param
                 "  ) bq ();\n"
                 "endmodule\n"},
    {.input = "  module foo   ; bar bq(aa,bb,cc);endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (\n"
                 "      aa,\n"
                 "      bb,\n"
                 "      cc\n"
                 "  );\n"  // multiple positional ports, one per line
                 "endmodule\n"},
    {.input = "  module foo   ; bar bq(aa,\n"
              "`ifdef BB\n"
              "bb,\n"
              "`endif\n"
              "cc);endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (\n"
                 "      aa,\n"
                 "`ifdef BB\n"
                 "      bb,\n"  // keep same indentation as outside conditional
                 "`endif\n"
                 "      cc\n"
                 "  );\n"  // multiple positional ports, one per line
                 "endmodule\n"},
    {.input = "  module foo   ; bar bq(.aa,.bb);endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (\n"
                 "      .aa,\n"
                 "      .bb\n"
                 "  );\n"  // multiple named ports, one per line
                 "endmodule\n"},
    {.input = "  module foo   ; bar bq(.aa(aa),.bb(bb));endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (\n"
                 "      .aa(aa),\n"
                 "      .bb(bb)\n"
                 "  );\n"  // multiple named ports, one per line
                 "endmodule\n"},
    {.input = "  module foo   ; bar bq(.aa(aa),\n"
              "`ifdef ZZ\n"
              ".zz(  zz  ),\n"
              "`else\n"
              ".yy(  yy  ),\n"
              "`endif\n"
              ".bb(bb)\n"
              ");endmodule\n",
     .expected = "module foo;\n"
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
    {.input = "  module foo   ; bar#(NNNNNNNN)"
              "bq(.aa(aaaaaa),.bb(bbbbbb));endmodule\n",
     .expected = "module foo;\n"
                 "  bar #(NNNNNNNN) bq (\n"
                 "      .aa(aaaaaa),\n"
                 "      .bb(bbbbbb)\n"
                 "  );\n"
                 "endmodule\n"},
    {.input = " module foo   ; barrrrrrr "
              "bq(.aaaaaa(aaaaaa),.bbbbbb(bbbbbb));endmodule\n",
     .expected = "module foo;\n"
                 "  barrrrrrr bq (\n"
                 "      .aaaaaa(aaaaaa),\n"
                 "      .bbbbbb(bbbbbb)\n"
                 "  );\n"
                 "endmodule\n"},
    {.input =
         "module foo; bar #(.NNNNN(NNNNN)) bq (.bussss(bussss));endmodule\n",
     // instance parameter and port does not fit on line
     .expected = "module foo;\n"
                 "  bar #(\n"
                 "      .NNNNN(NNNNN)\n"
                 "  ) bq (\n"
                 "      .bussss(bussss)\n"
                 "  );\n"
                 "endmodule\n"},
    {.input = "module foo; bar #(//\n.N(N)) bq (.bus(bus));endmodule\n",
     .expected =
         "module foo;\n"
         "  bar #(  //\n"  // would fit on one line, but forced to expand by //
         "      .N(N)\n"
         "  ) bq (\n"
         "      .bus(bus)\n"
         "  );\n"
         "endmodule\n"},
    {.input = "module foo; bar #(\n"
              "`ifdef MM\n"
              ".M(M)\n"
              "`else\n"
              ".N(N)\n"
              "`endif\n"
              ") bq (.bus(bus));endmodule\n",
     .expected = "module foo;\n"
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
    {.input = "module foo; bar #(.N(N)//\n) bq (.bus(bus));endmodule\n",
     .expected =
         "module foo;\n"
         "  bar #(\n"  // would fit on one line, but forced to expand by //
         "      .N(N)  //\n"
         "  ) bq (\n"
         "      .bus(bus)\n"
         "  );\n"
         "endmodule\n"},
    {.input = "module foo; bar #(.N(N)) bq (//\n.bus(bus));endmodule\n",
     .expected =
         "module foo;\n"
         "  bar #(\n"  // would fit on one line, but forced to expand by //
         "      .N(N)\n"
         "  ) bq (  //\n"
         "      .bus(bus)\n"
         "  );\n"
         "endmodule\n"},
    {.input = "module foo; bar #(.N(N)) bq (.bus(bus)//\n);endmodule\n",
     .expected =
         "module foo;\n"
         "  bar #(\n"  // would fit on one line, but forced to expand by //
         "      .N(N)\n"
         "  ) bq (\n"
         "      .bus(bus)  //\n"
         "  );\n"
         "endmodule\n"},
    {.input = " module foo   ; bar "
              "bq(.aaa(aaa),.bbb(bbb),.ccc(ccc),.ddd(ddd));endmodule\n",
     .expected =
         "module foo;\n"
         "  bar bq (\n"
         "      .aaa(aaa),\n"  // ports don't fit on one line, so expanded
         "      .bbb(bbb),\n"
         "      .ccc(ccc),\n"
         "      .ddd(ddd)\n"
         "  );\n"
         "endmodule\n"},
    {.input = " module foo   ; bar "
              "bq(.aa(aa),.bb(bb),.cc(cc),.dd(dd));endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (\n"
                 "      .aa(aa),\n"  // one named port per line
                 "      .bb(bb),\n"
                 "      .cc(cc),\n"
                 "      .dd(dd)\n"
                 "  );\n"
                 "endmodule\n"},
    {.input = " module foo   ; bar "
              "bq(.aa(aa),//\n.bb(bb),.cc(cc),.dd(dd));endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (\n"
                 "      .aa(aa),  //\n"  // forced to expand by //
                 "      .bb(bb),\n"
                 "      .cc(cc),\n"
                 "      .dd(dd)\n"
                 "  );\n"
                 "endmodule\n"},
    {.input = " module foo   ; bar "
              "bq(.aa(aa),.bb(bb),.cc(cc),.dd(dd)//\n);endmodule\n",
     .expected = "module foo;\n"
                 "  bar bq (\n"
                 "      .aa(aa),\n"
                 "      .bb(bb),\n"
                 "      .cc(cc),\n"
                 "      .dd(dd)   //\n"  // forced to expand by //
                 "  );\n"
                 "endmodule\n"},
    {// gate instantiation test
     .input = "module m;"
              "and\tx0(a, \t\tb,c);"
              "or\nx1(a,  \n b,    d);"
              "endmodule\n",
     .expected = "module m;\n"
                 "  and x0 (a, b, c);\n"
                 "  or x1 (a, b, d);\n"
                 "endmodule\n"},
    {// ifdef inside port actuals
     .input = "module m;  foo bar   (\n"
              "`ifdef   BAZ\n"
              "`endif\n"
              ")  ;endmodule\n",
     .expected = "module m;\n"
                 "  foo bar (\n"
                 "`ifdef BAZ\n"
                 "`endif\n"
                 "  );\n"
                 "endmodule\n"},
    {// ifdef inside port actuals after a port connection
     .input = "module m;  foo bar   ( .a (a) ,\n"
              "`ifdef   BAZ\n"
              "`endif\n"
              ")  ;endmodule\n",
     .expected = "module m;\n"
                 "  foo bar (\n"
                 "      .a(a),\n"
                 "`ifdef BAZ\n"
                 "`endif\n"
                 "  );\n"
                 "endmodule\n"},
    {// ifdef inside port actuals before a port connection
     .input = "module m;  foo bar   (\n"
              "`ifdef   BAZ\n"
              "`endif\n"
              ". b(b) )  ;endmodule\n",
     .expected = "module m;\n"
                 "  foo bar (\n"
                 "`ifdef BAZ\n"
                 "`endif\n"
                 "      .b(b)\n"
                 "  );\n"
                 "endmodule\n"},
    {// ifdef-conditional port connection
     .input = "module m;  foo bar   (\n"
              "`ifdef   BAZ\n"
              ". c (\tc) \n"
              "`endif\n"
              " )  ;endmodule\n",
     .expected = "module m;\n"
                 "  foo bar (\n"
                 "`ifdef BAZ\n"
                 "      .c(c)\n"
                 "`endif\n"
                 "  );\n"
                 "endmodule\n"},
    {// ifndef-else-conditional port connection
     .input = "module m;  foo bar   (\n"
              "`ifndef   BAZ\n"
              ". c (\tc) \n"
              "  `else\n"
              " . d(d\t)\n"
              "  `endif\n"
              " )  ;endmodule\n",
     .expected = "module m;\n"
                 "  foo bar (\n"
                 "`ifndef BAZ\n"
                 "      .c(c)\n"
                 "`else\n"
                 "      .d(d)\n"
                 "`endif\n"
                 "  );\n"
                 "endmodule\n"},

    {// comment following a delay in next line
     .input = "module t;\n"
              "reg x;\n"
              "initial begin\n"
              "#20\n"
              "//comment\n"
              "x = 1;\n"
              "x = 2;\n"
              "end\n"
              "endmodule\n",
     .expected = "module t;\n"
                 "  reg x;\n"
                 "  initial begin\n"
                 "    #20\n"
                 "    //comment\n"
                 "    x = 1;\n"
                 "    x = 2;\n"
                 "  end\n"
                 "endmodule\n"},
    {// comment following a delay in the same line
     .input = "module t;\n"
              "reg x;\n"
              "initial begin\n"
              "#20 //comment\n"
              "x = 1;\n"
              "x = 2;\n"
              "end\n"
              "endmodule\n",
     .expected = "module t;\n"
                 "  reg x;\n"
                 "  initial begin\n"
                 "    #20  //comment\n"
                 "    x = 1;\n"
                 "    x = 2;\n"
                 "  end\n"
                 "endmodule\n"},
    // Macro calls inside [...]
    {.input = "module foo;\nlogic [`BAR(1)\n+2] foo;\nendmodule",
     .expected = "module foo;\n"
                 "  logic [`BAR(1)\n"
                 "+2] foo;\n"
                 "endmodule\n"},
    {.input = "module foo;\nlogic [`BAR(1)+2] foo;\nendmodule",
     .expected = "module foo;\n"
                 "  logic [`BAR(1)+2] foo;\n"
                 "endmodule\n"},
    {.input = "module foo ();\n  function bar();\n    "
              "logic [12:34] data[`AAAAAAAAAAAAAAAAAAAAAAAA(1) : "
              "`BBBBBBBBBBBBBBBBBBBBBBBB(2) + 3];\n  "
              "endfunction\nendmodule",
     .expected = "module foo ();\n"
                 "  function bar();\n"
                 "    logic [12:34] data[\n"
                 "    `AAAAAAAAAAAAAAAAAAAAAAAA(1) :\n"
                 "    `BBBBBBBBBBBBBBBBBBBBBBBB(2) + 3];\n"
                 "  endfunction\n"
                 "endmodule\n"},
    {
        // test that alternate top-syntax mode works
        .input = "// verilog_syntax: parse-as-module-body\n"
                 "`define           FOO\n",
        .expected = "// verilog_syntax: parse-as-module-body\n"
                    "`define FOO\n",
    },
    {
        // test alternate parsing mode in macro expansion
        .input = "class foo;\n"
                 "`MY_MACRO(\n"
                 " // verilog_syntax: parse-as-statements\n"
                 " // EOL comment\n"
                 " int count;\n"
                 " if(cfg.enable) begin\n"
                 " count = 1;\n"
                 " end,\n"
                 " utils_pkg::decrement())\n"
                 "endclass\n",
        .expected = "class foo;\n"
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
    {.input = "a;",  // implicit type
     .expected = "a;\n"},
    {.input = "a\tb;",  // explicit type
     .expected = "a b;\n"},
    {.input = "a;b;",
     .expected = "a;\n"
                 "b;\n"},
    {.input = "a ,b;",  // implicit type
     .expected = "a, b;\n"},
    /* TODO(b/149591599): implicit type data declarations in module body
    {"module\tm ;a ;endmodule",
     "module m;\n"
     "  a;\n"
     "endmodule\n"},
    */
    {.input = "package\tp ;a ;endpackage",  // implicit type
     .expected = "package p;\n"
                 "  a;\n"
                 "endpackage\n"},
    {.input = "package\tp ;a ,b ;endpackage",  // implicit type
     .expected = "package p;\n"
                 "  a, b;\n"
                 "endpackage\n"},
    {.input = "package\tp ;a ;b ;endpackage",  // implicit type
     .expected = "package p;\n"
                 "  a;\n"
                 "  b;\n"
                 "endpackage\n"},
    /* TODO(b/149591627) : implicit type data declarations in class body
    {"class\tc ;a ;endclass",
     "class c;\n"
     "  a;\n"
     "endclass\n"},
     */
    {.input = "function\tf ;a ;endfunction",  // implicit type
     .expected = "function f;\n"
                 "  a;\n"
                 "endfunction\n"},
    {.input = "function\tf ;a   ;x ;endfunction",  // implicit type
     .expected = "function f;\n"
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
    {.input = "task\tt ;a ;endtask",  // implicit type
     .expected = "task t;\n"
                 "  a;\n"
                 "endtask\n"},
    {.input = "task\tt ;a   ;x ;endtask",  // implicit type
     .expected = "task t;\n"
                 "  a;\n"
                 "  x;\n"
                 "endtask\n"},

    {// tests bind declaration
     .input = "bind   foo   bar baz  ( . clk ( clk  ) ) ;",
     .expected = "bind foo bar baz (.clk(clk));\n"},
    {// tests bind declaration, with type params
     .input = "bind   foo   bar# ( . W ( W ) ) baz  ( . clk ( clk  ) ) ;",
     .expected = "bind foo bar #(.W(W)) baz (.clk(clk));\n"},
    {// tests bind declarations
     .input = "bind   foo   bar baz  ( ) ;"
              "bind goo  car  caz (   );",
     .expected = "bind foo bar baz ();\n"
                 "bind goo car caz ();\n"},

    {.input = "bind blah foo #( .MaxCount(MaxCount), .MaxDelta(MaxDelta)) bar ("
              "    .clk(clk), .rst(rst), .value(value) );",
     .expected = "bind blah foo #(\n"
                 "    .MaxCount(MaxCount),\n"
                 "    .MaxDelta(MaxDelta)\n"
                 ") bar (\n"
                 "    .clk  (clk),\n"
                 "    .rst  (rst),\n"
                 "    .value(value)\n"
                 ");\n"},
    {.input = "bind foo bar baz(\\\n"
              "`undef d\\\n"
              "`undef d);",
     .expected = "bind foo bar baz (\\\n"
                 "    `undef d\\\n"
                 "    `undef d\n"
                 ");\n"},
    {
        .input = "bind expaaaaaaaaaaand_meeee looooooooong_name# ("
                 ".W(W_CONST), .H(H_CONST), .D(D_CONST)  )"
                 "instaaance_name (.in(iiiiiiiin), .out(ooooooout), "
                 ".clk(ccccccclk));",
        .expected = "bind expaaaaaaaaaaand_meeee\n"
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
        .input = "bind expand_inst name# ("
                 ".W(W_CONST), .H(H_CONST), .D(D_CONST)  )"
                 "instaaance_name (.in(iiiiiiiin), .out(ooooooout), "
                 ".clk(ccccccclk));",
        .expected = "bind expand_inst name #(\n"
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
        .input = "import  foo_pkg :: bar ;",
        .expected = "import foo_pkg::bar;\n",
    },
    {
        // tests import declaration with wildcard
        .input = "import  foo_pkg :: * ;",
        .expected = "import foo_pkg::*;\n",
    },
    {
        // tests import declarations
        .input = "import  foo_pkg :: *\t;"
                 "import  goo_pkg\n:: thing ;",
        .expected = "import foo_pkg::*;\n"
                    "import goo_pkg::thing;\n",
    },
    // preserve spaces inside [] dimensions, but limit spaces around ':' to one
    // and adjust everything else
    {.input = "foo[W-1:0]a[0:K-1];",  // data declaration
     .expected = "foo [W-1:0] a[0:K-1];\n"},
    {.input = "foo[W-1 : 0]a[0 : K-1];",
     .expected = "foo [W-1 : 0] a[0 : K-1];\n"},
    {.input = "foo[W  -  1 : 0 ]a [ 0  :  K  -  1] ;",
     .expected = "foo [W  -  1 : 0] a[0 : K  -  1];\n"},
    // remove spaces between [...] [...] in multi-dimension arrays
    {.input = "foo[K] [W]a;",  //
     .expected = "foo [K][W] a;\n"},
    {.input = "foo b [K] [W] ;",  //
     .expected = "foo b[K][W];\n"},
    {.input = "logic[K:1] [W:1]a;",  //
     .expected = "logic [K:1][W:1] a;\n"},
    {.input = "logic b [K:1] [W:1] ;",  //
     .expected = "logic b[K:1][W:1];\n"},
    // spaces in bit slicing
    {
        // preserve 0 spaces
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7:2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7:2];\n"
                    "end\n",
    },
    {
        // preserve 1 space
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7 : 2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7 : 2];\n"
                    "end\n",
    },
    {
        // limit multiple spaces to 1
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7  :  2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7 : 2];\n"
                    "end\n",
    },
    {
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7  : 2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7 : 2];\n"
                    "end\n",
    },
    {
        // keep value on the left when symmetrizing
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7: 2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7:2];\n"
                    "end\n",
    },
    {
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7:  2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7:2];\n"
                    "end\n",
    },
    {
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7 :2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7 : 2];\n"
                    "end\n",
    },
    {
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7 :  2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7 : 2];\n"
                    "end\n",
    },
    {
        // use value on the left, but limit to 1 space
        .input = "always_ff @(posedge clk) begin "
                 "dummy  <=\tfoo  [  7  :2  ] ; "
                 "end",
        .expected = "always_ff @(posedge clk) begin\n"
                    "  dummy <= foo[7 : 2];\n"
                    "end\n",
    },

    // task test cases
    {.input = "task t ;endtask:t",  //
     .expected = "task t;\n"
                 "endtask : t\n"},
    {.input = "task t ;#   10 ;# 5ns ; # 0.1 ; # 1step ;endtask",
     .expected = "task t;\n"
                 "  #10;\n"  // no space in delay expression
                 "  #5ns;\n"
                 "  #0.1;\n"
                 "  #1step;\n"
                 "endtask\n"},
    {.input = "task t\n;a<=b ;c<=d ;endtask\n",
     .expected = "task t;\n"
                 "  a <= b;\n"
                 "  c <= d;\n"
                 "endtask\n"},
    {.input = "class c;   virtual protected task\tt  ( foo bar);"
              "a.a<=b.b;\t\tc.c\n<=   d.d; endtask   endclass",
     .expected = "class c;\n"
                 "  virtual protected task t(foo bar);\n"
                 "    a.a <= b.b;\n"
                 "    c.c <= d.d;\n"
                 "  endtask\n"
                 "endclass\n"},
    {.input = "task t;\n// statement comment\nendtask\n",
     .expected = "task t;\n"
                 "  // statement comment\n"  // indented
                 "endtask\n"},
    {.input = "task t( );\n// statement comment\nendtask\n",
     .expected = "task t();\n"
                 "  // statement comment\n"  // indented
                 "endtask\n"},
    {.input = "task t( input x  );\n"
              "// statement comment\n"
              "s();\n"
              "// statement comment\n"
              "endtask\n",
     .expected = "task t(input x);\n"
                 "  // statement comment\n"  // indented
                 "  s();\n"
                 "  // statement comment\n"  // indented
                 "endtask\n"},
    {.input = "task fj;fork join fork join\tendtask",
     .expected = "task fj;\n"
                 "  fork\n"
                 "  join\n"
                 "  fork\n"
                 "  join\n"
                 "endtask\n"},
    {.input = "task fj;fork join_any fork join_any\tendtask",
     .expected = "task fj;\n"
                 "  fork\n"
                 "  join_any\n"
                 "  fork\n"
                 "  join_any\n"
                 "endtask\n"},
    {.input = "task fj;fork join_none fork join_none\tendtask",
     .expected = "task fj;\n"
                 "  fork\n"
                 "  join_none\n"
                 "  fork\n"
                 "  join_none\n"
                 "endtask\n"},
    {.input = "task fj;fork\n"
              "//c1\njoin\n"
              "//c2\n"
              "fork  \n"
              "//c3\n"
              "join\tendtask",
     .expected = "task fj;\n"
                 "  fork\n"
                 "    //c1\n"
                 "  join\n"
                 "  //c2\n"
                 "  fork\n"
                 "    //c3\n"
                 "  join\n"
                 "endtask\n"},
    {.input = "task fj;\n"
              "fork "
              "begin "
              "end "
              "foo();"
              "begin "
              "end "
              "join_any endtask",
     .expected = "task fj;\n"
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
        .input = "task  t ;Fire() ;assert ( x);assert(y );endtask",
        .expected = "task t;\n"
                    "  Fire();\n"
                    "  assert (x);\n"
                    "  assert (y);\n"
                    "endtask\n",
    },
    {
        // assertion statements with body clause
        .input =
            "task  t ;Fire() ;assert ( x) fee ( );assert(y ) foo ( ) ;endtask",
        .expected = "task t;\n"
                    "  Fire();\n"
                    "  assert (x) fee();\n"
                    "  assert (y) foo();\n"
                    "endtask\n",
    },
    {
        // assertion statements with else clause
        .input = "task  t ;Fire() ;assert ( x) else  fee ( );"
                 "assert(y ) else  foo ( ) ;endtask",
        .expected = "task t;\n"
                    "  Fire();\n"
                    "  assert (x)\n"
                    "  else fee();\n"
                    "  assert (y)\n"
                    "  else foo();\n"
                    "endtask\n",
    },
    {
        // assertion statements with else clause
        .input = "task  t ;Fire() ;assert ( x) fa(); else  fee ( );"
                 "assert(y ) fi(); else  foo ( ) ;endtask",
        .expected = "task t;\n"
                    "  Fire();\n"
                    "  assert (x) fa();\n"
                    "  else fee();\n"
                    "  assert (y) fi();\n"
                    "  else foo();\n"
                    "endtask\n",
    },
    {
        // assume statements
        .input = "task  t ;Fire() ;assume ( x);assume(y );endtask",
        .expected = "task t;\n"
                    "  Fire();\n"
                    "  assume (x);\n"
                    "  assume (y);\n"
                    "endtask\n",
    },
    {
        // cover statements
        .input = "task  t ;Fire() ;cover ( x);cover(y );endtask",
        .expected = "task t;\n"
                    "  Fire();\n"
                    "  cover (x);\n"
                    "  cover (y);\n"
                    "endtask\n",
    },
    {
        // cover statements, with action
        .input = "task  t ;Fire() ;cover ( x)g( );cover(y ) h();endtask",
        .expected = "task t;\n"
                    "  Fire();\n"
                    "  cover (x) g();\n"
                    "  cover (y) h();\n"
                    "endtask\n",
    },
    {
        // cover statements, with action block
        .input = "task  t ;Fire() ;cover ( x) begin g( ); end "
                 "cover(y ) begin h(); end endtask",
        .expected = "task t;\n"
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
     .input = "task t; foo. shuffle  ( );bar .shuffle( ); endtask",
     .expected = "task t;\n"
                 "  foo.shuffle();\n"
                 "  bar.shuffle();\n"
                 "endtask\n"},
    {// wait statements (null)
     .input = "task t; wait  (a==b);wait(c<d); endtask",
     .expected = "task t;\n"
                 "  wait (a == b);\n"
                 "  wait (c < d);\n"
                 "endtask\n"},
    {// wait statements, single action statement
     .input = "task t; wait  (a==b) p();wait(c<d) q(); endtask",
     .expected = "task t;\n"
                 "  wait (a == b) p();\n"
                 "  wait (c < d) q();\n"
                 "endtask\n"},
    {// wait statements, block action statement
     .input = "task t; wait  (a==b) begin p(); end "
              "wait(c<d) begin q(); end endtask",
     .expected = "task t;\n"
                 "  wait (a == b) begin\n"
                 "    p();\n"
                 "  end\n"
                 "  wait (c < d) begin\n"
                 "    q();\n"
                 "  end\n"
                 "endtask\n"},
    {// wait fork statements
     .input = "task t ; wait\tfork;wait   fork ;endtask",
     .expected = "task t;\n"
                 "  wait fork;\n"
                 "  wait fork;\n"
                 "endtask\n"},
    {// labeled single statements (prefix-style)
     .input = "task t;l1:x<=y ;endtask",
     .expected = "task t;\n"
                 "  l1 : x <= y;\n"
                 "endtask\n"},
    {// labeled block statements (prefix-style)
     .input = "task t;l1:begin end:l1 endtask",
     .expected = "task t;\n"
                 "  l1 : begin\n"
                 "  end : l1\n"
                 "endtask\n"},
    {// labeled seq block statements
     .input = "task t;begin:l1 end:l1 endtask",
     .expected = "task t;\n"
                 "  begin : l1\n"
                 "  end : l1\n"
                 "endtask\n"},
    {// labeled par block statements
     .input = "task t;fork:l1 join:l1 endtask",
     .expected = "task t;\n"
                 "  fork : l1\n"
                 "  join : l1\n"
                 "endtask\n"},
    {// task with disable statements
     .input = "task  t ;fork\tjoin\tdisable\tfork;endtask",
     .expected = "task t;\n"
                 "  fork\n"
                 "  join\n"
                 "  disable fork;\n"
                 "endtask\n"},
    {.input = "task  t ;fork\tjoin_any\tdisable\tfork  ;endtask",
     .expected = "task t;\n"
                 "  fork\n"
                 "  join_any\n"
                 "  disable fork;\n"
                 "endtask\n"},
    {.input = "task  t ;disable\tbean_counter  ;endtask",
     .expected = "task t;\n"
                 "  disable bean_counter;\n"
                 "endtask\n"},
    {
        // task with if-statement
        .input = "task t;"
                 "if (r == t)"
                 "a.b(c);"
                 "endtask",
        .expected = "task t;\n"
                    "  if (r == t) a.b(c);\n"
                    "endtask\n",
    },
    {
        // task with system call inside if header
        .input = "task t;"
                 "if (!$cast(ssssssssssssssss,vvvvvvvvvv,gggggggg))begin end "
                 "endtask:t",
        .expected = "task t;\n"
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
        .input = "task t;"
                 "if (!$cast(ssssssssssssssss, vvvvvvvvvv.gggggggg("
                 ".ppppppp(ppppppp),"
                 ".yyyyy(\"xxxxxxxxxxxxx\")"
                 "))) begin "
                 "end "
                 "endtask : t",
        .expected = "task t;\n"
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
        .input = "task  t ;assert  property( x);assert\tproperty(y );endtask",
        .expected = "task t;\n"
                    "  assert property (x);\n"
                    "  assert property (y);\n"
                    "endtask\n",
    },
    {
        // assert property statements, with action
        .input = "task  t ;assert  property( x) j();assert\tproperty(y )k( "
                 ");endtask",
        .expected = "task t;\n"
                    "  assert property (x) j();\n"
                    "  assert property (y) k();\n"
                    "endtask\n",
    },
    {
        // assert property statement, with prefix inc/dec
        .input = "task  t ;assert  property( x) ++j; else --k;endtask",
        .expected = "task t;\n"
                    "  assert property (x) ++j;\n"
                    "  else --k;\n"
                    "endtask\n",
    },
    {
        // assert property statements, with action block
        .input = "task  t ;assert  property( x) begin j();end "
                 " assert\tproperty(y )begin\tk( );  end endtask",
        .expected = "task t;\n"
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
        .input = "task  t ;assert  property( x) else;assert\tproperty(y "
                 ")else;endtask",
        .expected = "task t;\n"
                    "  assert property (x)\n"
                    "  else;\n"
                    "  assert property (y)\n"
                    "  else;\n"
                    "endtask\n",
    },
    {
        // assert property statements, else with actions
        .input = "task  t ;assert  property( x) f(); else p(); "
                 "\tassert\tproperty(y ) g();else  q( );endtask",
        .expected = "task t;\n"
                    "  assert property (x) f();\n"
                    "  else p();\n"
                    "  assert property (y) g();\n"
                    "  else q();\n"
                    "endtask\n",
    },
    {
        // assert property statement, with action block, else statement
        .input =
            "task  t ;assert  property( x) begin j();end  else\tk( );  endtask",
        .expected = "task t;\n"
                    "  assert property (x) begin\n"
                    "    j();\n"
                    "  end else k();\n"
                    "endtask\n",
    },
    {
        // assert property statement, with action statement, else block
        .input = "task  t ;assert  property( x) j();  else  begin\tk( );end  "
                 "endtask",
        .expected = "task t;\n"
                    "  assert property (x) j();\n"
                    "  else begin\n"
                    "    k();\n"
                    "  end\n"
                    "endtask\n",
    },
    {
        // assert property statement, with action block, else block
        .input = "task  t ;assert  property( x)begin j();end  "
                 "else  begin\tk( );end  endtask",
        .expected = "task t;\n"
                    "  assert property (x) begin\n"
                    "    j();\n"
                    "  end else begin\n"
                    "    k();\n"
                    "  end\n"
                    "endtask\n",
    },

    {
        // assume property statements
        .input = "task  t ;assume  property( x);assume\tproperty(y );endtask",
        .expected = "task t;\n"
                    "  assume property (x);\n"
                    "  assume property (y);\n"
                    "endtask\n",
    },
    {
        // assume property statements, with action
        .input = "task  t ;assume  property( x) j();assume\tproperty(y )k( "
                 ");endtask",
        .expected = "task t;\n"
                    "  assume property (x) j();\n"
                    "  assume property (y) k();\n"
                    "endtask\n",
    },
    {
        // assume property statements, with action block
        .input = "task  t ;assume  property( x) begin j();end "
                 " assume\tproperty(y )begin\tk( );  end endtask",
        .expected = "task t;\n"
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
        .input = "task  t ;assume  property( x) else;assume\tproperty(y "
                 ")else;endtask",
        .expected = "task t;\n"
                    "  assume property (x)\n"
                    "  else;\n"
                    "  assume property (y)\n"
                    "  else;\n"
                    "endtask\n",
    },
    {
        // assume property statements, else with actions
        .input = "task  t ;assume  property( x) f(); else p(); "
                 "\tassume\tproperty(y ) g();else  q( );endtask",
        .expected = "task t;\n"
                    "  assume property (x) f();\n"
                    "  else p();\n"
                    "  assume property (y) g();\n"
                    "  else q();\n"
                    "endtask\n",
    },
    {
        // assume property statement, with action block, else statement
        .input =
            "task  t ;assume  property( x) begin j();end  else\tk( );  endtask",
        .expected = "task t;\n"
                    "  assume property (x) begin\n"
                    "    j();\n"
                    "  end else k();\n"
                    "endtask\n",
    },
    {
        // assume property statement, with action statement, else block
        .input = "task  t ;assume  property( x) j();  else  begin\tk( );end  "
                 "endtask",
        .expected = "task t;\n"
                    "  assume property (x) j();\n"
                    "  else begin\n"
                    "    k();\n"
                    "  end\n"
                    "endtask\n",
    },
    {
        // assume property statement, with action block, else block
        .input = "task  t ;assume  property( x)begin j();end  "
                 "else  begin\tk( );end  endtask",
        .expected = "task t;\n"
                    "  assume property (x) begin\n"
                    "    j();\n"
                    "  end else begin\n"
                    "    k();\n"
                    "  end\n"
                    "endtask\n",
    },

    {
        // expect property statements
        .input = "task  t ;expect  ( x);expect\t(y );endtask",
        .expected = "task t;\n"
                    "  expect (x);\n"
                    "  expect (y);\n"
                    "endtask\n",
    },
    {
        // expect property statements, with action
        .input = "task  t ;expect  ( x) j();expect\t(y )k( );endtask",
        .expected = "task t;\n"
                    "  expect (x) j();\n"
                    "  expect (y) k();\n"
                    "endtask\n",
    },
    {
        // expect property statements, with action block
        .input = "task  t ;expect  ( x) begin j();end "
                 " expect\t(y )begin\tk( );  end endtask",
        .expected = "task t;\n"
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
        .input = "task  t ;expect  ( x) else;expect\t(y )else;endtask",
        .expected = "task t;\n"
                    "  expect (x)\n"
                    "  else;\n"
                    "  expect (y)\n"
                    "  else;\n"
                    "endtask\n",
    },
    {
        // expect property statements, else with actions
        .input = "task  t ;expect  ( x) f(); else p(); "
                 "\texpect\t(y ) g();else  q( );endtask",
        .expected = "task t;\n"
                    "  expect (x) f();\n"
                    "  else p();\n"
                    "  expect (y) g();\n"
                    "  else q();\n"
                    "endtask\n",
    },
    {
        // expect property statement, with action block, else statement
        .input = "task  t ;expect  ( x) begin j();end  else\tk( );  endtask",
        .expected = "task t;\n"
                    "  expect (x) begin\n"
                    "    j();\n"
                    "  end else k();\n"
                    "endtask\n",
    },
    {
        // expect property statement, with action statement, else block
        .input = "task  t ;expect  ( x) j();  else  begin\tk( );end  endtask",
        .expected = "task t;\n"
                    "  expect (x) j();\n"
                    "  else begin\n"
                    "    k();\n"
                    "  end\n"
                    "endtask\n",
    },
    {
        // expect property statement, with action block, else block
        .input = "task  t ;expect  ( x)begin j();end  "
                 "else  begin\tk( );end  endtask",
        .expected = "task t;\n"
                    "  expect (x) begin\n"
                    "    j();\n"
                    "  end else begin\n"
                    "    k();\n"
                    "  end\n"
                    "endtask\n",
    },

    {
        // cover property statements
        .input = "task  t ;cover  property( x);cover\tproperty(y );endtask",
        .expected = "task t;\n"
                    "  cover property (x);\n"
                    "  cover property (y);\n"
                    "endtask\n",
    },
    {
        // cover property statements, with action
        .input =
            "task  t ;cover  property( x) j();cover\tproperty(y )k( );endtask",
        .expected = "task t;\n"
                    "  cover property (x) j();\n"
                    "  cover property (y) k();\n"
                    "endtask\n",
    },
    {
        // cover property statements, with action block
        .input = "task  t ;cover  property( x) begin j();end "
                 " cover\tproperty(y )begin\tk( );  end endtask",
        .expected = "task t;\n"
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
        .input = "task  t ;cover  sequence( x);cover\tsequence(y );endtask",
        .expected = "task t;\n"
                    "  cover sequence (x);\n"
                    "  cover sequence (y);\n"
                    "endtask\n",
    },
    {
        // cover sequence statements, with action
        .input =
            "task  t ;cover  sequence( x) j();cover\tsequence(y )k( );endtask",
        .expected = "task t;\n"
                    "  cover sequence (x) j();\n"
                    "  cover sequence (y) k();\n"
                    "endtask\n",
    },
    {
        // cover sequence statements, with action block
        .input = "task  t ;cover  sequence( x) begin j();end "
                 " cover\tsequence(y )begin\tk( );  end endtask",
        .expected = "task t;\n"
                    "  cover sequence (x) begin\n"
                    "    j();\n"
                    "  end\n"
                    "  cover sequence (y) begin\n"
                    "    k();\n"
                    "  end\n"
                    "endtask\n",
    },

    {// module with disable statements
     .input = "module m;always begin :block disable m.block; end endmodule",
     .expected = "module m;\n"
                 "  always begin : block\n"
                 "    disable m.block;\n"
                 "  end\n"
                 "endmodule\n"},
    {.input = "module m;always begin disable m.block; end endmodule",
     .expected = "module m;\n"
                 "  always begin\n"
                 "    disable m.block;\n"
                 "  end\n"
                 "endmodule\n"},

    // property test cases
    {.input = "module mp ;property p1 ; a|->b;endproperty endmodule",
     .expected = "module mp;\n"
                 "  property p1;\n"
                 "    a |-> b;\n"
                 "  endproperty\n"
                 "endmodule\n"},
    {.input = "module mp ;property p1 ; a|->b;endproperty:p1 endmodule",
     .expected = "module mp;\n"
                 "  property p1;\n"
                 "    a |-> b;\n"
                 "  endproperty : p1\n"  // with end label
                 "endmodule\n"},
    {.input = "module mp ;property p1 ; a|->## 1  b;endproperty endmodule",
     .expected = "module mp;\n"
                 "  property p1;\n"
                 "    a |-> ##1 b;\n"  // with delay
                 "  endproperty\n"
                 "endmodule\n"},
    {.input = "module mp ;property p1 ; a|->## [0: 1]  b;endproperty endmodule",
     .expected = "module mp;\n"
                 "  property p1;\n"
                 "    a |-> ##[0:1] b;\n"  // prefer no spaces with delay range
                 "  endproperty\n"
                 "endmodule\n"},
    {.input =
         "module mp ;property p1 ; a|->## [0  : 1]  b;endproperty endmodule",
     .expected = "module mp;\n"
                 "  property p1;\n"
                 "    a |-> ##[0 : 1] b;\n"  // limit to one space, symmetrize
                 "  endproperty\n"
                 "endmodule\n"},
    {.input = "module mp ;property p1 ; a## 1  b;endproperty endmodule",
     .expected = "module mp;\n"
                 "  property p1;\n"
                 "    a ##1 b;\n"
                 "  endproperty\n"
                 "endmodule\n"},
    {.input = "module mp ;property p1 ; (a^c)## 1  b;endproperty endmodule",
     .expected = "module mp;\n"
                 "  property p1;\n"
                 "    (a ^ c) ##1 b;\n"
                 "  endproperty\n"
                 "endmodule\n"},

    // covergroup test cases
    {// Minimal case
     .input = "covergroup c; endgroup\n",
     .expected = "covergroup c;\n"
                 "endgroup\n"},
    {// Minimal useful case
     .input = "covergroup c @ (posedge clk); coverpoint a; endgroup\n",
     .expected = "covergroup c @(posedge clk);\n"
                 "  coverpoint a;\n"
                 "endgroup\n"},
    {// Multiple coverpoints
     .input = "covergroup foo @(posedge clk); coverpoint a; coverpoint b; "
              "coverpoint c; coverpoint d; endgroup\n",
     .expected = "covergroup foo @(posedge clk);\n"
                 "  coverpoint a;\n"
                 "  coverpoint b;\n"
                 "  coverpoint c;\n"
                 "  coverpoint d;\n"
                 "endgroup\n"},
    {// Multiple bins
     .input = "covergroup memory @ (posedge ce); address  :coverpoint addr {"
              "bins low={0,127}; bins high={128,255};} endgroup\n",
     .expected = "covergroup memory @(posedge ce);\n"
                 "  address: coverpoint addr {\n"
                 "    bins low = {0, 127};\n"
                 "    bins high = {128, 255};\n"
                 "  }\n"
                 "endgroup\n"},
    {// Custom sample() function
     .input = "covergroup c with function sample(bit i); endgroup\n",
     .expected = "covergroup c with function sample (\n"
                 "    bit i\n"  // test cases are wrapped at 40
                 ");\n"
                 "endgroup\n"},

};

TEST(FormatterEndToEndTest, FunctionTaskFormatterTestCases) {
  RunFormatterTestCases40(kFunctionTaskFormatterTestCases);
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
