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

#include "verilog/analysis/checkers/struct_union_name_style_rule.h"

#include <initializer_list>

#include "gtest/gtest.h"
#include "common/analysis/linter_test_utils.h"
#include "common/analysis/syntax_tree_linter_test_utils.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace analysis {
namespace {

using verible::LintTestCase;
using verible::RunLintTestCases;

TEST(StructUnionNameStyleRuleTest, ValidStructNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef struct {logic foo; logic bar;} baz_t;"},
      {"typedef struct {logic foo; logic bar;} good_name_t;"},
      {"typedef struct {logic foo; logic bar;} b_a_z_t;"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

TEST(StructUnionNameStyleRuleTest, InvalidStructNames) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef struct {logic foo; logic bar;} ", {kToken, "HelloWorld"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "_baz"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "Bad_name"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "bad_Name"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "Bad2"}, ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "very_Bad_name"},
       ";"},
      {"typedef struct {logic foo; logic bar;} ", {kToken, "wrong_ending"},
       ";"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

TEST(StructUnionNameStyleRuleTest, ValidUnionNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef union {logic [8:0] foo; int bar;} baz_t;"},
      {"typedef union {logic [8:0] foo; int bar;} good_name_t;"},
      {"typedef union {logic [8:0] foo; int bar;} b_a_z_t;"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

TEST(StructUnionNameStyleRuleTest, InvalidUnionNames) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "HelloWorld"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "_baz"}, ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "Bad_name"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "bad_Name"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "Bad2"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "very_Bad_name"},
       ";"},
      {"typedef union {logic [8:0] foo; int bar;} ", {kToken, "wrong_ending"},
       ";"},
  };
  RunLintTestCases<VerilogAnalyzer, StructUnionNameStyleRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
