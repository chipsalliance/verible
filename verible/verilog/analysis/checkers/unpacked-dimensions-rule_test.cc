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

#include "verible/verilog/analysis/checkers/unpacked-dimensions-rule.h"

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

TEST(UnpackedDimensionsRuleTests, CheckRanges) {
  // TODO(fangism): Check that violation message matches.
  constexpr int kScalar = TK_OTHER;
  constexpr int kOrder = TK_OTHER;
  const std::initializer_list<LintTestCase> kTestCases = {
      // Test incorrect code
      {""},
      {"module foo; endmodule"},

      // basic net declarations
      {"wire w;"},
      {"wire w [", {kScalar, "0:0"}, "];"},
      {"wire w [", {kScalar, "1:0"}, "];"},
      {"wire w [", {kScalar, "0:1"}, "];"},
      {"wire w [", {kOrder, "2:1"}, "];"},
      {"wire w [1:2];"},
      {"wire w [", {kScalar, "z:0"}, "];"},
      {"wire w [", {kScalar, "0:z"}, "];"},
      {"wire w [1:z];"},     // inconclusive
      {"wire w [4];"},       // good: scalar dimension
      {"wire w [X];"},       // good: scalar dimension
      {"wire w [N][M-2];"},  // good: scalar dimension
      {"wire w [", {kScalar, "0:1"}, "][", {kScalar, "0:3"}, "];"},
      {"wire w [", {kScalar, "0:1"}, "][", {kScalar, "3:0"}, "];"},
      {"wire w [", {kScalar, "0:1"}, "][", {kOrder, "3:1"}, "];"},
      {"wire w [", {kOrder, "1:0"}, "][", {kScalar, "0:3"}, "];"},
      {"wire w [", {kScalar, "1:0"}, "][", {kScalar, "2:0"}, "];"},
      {"wire w [6][", {kScalar, "2:0"}, "];"},
      {"wire w [", {kScalar, "2:0"}, "][5];"},
      {"wire w [6][", {kOrder, "2:1"}, "];"},
      {"wire w [", {kOrder, "2:1"}, "][5];"},

      // module-local nets
      {"module m; wire w [", {kScalar, "0:0"}, "]; endmodule"},
      {"module m; wire w [", {kScalar, "0:1"}, "]; endmodule"},
      {"module m; wire w [", {kScalar, "1:0"}, "]; endmodule"},
      {"module m; wire w [", {kOrder, "3:2"}, "]; endmodule"},
      {"module m; wire w [1:1]; endmodule"},
      {"module m; wire w [", {kScalar, "1:0"}, "][4]; endmodule"},

      // module-ports
      {"module m(input wire w [", {kScalar, "0:0"}, "]); endmodule"},
      {"module m(input wire w [", {kScalar, "0:1"}, "]); endmodule"},
      {"module m(input wire w [", {kScalar, "1:0"}, "]); endmodule"},
      {"module m(input wire w [", {kOrder, "11:1"}, "]); endmodule"},
      {"module m(input wire w [1:1]); endmodule"},
      {"module m(input wire [1:0] w [", {kScalar, "1:0"}, "]); endmodule"},

      // class members
      {"class c; endclass"},
      {"class c; logic l [", {kScalar, "0:0"}, "]; endclass"},
      {"class c; logic l [", {kScalar, "0:1"}, "]; endclass"},
      {"class c; logic l [", {kScalar, "1:0"}, "]; endclass"},
      {"class c; logic l [1:1]; endclass"},
      {"class c; logic l [", {kOrder, "2:1"}, "]; endclass"},
      {"class c; logic l [1:2]; endclass"},

      // struct members
      {"struct { bit l [", {kScalar, "0:0"}, "]; } s;"},
      {"struct { bit l [", {kScalar, "0:1"}, "]; } s;"},
      {"struct { bit l [", {kScalar, "1:0"}, "]; } s;"},
      {"struct { bit l [x:y]; } s;"},
      {"struct { bit l [1:3]; } s;"},
      {"struct { bit l [", {kOrder, "3:1"}, "]; } s;"},

      // struct typedef members
      {"typedef struct { bit l [", {kScalar, "0:0"}, "]; } s_s;"},
      {"typedef struct { bit l [", {kScalar, "0:1"}, "]; } s_s;"},
      {"typedef struct { bit l [", {kScalar, "1:0"}, "]; } s_s;"},
      {"typedef struct { bit l [x:y]; } s_s;"},
      {"typedef struct { bit l [2:2]; } s_s;"},
      {"typedef struct { bit l [2:3]; } s_s;"},

      // associative arrays
      {"int some_array [bit];"},
      {"int some_array [", {kScalar, "7:0"}, "];"},
      // inner range is packed dimensions, should not be detected as unpacked
      {"int some_array [bit [7:0]];"},
      {"int some_array [bit [0:7]];"},

      // array of instances
      {"module test (input logic blah);", "endmodule", "module tb;",
       "  test test_i[3:0] (.blah (1'b0));", "endmodule"},

      {"module test (input logic blah);", "endmodule", "module tb;",
       "  test test_i[0:3] (.blah (1'b0));", "endmodule"}};
  RunLintTestCases<VerilogAnalyzer, UnpackedDimensionsRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
