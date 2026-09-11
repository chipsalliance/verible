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

#include <cctype>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/align.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/strings/position.h"
#include "verible/common/util/interval.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/formatting/format-style.h"
#include "verible/verilog/formatting/formatter-test-utils.h"
#include "verible/verilog/formatting/formatter.h"

namespace verilog {
namespace formatter {
namespace {

using absl::StatusCode;
using testing::HasSubstr;
using verible::AlignmentPolicy;
using verible::IndentationStyle;
using verible::LineNumberSet;

// Empty LineNumberSet means format all lines.
static const LineNumberSet kEnableAllLines;

// Small case set for diagnostic/control-path tests that previously iterated
// the monolithic kFormatterTestCases table.
static constexpr FormatterTestCase kDiagnosticFormatterTestCases[] = {
    {"", ""},
    {"\n", "\n"},
    {"module m;wire w;endmodule\n",
     "module m;\n"
     "  wire w;\n"
     "endmodule\n"},
    {"class  cc ;\n"
     "endclass:cc\n",
     "class cc;\n"
     "endclass : cc\n"},
};

TEST(FormatterEndToEndTest, AutoInferAlignment) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"class  cc ;\n"
       "endclass:cc\n",
       "class cc;\n"
       "endclass : cc\n"},

      // module port declarations
      {"module pd(\n"
       "input wire foo,\n"
       "output reg bar\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  wire foo,\n"  // flush-left vs. align are similar enough,
       "    output reg  bar\n"   // so automatic policy will align.
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input  foo_pkg::baz_t foo,\n"
       "output reg  bar\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input foo_pkg::baz_t foo,\n"  // alignment would add too many spaces
       "    output reg bar\n"             // so infer intent to flush-left.
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input  foo_pkg::baz_t foo,\n"
       "output     reg  bar\n"  // user injects 4 excess spaces here ...
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  foo_pkg::baz_t foo,\n"
       "    output reg            bar\n"  // ... and triggers alignment.
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "`ifdef FAA\n"  // inside preprocessing conditional
       "input  baaaz_t foo,\n"
       "output reg      bar\n"  // user injects 4 excess spaces here ...
       "`endif\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "`ifdef FAA\n"
       "    input  baaaz_t foo,\n"
       "    output reg     bar\n"  // ... and triggers alignment.
       "`endif\n"
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "`ifdef FAA\n"  // inside preprocessing conditional
       "input  baaaz_t foo,\n"
       "`else\n"
       "output reg      bar\n"  // user injects 4 excess spaces here ...
       "`endif\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "`ifdef FAA\n"
       "    input  baaaz_t foo,\n"  // aligned
       "`else\n"                    // aligned across preprocessing directives
       "    output reg     bar\n"   // ... and triggers alignment.
       "`endif\n"
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input logic [31:0] bus,\n"
       "input logic [7:0] bus2,\n"
       "`ifdef FAA\n"  // inside preprocessing conditional
       "input  baaaz_t foo,\n"
       "`else\n"
       "output reg      bar,\n"  // user injects 4 excess spaces here ...
       "`endif\n"
       "output out_t zout1,\n"
       "output out_t zout2\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  logic   [31:0] bus,\n"  // treated as one large group
       "    input  logic   [ 7:0] bus2,\n"
       "`ifdef FAA\n"
       "    input  baaaz_t        foo,\n"  // aligned
       "`else\n"  // aligned across preprocessing directives
       "    output reg            bar,\n"  // ... and triggers alignment.
       "`endif\n"
       "    output out_t          zout1,\n"
       "    output out_t          zout2\n"
       ");\n"
       "endmodule : pd\n"},
      {"module pd(\n"
       "input logic [7:0] bus2,\n"
       "`ifndef FAA\n"  // inside preprocessing conditional
       "input logic [31:0] bus,\n"
       "input  baaaz_t foo,\n"
       "`elsif BLA\n"
       "output reg      bar,\n"  // user injects 4 excess spaces here ...
       "output out_t zout1,\n"
       "`endif\n"
       "output out_t zout2\n"
       ");\n"
       "endmodule:pd\n",
       "module pd (\n"
       "    input  logic   [ 7:0] bus2,\n"
       "`ifndef FAA\n"
       "    input  logic   [31:0] bus,\n"  // treated as one large group
       "    input  baaaz_t        foo,\n"  // aligned
       "`elsif BLA\n"  // aligned across preprocessing directives
       "    output reg            bar,\n"  // ... and triggers alignment.
       "    output out_t          zout1,\n"
       "`endif\n"
       "    output out_t          zout2\n"
       ");\n"
       "endmodule : pd\n"},
      {// data declaration and net declaration in ports
       "module m(\n"
       "logic [x:y]a    ,\n"    // packed dimensions, induce alignment
       "wire [pp:qq] [e:f]b\n"  // packed dimensions, 2D
       ") ;\n"
       "endmodule\n",
       "module m (\n"
       "    logic [  x:y]      a,\n"
       "    wire  [pp:qq][e:f] b\n"
       ");\n"
       "endmodule\n"},
      {// used-defined data declarations in ports
       "module m(\n"
       "a::bb [x:y]a    ,\n"       // packed dimensions, induce alignment
       "c#(d,e) [pp:qq] [e:f]b\n"  // packed dimensions, 2D
       ") ;\n"
       "endmodule\n",
       "module m (\n"
       "    a::bb    [  x:y]      a,\n"
       "    c#(d, e) [pp:qq][e:f] b\n"
       ");\n"
       "endmodule\n"},

      // named parameter arguments
      {"module  mm ;\n"
       "foo #(\n"
       ".a(a),\n"
       ".bb(bb)\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a (a),\n"  // align doesn't add too many spaces, so align
       "      .bb(bb)\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       ".a(a),\n"
       ".bbcccc(bb)\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a(a),\n"  // align would add too many spaces, so flush-left
       "      .bbcccc(bb)\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       ".a(a    ),\n"  // user manually triggers alignment with excess spaces
       ".bbcccc(bb)\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a     (a),\n"  // induced alignment
       "      .bbcccc(bb)\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       "//c1\n"        // with comments (indented but not aligned)
       ".a(a    ),\n"  // user manually triggers alignment with excess spaces
       "//c2\n"
       ".bbcccc(bb)\n"
       "//c3\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      //c1\n"
       "      .a     (a),\n"  // induced alignment
       "      //c2\n"
       "      .bbcccc(bb)\n"
       "      //c3\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo #(\n"
       ".a( (1     +2)),\n"  // excess spaces, testing extra parentheses
       ".bbcccc((c*d)+(e*f))\n"
       ")bar( );\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo #(\n"
       "      .a     ((1 + 2)),\n"  // induced alignment
       "      .bbcccc((c * d) + (e * f))\n"
       "  ) bar ();\n"
       "endmodule : mm\n"},

      // named port connections
      {"module  mm ;\n"
       "foo bar(\n"
       ".a(a),\n"
       ".bb(bb)\n"
       ");\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo bar (\n"
       "      .a (a),\n"  // align doesn't add too many spaces, so align
       "      .bb(bb)\n"
       "  );\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo bar(\n"
       ".a(a),\n"
       ".bbbbbb(bb)\n"
       ");\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo bar (\n"
       "      .a(a),\n"  // align would add too many spaces, so flush-left
       "      .bbbbbb(bb)\n"
       "  );\n"
       "endmodule : mm\n"},
      {"module  mm ;\n"
       "foo bar(\n"
       ".a    (a),\n"  // user manually triggers alignment with excess spaces
       ".bbbbbb(bb)\n"
       ");\n"
       "endmodule:mm\n",
       "module mm;\n"
       "  foo bar (\n"
       "      .a     (a),\n"  // alignment fixed
       "      .bbbbbb(bb)\n"
       "  );\n"
       "endmodule : mm\n"},

      // net variable declarations
      {"module nn;\n"
       "wire wwwww;\n"
       "logic lll;\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  wire  wwwww;\n"  // alignment adds few spaces, so align
       "  logic lll;\n"
       "endmodule : nn\n"},
      {"module nn;\n"
       "wire wwwww;\n"
       "foo_pkg::baz_t lll;\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  wire wwwww;\n"  // alignment adds too many spaces, so flush-left
       "  foo_pkg::baz_t lll;\n"
       "endmodule : nn\n"},
      {"module nn;\n"
       "wire     wwwww;\n"  // user injects spaces to trigger alignment
       "foo_pkg::baz_t lll;\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  wire           wwwww;\n"  // ... and gets alignment
       "  foo_pkg::baz_t lll;\n"
       "endmodule : nn\n"},
      {// data/net declarations as generate items (conditional)
       "module nn;\n"
       "if (cc)begin:fff\n"
       "wire wwwww;\n"
       "logic lll;\n"
       "end:fff\n"
       "endmodule : nn\n",
       "module nn;\n"
       "  if (cc) begin : fff\n"
       "    wire  wwwww;\n"  // alignment adds few spaces, so align
       "    logic lll;\n"
       "  end : fff\n"
       "endmodule : nn\n"},

