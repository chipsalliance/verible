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

#include "verilog/analysis/checkers/parameter_name_style_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/strings/naming_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "verilog/CST/parameters.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;

// Register ParameterNameStyleRule.
VERILOG_REGISTER_LINT_RULE(ParameterNameStyleRule);

absl::string_view ParameterNameStyleRule::Name() {
  return "parameter-name-style";
}
const char ParameterNameStyleRule::kTopic[] = "constants";
const char ParameterNameStyleRule::kParameterMessage[] =
    "Parameter names must be styled with UpperCamelCase or ALL_CAPS.";
const char ParameterNameStyleRule::kLocalParamMessage[] =
    "Localparam names must be styled with UpperCamelCase.";

std::string ParameterNameStyleRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that parameter names follow UpperCamelCase or ALL_CAPS naming "
      "convention and that localparam names follow UpperCamelCase naming "
      "convention. See ",
      GetStyleGuideCitation(kTopic), ".");
}

void ParameterNameStyleRule::HandleSymbol(const verible::Symbol& symbol,
                                          const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (matcher_.Matches(symbol, &manager)) {
    const auto param_decl_token = GetParamKeyword(symbol);

    const verible::TokenInfo* param_name_token = nullptr;
    if (IsParamTypeDeclaration(symbol))
      param_name_token = &GetSymbolIdentifierFromParamDeclaration(symbol);
    else
      param_name_token = &GetParameterNameToken(symbol);

    const auto param_name = param_name_token->text;
    if (param_decl_token == TK_localparam) {
      if (!verible::IsUpperCamelCaseWithDigits(param_name))
        violations_.insert(
            LintViolation(*param_name_token, kLocalParamMessage, context));
    } else if (param_decl_token == TK_parameter) {
      if (!verible::IsUpperCamelCaseWithDigits(param_name) &&
          !verible::IsNameAllCapsUnderscoresDigits(param_name))
        violations_.insert(
            LintViolation(*param_name_token, kParameterMessage, context));
    }
  }
}

LintRuleStatus ParameterNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
