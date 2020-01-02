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

#include "verilog/analysis/checkers/forbidden_anonymous_structs_unions_rule.h"

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

// Tests that properly typedef'ed struct passes.
TEST(ForbiddenAnonymousStructsUnionsTest, AcceptsTypedefedStructs) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef struct {bit [8:0] op, arg1, arg2;} cmd;\ncmd a_cmd;"},
      {"typedef struct {byte a; reg b;} custom;\ncustom a_struct;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousStructsUnionsRule>(
      kTestCases);
}

// Tests that anonymous structs are detected
TEST(ForbiddenAnonymousStructsUnionsTest, RejectsAnonymousStructs) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_struct, "struct"}, " {bit [8:0] op, arg1, arg2;} cmd;"},
      {{TK_struct, "struct"}, " {byte a; reg b;} custom;\ncustom a_struct;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousStructsUnionsRule>(
      kTestCases);
}

// Tests that properly typedef'ed union passes.
TEST(ForbiddenAnonymousStructsUnionsTest, AcceptsTypedefedUnions) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef union {logic [8:0] arr; int status;} obj;\nobj a_obj;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousStructsUnionsRule>(
      kTestCases);
}

// Tests that anonymous unions are detected
TEST(ForbiddenAnonymousStructsUnionsTest, RejectsAnonymousUnions) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {{TK_union, "union"}, " {bit [8:0] flags; int val;} result;"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousStructsUnionsRule>(
      kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