      // continuous assignments
      {"module m_assign;\n"
       "assign foo = 1'b1;\n"  // alignment adds few spaces, so align
       "assign baar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  assign foo  = 1'b1;\n"  // aligned
       "  assign baar = 1'b0;\n"
       "endmodule\n"},
      {"module m_assign;\n"
       "assign foo  =  1'b1;\n"  // alignment adds too many spaces, so
                                 // flush-left
       "assign baaaaaar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  assign foo = 1'b1;\n"  // flush-left
       "  assign baaaaaar = 1'b0;\n"
       "endmodule\n"},
      {"module m_assign;\n"
       "assign foo  =     1'b1;\n"  // induce alignment with excess spaces
       "assign baaaaaar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  assign foo      = 1'b1;\n"  // aligned
       "  assign baaaaaar = 1'b0;\n"
       "endmodule\n"},
      {// currently, does not assign across ifdefs
       "module m_assign;\n"
       "`ifdef FOO\n"
       "assign foo  =     1'b1;\n"  // induce alignment with excess spaces
       "assign baaaaaar = 1'b0;\n"
       "`else\n"
       "assign zooo = 2'b11;\n"
       "assign yoo = 2'b00;\n"
       "`endif\n"
       "endmodule\n",
       "module m_assign;\n"
       "`ifdef FOO\n"
       "  assign foo      = 1'b1;\n"  // aligned
       "  assign baaaaaar = 1'b0;\n"
       "`else\n"                   // aligned separately above/below
       "  assign zooo = 2'b11;\n"  // aligned
       "  assign yoo  = 2'b00;\n"  // aligned
       "`endif\n"
       "endmodule\n"},
      {// mixed net declaration and continuous assignment, both groups aligned
       "module m_assign;\n"
       "wire     wwwww;\n"  // induce alignment
       "foo_pkg::baz_t lll;\n"
       "assign foo  =     1'b1;\n"  // induce alignment
       "assign baaaaaar = 1'b0;\n"
       "endmodule\n",
       "module m_assign;\n"
       "  wire           wwwww;\n"  // aligned
       "  foo_pkg::baz_t lll;\n"
       "  assign foo      = 1'b1;\n"  // aligned
       "  assign baaaaaar = 1'b0;\n"
       "endmodule\n"},
      {// continuous assignments as generate items (conditional)
       "module m_assign;\n"
       "if (xy) begin\n"
       "assign foo  =  1'b0;\n"  // align: adds few spaces
       "assign baaar = 1'b1;\n"
       "end else begin\n"
       "assign goo  =      1'b1;\n"  // induce alignment with excess spaces
       "assign zaaaaaar = 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module m_assign;\n"
       "  if (xy) begin\n"
       "    assign foo   = 1'b0;\n"  // aligned
       "    assign baaar = 1'b1;\n"
       "  end else begin\n"
       "    assign goo      = 1'b1;\n"  // induce alignment with excess spaces
       "    assign zaaaaaar = 1'b0;\n"
       "  end\n"
       "endmodule\n"},
      {// continuous assignments as generate items (loop)
       "module m_assign;\n"
       "for(genvar i=0; i<k; ++i ) begin\n"
       "assign foo  =  1'b0;\n"  // align: adds few spaces
       "assign baaar = 1'b1;\n"
       "end\n"
       "endmodule\n",
       "module m_assign;\n"
       "  for (genvar i = 0; i < k; ++i) begin\n"
       "    assign foo   = 1'b0;\n"  // aligned
       "    assign baaar = 1'b1;\n"
       "  end\n"
       "endmodule\n"},
      {// continuous assignments as generate items (case)
       "module m_assign;\n"
       "case (c)\n"
       "jk:begin\n"
       "assign foo  =  1'b0;\n"  // align: adds few spaces
       "assign baaar = 1'b1;\n"
       "end\n"
       "endcase\n"
       "endmodule\n",
       "module m_assign;\n"
       "  case (c)\n"
       "    jk: begin\n"
       "      assign foo   = 1'b0;\n"  // aligned
       "      assign baaar = 1'b1;\n"
       "    end\n"
       "  endcase\n"
       "endmodule\n"},
      {// continuous assignment with comment
       "module m;\n"
       "// comment1\n"
       "assign aaaaa = (bbbbb != ccccc) &\n"
       "// comment2\n"
       "(ddddd | (eeeee & ffffff));\n"
       "endmodule\n",
       "module m;\n"
       "  // comment1\n"
       "  assign aaaaa = (bbbbb != ccccc) &\n"
       "      // comment2\n"
       "      (ddddd | (eeeee & ffffff));\n"
       "endmodule\n"},

      // net/variable assignments: blocking and nonblocking
      {"module  ma ;\n"
       "initial  begin\n"
       "aa = b;\n"
       "c = 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  initial begin\n"
       "    aa = b;\n"
       "    c  = 1'b0;\n"  // only one space to align
       "  end\n"
       "endmodule\n"},
      {"function void  fa ;\n"
       "c = 1'b0;\n"
       "aa = b;\n"
       "endfunction\n",
       "function void fa;\n"
       "  c  = 1'b0;\n"  // only one space to align
       "  aa = b;\n"
       "endfunction\n"},
      {"task  ta ; \n"
       "aa =  b;\n"
       "c = 1'b0;\n"
       "endtask\n",
       "task ta;\n"
       "  aa = b;\n"
       "  c  = 1'b0;\n"  // only one space to align
       "endtask\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaa <= b;\n"
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaa <= b;\n"
       "    c   <= 1'b0;\n"  // only two spaces to align
       "  end\n"
       "endmodule\n"},
      {"function int  fa ;\n"
       "c <= 1'b0;\n"
       "aa <= b;\n"
       "return 0 ;\n"
       "endfunction\n",
       "function int fa;\n"
       "  c  <= 1'b0;\n"  // only one space to align
       "  aa <= b;\n"
       "  return 0;\n"
       "endfunction\n"},
      {"task  ta ; \n"
       "$display (\"hello\" );\n"
       "aa <=  b;\n"
       "c <= 1'b0;\n"
       "endtask\n",
       "task ta;\n"
       "  $display(\"hello\");\n"
       "  aa <= b;\n"
       "  c  <= 1'b0;\n"  // only one space to align
       "endtask\n"},
      {// mixed blocking and nonblocking assignments
       "module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaaa  = b;\n"
       "ccc  = 1'b0;\n"
       "aaa <= b;\n"
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaaa = b;\n"
       "    ccc   = 1'b0;\n"  // only two spaces to align
       "    aaa <= b;\n"
       "    c   <= 1'b0;\n"  // only two spaces to align (separate group)
       "  end\n"
       "endmodule\n"},
      {"task  ta ; \n"
       "aa <=  b;\n"
       "c <= 1'b0;\n"
       "$display (\"hello\" );\n"  // separates above/below alignment groups
       "zzaa <=  b;\n"
       "zzc <= 1'b0;\n"
       "endtask\n",
       "task ta;\n"
       "  aa <= b;\n"
       "  c  <= 1'b0;\n"  // only one space to align
       "  $display(\"hello\");\n"
       "  zzaa <= b;\n"
       "  zzc  <= 1'b0;\n"  // only one space to align
       "endtask\n"},
      {"task  ta ; \n"
       "$display (\"hello\" );\n"
       "aaaaa <=  b;\n"
       "c <= 1'b0;\n"  // need too many spaces to align
       "endtask\n",
       "task ta;\n"
       "  $display(\"hello\");\n"
       "  aaaaa <= b;\n"
       "  c <= 1'b0;\n"  // so keep flush-left
       "endtask\n"},
      {"function void  fa ; \n"
       "$display (\"hello\" );\n"
       "aaaaa =  b;\n"
       "c = 1'b0;\n"  // need too many spaces to align
       "endfunction\n",
       "function void fa;\n"
       "  $display(\"hello\");\n"
       "  aaaaa = b;\n"
       "  c = 1'b0;\n"  // so keep flush-left
       "endfunction\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaxx <= b;\n"
       "c <= 1'b0;\n"  // need too many spaces to align
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaxx <= b;\n"
       "    c <= 1'b0;\n"  // so keep flush-left
       "  end\n"
       "endmodule\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaxx <= b    ;\n"  // inject 4 spaces to induce alignment
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaxx <= b;\n"
       "    c     <= 1'b0;\n"  // induced alignment
       "  end\n"
       "endmodule\n"},
      {"module  ma ;\n"
       "always@( posedge clk) begin\n"
       "aaaxx <= b    ;\n"  // inject 4 spaces to induce alignment
       "//comment\n"
       "c <= 1'b0;\n"
       "end\n"
       "endmodule\n",
       "module ma;\n"
       "  always @(posedge clk) begin\n"
       "    aaaxx <= b;\n"
       "    //comment\n"       // ignored within alignment group
       "    c     <= 1'b0;\n"  // induced alignment
       "  end\n"
       "endmodule\n"},

      // local variable declarations as statements
      {"task tt ;\n"
       "int foo;\n"  // only 2 spaces needed to align
       "bar_t baz;\n"
       "endtask\n",
       "task tt;\n"
       "  int   foo;\n"  // aligned
       "  bar_t baz;\n"
       "endtask\n"},
      {"function ff ;\n"
       "bar_t baz;\n"
       "int foo;\n"  // only 2 spaces needed to align
       "endfunction\n",
       "function ff;\n"
       "  bar_t baz;\n"
       "  int   foo;\n"  // aligned
       "endfunction\n"},
      {"task tt ;\n"
       "int  foo;\n"  // too many spaces needed to align
       "baaaar_t baz;\n"
       "endtask\n",
       "task tt;\n"
       "  int foo;\n"  // so flush-left
       "  baaaar_t baz;\n"
       "endtask\n"},
      {"function ff ;\n"
       "baaaar_t baz;\n"
       "int  foo;\n"  // too many spaces needed to align
       "endfunction\n",
       "function ff;\n"
       "  baaaar_t baz;\n"
       "  int foo;\n"  // so flush-left
       "endfunction\n"},
      {"task tt ;\n"
       "int        foo;\n"  // injected spaces to induce alignment
       "baaaar_t baz;\n"
       "endtask\n",
       "task tt;\n"
       "  int      foo;\n"  // aligned
       "  baaaar_t baz;\n"
       "endtask\n"},
      {"function ff ;\n"
       "baaaar_t baz    ;\n"  // injected spaces to induce alignment
       "int  foo;\n"
       "endfunction\n",
       "function ff;\n"
       "  baaaar_t baz;\n"
       "  int      foo;\n"  // so aligned
       "endfunction\n"},

      // formal parameters
      {"module pp #(\n"
       "int W,\n"
       "type T\n"
       ") ();\n"
       "endmodule : pp\n",
       "module pp #(\n"
       "    int  W,\n"  // alignment adds few spaces, so do it
       "    type T\n"
       ") ();\n"
       "endmodule : pp\n"},
      {"module pp #(\n"
       "int W,\n"
       "int[xx:yy] T\n"
       ") ();\n"
       "endmodule : pp\n",
       "module pp #(\n"
       "    int W,\n"  // alignment adds many spaces, so flush-left
       "    int [xx:yy] T\n"
       ") ();\n"
       "endmodule : pp\n"},
      {"module pp #(\n"
       "int W,\n"
       "int[xx:yy]     T\n"  // user injected spaces intentionally
       ") ();\n"
       "endmodule : pp\n",
       "module pp #(\n"
       "    int         W,\n"  // ... trigger alignment
       "    int [xx:yy] T\n"
       ") ();\n"
       "endmodule : pp\n"},

