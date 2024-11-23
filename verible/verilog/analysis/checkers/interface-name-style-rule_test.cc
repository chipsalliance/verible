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

#include "verible/verilog/analysis/checkers/interface-name-style-rule.h"

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

TEST(InterfaceNameStyleRuleTest, ValidInterfaceDeclarationNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"interface foo_if; endinterface"},
      {"interface good_name_if; endinterface"},
      {"interface b_a_z_if; endinterface"},

      // Should not apply to typedefs of virtual interfaces
      {"typedef virtual interface foo foo_if;"},
      {"typedef virtual interface foo good_name_if;"},
      {"typedef virtual interface foo b_a_z_if;"},
      {"typedef virtual foo foo_if;"},
      {"typedef virtual foo good_name_if;"},
      {"typedef virtual foo b_a_z_if;"},
      {"typedef virtual interface foo HelloWorld;"},
      {"typedef virtual interface foo _baz;"},
      {"typedef virtual interface foo Bad_name;"},
      {"typedef virtual interface foo bad_Name;"},
      {"typedef virtual interface foo Bad2;"},
      {"typedef virtual interface foo very_Bad_name;"},
      {"typedef virtual interface foo wrong_ending;"},
      {"typedef virtual interface foo _if;"},
      {"typedef virtual interface foo _i_f;"},
      {"typedef virtual interface foo i_f;"},
      {"typedef virtual interface foo _;"},
      {"typedef virtual interface foo foo_;"},
      {"typedef virtual foo HelloWorld;"},
      {"typedef virtual foo _baz;"},
      {"typedef virtual foo Bad_name;"},
      {"typedef virtual foo bad_Name;"},
      {"typedef virtual foo Bad2;"},
      {"typedef virtual foo very_Bad_name;"},
      {"typedef virtual foo wrong_ending;"},
      {"typedef virtual foo _if;"},
      {"typedef virtual foo _i_f;"},
      {"typedef virtual foo i_f;"},
      {"typedef virtual foo _;"},
      {"typedef virtual foo foo_;"},
  };
  RunLintTestCases<VerilogAnalyzer, InterfaceNameStyleRule>(kTestCases);
}

TEST(InterfaceNameStyleRuleTest, InvalidInterfaceDeclarationNames) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"interface ", {kToken, "HelloWorld"}, "; endinterface"},
      {"interface ", {kToken, "_baz"}, "; endinterface"},
      {"interface ", {kToken, "Bad_name"}, "; endinterface"},
      {"interface ", {kToken, "bad_Name"}, "; endinterface"},
      {"interface ", {kToken, "Bad2"}, "; endinterface"},
      {"interface ", {kToken, "very_Bad_name"}, "; endinterface"},
      {"interface ", {kToken, "wrong_ending"}, "; endinterface"},
      {"interface ", {kToken, "_if"}, "; endinterface"},
      {"interface ", {kToken, "_i_f"}, "; endinterface"},
      {"interface ", {kToken, "i_f"}, "; endinterface"},
      {"interface ", {kToken, "_"}, "; endinterface"},
      {"interface ", {kToken, "foo_"}, "; endinterface"},
  };
  RunLintTestCases<VerilogAnalyzer, InterfaceNameStyleRule>(kTestCases);
}

TEST(InterfaceNameStyleRuleTest, UpperSnakeCaseTests) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"interface FOO_IF; endinterface"},
      {"interface GOOD_NAME_IF; endinterface"},
      {"interface B_A_Z_IF; endinterface"},
      {"interface ", {kToken, "HelloWorld"}, "; endinterface"},
      {"interface ", {kToken, "_baz"}, "; endinterface"},
      {"interface ", {kToken, "Bad_name"}, "; endinterface"},
      {"interface ", {kToken, "bad_Name"}, "; endinterface"},
      {"interface ", {kToken, "Bad2"}, "; endinterface"},
      {"interface ", {kToken, "very_Bad_name"}, "; endinterface"},
      {"interface ", {kToken, "wrong_ending"}, "; endinterface"},
      {"interface ", {kToken, "_if"}, "; endinterface"},
      {"interface ", {kToken, "_i_f"}, "; endinterface"},
      {"interface ", {kToken, "i_f"}, "; endinterface"},
      {"interface ", {kToken, "_"}, "; endinterface"},
      {"interface ", {kToken, "foo_"}, "; endinterface"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, InterfaceNameStyleRule>(
      kTestCases, "style_regex:[A-Z_0-9]+(_IF)");
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
