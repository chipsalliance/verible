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

#include "verible/verilog/analysis/checkers/port-name-suffix-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that PortNameSuffixRule correctly accepts valid names.
TEST(PortNameSuffixRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module t (input logic name_i); endmodule;"},
      {"module t (output logic abc_o); endmodule;"},
      {"module t (inout logic xyz_io); endmodule;"},
      {"module t (input logic long_name_i); endmodule;"},
      {"module t (output logic long_abc_o); endmodule;"},
      {"module t (inout logic long_xyz_io); endmodule;"},
      {"module t (input logic name_ni); endmodule;"},
      {"module t (output logic abc_no); endmodule;"},
      {"module t (inout logic xyz_nio); endmodule;"},
      {"module t (input logic name_pi); endmodule;"},
      {"module t (output logic abc_po); endmodule;"},
      {"module t (inout logic xyz_pio); endmodule;"},
      {"module t (input logic [7:0] name_i); endmodule;"},
      {"module t (output logic [2:0] abc_o); endmodule;"},
      {"module t (inout logic [3:0] xyz_io); endmodule;"},
      {"module t (input logic [7:0] name_ni); endmodule;"},
      {"module t (output logic [2:0] abc_no); endmodule;"},
      {"module t (inout logic [3:0] xyz_nio); endmodule;"},
      {"module t (input logic [7:0] name_pi); endmodule;"},
      {"module t (output logic [2:0] abc_po); endmodule;"},
      {"module t (inout logic [3:0] xyz_pio); endmodule;"},
      {"module t (input bit name_i); endmodule;"},
      {"module t (output bit abc_o); endmodule;"},
      {"module t (inout bit xyz_io); endmodule;"},
      {"module t (input logic name_i,\n"
       "output logic abc_o,\n"
       "inout logic xyz_io,\n"
       "input logic [7:0] namea_i,\n"
       "output logic [2:0] abca_o,\n"
       "inout logic [3:0] xyza_io,\n"
       "input bit nameb_i,\n"
       "output bit abcb_o,\n"
       "inout bit xyzb_io);\n"
       "endmodule;"},
  };
  RunLintTestCases<VerilogAnalyzer, PortNameSuffixRule>(kTestCases);
}

// Tests that PortNameSuffixRule rejects invalid names.
TEST(PortNameSuffixRuleTest, RejectTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      // General tests
      {"module t (input logic ", {kToken, "name"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "abc"}, "); endmodule;"},
      {"module t (inout logic ", {kToken, "xyz"}, "); endmodule;"},
      {"module t (input logic [7:0] ", {kToken, "name"}, "); endmodule;"},
      {"module t (output logic [2:0] ", {kToken, "abc"}, "); endmodule;"},
      {"module t (inout logic [3:0] ", {kToken, "xyz"}, "); endmodule;"},
      {"module t (input bit ", {kToken, "name"}, "); endmodule;"},
      {"module t (output bit ", {kToken, "abc"}, "); endmodule;"},
      {"module t (inout bit ", {kToken, "xyz"}, "); endmodule;"},

      {"module t (input logic ", {kToken, "_i"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "_o"}, "); endmodule;"},
      {"module t (inout logic ", {kToken, "_io"}, "); endmodule;"},

      {"module t (input logic ", {kToken, "namei"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "nam_ei"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "name_o"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "name_io"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "name_no"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "name_nio"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "name_po"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "name_pio"}, "); endmodule;"},

      {"module t (output logic ", {kToken, "nameo"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "nam_eo"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_i"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_oi"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_ni"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_nio"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_pi"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_pio"}, "); endmodule;"},

      {"module t (input logic ",
       {kToken, "name"},
       ",\n"
       "output logic abc_o,\n"
       "inout logic ",
       {kToken, "xyz"},
       ",\n"
       "input logic [7:0] namea_i,\n"
       "output logic [2:0] ",
       {kToken, "abca"},
       ",\n"
       "inout logic [3:0] xyza_io,\n"
       "input bit ",
       {kToken, "nameb"},
       ",\n"
       "output bit ",
       {kToken, "abcb"},
       ",\n"
       "inout bit xyzb_io);\n"
       "endmodule;"},
      // Invalid casing
      {"module t (input logic ", {kToken, "name_I"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "abc_O"}, "); endmodule;"},
      {"module t (inout logic ", {kToken, "xyz_IO"}, "); endmodule;"},
      // no underscore
      {"module t (input logic ", {kToken, "namei"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "abco"}, "); endmodule;"},
      {"module t (inout logic ", {kToken, "xyzio"}, "); endmodule;"},
      // Mismatched suffix tests
      {"module t (input logic ", {kToken, "name_o"}, "); endmodule;"},
      {"module t (input logic ", {kToken, "name_io"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_i"}, "); endmodule;"},
      {"module t (output logic ", {kToken, "name_io"}, "); endmodule;"},
      {"module t (inout logic ", {kToken, "name_i"}, "); endmodule;"},
      {"module t (inout logic ", {kToken, "name_o"}, "); endmodule;"},
  };
  RunLintTestCases<VerilogAnalyzer, PortNameSuffixRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
