// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/analysis/checkers/forbidden_anonymous_enums_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

// Tests that properly typedef'ed enum passes.
TEST(ForbiddenAnonymousEnumsTest, AcceptsTypedefedEnums) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef enum { OneValue, TwoValue } my_name_e;\nmy_name_e a_instance;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousEnumsRule>(kTestCases);
}

// Tests that anonymous enums are detected
TEST(ForbiddenAnonymousEnumsTest, RejectsAnonymousEnums) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_enum, "enum"}, " { OneValue, TwoValue } a_instance;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousEnumsRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
