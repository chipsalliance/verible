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

#include "verible/verilog/analysis/checkers/explicit-function-lifetime-rule.h"

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

// Tests that expected number of explicit-function-lifetime rule violations are
// found.
TEST(ExplicitFunctionLifetimeRuleTest, Tests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"function automatic foo(); endfunction"},
      {"function static foo(); endfunction"},
      {"function ", {kToken, "foo"}, "(); endfunction"},

      // TODO(fangism): class methods must have automatic lifetime [LRM], so
      // cases like this should be flagged as redundant.
      {"function automatic myclass::foo(); endfunction"},
      // TODO(fangism): class methods must have automatica lifetime [LRM], so
      // this cases should be rejected as invalid (elsewhere).
      {"function static myclass::foo(); endfunction"},

      {"function myclass::foo(); endfunction"},
      {"function myclass::new(); endfunction"},

      // TODO(fangism): class methods always have automatic lifetime [LRM],
      // so we should advise against including a lifetime spec.
      {"class bar; function automatic foo(); endfunction endclass"},
      {"class bar; function static foo(); endfunction endclass"},

      {"class bar; function foo(); endfunction endclass"},
      {"class bar; function new(); endfunction endclass"},
      // static methods have automatic lifetime
      {"class bar; static function foo(); endfunction endclass"},

      {"module bar; function ", {kToken, "foo"}, "(); endfunction endmodule"},
      {"module bar; function automatic foo(); endfunction endmodule"},
      {"module bar; function static foo(); endfunction endmodule"},

      {"package bar; function ", {kToken, "foo"}, "(); endfunction endpackage"},
      {"package bar; function automatic foo(); endfunction endpackage"},
      {"package bar; function static foo(); endfunction endpackage"},
  };

  RunLintTestCases<VerilogAnalyzer, ExplicitFunctionLifetimeRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
