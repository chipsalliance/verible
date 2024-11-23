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

#include "verible/verilog/analysis/checkers/packed-dimensions-rule.h"

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

TEST(PackedDimensionsRuleTests, CheckRanges) {
  constexpr int kType = TK_OTHER;
  // TODO(fangism): use NodeEnum::kDimensionRange

  const std::initializer_list<LintTestCase> kTestCases = {
      // Test basic wire declarations
      {""},
      {"wire [0:0] w;"},
      {"wire [1:0] w;"},
      {"wire [", {kType, "0:1"}, "] w;"},
      {"wire [2:1] w;"},
      {"wire [", {kType, "1:2"}, "] w;"},
      {"wire [z:0] w;"},
      {"wire [", {kType, "0:z"}, "] w;"},
      {"wire [z:1] w;"},  // inconclusive
      {"wire [1:0][2:0] w;"},
      {"wire [", {kType, "0:1"}, "][2:0] w;"},
      {"wire [1:0][", {kType, "0:2"}, "] w;"},
      {"wire [", {kType, "0:1"}, "][", {kType, "0:3"}, "] w;"},

      // module-local nets
      {"module m; endmodule"},
      {"module m; wire [0:0] w; endmodule"},
      {"module m; wire [", {kType, "0:1"}, "] w; endmodule"},
      {"module m; wire [1:0] w; endmodule"},
      {"module m; wire [1:1] w; endmodule"},
      {"module m; wire [", {kType, "0:1"}, "] w [4]; endmodule"},

      // module-ports
      {"module m(input wire [0:0] w); endmodule"},
      {"module m(input wire [", {kType, "0:1"}, "] w); endmodule"},
      {"module m(input wire [1:0] w); endmodule"},
      {"module m(input wire [1:1] w); endmodule"},
      {"module m(input wire [", {kType, "0:1"}, "] w [4]); endmodule"},

      // class members
      {"class c; endclass"},
      {"class c; logic [0:0] l; endclass"},
      {"class c; logic [", {kType, "0:1"}, "] l; endclass"},
      {"class c; logic [1:0] l; endclass"},
      {"class c; logic [1:1] l; endclass"},

      // struct members
      {"struct { bit [0:0] l; } s;"},
      {"struct { bit [", {kType, "0:1"}, "] l; } s;"},
      {"struct { bit [1:0] l; } s;"},
      {"struct { bit [x:y] l; } s;"},

      // struct typedef members
      {"typedef struct { bit [0:0] l; } s_s;"},
      {"typedef struct { bit [", {kType, "0:1"}, "] l; } s_s;"},
      {"typedef struct { bit [1:0] l; } s_s;"},
      {"typedef struct { bit [x:y] l; } s_s;"},
  };
  RunLintTestCases<VerilogAnalyzer, PackedDimensionsRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
