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

#include "verilog/analysis/checkers/banned_declared_name_patterns_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that DeclaredIdentifierStyleRule correctly accepts valid identifiers.
TEST(BannedDeclaredNamePatternsRuleTest, AcceptTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"module foo; endmodule"},
      {"package p; endpackage"},

  };
  RunLintTestCases<VerilogAnalyzer, BannedDeclaredNamePatternsRule>(kTestCases);
}

// Tests that DeclaredIdentifierStyleRule correctly rejects invalid patterns.
TEST(BannedDeclaredNamePatternsRuleTest, RejectTests) {
  constexpr int kToken = 1;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"module ", {kToken, "ILLEGALNAME"}, "; endmodule"},
      {"package ", {kToken, "ILLEGALNAME"}, "; endpackage"},
      {"module ", {kToken, "illegalname"}, "; endmodule"},
      {"package ", {kToken, "IllegalName"}, "; endpackage"},

  };
  RunLintTestCases<VerilogAnalyzer, BannedDeclaredNamePatternsRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
