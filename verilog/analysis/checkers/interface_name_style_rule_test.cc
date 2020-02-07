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

#include "verilog/analysis/checkers/interface_name_style_rule.h"

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

TEST(InterfaceNameStyleRuleTest, ValidInterfaceDeclarationNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"interface foo_if; endinterface"},
      {"interface good_name_if; endinterface"},
      {"interface b_a_z_if; endinterface"},
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

TEST(InterfaceNameStyleRuleTest, ValidInterfaceTypeNames) {
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"typedef virtual interface foo foo_if;"},
      {"typedef virtual interface foo good_name_if;"},
      {"typedef virtual interface foo b_a_z_if;"},
      {"typedef virtual foo foo_if;"},
      {"typedef virtual foo good_name_if;"},
      {"typedef virtual foo b_a_z_if;"},
  };
  RunLintTestCases<VerilogAnalyzer, InterfaceNameStyleRule>(kTestCases);
}

TEST(InterfaceNameStyleRuleTest, InvalidInterfaceTypeNames) {
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {"typedef virtual interface foo ", {kToken, "HelloWorld"}, ";"},
      {"typedef virtual interface foo ", {kToken, "_baz"}, ";"},
      {"typedef virtual interface foo ", {kToken, "Bad_name"}, ";"},
      {"typedef virtual interface foo ", {kToken, "bad_Name"}, ";"},
      {"typedef virtual interface foo ", {kToken, "Bad2"}, ";"},
      {"typedef virtual interface foo ", {kToken, "very_Bad_name"}, ";"},
      {"typedef virtual interface foo ", {kToken, "wrong_ending"}, ";"},
      {"typedef virtual interface foo ", {kToken, "_if"}, ";"},
      {"typedef virtual interface foo ", {kToken, "_i_f"}, ";"},
      {"typedef virtual interface foo ", {kToken, "i_f"}, ";"},
      {"typedef virtual interface foo ", {kToken, "_"}, ";"},
      {"typedef virtual interface foo ", {kToken, "foo_"}, ";"},
      {"typedef virtual foo ", {kToken, "HelloWorld"}, ";"},
      {"typedef virtual foo ", {kToken, "_baz"}, ";"},
      {"typedef virtual foo ", {kToken, "Bad_name"}, ";"},
      {"typedef virtual foo ", {kToken, "bad_Name"}, ";"},
      {"typedef virtual foo ", {kToken, "Bad2"}, ";"},
      {"typedef virtual foo ", {kToken, "very_Bad_name"}, ";"},
      {"typedef virtual foo ", {kToken, "wrong_ending"}, ";"},
      {"typedef virtual foo ", {kToken, "_if"}, ";"},
      {"typedef virtual foo ", {kToken, "_i_f"}, ";"},
      {"typedef virtual foo ", {kToken, "i_f"}, ";"},
      {"typedef virtual foo ", {kToken, "_"}, ";"},
      {"typedef virtual foo ", {kToken, "foo_"}, ";"},
  };
  RunLintTestCases<VerilogAnalyzer, InterfaceNameStyleRule>(kTestCases);
}

}  // namespace
}  // namespace analysis
}  // namespace verilog
