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

#include "verible/verilog/analysis/checkers/forbid-defparam-rule.h"

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

TEST(ForbidDefparamRuleTests, Various) {
  constexpr int kToken = TK_defparam;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module parameterized #(parameter int MY_PARAM = 0); endmodule; "
       "module foo; parameterized p0(); endmodule"},

      {"module parameterized #(parameter int MY_PARAM = 0); endmodule; "
       "module foo; ",
       {kToken, "defparam"},
       " p0.MY_PARAM = 1; "
       "parameterized p0(); endmodule"},

      {"module parameterized #(parameter int MY_PARAM = 0); endmodule; "
       "module foo; ",
       "/* defparam p0.MY_PARAM = 1; */"
       "parameterized p0(); endmodule"},

      {"module parameterized #("
       "parameter int MY_PARAM = 0, parameter int SECOND_PARAM = 0);"
       "endmodule; "
       "module foo; ",
       {kToken, "defparam"},
       " p0.MY_PARAM = 1; ",
       {kToken, "defparam"},
       " p0.SECOND_PARAM = 2; "
       "parameterized p0(); endmodule"},

      {"module foo; wire not_defparam; endmodule"},
  };

  RunLintTestCases<VerilogAnalyzer, ForbidDefparamRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
