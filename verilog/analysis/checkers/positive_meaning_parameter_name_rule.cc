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

#include "verilog/analysis/checkers/positive_meaning_parameter_name_rule.h"

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

// Register PositiveMeaningParameterNameRule.
VERILOG_REGISTER_LINT_RULE(PositiveMeaningParameterNameRule);

absl::string_view PositiveMeaningParameterNameRule::Name() {
  return "positive-meaning-parameter-name";
}
const char PositiveMeaningParameterNameRule::kTopic[] = "binary-parameters";

const char PositiveMeaningParameterNameRule::kMessage[] =
    "Use positive naming for parameters, start the name with 'enable' instead.";

std::string PositiveMeaningParameterNameRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that no parameter name starts with 'disable', using positive "
      "naming (starting with 'enable') is recommended. See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

void PositiveMeaningParameterNameRule::HandleSymbol(
    const verible::Symbol& symbol, const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    if (IsParamTypeDeclaration(symbol)) return;

    auto identifiers = GetAllParameterNameTokens(symbol);
    for (const auto& id : identifiers) {
      const auto param_name = id->text();

      if (absl::StartsWithIgnoreCase(param_name, "disable"))
        violations_.insert(LintViolation(
            *id, absl::StrCat(kMessage, "  (got: ", param_name, ")"), context));
    }
  }
}

LintRuleStatus PositiveMeaningParameterNameRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
