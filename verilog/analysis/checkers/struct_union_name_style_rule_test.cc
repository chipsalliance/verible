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

#include "verilog/analysis/checkers/struct_union_name_style_rule.h"

#include <initializer_list>

#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "gtest/gtest.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(StructUnionNameStyleRuleTest, Configuration) {
  StructUnionNameStyleRule rule;
  absl::Status status;
  EXPECT_TRUE((status = rule.Configure("")).ok()) << status.message();
  EXPECT_TRUE((status = rule.Configure("exceptions:12B,121GW")).ok())
      << status.message();
}

TEST(StructUnionNameStyleRuleTest, ValidStructNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef struct baz_t;"},
      {"typedef struct good_name_t;"},
      {"typedef struct b_a_z_t;"},
      {"typedef struct {logic foo; logic bar;} baz_t;"},
      {"typedef struct {logic foo; logic bar;} good_name_t;"},
      {"typedef struct {logic foo; logic bar;} b_a_z_t;"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

TEST(StructUnionNameStyleRuleTestConfigured, ValidStructNames) {
  const absl::string_view exceptions = "exceptions:12B,11GB,14kJ,B10,t";
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef struct baz_t;"},
      {"typedef struct good_name_t;"},
      {"typedef struct b_a_z_t;"},
      {"typedef struct B10_14kJ_t;"},
      {"typedef struct {logic foo; logic bar;} baz_12B_t;"},
      {"typedef struct {logic foo; logic bar;} good_14kJ_name_t;"},
      {"typedef struct {logic foo; logic bar;} b_a_11GB_z_t;"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(
      kTestCases, exceptions);
}

TEST(StructUnionNameStyleRuleTestConfigured, InvalidStructNames) {
  const absl::string_view exceptions = "exceptions:12b,11,14Kj";
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef struct baz_t;"},
      {"typedef struct good_name_t;"},
      {"typedef struct b_a_z_t;"},
      {"typedef struct {logic foo; logic bar;}", {kToken, "baz_12B_t"}, ";"},
      {"typedef struct {logic foo; logic bar;}", {kToken, "_baz_12B_t"}, ";"},
      {"typedef struct {logic foo; logic bar;}",
       {kToken, "bad_14kJ_name_t"},
       ";"},
      {"typedef struct {logic foo; logic bar;}", {kToken, "b_a_11GB_z_t"}, ";"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(
      kTestCases, exceptions);
}

TEST(StructUnionNameStyleRuleTest, InvalidStructNames) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef struct ", {kToken, "HelloWorld"}, ";"},
      {"typedef struct ", {kToken, "_baz"}, ";"},
      {"typedef struct ", {kToken, "Bad_name"}, ";"},
      {"typedef struct ", {kToken, "bad_Name"}, ";"},
      {"typedef struct ", {kToken, "Bad2"}, ";"},
      {"typedef struct ", {kToken, "very_Bad_name"}, ";"},
      {"typedef struct ", {kToken, "wrong_ending"}, ";"},
      {"typedef struct ", {kToken, "_t"}, ";"},
      {"typedef struct ", {kToken, "t"}, ";"},
      {"typedef struct ", {kToken, "_"}, ";"},
      {"typedef struct ", {kToken, "foo_"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "HelloWorld"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "_baz"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "Bad_name"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "bad_Name"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "Bad2"}, ";"},
      {"typedef struct {logic foo; logic bar;} ",
       {kToken, "very_Bad_name"},
       ";"},
      {"typedef struct {logic foo; logic bar;} ",
       {kToken, "wrong_ending"},
       ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "_t"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "t"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "_"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "foo_"}, ";"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

TEST(StructUnionNameStyleRuleTest, ValidUnionNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef union baz_t;"},
      {"typedef union good_name_t;"},
      {"typedef union b_a_z_t;"},
      {"typedef union {logic [8:0] foo; int bar;} baz_t;"},
      {"typedef union {logic [8:0] foo; int bar;} good_name_t;"},
      {"typedef union {logic [8:0] foo; int bar;} b_a_z_t;"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

TEST(StructUnionNameStyleRuleTestConfigured, ValidUnionNames) {
  const absl::string_view exceptions = "exceptions:12B,11GB,14kJ,B10,t";
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef union baz_t;"},
      {"typedef union good_name_t;"},
      {"typedef union b_a_z_t;"},
      {"typedef union B10_14kJ_t;"},
      {"typedef union {logic foo; logic bar;} baz_12B_t;"},
      {"typedef union {logic foo; logic bar;} good_14kJ_name_t;"},
      {"typedef union {logic foo; logic bar;} b_a_11GB_z_t;"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(
      kTestCases, exceptions);
}

TEST(StructUnionNameStyleRuleTest, InvalidUnionNames) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef union ", {kToken, "HelloWorld"}, ";"},
      {"typedef union ", {kToken, "_baz"}, ";"},
      {"typedef union ", {kToken, "Bad_name"}, ";"},
      {"typedef union ", {kToken, "bad_Name"}, ";"},
      {"typedef union ", {kToken, "Bad2"}, ";"},
      {"typedef union ", {kToken, "very_Bad_name"}, ";"},
      {"typedef union ", {kToken, "wrong_ending"}, ";"},
      {"typedef union ", {kToken, "_t"}, ";"},
      {"typedef union ", {kToken, "t"}, ";"},
      {"typedef union ", {kToken, "_"}, ";"},
      {"typedef union ", {kToken, "foo_"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ",
       {kToken, "HelloWorld"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "_baz"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "Bad_name"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "bad_Name"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "Bad2"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ",
       {kToken, "very_Bad_name"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ",
       {kToken, "wrong_ending"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "_t"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "t"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "_"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "foo_"}, ";"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

TEST(StructUnionNameStyleRuleTestConfigured, InvalidUnionNames) {
  const absl::string_view exceptions = "exceptions:12b,11,14Kj,t";
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef union baz_t;"},
      {"typedef union good_name_t;"},
      {"typedef union b_a_z_t;"},
      {"typedef union {logic foo; logic bar;}", {kToken, "baz_12B_t"}, ";"},
      {"typedef union {logic foo; logic bar;}", {kToken, "_baz_12B_t"}, ";"},
      {"typedef union {logic foo; logic bar;}",
       {kToken, "bad_14kJ_name_t"},
       ";"},
      {"typedef union {logic foo; logic bar;}", {kToken, "b_a_11GB_z_t"}, ";"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(
      kTestCases, exceptions);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
