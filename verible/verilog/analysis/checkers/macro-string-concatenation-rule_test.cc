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

#include "verible/verilog/analysis/checkers/macro-string-concatenation-rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/token-stream-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(MacroStringConcatenationRuleTest, BasicTests) {
  constexpr int kToken = TK_StringLiteral;
  const std::initializer_list<LintTestCase> kTestCases = {
      // Non rule violation cases
      {R"(`define FOO(arg) `"foo``arg``foo`")"
       "\n"},
      {R"(`define FOO(arg) `define BAR `define BAZ `"foobar``arg``baz`")"
       "\n"},
      {R"(`define FOO "foo")"
       "\n",
       R"(`define BAR(arg) `"bar``arg``bar`")"
       "\n",
       R"(`define BAZ(a, b) `"baz``a``baz``b``baz`")"
       "\n"},

      {R"(`define FOO(arg) "foo foo")"
       "\n"},
      {R"(`define FOO(arg) `define BAR `define BAZ "foobar baz")"
       "\n"},
      {R"(`define FOO "foo")"
       "\n",
       R"(`define BAR(arg) "bar bar")"
       "\n",
       R"(`define BAZ(a, b) "baz baz baz")"
       "\n"},

      // Rule Violation cases
      {R"(`define FOO(arg) "foo)",
       {kToken, "``"},
       R"(arg)",
       {kToken, "``"},
       R"(foo")"
       "\n"},
      {R"(`define FOO(arg) `define BAR `define BAZ "foobar)",
       {kToken, "``"},
       R"(arg)",
       {kToken, "``"},
       R"(baz")"
       "\n"},
      {R"(`define FOO "foo")"
       "\n",
       R"(`define BAR(arg) "bar)",
       {kToken, "``"},
       R"(arg)",
       {kToken, "``"},
       R"(bar")"
       "\n",
       R"(`define BAZ(a, b) "baz)",
       {kToken, "``"},
       R"(a)",
       {kToken, "``"},
       R"(baz)",
       {kToken, "``"},
       R"(b)",
       {kToken, "``"},
       R"(baz")"
       "\n"},
  };
  RunLintTestCases<VerilogAnalyzer, MacroStringConcatenationRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
