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

#include "verilog/analysis/checkers/no_trailing_spaces_rule.h"

#include <initializer_list>

#include "common/analysis/line_linter_test_utils.h"
#include "common/analysis/linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that compliant text passes trailing-spaces check.
TEST(NoTrailingSpacesRuleTest, AcceptsText) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"\n"},
      {"\n\n\n"},
      {"\naaa"},
      {"x y z\n"},
      {"  xxx\n\n"},
      {"\txxx\n\n"},
      {"  xxx\n  yyy\n"},
      {"module foo;\nendmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, NoTrailingSpacesRule>(kTestCases);
}

// Tests that trailng spaces are detected and reported.
TEST(NoTrailingSpacesRuleTest, RejectsTrailingSpaces) {
  constexpr int kToken = TK_SPACE;
  const std::initializer_list<LintTestCase> kTestCases = {
      {{kToken, " "}},
      {"foo", {kToken, "     "}},
      {"foo", {kToken, "\t"}},
      {{kToken, "\t"}},
      {{kToken, "\t\t"}},
      {{kToken, "\t"}, "\n"},
      {"foo", {kToken, "\t"}, "\n"},
      {{kToken, "\t"}, "\n", {kToken, "\t"}, "\n"},
      {"a  b", {kToken, "   "}, "\n\n1  2", {kToken, "  "}, "\n"},
      {"module foo;", {kToken, " "}, "\nendmodule\n"},
      {"module foo;\nendmodule", {kToken, " "}, "\n"},
      {"`define\tIGNORANCE\t\"strength\"", {kToken, " "}, "\n"},
  };
  RunLintTestCases<VerilogAnalyzer, NoTrailingSpacesRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
