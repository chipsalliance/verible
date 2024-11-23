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

#include "verible/verilog/analysis/checkers/forbidden-anonymous-structs-unions-rule.h"

#include <initializer_list>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/linter-test-utils.h"
#include "verible/common/analysis/syntax-tree-linter-test-utils.h"
#include "verible/verilog/analysis/verilog-analyzer.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(ForbiddenAnonymousStructsUnionsTest, Configuration) {
  ForbiddenAnonymousStructsUnionsRule rule;
  absl::Status s;
  EXPECT_TRUE((s = rule.Configure("")).ok()) << s.message();
  EXPECT_TRUE((s = rule.Configure("allow_anonymous_nested")).ok())
      << s.message();
  EXPECT_TRUE((s = rule.Configure("allow_anonymous_nested:true")).ok())
      << s.message();
  EXPECT_TRUE((s = rule.Configure("allow_anonymous_nested:false")).ok())
      << s.message();

  EXPECT_FALSE((s = rule.Configure("foo")).ok());
  EXPECT_TRUE(absl::StrContains(s.message(), "supported parameter"));

  EXPECT_FALSE((s = rule.Configure("allow_anonymous_nested:bogus")).ok());
  EXPECT_TRUE(absl::StrContains(s.message(), "Boolean value should be one of"));
}

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
      {"module m #(parameter type T = ",
       {TK_struct, "struct"},
       " packed { int i; }) (); endmodule\n"},
      {"module nn; m #(.T(",
       {TK_struct, "struct"},
       " packed { int i;})) mm; endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousStructsUnionsRule>(
      kTestCases);
}

TEST(ForbiddenAnonymousStructsUnionsTest,
     AcceptInnerAnonymousStructsIfConfigured) {
  {
    // Without waived nested configuration we expect complains at
    // every anonymous struct/union, inside or nested.
    const std::initializer_list<LintTestCase> kTestCases = {
        // outer and inner struct/union is complained about.
        {{TK_struct, "struct"},
         " {byte a;",
         {TK_struct, "struct"},
         " { byte x, y;} b;} z;"},
        {{TK_union, "union"},
         " {byte a;",
         {TK_struct, "struct"},
         " { byte x, y;} b;} z;"},
        {"typedef struct {byte a;",
         {TK_struct, "struct"},
         " { byte x, y;} b;} foo_t;\nfoo_t z;"},
        {"typedef struct {byte a;",
         {TK_union, "union"},
         " { byte x, y;} b;} foo_t;\nfoo_t z;"},
        {"typedef union {byte a;",
         {TK_union, "union"},
         " { byte x, y;} b;} foo_t;\nfoo_t z;"},
    };
    for (const auto not_waived_cfg : {"", "allow_anonymous_nested:false"}) {
      RunConfiguredLintTestCases<VerilogAnalyzer,
                                 ForbiddenAnonymousStructsUnionsRule>(
          kTestCases, not_waived_cfg);
    }
  }

  {
    // With waived nested configuration, lint will only complain about outer
    // anonymous struct/unions.
    const std::initializer_list<LintTestCase> kTestCases = {
        // Just outer struct/union is complained about, but anonymous inner ok
        {{TK_struct, "struct"}, " {byte a; struct { byte x, y;} b;} z;"},
        {{TK_union, "union"}, " {byte a; struct { byte x, y;} b;} z;"},
        // Here we are entirely happy: outside with typedef, inner w/o fine.
        {"typedef struct {byte a; struct { byte x, y;} b;} foo_t;\nfoo_t z;"},
        {"typedef struct {byte a; union { byte x, y;} b;} foo_t;\nfoo_t z;"},
        {"typedef union {byte a; union { byte x, y;} b;} foo_t;\nfoo_t z;"},
    };
    for (const auto waived_cfg :
         {"allow_anonymous_nested", "allow_anonymous_nested:true"}) {
      RunConfiguredLintTestCases<VerilogAnalyzer,
                                 ForbiddenAnonymousStructsUnionsRule>(
          kTestCases, waived_cfg);
    }
  }
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
      {"module m #(parameter type T = ",
       {TK_union, "union"},
       " packed { int i; }) (); endmodule\n"},
      {"module nn; m #(.T(",
       {TK_union, "union"},
       " packed { int i;})) mm; endmodule\n"},
  };
  RunLintTestCases<VerilogAnalyzer, ForbiddenAnonymousStructsUnionsRule>(
      kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
