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

#include "verible/verilog/analysis/checkers/forbidden-anonymous-enums-rule.h"

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

// Tests that properly typedef'ed enum passes.
TEST(ForbiddenAnonymousEnumsTest, AcceptsTypedefedEnums) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef enum { OneValue, TwoValue } my_name_e;\nmy_name_e a_instance;"},
      {"typedef enum logic [1:0] { Fir, Oak, Pine } tree_e;\ntree_e a_tree;"},
      {"typedef enum { Red=3, Green=5 } state_e;\nstate_e a_state;"},
      {"typedef // We declare a type here"
       "enum { Idle, Busy } status_e;\nstatus_e a_status;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousEnumsRule>(kTestCases);
}

// Tests that anonymous enums are detected
TEST(ForbiddenAnonymousEnumsTest, RejectsAnonymousEnums) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_enum, "enum"}, " { OneValue, TwoValue } a_instance;"},
      {{TK_enum, "enum"}, " logic [1:0] { Fir, Oak, Pine, Larch } tree;"},
      {{TK_enum, "enum"}, " { Red=3, Green=5 } state;"},
      {{TK_enum, "enum"}, " { Idle, Busy } status;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousEnumsRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
