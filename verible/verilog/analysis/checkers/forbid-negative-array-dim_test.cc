// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/forbid-negative-array-dim.h"

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

TEST(ForbidNegativeArrayDim, ForbidNegativeArrayDims) {
  constexpr int kScalar = TK_OTHER;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      /* Unpacked */
      {"logic l [", {kScalar, "-10"}, ":0];"},
      {"logic l [+1:0];"},
      {"logic l [-p:0];"},
      {"logic l [10+(-5):0];"},
      {"logic l [(", {kScalar, "-5"}, "):0];"},
      {"logic l [-(-5):0];"},
      /* Can't detect this at the moment */
      {"logic l [+(-5):0];"},
      /* Packed */
      {"logic [", {kScalar, "-10"}, ":-0] l;"},
      {"logic [0:", {kScalar, "-10"}, "] l;"},
      {"logic [1:0] l;"},
      /* Packed AND Unpacked */
      {"logic [", {kScalar, "-10"}, ":0] l [0:", {kScalar, "-10"}, "];"},

      /* Inside modules, in port declarations, ... */
      {"module k(); logic l [(", {kScalar, "-5"}, "):0]; endmodule"},
      {"module k(); logic l [1:0]; endmodule"},
      {"module k(logic l [(", {kScalar, "-5"}, "):0]);  endmodule"},
      {"module k(logic l [1:0]);  endmodule"},
      {"module k(logic l [(", {kScalar, "-5"}, "):0]);  endmodule"},
      {"struct { bit [1:0] l; } p;"},
      {"struct { logic [", {kScalar, "-1"}, ":0] l; } p;"},
      {"class p; bit [1:0] l; }; endclass"},
      {"class p; logic [", {kScalar, "-1"}, ":0] l; endclass"},
      {"function p(); logic [", {kScalar, "-1"}, ":0] l; endfunction"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbidNegativeArrayDim>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
