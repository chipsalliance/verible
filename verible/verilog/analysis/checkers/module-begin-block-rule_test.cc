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

#include "verible/verilog/analysis/checkers/module-begin-block-rule.h"

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

TEST(ModuleBeginBlockRuleTests, Various) {
  constexpr int kToken = TK_begin;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module foo; endmodule"},
      {"module foo; wire bar; endmodule"},
      {"module foo; ", {kToken, "begin"}, " end endmodule"},
      {"module foo; ", {kToken, "begin"}, " wire bar; end endmodule"},
      {"module foo; ",
       {kToken, "begin"},
       " end ",
       {kToken, "begin"},
       " end endmodule"},
      {"module foo; ",
       {kToken, "begin"},
       " ",
       {kToken, "begin"},
       " end end endmodule"},
      {"module foo; wire bar; ", {kToken, "begin"}, " end endmodule"},
      {"module foo; ", {kToken, "begin"}, " end wire bar; endmodule"},
      {"module foo; if (1) begin wire bar; end endmodule"},
      {"module foo; ", {kToken, "begin"}, " : bar end endmodule"},
      {"module foo; ", {kToken, "begin"}, " : bar end : bar endmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, ModuleBeginBlockRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
