// Copyright 2017-2023 The Verible Authors.
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

#include "verible/verilog/analysis/checkers/case-missing-default-rule.h"

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

// Tests that the expected number of case-missing-default-rule violations are
// found for basic tests.
TEST(CaseMissingDefaultRuleTest, BasicTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
  };

  RunLintTestCases<VerilogAnalyzer, CaseMissingDefaultRule>(kTestCases);
}

// Tests that the expected number of case-missing-default-rule violations inside
// functions are found.
TEST(CaseMissingDefaultRuleTest, CaseInsideFunctionTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // case tests
      {R"(
       function automatic int foo (input in);
         case (in)
           default: return 0;
         endcase
       endfunction
       )"},
      {R"(
       function automatic int foo (input in);
         )",
       {TK_case, "case"},
       R"( (in)
           1: return 0;
         endcase
       endfunction
       )"},

      // casex tests
      {R"(
       function automatic int foo (input in);
         casex (in)
           default: return 0;
         endcase
       endfunction
       )"},
      {R"(
       function automatic int foo (input in);
         )",
       {TK_casex, "casex"},
       R"( (in)
           1: return 0;
         endcase
       endfunction
       )"},

      // casez tests
      {R"(
       function automatic int foo (input in);
         casez (in)
           default: return 0;
         endcase
       endfunction
       )"},
      {R"(
       function automatic int foo (input in);
         )",
       {TK_casez, "casez"},
       R"( (in)
           1: return 0;
         endcase
       endfunction
       )"},

      // randcase should not be flagged
      {R"(
       function automatic int foo (input in);
         randcase
           3: return 3;
           4: return 4;
           5: return 5;
         endcase
       endfunction
       )"},
  };

  RunLintTestCases<VerilogAnalyzer, CaseMissingDefaultRule>(kTestCases);
}

// Tests that the expected number of case-missing-default-rule violations inside
// tasks are found.
TEST(CaseMissingDefaultRuleTest, CaseInsideTaskTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // case tests
      {R"(
       task automatic foo (input in);
         case (in)
           default: return 0;
         endcase
       endtask
       )"},
      {R"(
       task automatic foo (input in);
         )",
       {TK_case, "case"},
       R"( (in)
           1: return 0;
         endcase
       endtask
       )"},

      // casex tests
      {R"(
       task automatic foo (input in);
         casex (in)
           default: return 0;
         endcase
       endtask
       )"},
      {R"(
       task automatic foo (input in);
         )",
       {TK_casex, "casex"},
       R"( (in)
           1: return 0;
         endcase
       endtask
       )"},

      // casez tests
      {R"(
       task automatic foo (input in);
         casez (in)
           default: return 0;
         endcase
       endtask
       )"},
      {R"(
       task automatic foo (input in);
         )",
       {TK_casez, "casez"},
       R"( (in)
           1: return 0;
         endcase
       endtask
       )"},

      // randcase should not be flagged
      {R"(
       task automatic foo (input in);
         randcase
           3: return 3;
           4: return 4;
           5: return 5;
         endcase
       endtask
       )"},
  };

  RunLintTestCases<VerilogAnalyzer, CaseMissingDefaultRule>(kTestCases);
}

// Tests that the expected number of case-missing-default-rule violations are
// found within nested cases inside functions.
TEST(CaseMissingDefaultRuleTest, NestedCaseInsideFunctionTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Inner case doesn't have default, outer case does.
      {R"(
       function automatic foo (input in);
         case (in)
           1: begin;
             )",
       {TK_case, "case"},
       R"( (in)
               1: return 1;
             endcase
           end
           default: return 1;
         endcase
       endfunction
       )"},

      // Inner case does have default, outer case doesn't.
      {R"(
       function automatic foo (input in);
         )",
       {TK_case, "case"},
       R"( (in)
           1: begin;
             case (in)
               1: return 1;
               default: return 1;
             endcase
           end
         endcase
       endfunction
       )"},

      // Both inner and outer case have default case statements.
      {R"(
       function automatic foo (input in);
         case (in)
           1: begin;
             case (in)
               1: return 1;
               default: return 1;
             endcase
           end
           default: return 1;
         endcase
       endfunction
       )"},

      // Neither of the cases have default case statements.
      {R"(
       function automatic foo (input in);
         )",
       {TK_case, "case"},
       R"( (in)
           1: begin;
             )",
       {TK_case, "case"},
       R"( (in)
               1: return 1;
             endcase
           end
         endcase
       endfunction
       )"},
  };

  RunLintTestCases<VerilogAnalyzer, CaseMissingDefaultRule>(kTestCases);
}

// Tests that the expected number of case-missing-default-rule violations are
// found within nested cases inside tasks.
TEST(CaseMissingDefaultRuleTest, NestedCaseInsideTaskTests) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // Inner case doesn't have default, outer case does.
      {R"(
       task automatic foo (input in);
         case (in)
           1: begin;
             )",
       {TK_case, "case"},
       R"( (in)
               1: return 1;
             endcase
           end
           default: return 1;
         endcase
       endtask
       )"},

      // Inner case does have default, outer case doesn't.
      {R"(
       task automatic foo (input in);
         )",
       {TK_case, "case"},
       R"( (in)
           1: begin;
             case (in)
               1: return 1;
               default: return 1;
             endcase
           end
         endcase
       endtask
       )"},

      // Both inner and outer case have default case statements.
      {R"(
       task automatic foo (input in);
         case (in)
           1: begin;
             case (in)
               1: return 1;
               default: return 1;
             endcase
           end
           default: return 1;
         endcase
       endtask
       )"},

      // Neither of the cases have default case statements.
      {R"(
       task automatic foo (input in);
         )",
       {TK_case, "case"},
       R"( (in)
           1: begin;
             )",
       {TK_case, "case"},
       R"( (in)
               1: return 1;
             endcase
           end
         endcase
       endtask
       )"},
  };

  RunLintTestCases<VerilogAnalyzer, CaseMissingDefaultRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