      // class member variables
      {"class  cc ;\n"
       "int my_int;\n"
       "bar_t my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int   my_int;\n"  // align doesn't add too many spaces, so align
       "  bar_t my_bar;\n"
       "endclass : cc\n"},
      {"class  cc ;\n"
       "int   my_int;\n"
       "foo_pkg::bar_t my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int my_int;\n"  // align would add too many spaces, so flush-left
       "  foo_pkg::bar_t my_bar;\n"
       "endclass : cc\n"},
      {"class  cc ;\n"
       "int     my_int;\n"  // intentional excessive spaces, trigger alignment
       "foo_pkg::bar_t my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int            my_int;\n"
       "  foo_pkg::bar_t my_bar;\n"
       "endclass : cc\n"},
      {"class  cc ;\n"
       "int    my_int;\n"  // unable to infer user's intent, so preserve
       "foo_pkg::bar_t  my_bar;\n"
       "endclass:cc\n",
       "class cc;\n"
       "  int    my_int;\n"  // ... but still indent
       "  foo_pkg::bar_t  my_bar;\n"
       "endclass : cc\n"},

      // case item test cases
      {// small difference between flush-left and align, so align
       "function f; case (x)kZZZZ  :if( b )break; default :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZZZZ:   if (b) break;\n"  // aligned, only adds 2 spaces
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// small error relative to flush-left, so flush-left
       "function f; case (x)kZ  :if( b )break; default :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ: if (b) break;\n"  // flush-left
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// intentional spacing error (delta=4) induces alignment
       "function f; case (x)kZ  :if( b )break; default    :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:      if (b) break;\n"
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, with ignored comments
       "function f; case (x)kZ  :if( b )break; \n//c1\n kXX: g = f; "
       "\n//c2\ndefault    :return 2;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:      if (b) break;\n"
       "    //c1\n"
       "    kXX:     g = f;\n"
       "    //c2\n"
       "    default: return 2;\n"
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, ignore multiline case item in the middle
       "function f; case (x)"
       "kZ  :if( b )break; "
       "kYY    :return 2;"          // excess spaces induce alignment
       "    kXXXXXXXXX: begin end"  // multi-line, ignored
       "    kWWWWW: cc = 23;\n"
       "    kVVV: cd = 24;\n"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:     if (b) break;\n"
       "    kYY:    return 2;\n"
       "    kXXXXXXXXX: begin\n"  // separates above/below groups
       "    end\n"
       "    kWWWWW: cc = 23;\n"
       "    kVVV:   cd = 24;\n"  // aligned
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, ignore multiline case item in the middle
       "function f; case (x)"
       "kZ  :if( b )break; "
       "kYY    :return 2;"               // excess spaces induce alignment
       "    kXXXXXXXXX: if(w)begin end"  // multi-line, ignored
       "    kWWWWW: cc = 23;\n"
       "    kVVV: cd = 24;\n"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x)\n"
       "    kZ:     if (b) break;\n"
       "    kYY:    return 2;\n"
       "    kXXXXXXXXX:\n"   // TODO(fangism): allow this to merge with if()
                             // else indent the following two lines
       "    if (w) begin\n"  // separates above/below groups
       "    end\n"
       "    kWWWWW: cc = 23;\n"
       "    kVVV:   cd = 24;\n"  // aligned
       "  endcase\n"
       "endfunction\n"},
      {// induced alignment, ignore multiline case item in the middle
       "task t; case (x)"
       "kZ  :if( b )break; "
       "kYY    :return 2;"           // excess spaces induce alignment
       "    kXXXXXXXXX: fork  join"  // multi-line, ignored
       "    kWWWWW: cc = 23;\n"
       "    kVVV: cd = 24;\n"
       "endcase endtask\n",
       "task t;\n"
       "  case (x)\n"
       "    kZ:     if (b) break;\n"
       "    kYY:    return 2;\n"
       "    kXXXXXXXXX:\n"  // TODO(fangism): allow this to merge with fork
                            // else indent the following two lines
       "    fork\n"         // separates above/below groups
       "    join\n"
       "    kWWWWW: cc = 23;\n"
       "    kVVV:   cd = 24;\n"  // aligned
       "  endcase\n"
       "endtask\n"},
      {// case-inside: small difference between flush-left and align, so align
       "function f; case (x)inside [0:3]  :yy=zzz; [4:11] :yy=zz;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    [0 : 3]:  yy = zzz;\n"  // aligned, only adds 1 spaces
       "    [4 : 11]: yy = zz;\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-inside: align with comments
       "function f; case (x)inside \n//c1\n[0:3]  :yy=zzz;\n//c2\n"
       " [4:11] :yy=zz;\n//c3\n"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    //c1\n"
       "    [0 : 3]:  yy = zzz;\n"  // aligned, only adds 1 spaces
       "    //c2\n"
       "    [4 : 11]: yy = zz;\n"
       "    //c3\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-inside: flush left
       "function f; case (x)inside [0:3]  :yy=zzz; [4:999999] :yy=zz;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    [0 : 3]: yy = zzz;\n"  // flush-left
       "    [4 : 999999]: yy = zz;\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-inside: induce alignment
       "function f; case (x)inside [0:3    ]  :yy=zzz; [4:999999] :yy=zz;"
       "endcase endfunction\n",
       "function f;\n"
       "  case (x) inside\n"
       "    [0 : 3]:      yy = zzz;\n"  // aligned
       "    [4 : 999999]: yy = zz;\n"
       "  endcase\n"
       "endfunction\n"},
      {// case-generate: align would add few spaces, so align
       "module mc ; case (x)kZ  : gg h(); kXYY :j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ:   gg h ();\n"  // align
       "    kXYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// case-generate + comment: align would add few spaces, so align
       "module mc ; case (x)kZ  : gg h(); \n//c1\n kXYY :j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ:   gg h ();\n"  // align
       "    //c1\n"
       "    kXYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// case-generate: align would add too many space, so flush-left
       "module mc ; case (x)kZ  : gg h(); kXYYYY :j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ: gg h ();\n"  // flush-left
       "    kXYYYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// case-generate: inject spaces to induce alignment
       "module mc ; case (x)kZ  : gg h(); kXYYYY :     j kk();"
       "endcase endmodule\n",
       "module mc;\n"
       "  case (x)\n"
       "    kZ:     gg h ();\n"  // align
       "    kXYYYY: j kk ();\n"
       "  endcase\n"
       "endmodule\n"},
      {// randcase: align (small difference from flush-left)
       "task trc  ;randcase 10: x = 1; 1: x = 3; endcase endtask",
       "task trc;\n"
       "  randcase\n"
       "    10: x = 1;\n"
       "    1:  x = 3;\n"  // aligned
       "  endcase\n"
       "endtask\n"},
      {// randcase: inferred flush-left
       "task trc  ;randcase 10000: x = 1; 1: x = 3; endcase endtask",
       "task trc;\n"
       "  randcase\n"
       "    10000: x = 1;\n"
       "    1: x = 3;\n"
       "  endcase\n"
       "endtask\n"},
      {// randcase: induce alignment
       "task trc  ;randcase 10000: x = 1    ; 1: x = 3; endcase endtask",
       "task trc;\n"
       "  randcase\n"
       "    10000: x = 1;\n"
       "    1:     x = 3;\n"  // aligned
       "  endcase\n"
       "endtask\n"},

      // distributions
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "8'hfe :=  9 , \n"
       "12'hfff  := 1 }; "
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      8'hfe   := 9,\n"  // only two spaces needed to align
       "      12'hfff := 1\n"   // so align
       "    };\n"
       "  }\n"
       "endclass\n"},
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "[ 8'h0 : 8'hfe ] :/  9 , \n"
       "8'hff  :/ 1 }; "  // takes many spaces to align this, so...
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      [8'h0 : 8'hfe] :/ 9,\n"
       "      8'hff :/ 1\n"  // flush-left
       "    };\n"
       "  }\n"
       "endclass\n"},
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "[ 8'h0 : 8'hfe ] :/  9 , \n"
       "8'hff  :/     1 }; "  // inject excess spaces to trigger alignment
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      [8'h0 : 8'hfe] :/ 9,\n"
       "      8'hff          :/ 1\n"  // aligned
       "    };\n"
       "  }\n"
       "endclass\n"},
      {"class foo;\n"
       "constraint c { "
       "timer_enable dist {\n"
       "//comment1\n"
       "[ 8'h0 : 8'hfe ] :/  9 , \n"
       "//comment2\n"
       "8'hff  :/     1 \n"
       "//comment3\n"
       "}; "  // inject excess spaces to trigger alignment
       "} endclass\n",
       "class foo;\n"
       "  constraint c {\n"
       "    timer_enable dist {\n"
       "      //comment1\n"
       "      [8'h0 : 8'hfe] :/ 9,\n"
       "      //comment2\n"           // align across comment
       "      8'hff          :/ 1\n"  // aligned
       "      //comment3\n"
       "    };\n"
       "  }\n"
       "endclass\n"},
      {"module foo ();\n"
       "  always @(posedge bar) begin\n"
       "    if (1==1) begin\n"
       "      ham();\n"
       "    end else if (2==2) begin\n"
       "      jam();\n"
       "    end else begin\n"
       "      spam();\n"
       "    end\n"
       "  end\n"
       "endmodule\n",
       "module foo ();\n"
       "  always @(posedge bar) begin\n"
       "    if (1 == 1) begin\n"
       "      ham();\n"
       "    end else if (2 == 2) begin\n"
       "      jam();\n"
       "    end else begin\n"
       "      spam();\n"
       "    end\n"
       "  end\n"
       "endmodule\n"},
  };
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Override some settings to test auto-inferred alignment.
  style.ApplyToAllAlignmentPolicies(AlignmentPolicy::kInferUserIntent);

  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}  // NOLINT(readability/fn_size)

