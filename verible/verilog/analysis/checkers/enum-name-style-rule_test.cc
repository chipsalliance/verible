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

#include "verible/verilog/analysis/checkers/enum-name-style-rule.h"

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
using verible::RunConfiguredLintTestCases;
using verible::RunLintTestCases;

TEST(EnumNameStyleRuleTest, ValidEnumNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef enum baz_t;"},
      {"typedef enum good_name_t;"},
      {"typedef enum b_a_z_t;"},
      {"typedef enum baz_e;"},
      {"typedef enum good_name_e;"},
      {"typedef enum b_a_z_e;"},
      {"typedef enum { OneValue, TwoValue } my_name_e;\nmy_name_e a_instance;"},
      {"typedef enum logic [1:0] { Fir, Oak, Pine } tree_e;\ntree_e a_tree;"},
      {"typedef enum { Red=3, Green=5 } state_e;\nstate_e a_state;"},
      {"typedef // We declare a type here"
       "enum { Idle, Busy } status_e;\nstatus_e a_status;"},
      {"typedef enum { OneValue, TwoValue } my_name_t;\nmy_name_t a_instance;"},
      {"typedef enum logic [1:0] { Fir, Oak, Pine } tree_t;\ntree_t a_tree;"},
      {"typedef enum { Red=3, Green=5 } state_t;\nstate_t a_state;"},
      {"typedef // We declare a type here"
       "enum { Idle, Busy } status_t;\nstatus_t a_status;"},
      // Declarations inside a class
      {"class foo;\n"
       "typedef enum { Red=3, Green=5 } state_e;\n"
       "state_e a_state;\n"
       "endclass"},
      {"class foo;\n"
       "typedef enum logic [1:0] { Fir, Oak, Pine } tree_t;\n"
       "tree_t a_tree;\n"
       "endclass"},
      // Declarations inside a module
      {"module foo;\n"
       "typedef enum { Red=3, Green=5 } state_e;\n"
       "state_e a_state;\n"
       "endmodule"},
      {"module foo;\n"
       "typedef enum logic [1:0] { Fir, Oak, Pine } tree_t;\n"
       "tree_t a_tree;\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, EnumNameStyleRule>(kTestCases);
}

TEST(EnumNameStyleRuleTest, InvalidEnumNames) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef enum ", {kToken, "HelloWorld"}, ";"},
      {"typedef enum ", {kToken, "_baz"}, ";"},
      {"typedef enum ", {kToken, "Bad_name"}, ";"},
      {"typedef enum ", {kToken, "bad_Name"}, ";"},
      {"typedef enum ", {kToken, "Bad2"}, ";"},
      {"typedef enum ", {kToken, "very_Bad_name"}, ";"},
      {"typedef enum ", {kToken, "wrong_ending"}, ";"},
      {"typedef enum ", {kToken, "almost_righ_T"}, ";"},
      {"typedef enum ", {kToken, "a_nam_E"}, ";"},
      {"typedef enum ", {kToken, "_t"}, ";"},
      {"typedef enum ", {kToken, "t"}, ";"},
      {"typedef enum ", {kToken, "_e"}, ";"},
      {"typedef enum ", {kToken, "e"}, ";"},
      {"typedef enum ", {kToken, "_"}, ";"},
      {"typedef enum ", {kToken, "foo_"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "HelloWorld"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "_baz"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "Bad_name"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "bad_Name"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "Bad2"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "very_Bad_name"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "wrong_ending"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "_t"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "t"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "_e"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "e"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "_T"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "T"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "_E"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "E"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "_"}, ";"},
      {"typedef enum {foo, bar} ", {kToken, "foo_"}, ";"},
      // Declarations inside a class
      {"class foo;\n"
       "typedef enum {foo, bar} ",
       {kToken, "HelloWorld"},
       ";\n"
       "HelloWorld hi;\n"
       "endclass"},
      {"class foo;\n"
       "typedef enum {bar, baz} ",
       {kToken, "bad_"},
       ";\n"
       "bad_ hi;\n"
       "endclass"},
      // Declarations inside a module
      {"module foo;\n"
       "typedef enum {bar, baz} ",
       {kToken, "HelloWorld"},
       ";\n"
       "HelloWorld hi;\n"
       "endmodule"},
      {"module foo;\n"
       "typedef enum {bar, baz} ",
       {kToken, "bad_"},
       ";\n"
       "bad_ hi;\n"
       "endmodule"},
  };
  RunLintTestCases<VerilogAnalyzer, EnumNameStyleRule>(kTestCases);
}

