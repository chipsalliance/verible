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

#include "verible/verilog/analysis/checkers/explicit-task-lifetime-rule.h"

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

// Tests that expected number of explicit-task-lifetime rule violations are
// found.
TEST(ExplicitTaskLifetimeRuleTest, Tests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"task automatic foo(); endtask"},
      {"task static foo(); endtask"},
      {"task ", {kToken, "foo"}, "(); endtask"},

      // TODO(fangism): class methods must have automatic lifetime [LRM], so
      // cases like this should be flagged as redundant.
      {"task automatic myclass::foo(); endtask"},
      // TODO(fangism): class methods must have automatical lifetime [LRM], so
      // this cases should be rejected as invalid (elsewhere).
      {"task static myclass::foo(); endtask"},
      {"task myclass::foo(); endtask"},

      {"class bar; task automatic foo(); endtask endclass"},
      {"class bar; task static foo(); endtask endclass"},
      {"class bar; task foo(); endtask endclass"},
      // static methods have automatic lifetime
      {"class bar; static task foo(); endtask endclass"},

      {"module bar; task ", {kToken, "foo"}, "(); endtask endmodule"},
      {"module bar; task automatic foo(); endtask endmodule"},
      {"module bar; task static foo(); endtask endmodule"},

      {"package bar; task ", {kToken, "foo"}, "(); endtask endpackage"},
      {"package bar; task automatic foo(); endtask endpackage"},
      {"package bar; task static foo(); endtask endpackage"},
  };

  RunLintTestCases<VerilogAnalyzer, ExplicitTaskLifetimeRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