static constexpr FormatterTestCase kFormatterWideTestCases[] = {
    // specify blocks
    {"module  specify_tests ;\n"
     "specify\n"  // empty list
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify\n"
     "$recrem (posedge R, posedge C,\n"
     "t1, t2);\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    $recrem(posedge R, posedge C, t1, t2);\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify\n"
     "// TODO: add this\n"
     "endspecify \n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    // TODO: add this\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify  \n"
     "  //c1\n"
     "$setup (  posedge A, posedge B,\n"
     "t1);//c2\n"
     " //c3\n"
     "$hold (  posedge B , posedge A,t2);    //c4\n"
     "\t//c5\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    //c1\n"
     "    $setup(posedge A, posedge B, t1);  //c2\n"
     "    //c3\n"
     "    $hold(posedge B, posedge A, t2);  //c4\n"
     "    //c5\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify  \n"
     "$setup (  posedge A, posedge B,\n"
     "t1);\n"
     "$hold (  posedge B , posedge A,t2);\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "    $setup(posedge A, posedge B, t1);\n"
     "    $hold(posedge B, posedge A, t2);\n"
     "  endspecify\n"
     "endmodule\n"},
    {"module  specify_tests ;\n"
     "specify  \n"
     "  `ifdef CCC\n"
     "$setup (  posedge A, posedge B,\n"
     "t1);\n"
     " `else\n"
     "$hold (  posedge B , posedge A,t2);   \n"
     "\t`endif\n"
     "endspecify\n"
     "endmodule",
     "module specify_tests;\n"
     "  specify\n"
     "`ifdef CCC\n"
     "    $setup(posedge A, posedge B, t1);\n"
     "`else\n"
     "    $hold(posedge B, posedge A, t2);\n"
     "`endif\n"
     "  endspecify\n"
     "endmodule\n"},
};

