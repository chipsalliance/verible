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

#include <set>
#include <string_view>

#include "absl/strings/match.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "verilog/CST/module.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(InterfaceNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

static constexpr std::string_view kMessage =
    "Interface names must use lower_snake_case naming convention "
    "and end with _if.";

const LintRuleDescriptor& InterfaceNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "interface-name-style",
      .topic = "interface-conventions",
      .desc =
          "Checks that `interface` names use lower_snake_case "
          "naming convention and end with `_if`.",
  };
  return d;
}

static const Matcher& InterfaceMatcher() {
  static const Matcher matcher(NodekInterfaceDeclaration());
  return matcher;
}

void InterfaceNameStyleRule::HandleSymbol(const verible::Symbol& symbol,
                                          const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  std::string_view name;
  const verible::TokenInfo* identifier_token;
  if (InterfaceMatcher().Matches(symbol, &manager)) {
    identifier_token = GetInterfaceNameToken(symbol);
    name = identifier_token->text();

    if (!verible::IsLowerSnakeCaseWithDigits(name) ||
        !absl::EndsWith(name, "_if")) {
      violations_.insert(LintViolation(*identifier_token, kMessage, context));
    }
  }
}

LintRuleStatus InterfaceNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
