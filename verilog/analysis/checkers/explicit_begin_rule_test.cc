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

#include "verilog/analysis/checkers/explicit_begin_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/token_stream_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunApplyFixCases;
using verible::RunLintTestCases;

// Tests that space-only text passes.
TEST(ExplicitBeginRuleTest, AcceptsBlank) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {" "},
      {"\n"},
      {" \n\n"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(kTestCases);
}

// Tests that properly matched if/begin passes.
TEST(ExplicitBeginRuleTest, AcceptsBlocksWithBegin) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"if (FOO) begin"},
      {"if (FOO)\n begin"},
      {"if (FOO) //Comment\n begin"},
      {"else begin \n FOO"},
      {"else \nbegin \n FOO"},
      {"else //Comment\n begin \n FOO"},
      {"else \n //Comment\n begin \n FOO"},
      {"else if (FOO) begin"},
      {"else if (FOO)\n begin"},
      {"else if (FOO) //Comment\n begin"},
      {"else if (FOO)\n //Comment\n begin"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(kTestCases);
}

// Tests that unmatched block/begin fails is detected.
TEST(ExplicitBeginRuleTest, RejectBlocksWithoutBegin) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_if, "if"}, " (FOO) BAR"},
      {{TK_if, "if"}, " (FOO)\n BAR"},
      {{TK_if, "if"}, " (FOO) //Comment\n BAR"},
      {{TK_else, "else"}, " \n FOO"},
      {{TK_else, "else"}, " \n \n FOO"},
      {{TK_else, "else"}, " //Comment\n  FOO"},
      {{TK_else, "else"}, " \n //Comment\n FOO"},
      {"else ", {TK_if, "if"}, " (FOO) BAR"},
      {"else ", {TK_if, "if"}, " (FOO)\n BAR"},
      {"else ", {TK_if, "if"}, " (FOO) //Comment\n BAR"},
      {"else ", {TK_if, "if"}, " (FOO)\n //Comment\n BAR"},
  };
  RunLintTestCases<VerilogAnalyzer, ExplicitBeginRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