// These tests just need a larger column limit to fit on one line.
TEST(FormatterEndToEndTest, VerilogFormatWideTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 60;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto &test_case : kFormatterWideTestCases) {
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
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"  // empty ports formatted compactly
       "endmodule\n"},
      {// for a single port, the alignment handler doesn't even consider it a
       // group so it falls back to standard flush-left behavior.
       "module  m   ( input     clk  )\t;\n"
       "  endmodule\n",
       "module m (\n"
       "    input clk\n"
       ");\n"
       "endmodule\n"},
      {// example with two ports
       "module  m   (\n"
       "input  clk,\n"
       "output bar\n"
       ")\t;\n"
       "  endmodule\n",
       "module m (\n"
       "    input  clk,\n"  // indented, but internal pre-existing spacing
                            // preserved
       "    output bar\n"
       ");\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.port_declarations_alignment = verible::AlignmentPolicy::kPreserve;
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

TEST(FormatterEndToEndTest, DisableModuleInstantiations) {
  static constexpr FormatterTestCase kTestCases[] = {
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
       "  foo bar ();\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "logic   xyz;"
       "wire\tabc;"
       "  endmodule\n",
       "module m;\n"
       "  logic xyz;\n"  // indentation still takes effect
       "  wire  abc;\n"  // aligned too
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
       "  foo bar (.baz(baz));\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo  bar(\n"
       "        .baz  (baz  ),\n"  // example of user-manual alignment
       "        .blaaa(blaaa)\n"
       ");"
       "  endmodule\n",
       "module m;\n"
       "  foo bar (\n"           // indentation still takes effect
       "      .baz  (baz  ),\n"  // named port connections preserved
       "      .blaaa(blaaa)\n"   // named port connections preserved
       "  );\n"                  // this indentation is fixed
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo  #(   .baz(baz)   ) bar();"  // named parameters
       "  endmodule\n",
       "module m;\n"
       "  foo #(.baz(baz)) bar ();\n"  // indentation still takes effect
       "endmodule\n"},
      {"module  m  ;\t\n"
       "foo  #(\n"
       "        .baz  (baz  ),\n"  // example of user-manual alignment
       "        .blaaa(blaaa)\n"
       ")  bar( );"
       "  endmodule\n",
       "module m;\n"
       "  foo #(\n"              // indentation still takes effect
       "      .baz  (baz  ),\n"  // named parameter arguments preserved
       "      .blaaa(blaaa)\n"   // named parameter arguments preserved
       "  ) bar ();\n"           // this indentation is fixed
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Testing preservation of spaces
  style.named_parameter_alignment = AlignmentPolicy::kPreserve;
  style.named_port_alignment = AlignmentPolicy::kPreserve;
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

TEST(FormatterEndToEndTest, DisableTryWrapLongLines) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "initial assign a = b;\n"
       "  endmodule\n",
       "module m ();\n"
       "  initial assign a = b;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "initial assign a = {never +gonna +give +you +up,\n"
       "never + gonna +Let +you +down};\n"
       "  endmodule\n",
       "module m ();\n"
       "  initial\n"
       "    assign a = {\n"
       "      never + gonna + give + you + up,\n"
       "      never + gonna + Let + you + down\n"
       "    };\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "initial assign a = {never +gonna +give +you +up+\n"  // over 40 columns,
                                                             // give up
       "never + gonna +Let +you +down};\n"
       "  endmodule\n",
       "module m ();\n"
       "  initial\n"
       "    assign a = {\n"
       "      never +gonna +give +you +up+\n"  // indented properly, but
       "never + gonna +Let +you +down\n"       // preserved original
       "    };\n"
       "endmodule\n"},
      {// The if-header is a single leaf partition, and does not fit,
       // so its original spacing should be preserved.
       // We deliberately insert weird spacing to show that it is preserved.
       "function f;\n"
       "if ((xxx.aaaa >= bbbbbbbbbbbbbbb) &&\n"
       "      ((ccc.ddd  +  eee.ffffff * g) <=\n"
       "       (hhhhhhhhhhhhhhh+iiiiiiiiiiiiiiiiiiii))) begin\n"
       "end\n"
       "endfunction\n",
       "function f;\n"
       "  if ((xxx.aaaa >= bbbbbbbbbbbbbbb) &&\n"  // indentation fixed
       "      ((ccc.ddd  +  eee.ffffff * g) <=\n"
       "       (hhhhhhhhhhhhhhh+iiiiiiiiiiiiiiiiiiii))) begin\n"
       "  end\n"  // indentation fixed
       "endfunction\n"},

      // Make sure indentation still works with wrapping disabled,
      // and leaf partitions fit on one line.
      {"function void f();\n"
       "for (int i = N; i > 0; i--) begin\n"
       "end\n"
       "endfunction\n",
       "function void f();\n"
       "  for (int i = N; i > 0; i--) begin\n"
       "  end\n"
       "endfunction\n"},
      {"function void f();"
       "if(i > 0 ) begin end "
       "endfunction",
       "function void f();\n"
       "  if (i > 0) begin\n"
       "  end\n"
       "endfunction\n"},
      {"function void f();"  // newlines absent from input
       "for (int i=N; i>0; i--) begin end "
       "endfunction",
       "function void f();\n"
       "  for (int i = N; i > 0; i--) begin\n"  // spacing corrected
       "  end\n"
       "endfunction\n"},
      {"module m( );\n"
       "  always_ff  @  (  posedge  (  clk  )  ) begin\n"
       "out  <=  rst_clk  ?  0 : in  ;\n"
       "end\n"
       "endmodule : simple\n",
       "module m ();\n"
       "  always_ff @(posedge (clk)) begin\n"
       "    out <= rst_clk ? 0 : in;\n"
       "  end\n"
       "endmodule : simple\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.try_wrap_long_lines = false;
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

TEST(FormatterEndToEndTest, ModulePortDeclarationsIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"  // empty ports formatted compactly
       "endmodule\n"},
      {// single port example
       "module  m   ( input     clk  )\t;\n"
       "  endmodule\n",
       "module m (\n"
       "  input clk\n"  // 2 spaces
       ");\n"
       "endmodule\n"},
      {// example with two ports
       "module  m   (\n"
       "input  clk,\n"
       "output bar\n"
       ")\t;\n"
       "  endmodule\n",
       "module m (\n"
       "  input  clk,\n"  // indented 2 spaces, and aligned
       "  output bar\n"
       ");\n"
       "endmodule\n"},
      {// interface example
       "interface  handshake   (\n"
       "wire req,\n"
       "wire ack\n"
       ")\t;\n"
       "  endinterface\n",
       "interface handshake (\n"
       "  wire req,\n"  // indented 2 spaces
       "  wire ack\n"
       ");\n"
       "endinterface\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.port_declarations_indentation = IndentationStyle::kIndent;
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

TEST(FormatterEndToEndTest, NamedPortConnectionsIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m(   ) ;\n"
       "  endmodule\n",
       "module m ();\n"  // empty ports formatted compactly
       "endmodule\n"},
      {// single port example
       "module  m ;\n"
       "foo bar( .clk( clk ) )\t;\n"
       "  endmodule\n",
       "module m;\n"
       "  foo bar (.clk(clk));\n"
       "endmodule\n"},
      {// two port example
       "module  m ;\n"
       "foo bar( .clk2( clk ),.data (data) )\t;\n"
       "  endmodule\n",
       "module m;\n"
       "  foo bar (\n"
       "    .clk2(clk),\n"  // indent only +2 spaces
       "    .data(data)\n"
       "  );\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.named_port_indentation = IndentationStyle::kIndent;
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

TEST(FormatterEndToEndTest, WrapEndElseStatements) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"module foo ();\n"
       "  always @(posedge bar) begin\n"
       "    if (1==1) begin\n"
       "      ham();\n"
       "    end else if (2==2) begin\n"
       "      jam();\n"
       "    end else begin\n"
       "      spam();\n"
       "    end\n"
       "  end\n"
       "endmodule\n",
       "module foo ();\n"
       "  always @(posedge bar) begin\n"
       "    if (1 == 1) begin\n"
       "      ham();\n"
       "    end\n"
       "    else if (2 == 2) begin\n"
       "      jam();\n"
       "    end\n"
       "    else begin\n"
       "      spam();\n"
       "    end\n"
       "  end\n"
       "endmodule\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.wrap_end_else_clauses = true;
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

TEST(FormatterEndToEndTest, FormalParametersIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m #(   ) ;\n"  // empty parameters
       "  endmodule\n",
       "module m #();\n"
       "endmodule\n"},
      {// single parameter example
       "module  m   #( int W = 2)\t;\n"
       "  endmodule\n",
       "module m #(\n"
       "  int W = 2\n"  // indented 2 spaces
       ");\n"
       "endmodule\n"},
      {// module with two parameters
       "module  m   #(\n"
       "int W = 2,\n"
       "int L = 4\n"
       ")\t;\n"
       "  endmodule\n",
       "module m #(\n"
       "  int W = 2,\n"  // indented 2 spaces
       "  int L = 4\n"
       ");\n"
       "endmodule\n"},
      {// interface with two parameters
       "interface  m   #(\n"
       "int W = 2,\n"
       "int L = 4\n"
       ")\t;\n"
       "  endinterface\n",
       "interface m #(\n"
       "  int W = 2,\n"  // indented 2 spaces
       "  int L = 4\n"
       ");\n"
       "endinterface\n"},
      {// class with two parameters
       "class  c   #(\n"
       "int W = 2,\n"
       "int L = 4\n"
       ")\t;\n"
       "  endclass\n",
       "class c #(\n"
       "  int W = 2,\n"  // indented 2 spaces
       "  int L = 4\n"
       ");\n"
       "endclass\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.formal_parameters_indentation = IndentationStyle::kIndent;
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

TEST(FormatterEndToEndTest, NamedParametersIndentNotWrap) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},
      {"\n", "\n"},
      {"\n\n", "\n\n"},
      {"module  m  ;\t\n"
       "  endmodule\n",
       "module m;\n"
       "endmodule\n"},
      {"module  m #(   ) ;\n"  // empty parameters
       "  endmodule\n",
       "module m #();\n"
       "endmodule\n"},
      {"module  m  ;\t\n"
       " foo #()bar();\n"
       "  endmodule\n",
       "module m;\n"
       "  foo #() bar ();\n"
       "endmodule\n"},
      {// one named parameter
       "module  m ;\n"
       "foo #(.W(1)) bar();\n"
       "  endmodule\n",
       "module m;\n"
       "  foo #(.W(1)) bar ();\n"
       "endmodule\n"},
      {// two named parameters
       "module  m ;\n"
       "foo #(.W(1), .L(2)) bar();\n"
       "  endmodule\n",
       "module m;\n"
       "  foo #(\n"
       "    .W(1),\n"  // indent +2 spaces only
       "    .L(2)\n"
       "  ) bar ();\n"
       "endmodule\n"},
      {// class data member with two parameters
       "class  c  ;\n"
       " foo_pkg::bar_t#(\n"
       ".W(2),.L(4)"
       ") baz;\n"
       "  endclass\n",
       "class c;\n"
       "  foo_pkg::bar_t #(\n"
       "    .W(2),\n"  // indent +2 spaces only
       "    .L(4)\n"
       "  ) baz;\n"
       "endclass\n"},
      {// typedef with two parameters
       "typedef \n"
       " foo_pkg::bar_t  #("
       ".W(2),.L(4)"
       ") baz;\n",
       "typedef foo_pkg::bar_t#(\n"
       "  .W(2),\n"  // indent +2 spaces only
       "  .L(4)\n"
       ") baz;\n"},
  };
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  // Indent 2 spaces instead of wrapping 4 spaces.
  style.named_parameter_indentation = IndentationStyle::kIndent;
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

struct SelectLinesTestCase {
  std::string_view input;
  LineNumberSet lines;  // explicit set of lines to enable formatting
  std::string_view expected;
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
      {"module m;\n"
       "  if (foo) begin:l1\n"
       "    if (foo) begin:l2\n"
       "      always_comb\n"  // normally this line and next would fit together
       "        d<=#1ps   x_lat\t;\n"  // only format this line, 5
       "    end : l2\n"
       "  end : l1\n"
       "endmodule\n",
       {{5, 6}},
       "module m;\n"
       "  if (foo) begin:l1\n"
       "    if (foo) begin:l2\n"
       "      always_comb\n"  // incremental mode prevents joining next line
       "        d <= #1ps x_lat;\n"  // only this line changed
       "    end : l2\n"
       "  end : l1\n"
       "endmodule\n"},

      // Next three test cases: one whole-file, two incremental
      {"module m(\n"
       "  input wire f,\n"
       "  input  foo::bar  ggg\n"
       ");\n"
       "endmodule:m\n",
       {},  // format all lines
       "module m (\n"
       "    input wire     f,\n"
       "    input foo::bar ggg\n"
       ");\n"
       "endmodule : m\n"},
      {"module m(\n"
       "  input wire f,\n"
       "  input  foo::bar  ggg\n"  // "new line", formatted incrementally
       ");\n"
       "endmodule:m\n",
       {{3, 4}},  // format incrementally
       "module m(\n"
       "  input wire f,\n"
       "    input  foo::bar  ggg\n"  // "new line" remains untouched
       ");\n"
       "endmodule:m\n"},
      {"module m(\n"
       "  input  wire   f,\n"  // "new line", formatted incrementally
       "  input  foo::bar  ggg\n"
       ");\n"
       "endmodule:m\n",
       {{2, 3}},  // format incrementally
       "module m(\n"
       "    input  wire   f,\n"  // "new line" indented, but other spaces kept
       "  input  foo::bar  ggg\n"
       ");\n"
       "endmodule:m\n"},
  };
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status = FormatVerilog(test_case.input, "<filename>", style,
                                      stream, test_case.lines);
    EXPECT_OK(status) << status.message() << '\n'
                      << "Lines: " << test_case.lines;
    EXPECT_EQ(stream.str(), test_case.expected)
        << "code:\n"
        << test_case.input << "\nlines: " << test_case.lines;
  }
}

// These tests verify the mode where horizontal spacing is discarded while
// vertical spacing is preserved.
TEST(FormatterEndToEndTest, PreserveVSpacesOnly) {
  static constexpr FormatterTestCase kTestCases[] = {
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
  for (const auto &test_case : kTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kFormatterTestCasesElseStatements[] = {
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
    {"module foo();\n"
     "always_comb begin\n"
     "value = function_name(.long_parameter(8'hA), .parameter_three(foobar));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(\n"
     "      .long_parameter(8'hA),\n"
     "      .parameter_three(foobar)\n"
     "    );\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo();\n"
     "always_comb begin\n"
     "value = function_name(.a(1), .b(2));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(.a(1), .b(2));\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo ();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, foobar, signal_1234); end\n"
     "always_comb begin\n"
     "value = function_name(8'hA, foobar); end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(8'hA, foobar,\n"
     "                          signal_1234);\n"
     "  end\n"
     "  always_comb begin\n"
     "    value = function_name(8'hA, foobar);\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo ();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, foobar, signal_1234);\n"
     "value = function_name(8'hA, foobar, signal_1234); end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(8'hA, foobar,\n"
     "                          signal_1234);\n"
     "    value = function_name(8'hA, foobar,\n"
     "                          signal_1234);\n"
     "  end\n"
     "endmodule : foo\n"},
    {"always_comb begin\n"
     "value = "
     "f(long_parameter_exceeding_col_limit, foo, bar); end\n",
     "always_comb begin\n"
     "  value = f(\n"
     "    long_parameter_exceeding_col_limit,\n"
     "    foo,\n"
     "    bar\n"
     "  );\n"
     "end\n"},
    {"module foo();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, .parameter_three(foobar));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(\n"
     "      8'hA,\n"
     "      .parameter_three(foobar)\n"
     "    );\n"
     "  end\n"
     "endmodule : foo\n"},
    {"module foo ();\n"
     "always_comb begin\n"
     "value = function_name(8'hA, 8'hB, 8'hC, .parameter_four(foo), "
     ".par_five(bar));\n"
     "end\n"
     "endmodule : foo\n",
     "module foo ();\n"
     "  always_comb begin\n"
     "    value = function_name(\n"
     "      8'hA,\n"
     "      8'hB,\n"
     "      8'hC,\n"
     "      .parameter_four(foo),\n"
     "      .par_five(bar)\n"
     "    );\n"
     "  end\n"
     "endmodule : foo\n"},
    {"class dv_base_mem; function void configure(); \nbegin\n"
     "value = func(8'hA, foobar, signal_1234);\n"
     "value = new(8'hA, foobar, signal_1234); end\n"
     "endfunction : configure endclass\n",
     "class dv_base_mem;\n"
     "  function void configure();\n"
     "    begin\n"
     "      value = func(8'hA, foobar,\n"
     "                   signal_1234);\n"
     "      value = new(8'hA, foobar,\n"
     "                  signal_1234);\n"
     "    end\n"
     "  endfunction : configure\n"
     "endclass\n"},
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
  for (const auto &test_case : kFormatterTestCasesElseStatements) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

TEST(FormatterEndToEndTest, ConstraintExpressions) {
  static constexpr FormatterTestCase kTestCases[] = {
      {"", ""},

      // class members
      {"class Foo; constraint if_c { if (zzzzzzzzzzzzzzzzzzzzz)"
       "{ soft xxxxxxxxxxxxxxxxxxxxxx == yyyyyyyyyyyyyyyyyyy; } } endclass",
       "class Foo;\n"
       "  constraint if_c {\n"
       "    if (zzzzzzzzzzzzzzzzzzzzz) {\n"
       "      soft xxxxxxxxxxxxxxxxxxxxxx == yyyyyyyyyyyyyyyyyyy;\n"
       "    }\n"
       "  }\n"
       "endclass\n"},

      // constraints with if-constraint expressions
      {"constraint xx { if (a) b; }\n", "constraint xx {if (a) b;}\n"},

      {"constraint xx { if (a) {b;} }\n",
       "constraint xx {\n"
       "  if (a) {\n"
       "    b;\n"
       "  }\n"
       "}\n"},

      // multi item constraint
      {"constraint yy { a == b;c==d;}",
       "constraint yy {\n"
       "  a == b;\n"
       "  c == d;\n"
       "}\n"},

      // one-line constraints
      {"constraint only_vec_instr_c {soft only_vec_instr == 0;}",
       "constraint only_vec_instr_c {soft only_vec_instr == 0;}\n"},

      {"constraint\nnum_trans_c\n\n\n{\n\n\nnum_trans inside{[800:1000]};}",
       "constraint num_trans_c {num_trans inside {[800 : 1000]};}\n"},

      // if-vs-concatenation expression
      {"constraint c_operation{  if(fixed_operation_en){"
       "aes_operation == fixed_operation"
       ";}}",
       "constraint c_operation {\n"
       "  if (fixed_operation_en) {\n"
       "    aes_operation == fixed_operation;\n"
       "  }\n"
       "}\n"},

      {"constraint c_operation{  if(fixed_operation_en){"
       "aes_operation == fixed_operation"
       "};}",
       "constraint c_operation {\n"
       "  if (fixed_operation_en)\n"
       "  {aes_operation == fixed_operation};\n"
       "}\n"},

      // concatenation expression
      {"constraint c_iv {if (fixed_iv_en) {aes_iv == fixed_iv};}",
       "constraint c_iv {\n"
       "  if (fixed_iv_en)\n"
       "  {aes_iv == fixed_iv};\n"
       "}\n"},

      // looooong test
      {"constraint xx {"
       "if (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)"
       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa=="
       "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb;"
       "ccccccccccccccccccccccc==dddddddddddddddddddddd;}",
       "constraint xx {\n"
       "  if (aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)\n"
       "  aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa == "
       "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb;\n"
       "  ccccccccccccccccccccccc == dddddddddddddddddddddd;\n"
       "}\n"},
  };
  FormatStyle style;
  style.column_limit =
      100;  // smaller column_limit forces expansion of constraint blocks
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.port_declarations_alignment = verible::AlignmentPolicy::kPreserve;
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

static constexpr FormatterTestCase kFormatterTestCasesEnumDeclarations[] = {
    // Inferring user intent: not too many spaces to be added: align.
    {"typedef enum { kA=1, kAB=2, kABC=3} x;",
     "typedef enum {\n"
     "  kA   = 1,\n"
     "  kAB  = 2,\n"
     "  kABC = 3\n} x;\n"},

    // Lots of spaces would've to be be added - keep flush
    {"typedef enum { kA=1, kAB=2, kABCDEFGHIJKLMN=3} x;",
     "typedef enum {\n"
     "  kA = 1,\n"
     "  kAB = 2,\n"
     "  kABCDEFGHIJKLMN = 3\n} x;\n"},

    // Source spaces indicate alignment wish.
    {"typedef enum { kA     =1, kAB=2, kABCDEFGHIJKLMN=3} x;",
     "typedef enum {\n"
     "  kA              = 1,\n"
     "  kAB             = 2,\n"
     "  kABCDEFGHIJKLMN = 3\n} x;\n"},

    // An empty line groups into several sections, to be aligned independently
    {"typedef enum { kA=1, kAB=2, \n\n kABC=3} x;",
     "typedef enum {\n"
     "  kA  = 1,\n"
     "  kAB = 2,\n"
     "\n"
     "  kABC = 3\n} x;\n"},

    // Lines with comments don't interrupt alignment
    {"typedef enum { kA=1,\n"
     "// hello world\n"
     "kAB=2, kABC=3} x;",
     "typedef enum {\n"
     "  kA   = 1,\n"
     "  // hello world\n"
     "  kAB  = 2,\n"
     "  kABC = 3\n} x;\n"},

    // Generally, comment locations are preserved, and full line comments
    // indented to enum name level.
    {"typedef enum { kA=1,// value kA\n"
     "// hello world\n"
     "kAB=2,// value kAB\n"
     "kABC=3// value kABC\n"
     "} x;",
     "typedef enum {\n"
     "  kA   = 1,  // value kA\n"
     "  // hello world\n"
     "  kAB  = 2,  // value kAB\n"
     "  kABC = 3   // value kABC"
     "\n} x;\n"},

    {"typedef enum { kA=1,// value kA\n"
     "kAB=2,\n"
     "kABC=3// value kABC\n"
     "} x;",
     "typedef enum {\n"
     "  kA   = 1,  // value kA\n"
     "  kAB  = 2,\n"
     "  kABC = 3   // value kABC"
     "\n} x;\n"},

    // Numeric constants are currently flushed left, but maybe todo
    // align right in the future ?
    {"typedef enum { kA=1, kAB=10, kABC=100} x;",
     "typedef enum {\n"
     "  kA   = 1,\n"
     "  kAB  = 10,\n"
     "  kABC = 100\n} x;\n"},
};

TEST(FormatterEndToEndTest, FormatAlignEnumDeclarations) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.enum_assignment_statement_alignment = AlignmentPolicy::kInferUserIntent;
  for (const auto &test_case : kFormatterTestCasesEnumDeclarations) {
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
  for (const auto &test_case : kDiagnosticFormatterTestCases) {
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
  for (const auto &test_case : kDiagnosticFormatterTestCases) {
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
  for (const auto &test_case : kDiagnosticFormatterTestCases) {
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

  const std::string_view code("parameter int x = 1+1;\n");

  std::ostringstream stream, debug_stream;
  ExecutionControl control;
  control.max_search_states = 2;  // Cause search to abort early.
  control.stream = &debug_stream;
  const auto status = FormatVerilog(code, "<filename>", style, stream,
                                    kEnableAllLines, control);
  EXPECT_EQ(status.code(), StatusCode::kResourceExhausted);
  EXPECT_TRUE(absl::StartsWith(status.message(), "***"));
}

static constexpr FormatterTestCase kOnelineFormatBaselineTestCases[] = {
    // Reference - following test cases should not be affected by the switch
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
     "bins low={LOW}; bins high={HIGH};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  address: coverpoint addr {\n"
     "    bins low = {LOW};\n"
     "    bins high = {HIGH};\n"
     "  }\n"
     "endgroup\n"},
    {// Multiple bins with multiple elements
     "covergroup memory @ (posedge ce); address  :coverpoint addr {"
     "bins low={0,127}; bins high={128,255};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  address: coverpoint addr {\n"
     "    bins low = {0, 127};\n"
     "    bins high = {128, 255};\n"
     "  }\n"
     "endgroup\n"},
};

// Tests that constructs that could be formatted as one-liners are formatted
// correctly. This is the baseline test with default style, test cases should
// not be affected by the setting.
TEST(FormatterEndToEndTest, OnelineFormatBaselineTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.expand_coverpoints = false;
  for (const auto &test_case : kOnelineFormatBaselineTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
  // Test again with the switch, should not affect formatting
  style.expand_coverpoints = true;
  for (const auto &test_case : kOnelineFormatBaselineTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Following two test sets are affected by the expand_coverponits switch
// They should contain corresponding test cases
static constexpr FormatterTestCase kOnelineFormatReferenceTestCases[] = {
    {// Coverpoint that could fit on one line
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {bins l = {0};}\n"
     "endgroup\n"},
    {// Fit with a reference
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={LOW};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {bins l = {LOW};}\n"
     "endgroup\n"},
    {// Fit with multiple elements
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0,8};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {bins l = {0, 8};}\n"
     "endgroup\n"},
};

static constexpr FormatterTestCase kOnelineFormatExpandTestCases[] = {
    {// Coverpoint that could fit on one line
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {\n"
     "    bins l = {0};\n"
     "  }\n"
     "endgroup\n"},
    {// Fit with a reference
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={LOW};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {\n"
     "    bins l = {LOW};\n"
     "  }\n"
     "endgroup\n"},
    {// Fit with multiple elements
     "covergroup memory @ (posedge ce); a :coverpoint d {"
     "bins l={0,8};} endgroup\n",
     "covergroup memory @(posedge ce);\n"
     "  a: coverpoint d {\n"
     "    bins l = {0, 8};\n"
     "  }\n"
     "endgroup\n"},
};

TEST(FormatterEndToEndTest, OnelineFormatReferenceTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.expand_coverpoints = false;

  for (const auto &test_case : kOnelineFormatReferenceTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Tests that constructs that could be formatted as one-liners are expanded
// correctly. This is the baseline test with default style
TEST(FormatterEndToEndTest, OnelineFormatExpandTest) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.expand_coverpoints = true;

  for (const auto &test_case : kOnelineFormatExpandTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

struct DimensionsAlignmentTestCase {
  std::string_view input;
  std::string_view expected[4];
};

static constexpr DimensionsAlignmentTestCase
    kPortDeclarationDimensionsAlignmentTestCases[] = {
        {"import \"DPI-C\" context function void func(\n"
         "input bit foo,\n"
         "input bit [2] foobar,\n"
         "input bit [24:16] bar,\n"
         "input bit [2:0][31:0] baz,\n"
         "input bit [4][24:0][16:0] foobarbaz[],\n"
         "input bit [FOOBAZ][31:24] qux[16],\n"
         "input bit [16] quux[8:2][16][32],\n"
         "output bit [2:0][2:0][2:0] quuz[8][16:12]\n"
         ");\n",
         {
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                       foo,\n"
             "  input  bit [     2]              foobar,\n"
             "  input  bit [ 24:16]              bar,\n"
             "  input  bit [   2:0][ 31:0]       baz,\n"
             "  input  bit [     4][ 24:0][16:0] foobarbaz[   ],\n"
             "  input  bit [FOOBAZ][31:24]       qux      [ 16],\n"
             "  input  bit [    16]              quux     [8:2][   16][32],\n"
             "  output bit [   2:0][  2:0][ 2:0] quuz     [  8][16:12]\n"
             ");\n",
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                       foo,\n"
             "  input  bit [     2]              foobar,\n"
             "  input  bit [ 24:16]              bar,\n"
             "  input  bit [   2:0][ 31:0]       baz,\n"
             "  input  bit [     4][ 24:0][16:0] foobarbaz         [     ],\n"
             "  input  bit [FOOBAZ][31:24]       qux               [   16],\n"
             "  input  bit [    16]              quux     [8:2][16][   32],\n"
             "  output bit [   2:0][  2:0][ 2:0] quuz          [ 8][16:12]\n"
             ");\n",
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                      foo,\n"
             "  input  bit              [    2] foobar,\n"
             "  input  bit              [24:16] bar,\n"
             "  input  bit      [   2:0][ 31:0] baz,\n"
             "  input  bit [  4][  24:0][ 16:0] foobarbaz[   ],\n"
             "  input  bit      [FOOBAZ][31:24] qux      [ 16],\n"
             "  input  bit              [   16] quux     [8:2][   16][32],\n"
             "  output bit [2:0][   2:0][  2:0] quuz     [  8][16:12]\n"
             ");\n",
             "import \"DPI-C\" context function void func(\n"
             "  input  bit                      foo,\n"
             "  input  bit              [    2] foobar,\n"
             "  input  bit              [24:16] bar,\n"
             "  input  bit      [   2:0][ 31:0] baz,\n"
             "  input  bit [  4][  24:0][ 16:0] foobarbaz         [     ],\n"
             "  input  bit      [FOOBAZ][31:24] qux               [   16],\n"
             "  input  bit              [   16] quux     [8:2][16][   32],\n"
             "  output bit [2:0][   2:0][  2:0] quuz          [ 8][16:12]\n"
             ");\n",
         }},
        {"module m(\n"
         "output bit [2:0][2:0][2:0] quuz[8][16:12],\n"
         "input bit [16] quux[8:2][16][32],\n"
         "input bit [FOOBAZ][31:24] qux[16],\n"
         "input bit [4][24:0][16:0] foobaz[],\n"
         "input bit [2:0][31:0] baz,\n"
         "input bit [24:16] bar,\n"
         "input bit [2] foobar,\n"
         "input bit foo\n"
         "); endmodule:m\n",
         {
             "module m (\n"
             "    output bit [   2:0][  2:0][ 2:0] quuz  [  8][16:12],\n"
             "    input  bit [    16]              quux  [8:2][   16][32],\n"
             "    input  bit [FOOBAZ][31:24]       qux   [ 16],\n"
             "    input  bit [     4][ 24:0][16:0] foobaz[   ],\n"
             "    input  bit [   2:0][ 31:0]       baz,\n"
             "    input  bit [ 24:16]              bar,\n"
             "    input  bit [     2]              foobar,\n"
             "    input  bit                       foo\n"
             ");\n"
             "endmodule : m\n",
             "module m (\n"
             "    output bit [   2:0][  2:0][ 2:0] quuz       [ 8][16:12],\n"
             "    input  bit [    16]              quux  [8:2][16][   32],\n"
             "    input  bit [FOOBAZ][31:24]       qux            [   16],\n"
             "    input  bit [     4][ 24:0][16:0] foobaz         [     ],\n"
             "    input  bit [   2:0][ 31:0]       baz,\n"
             "    input  bit [ 24:16]              bar,\n"
             "    input  bit [     2]              foobar,\n"
             "    input  bit                       foo\n"
             ");\n"
             "endmodule : m\n",
             "module m (\n"
             "    output bit [2:0][   2:0][  2:0] quuz  [  8][16:12],\n"
             "    input  bit              [   16] quux  [8:2][   16][32],\n"
             "    input  bit      [FOOBAZ][31:24] qux   [ 16],\n"
             "    input  bit [  4][  24:0][ 16:0] foobaz[   ],\n"
             "    input  bit      [   2:0][ 31:0] baz,\n"
             "    input  bit              [24:16] bar,\n"
             "    input  bit              [    2] foobar,\n"
             "    input  bit                      foo\n"
             ");\n"
             "endmodule : m\n",
             "module m (\n"
             "    output bit [2:0][   2:0][  2:0] quuz       [ 8][16:12],\n"
             "    input  bit              [   16] quux  [8:2][16][   32],\n"
             "    input  bit      [FOOBAZ][31:24] qux            [   16],\n"
             "    input  bit [  4][  24:0][ 16:0] foobaz         [     ],\n"
             "    input  bit      [   2:0][ 31:0] baz,\n"
             "    input  bit              [24:16] bar,\n"
             "    input  bit              [    2] foobar,\n"
             "    input  bit                      foo\n"
             ");\n"
             "endmodule : m\n",
         }},
};

TEST(FormatterEndToEndTest, PortDeclarationDimensionsAlignmentTest) {
  FormatStyle style;
  style.column_limit = 64;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  // All combinations of port_declaration_right_align_{un,}packed_dimensions
  static const bool right_align_combinations[][2] = {
      {false, false}, {false, true}, {true, false}, {true, true}};

  for (const auto &test_case : kPortDeclarationDimensionsAlignmentTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    size_t expected_index = 0;
    for (const auto &right_align : right_align_combinations) {
      VLOG(1) << "style variant:\n"
              << "port_declarations_right_align_packed_dimensions: "
              << right_align[0] << "\n"
              << "port_declarations_right_align_unpacked_dimensions: "
              << right_align[1] << "\n";
      style.port_declarations_right_align_packed_dimensions = right_align[0];
      style.port_declarations_right_align_unpacked_dimensions = right_align[1];

      std::ostringstream stream;

      const auto status =
          FormatVerilog(test_case.input, "<filename>", style, stream);
      // Require these test cases to be valid.
      EXPECT_OK(status) << status.message();
      EXPECT_EQ(stream.str(), test_case.expected[expected_index++])
          << "code:\n"
          << test_case.input;
    }
  }
}

// TODO(fangism): directed tests using style variations

static constexpr FormatterTestCase kNestedFunctionsTestCases40ColumnsLimit[] = {
    {"module foo;"
     "`uvm_info(`gfn, $sformatf(\n"
     "\"\\n  base_vseq: generate %0d pulse in channel %0d\", cfg.num_pulses, "
     "i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n  base_vseq: generate %0d pulse in channel %0d\",\n"
     "          cfg.num_pulses, i), UVM_DEBUG)\n"
     "endmodule\n"},
    {"module foo;`uvm_info(`gfn, $sformatf("
     "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
     "cfg.convert2string()), UVM_LOW)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\",\n"
     "          cfg.convert2string()),\n"
     "      UVM_LOW)\n"  // FIXME: Wrapped by SearchLineWraps
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases40ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto &test_case : kNestedFunctionsTestCases40ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kNestedFunctionsTestCases60ColumnsLimit[] = {
    {"module foo;"
     "`uvm_info(`gfn, $sformatf(\n"
     "\"\\n  base_vseq: generate %0d pulse in channel %0d\", cfg.num_pulses, "
     "i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n  base_vseq: generate %0d pulse in channel %0d\",\n"
     "          cfg.num_pulses, i), UVM_DEBUG)\n"
     "endmodule\n"},
    {"module foo;`uvm_info(`gfn, $sformatf("
     "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
     "cfg.convert2string()), UVM_LOW)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\",\n"
     "          cfg.convert2string()), UVM_LOW)\n"
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases60ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 60;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto &test_case : kNestedFunctionsTestCases60ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kNestedFunctionsTestCases80ColumnsLimit[] = {
    {"module foo;"
     "`uvm_info(`gfn, $sformatf(\n"
     "\"\\n  base_vseq: generate %0d pulse in channel %0d\", cfg.num_pulses, "
     "i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d pulse in "
     "channel %0d\",\n"
     "                            cfg.num_pulses, i), UVM_DEBUG)\n"
     "endmodule\n"},
    {"module foo;`uvm_info(`gfn, $sformatf("
     "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
     "cfg.convert2string()), UVM_LOW)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, $sformatf(\n"
     "                      \"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE "
     "|----\\n %s\",\n"
     "                      cfg.convert2string()), UVM_LOW)\nendmodule\n"},
    {"module x;"
     "`uvm_fatal(`gfn, $sformatf("
     "\"The data 0x%0h written to the signature address is formatted "
     "incorrectly.\","
     "signature_data))\n"
     "endmodule",
     "module x;\n"
     "  `uvm_fatal(\n"
     "      `gfn,\n"
     "      $sformatf(\n"
     "          \"The data 0x%0h written to the signature address is formatted "
     "incorrectly.\",\n"
     "          signature_data))\n"
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases80ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 80;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto &test_case : kNestedFunctionsTestCases80ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kNestedFunctionsTestCases100ColumnsLimit[] =
    {
        {"module foo;"
         "`uvm_info(`gfn, $sformatf(\n"
         "\"\\n  base_vseq: generate %0d pulse in channel %0d\", "
         "cfg.num_pulses, i), UVM_DEBUG)\n"
         "endmodule",
         "module foo;\n"
         "  `uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d pulse in "
         "channel %0d\", cfg.num_pulses, i),\n"
         "            UVM_DEBUG)\n"
         "endmodule\n"},
        {"module foo;`uvm_info(`gfn, $sformatf("
         "\"\\n\\n\\t ----| STARTING AES MAIN SEQUENCE |----\\n %s\","
         "cfg.convert2string()), UVM_LOW)\n"
         "endmodule",
         "module foo;\n"
         "  `uvm_info(`gfn, $sformatf(\"\\n\\n\\t ----| STARTING AES MAIN "
         "SEQUENCE |----\\n %s\",\n"
         "                            cfg.convert2string()), UVM_LOW)\n"
         "endmodule\n"},
        {"module x;"
         "`uvm_fatal(`gfn, $sformatf("
         "\"The data 0x%0h written to the signature address is formatted "
         "incorrectly.\","
         "signature_data))\n"
         "endmodule",
         "module x;\n"
         "  `uvm_fatal(`gfn, $sformatf(\n"
         "                       \"The data 0x%0h written to the signature "
         "address is formatted incorrectly.\",\n"
         "                       signature_data))\n"
         "endmodule\n"},
        {// nested modules, three-levels
         "module x; module y; module z;"
         "`uvm_fatal(`gfn, $sformatf("
         "\"The data 0x%0h written to the signature address is formatted "
         "incorrectly.\","
         "signature_data))\n"
         "endmodule endmodule endmodule",
         "module x;\n"
         "  module y;\n"
         "    module z;\n"
         "      `uvm_fatal(`gfn,\n"
         "                 $sformatf(\n"
         "                     \"The data 0x%0h written to the signature "
         "address is formatted incorrectly.\",\n"
         "                     signature_data))\n"
         "    endmodule\n"
         "  endmodule\n"
         "endmodule\n"},
};

TEST(FormatterEndToEndTest, FormatNestedFunctionsTestCases100ColumnsLimit) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 100;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto &test_case : kNestedFunctionsTestCases100ColumnsLimit) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase noCompactIndexingAndSelectionsTestCases[]{
    {"module m1 ();\n"
     "  assign s1 = msg[1 : 2 + 3];\n"
     "  assign s2 = {<<8{msg[1 : 2 + 3]}};\n"
     "endmodule\n",
     "module m1 ();\n"
     "  assign s1 = msg[1 : 2 + 3];\n"
     "  assign s2 = {<< 8{msg[1 : 2 + 3]}};\n"
     "endmodule\n"}};

TEST(FormatterEndToEndTest, compactIndexingAndSelectionsTestCases) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 100;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.compact_indexing_and_selections = false;

  for (const auto &test_case : noCompactIndexingAndSelectionsTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase
    kSpaceBeforeHashInUnqualifiedTypedefTestCases[] = {
        // unqualified parameterized type keeps a space before '#'
        {"typedef dv_base_env_cov #(.CFG_T(tl_agent_env_cfg)) "
         "tl_agent_env_cov;\n",
         "typedef dv_base_env_cov #(\n"
         "    .CFG_T(tl_agent_env_cfg)\n"
         ") tl_agent_env_cov;\n"},
        // ... and is inserted when absent
        {"typedef dv_base_env_cov#(.CFG_T(tl_agent_env_cfg)) "
         "tl_agent_env_cov;\n",
         "typedef dv_base_env_cov #(\n"
         "    .CFG_T(tl_agent_env_cfg)\n"
         ") tl_agent_env_cov;\n"},
        // single short parameter stays on one line
        {"typedef my_class #(.P(P)) my_class_t;\n",
         "typedef my_class #(.P(P)) my_class_t;\n"},
        // package-qualified types are unaffected (no space before '#')
        {"typedef foo_pkg::baz_t#(.L(L), .W(W)) bar_t;\n",
         "typedef foo_pkg::baz_t#(\n"
         "    .L(L),\n"
         "    .W(W)\n"
         ") bar_t;\n"},
};

TEST(FormatterEndToEndTest, SpaceBeforeHashInUnqualifiedTypedefTestCases) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 40;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.class_parameter_space = true;

  for (const auto &test_case : kSpaceBeforeHashInUnqualifiedTypedefTestCases) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

static constexpr FormatterTestCase kFunctionCallsWithComments[] = {
    {// no comments
     "module foo;\n"
     "`uvm_info(`gfn, \"xx\",\n"
     "cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, \"xx\", cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule\n"},
    {// one comment
     "module foo;\n"
     "`uvm_info(`gfn, \"xx\",  // yy\n"
     "cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, \"xx\",  // yy\n"
     "            cfg.num_pulses, i, UVM_DEBUG)\n"
     "endmodule\n"},
    {// two comments
     "module foo;\n"
     "`uvm_info(`gfn, \"xx\",  "
     "    cfg.num_pulses, // xx\n"
     " i, UVM_DEBUG)// uuu\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, \"xx\", cfg.num_pulses,  // xx\n"
     "            i, UVM_DEBUG)  // uuu\n"
     "endmodule\n"},

    {// nested function calls with comments
     "module foo;"
     "`uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d"
     " pulse in channel %0d\", cfg.num_pulses, // comment\n"
     " i), UVM_DEBUG)\n"
     "endmodule",
     "module foo;\n"
     "  `uvm_info(`gfn, $sformatf(\"\\n  base_vseq: generate %0d pulse in "
     "channel %0d\",\n"
     "                            cfg.num_pulses,  // comment\n"
     "                            i), UVM_DEBUG)\n"
     "endmodule\n"},
};

TEST(FormatterEndToEndTest, FunctionCallsWithComments) {
  // Use a fixed style.
  FormatStyle style;
  style.column_limit = 100;
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;

  for (const auto &test_case : kFunctionCallsWithComments) {
    VLOG(1) << "code-to-format:\n" << test_case.input << "<EOF>";
    std::ostringstream stream;
    const auto status =
        FormatVerilog(test_case.input, "<filename>", style, stream);
    // Require these test cases to be valid.
    EXPECT_OK(status) << status.message();
    EXPECT_EQ(stream.str(), test_case.expected) << "code:\n" << test_case.input;
  }
}

// Extracts the first non-whitespace token and prepends it with number of
// of newlines seen in front of it. So "\n \n\n  foo" -> "3foo"
std::string NLCountAndfirstWord(std::string_view str) {
  std::string result;
  int newline_count = 0;
  std::string_view::const_iterator begin = str.begin();
  for (/**/; begin < str.end(); ++begin) {
    if (!isspace(*begin)) break;
    newline_count += (*begin == '\n');
  }
  // Emit number of newlines seen up to first token.
  result.append(1,
                static_cast<char>(newline_count + '0'));  // single digit itoa
  std::string_view::const_iterator end_str = begin;
  for (/**/; end_str < str.end() && !isspace(*end_str); ++end_str) {
  }
  result.append(begin, end_str);
  return result;
}

// Similar to NLCountAndfirstWord() but looking at the last token and trailing
// newlines.
std::string lastWordAndNLCount(std::string_view str) {
  std::string result;
  int newline_count = 0;
  std::string_view::const_iterator back = str.end() - 1;
  for (/**/; back >= str.begin(); --back) {
    if (!isspace(*back)) break;
    newline_count += (*back == '\n');
  }

  std::string_view::const_iterator start_str = back;
  for (/**/; start_str >= str.begin() && !isspace(*start_str); --start_str) {
  }
  result.append(&*(start_str + 1), back - start_str);
  // Emit number of newlines seen following last token.
  result.append(1,
                static_cast<char>(newline_count + '0'));  // single digit itoa
  return result;
}

// Testing the tester...
TEST(FormatterTestInternal, TokenExtractorAndLineCounterTestFixtureTest) {
  std::string_view str = "\n \nhello world \n \n";
  ASSERT_EQ(NLCountAndfirstWord(str), "2hello");
  ASSERT_EQ(NLCountAndfirstWord(str.substr(1)), "1hello");
  ASSERT_EQ(NLCountAndfirstWord(str.substr(3)), "0hello");

  ASSERT_EQ(lastWordAndNLCount(str), "world2");
  ASSERT_EQ(lastWordAndNLCount(str.substr(0, str.length() - 1)), "world1");
  ASSERT_EQ(lastWordAndNLCount(str.substr(0, str.length() - 2)), "world1");
  ASSERT_EQ(lastWordAndNLCount(str.substr(0, str.length() - 3)), "world0");

  str = "aworld \n \n";  // Make sure to not overshoot begin.
  ASSERT_EQ(lastWordAndNLCount(str.substr(1)), "world2");
}

TEST(FormatterEndToEndTest, RangeFormattingOnlyEmittingRelevantLines) {
  // Including some empty lines to make sure the formatting
  static constexpr std::string_view unformatted =
      R"(     module foo (// non-port comment

     // some comment

  input  logic  a, input logic  b,input bit [2]
foobar, input    bit [4] foobaz,
    input bit [2]    quux


        ); endmodule
)";

  std::vector<std::string_view> lines = absl::StrSplit(unformatted, '\n');
  const int kLineCount = lines.size();

  FormatStyle style;

  // Go through all possible sub-ranges, format these, and compare that the
  // output of the range output is contained inside the full format given the
  // same sub-range.
  for (int start_line = 0; start_line < kLineCount; ++start_line) {
    for (int end_line = start_line; end_line < kLineCount; ++end_line) {
      // Line numbers are 1-index based.
      const verible::Interval<int> range = {start_line + 1, end_line + 1};

      // Format full text for reference
      std::ostringstream full_format;
      absl::Status status =
          FormatVerilog(unformatted, "<filename>", style, full_format, {range});
      EXPECT_OK(status) << status.message();

      // To test: range formatted.
      std::string range_formatted;
      status = FormatVerilogRange(unformatted, "<filename>", style,
                                  &range_formatted, range);
      EXPECT_OK(status) << status.message();
      if (range.empty()) {  // Nothing to format: expect empty output.
        EXPECT_TRUE(range_formatted.empty());
        continue;
      }

      // Area we cover in the input (include the final newline);
      const auto source_begin = lines[start_line].begin();
      const auto source_end = lines[end_line - 1].end() + 1;  // include \n
      const std::string_view range_unformatted(&*source_begin,
                                               source_end - source_begin);

      // To compare that we indeed formatted the requested reqgion, we make
      // sure that the first and last token (simplified: non-whitespace word)
      // in the input of the range to be formatted is
      // exactly the first and last token that comes out of the range
      // formatted snippet.
      //
      // While the whitespace might be different at the beginning and end
      // due to formatting, the number of _newlines_ at the beginning and end
      // should be the same, so we include the newline count in the comparison.

      EXPECT_EQ(NLCountAndfirstWord(range_unformatted),
                NLCountAndfirstWord(range_formatted))
          << "'" << range_unformatted << "' vs. '" << range_formatted << "'";

      // ... same for the last word.
      EXPECT_EQ(lastWordAndNLCount(range_unformatted),
                lastWordAndNLCount(range_formatted))
          << "'" << range_unformatted << "' vs. '" << range_formatted << "'";

      EXPECT_LE(range_formatted.length(), full_format.str().length());
      EXPECT_THAT(full_format.str(), HasSubstr(range_formatted));
    }
  }
}

// Creates a string_view spanning a whole string literal.
// Works correctly with strings containing null bytes.
template <std::size_t N>
constexpr std::string_view string_view_from_literal(const char (&s)[N]) {
  return std::string_view(s, N - 1);
}

// The following regressions have been found by a fuzzer, so the input might
// look a bit 'funny'. Nevertheless, they expose real bugs in the code.

[[maybe_unused]] void TestForNonCrash(std::string_view input) {
  using verible::EscapeString;
  FormatStyle style;
  std::ostringstream stream;
  const auto status = FormatVerilog(input, "<filename>", style, stream);
  // If we are here, we did at least not crash, which is all we desire for
  // the fuzzer tests.
  // Other issues that are reported correctly (probably due to strange inputs
  // generated by the fuzzer) are ok, so we just log them here FYI.
  if (!status.ok()) {
    LOG(INFO) << "No crash, but format failed for other reason: "
              << status.message() << "\nEscaped input: \""
              << EscapeString(input) << "\"";
  }
}

#if 0
// https://github.com/chipsalliance/verible/issues/1381
TEST(FormatterEndToEndTest, FuzzingRegression_1381) {
  TestForNonCrash(string_view_from_literal("P#(\0\0//\0//\0,);"));
  TestForNonCrash(string_view_from_literal("P#(\0\0//\0,,//\0,"));
}
#endif

// https://github.com/chipsalliance/verible/issues/1384
TEST(FormatterEndToEndTest, FuzzingRegression_UseAfterFree_1384) {
  TestForNonCrash("`c(`c(//););");
}

// https://github.com/chipsalliance/verible/issues/1386
TEST(FormatterEndToEndTest, FuzzingRegressionHierachyInvariant) {
  // Original sample input from fuzzer
  TestForNonCrash(string_view_from_literal("`c(`c()\0;);"));
  // Lexically correct variant (`\0` -> `\n`)
  TestForNonCrash(string_view_from_literal("`c(`c()\n;);"));
  // Lexically correct variant (`\0` removed)
  TestForNonCrash(string_view_from_literal("`c(`c(););"));
}

// Possibly same ? https://github.com/chipsalliance/verible/issues/1386
TEST(FormatterEndToEndTest, FuzzingRegression_outofmemory) {
  TestForNonCrash(string_view_from_literal("`f(1'O`f())\n"));
}

}  // namespace
}  // namespace formatter
}  // namespace verilog
