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

#include "verilog/analysis/checkers/parameter_type_name_style_rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "verilog/CST/parameters.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register ParameterTypeNameStyleRule.
VERILOG_REGISTER_LINT_RULE(ParameterTypeNameStyleRule);

absl::string_view ParameterTypeNameStyleRule::Name() {
  return "parameter-type-name-style";
}
const char ParameterTypeNameStyleRule::kTopic[] = "parametrized-objects";

const char ParameterTypeNameStyleRule::kMessage[] =
    "Parameter type names must use the lower_snake_case naming convention"
    " and end with _t.";

std::string ParameterTypeNameStyleRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that parameter type names follow the lower_snake_case naming "
      "convention and end with _t. See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

void ParameterTypeNameStyleRule::HandleSymbol(
    const verible::Symbol& symbol, const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    const verible::TokenInfo* param_name_token = nullptr;
    if (!IsParamTypeDeclaration(symbol)) return;

    param_name_token = &GetSymbolIdentifierFromParamDeclaration(symbol);
    const auto param_name = param_name_token->text();

    if (!verible::IsLowerSnakeCaseWithDigits(param_name) ||
        !absl::EndsWith(param_name, "_t"))
      violations_.insert(LintViolation(*param_name_token, kMessage, context));
  }
}

LintRuleStatus ParameterTypeNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
