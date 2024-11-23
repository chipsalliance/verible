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

#include "verible/verilog/analysis/checkers/suspicious-semicolon-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace analysis {
namespace {

static constexpr int kToken = ';';
TEST(SuspiciousSemicolon, DetectSuspiciousSemicolons) {
  const std::initializer_list<verible::LintTestCase>
      kSuspiciousSemicolonTestCases = {
          {"module m; initial begin if(x)", {kToken, ";"}, " end endmodule"},
          {"module m; initial begin if(x) x; else",
           {kToken, ";"},
           " y; end endmodule"},
          {"module m; initial begin while(x)", {kToken, ";"}, " end endmodule"},
          {"module m; initial begin forever", {kToken, ";"}, " end endmodule"},
          {"module m; always_ff @(posedge clk)", {kToken, ";"}, " endmodule"},
          {"module m; initial begin for(;;)", {kToken, ";"}, " end endmodule"},
          {"module m; initial begin foreach (array[i])",
           {kToken, ";"},
           " end endmodule"},
      };

  verible::RunLintTestCases<VerilogAnalyzer, SuspiciousSemicolon>(
      kSuspiciousSemicolonTestCases);
}

TEST(SuspiciousSemicolon, ShouldNotComplain) {
  const std::initializer_list<verible::LintTestCase>
      kSuspiciousSemicolonTestCases = {
          {""},
          {"module m; initial begin if(x) begin end end endmodule"},
          {"module m; @(posedge clk); endmodule"},
          {"module m; always_ff @(posedge clk) begin ; end endmodule"},
          {"module m; endmodule;"},
          {"class c; int x;; endclass"},
      };

  verible::RunLintTestCases<VerilogAnalyzer, SuspiciousSemicolon>(
      kSuspiciousSemicolonTestCases);
}

TEST(SuspiciousSemicolon, ApplyAutoFix) {
  const std::initializer_list<verible::AutoFixInOut>
      kSuspiciousSemicolonTestCases = {
          {"module m; initial begin if(x); end endmodule",
           "module m; initial begin if(x) end endmodule"},
          {"module m; initial begin if(x) x; else; y; end endmodule",
           "module m; initial begin if(x) x; else y; end endmodule"},
          {"module m; initial begin while(x); end endmodule",
           "module m; initial begin while(x) end endmodule"},
          {"module m; initial begin forever; end endmodule",
           "module m; initial begin forever end endmodule"},
          {"module m; always_ff @(posedge clk); endmodule",
           "module m; always_ff @(posedge clk) endmodule"},
          {"module m; initial begin foreach (array[i]); end endmodule",
           "module m; initial begin foreach (array[i]) end endmodule"},
      };

  verible::RunApplyFixCases<VerilogAnalyzer, SuspiciousSemicolon>(
      kSuspiciousSemicolonTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