TEST(EnumNameStyleRuleTest, UncheckedCases) {
  const std::initializer_list<LintTestCase> kTestCases = {
      // No name to check
      {"enum {foo, bar} baz;"},
      {"class foo;\n"
       "enum {foo, bar} baz;\n"
       "endclass"},
      {"module foo;\n"
       "enum {foo, bar} baz;\n"
       "endmodule"},
      // Struct and unions typedefs should not be checked here
      {"typedef struct {logic foo; logic bar;} baz_t;"},
      {"typedef union {logic [8:0] foo; int bar;} baz_t;"},
      {"typedef struct {logic foo; logic bar;} badStruct;"},
      {"typedef union {logic [8:0] foo; int bar;} invalid_Union_name;"},
  };
  RunLintTestCases<VerilogAnalyzer, EnumNameStyleRule>(kTestCases);
}

TEST(EnumNameStyleRuleTest, UpperSnakeCaseTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {""},
      {"typedef enum BAZ_T;"},
      {"typedef enum GOOD_NAME_T;"},
      {"typedef enum B_A_Z_T;"},
      {"typedef enum BAZ_E;"},
      {"typedef enum GOOD_NAME_E;"},
      {"typedef enum B_A_Z_E;"},
      {"typedef enum { OneValue, TwoValue } MY_NAME_E;\nmy_name_e a_instance;"},
      {"typedef enum logic [1:0] { Fir, Oak, Pine } TREE_E;\ntree_e a_tree;"},
      {"typedef enum { Red=3, Green=5 } STATE_E;\nstate_e a_state;"},
      {"typedef // We declare a type here"
       "enum { Idle, Busy } STATUS_E;\nstatus_e a_status;"},
      {"typedef enum { OneValue, TwoValue } MY_NAME_T;\nmy_name_t a_instance;"},
      {"typedef enum logic [1:0] { Fir, Oak, Pine } TREE_T;\ntree_t a_tree;"},
      {"typedef enum { Red=3, Green=5 } STATE_T;\nstate_t a_state;"},
      {"typedef // We declare a type here"
       "enum { Idle, Busy } STATUS_T;\nstatus_t a_status;"},
      // Declarations inside a class
      {"class foo;\n"
       "typedef enum { Red=3, Green=5 } STATE_E;\n"
       "state_e a_state;\n"
       "endclass"},
      {"class foo;\n"
       "typedef enum logic [1:0] { Fir, Oak, Pine } TREE_T;\n"
       "tree_t a_tree;\n"
       "endclass"},
      // Declarations inside a module
      {"module foo;\n"
       "typedef enum { Red=3, Green=5 } STATE_E;\n"
       "state_e a_state;\n"
       "endmodule"},
      {"module foo;\n"
       "typedef enum logic [1:0] { Fir, Oak, Pine } TREE_T;\n"
       "tree_t a_tree;\n"
       "endmodule"},
      {"typedef enum ", {kToken, "HelloWorld"}, ";"},
      {"typedef enum ", {kToken, "_baz"}, ";"},
      {"typedef enum ", {kToken, "Bad_name"}, ";"},
      {"typedef enum ", {kToken, "bad_Name"}, ";"},
      {"typedef enum ", {kToken, "Bad2"}, ";"},

  };
  RunConfiguredLintTestCases<VerilogAnalyzer, EnumNameStyleRule>(
      kTestCases, "style_regex:[A-Z_0-9]+(_T|_E)");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
