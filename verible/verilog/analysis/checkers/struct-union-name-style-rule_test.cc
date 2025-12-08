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

#include "verible/verilog/analysis/checkers/struct-union-name-style-rule.h"

#include <initializer_list>
#include <string_view>

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

TEST(StructUnionNameStyleRuleTest, ConfigurationPass) {
  StructUnionNameStyleRule rule;
  absl::Status status;
  EXPECT_TRUE((status = rule.Configure("")).ok()) << status.message();
  EXPECT_TRUE((status = rule.Configure("exceptions:12B,121GW")).ok())
      << status.message();
  EXPECT_TRUE((status = rule.Configure("exceptions:B,GiW")).ok())
      << status.message();
}

TEST(StructUnionNameStyleRuleTest, ConfigurationFail) {
  StructUnionNameStyleRule rule;
  absl::Status status;
  EXPECT_FALSE((status = rule.Configure("bad_exceptions:,")).ok())
      << status.message();

  EXPECT_FALSE((status = rule.Configure("exceptions:,")).ok())
      << status.message();
  EXPECT_TRUE(
      absl::StrContains(status.message(), "at least one alphabetic character"));
  EXPECT_FALSE((status = rule.Configure("exceptions: 12B")).ok())
      << status.message();
  EXPECT_TRUE(absl::StrContains(status.message(),
                                "digits and alphabetic characters only"));
  EXPECT_FALSE((status = rule.Configure("exceptions:12")).ok())
      << status.message();
  EXPECT_TRUE(
      absl::StrContains(status.message(), "at least one alphabetic character"));
  EXPECT_FALSE((status = rule.Configure("exceptions:GB12")).ok())
      << status.message();
  EXPECT_TRUE(
      absl::StrContains(status.message(), "after the unit are not allowed"));
  EXPECT_FALSE((status = rule.Configure("exceptions:12_B")).ok())
      << status.message();
  EXPECT_TRUE(absl::StrContains(status.message(),
                                "digits and alphabetic characters only"));
  EXPECT_FALSE((status = rule.Configure("exceptions:Gw,12")).ok())
      << status.message();
  EXPECT_TRUE(
      absl::StrContains(status.message(), "at least one alphabetic character"));
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
  const std::string_view exceptions = "exceptions:12B,11GiB,KJ,Kg";
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef struct {logic foo; logic bar;} b_a_z_t;"},
      {"typedef struct {logic foo; logic bar;} b_a_11gb_z_t;"},
      {"typedef struct {logic foo; logic bar;} baz_12_t;"},
      {"typedef struct {logic foo; logic bar;} baz_12B_t;"},
      {"typedef struct {logic foo; logic bar;} baz_11GiB_t;"},
      {"typedef struct {logic foo; logic bar;} good_14KJ_name_t;"},
      {"typedef struct {logic foo; logic bar;} good_10Kg_name_t;"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(
      kTestCases, exceptions);
}

TEST(StructUnionNameStyleRuleTestConfigured, InvalidStructNames) {
  const std::string_view exceptions = "exceptions:12B,KJ,Kg,t";
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef struct {logic foo; logic bar;}", {kToken, "baz_12B_"}, ";"},
      {"typedef struct {logic foo; logic bar;}", {kToken, "_baz_12B_t"}, ";"},
      {"typedef struct {logic foo; logic bar;}", {kToken, "baz_B12_t"}, ";"},
      {"typedef struct {logic foo; logic bar;}", {kToken, "b_a_KJ_z_t"}, ";"},
      {"typedef struct {logic foo; logic bar;}", {kToken, "b_a_10KG_z_t"}, ";"},
      {"typedef struct {logic foo; logic bar;}",
       {kToken, "b_a_10Kg10_z_t"},
       ";"},
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
  const std::string_view exceptions = "exceptions:12B,11GiB,KJ,Kg";
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef union {logic foo; logic bar;} b_a_z_t;"},
      {"typedef union {logic foo; logic bar;} b_a_11gb_z_t;"},
      {"typedef union {logic foo; logic bar;} baz_12_t;"},
      {"typedef union {logic foo; logic bar;} baz_12B_t;"},
      {"typedef union {logic foo; logic bar;} baz_11GiB_t;"},
      {"typedef union {logic foo; logic bar;} good_14KJ_name_t;"},
      {"typedef union {logic foo; logic bar;} good_10Kg_name_t;"},
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
  const std::string_view exceptions = "exceptions:12B,KJ,Kg,t";
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef union {logic foo; logic bar;}", {kToken, "baz_12B_"}, ";"},
      {"typedef union {logic foo; logic bar;}", {kToken, "_baz_12B_t"}, ";"},
      {"typedef union {logic foo; logic bar;}", {kToken, "baz_B12_t"}, ";"},
      {"typedef union {logic foo; logic bar;}", {kToken, "b_a_KJ_z_t"}, ";"},
      {"typedef union {logic foo; logic bar;}", {kToken, "b_a_10KG_z_t"}, ";"},
      {"typedef union {logic foo; logic bar;}",
       {kToken, "b_a_10Kg10_z_t"},
       ";"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(
      kTestCases, exceptions);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
