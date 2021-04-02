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

#include "absl/strings/match.h"
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

TEST(InterfaceNameStyleRuleTestRegex, ConfigurationPass) {
  InterfaceNameStyleRule rule;
  absl::Status status;
  EXPECT_TRUE((status = rule.Configure("name_regex:[a-z]")).ok())
      << status.message();
}

TEST(InterfaceNameStyleRuleTestRegex, ConfigurationFail) {
  InterfaceNameStyleRule rule;
  absl::Status status;
  EXPECT_FALSE((status = rule.Configure("bad_name_regex:")).ok())
      << status.message();

  EXPECT_FALSE((status = rule.Configure("name_regex:[a-z")).ok())
      << status.message();
  EXPECT_TRUE(absl::StrContains(status.message(), "Invalid regex specified"));
}

TEST(InterfaceNameStyleRuleTestConfiguredRegex,
     ValidInterfaceDeclarationNames) {
  const absl::string_view regex = "name_regex:.*_i";
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"interface foo_i; endinterface"},
      {"interface _foo_i; endinterface"},
      {"interface foo12_i; endinterface"},
      {"interface good_12W_name_i; endinterface"},
      {"typedef virtual interface foo foo_i;"},
      {"typedef virtual interface foo fOO_i;"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, InterfaceNameStyleRule>(
      kTestCases, regex);
}

TEST(InterfaceNameStyleRuleTestConfiguredRegex,
     InvalidInterfaceDeclarationNames) {
  const absl::string_view regex = "name_regex:[a-zA-Z_]*_i";
  constexpr int kToken = SymbolIdentifier;
  const std::initializer_list<LintTestCase> kTestCases = {
      {""},
      {"interface ", {kToken, "baz_12_fOo_i"}, "; endinterface"},
      {"interface ", {kToken, "baz_fOo_t"}, "; endinterface"},
  };
  RunConfiguredLintTestCases<VerilogAnalyzer, InterfaceNameStyleRule>(
      kTestCases, regex);
}

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

}  // namespace
}  // namespace analysis
}  // namespace verilog
