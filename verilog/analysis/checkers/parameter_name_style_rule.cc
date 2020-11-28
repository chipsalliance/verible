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
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/config_utils.h"
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
using Matcher = verible::matcher::Matcher;

// Register ParameterNameStyleRule.
VERILOG_REGISTER_LINT_RULE(ParameterNameStyleRule);

absl::string_view ParameterNameStyleRule::Name() {
  return "parameter-name-style";
}
const char ParameterNameStyleRule::kTopic[] = "constants";

std::string ParameterNameStyleRule::GetDescription(
    DescriptionType description_type) {
  static std::string basic_desc = absl::StrCat(
      "Checks that non-type parameter and localparam names follow at least one "
      "of the naming conventions from a choice of CamelCase and ALL_CAPS, ORed "
      "together with the pipe-symbol(|). "
      "Empty configuration: no style enforcement. See ",
      GetStyleGuideCitation(kTopic), ".");
  if (description_type == DescriptionType::kHelpRulesFlag) {
    return absl::StrCat(basic_desc,
                        "Parameters: localparam_style:CamelCase;"
                        "parameter_style:CamelCase|ALL_CAPS");
  } else {
    return absl::StrCat(basic_desc,
                        "\n##### Parameters\n"
                        " * `localparam_style` Default: `CamelCase`\n"
                        " * `parameter_style` Default: `CamelCase|ALL_CAPS`\n");
  }
}

static const Matcher& ParamDeclMatcher() {
  static const Matcher matcher(NodekParamDeclaration());
  return matcher;
}

std::string ParameterNameStyleRule::ViolationMsg(absl::string_view symbol_type,
                                                 uint32_t allowed_bitmap) {
  // TODO(hzeller): there are multiple places in this file referring to the
  // same string representations of these options.
  static constexpr std::pair<uint32_t, const char*> kBitNames[] = {
      {kUpperCamelCase, "CamelCase"}, {kAllCaps, "ALL_CAPS"}};
  std::string bit_list;
  for (const auto& b : kBitNames) {
    if (allowed_bitmap & b.first) {
      if (!bit_list.empty()) bit_list.append(" or ");
      bit_list.append(b.second);
    }
  }
  return absl::StrCat("Non-type ", symbol_type, " names must be styled with ",
                      bit_list);
}

void ParameterNameStyleRule::HandleSymbol(const verible::Symbol& symbol,
                                          const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (ParamDeclMatcher().Matches(symbol, &manager)) {
    if (IsParamTypeDeclaration(symbol)) return;

    const auto param_decl_token = GetParamKeyword(symbol);

    auto identifiers = GetAllParameterNameTokens(symbol);

    for (auto id : identifiers) {
      const auto param_name = id->text();
      uint32_t observed_style = 0;
      if (verible::IsUpperCamelCaseWithDigits(param_name))
        observed_style |= kUpperCamelCase;
      if (verible::IsNameAllCapsUnderscoresDigits(param_name))
        observed_style |= kAllCaps;

      if (param_decl_token == TK_localparam && localparam_allowed_style_ &&
          (observed_style & localparam_allowed_style_) == 0) {
        violations_.insert(LintViolation(
            *id, ViolationMsg("localparam", localparam_allowed_style_),
            context));
      } else if (param_decl_token == TK_parameter && parameter_allowed_style_ &&
                 (observed_style & parameter_allowed_style_) == 0) {
        violations_.insert(LintViolation(
            *id, ViolationMsg("parameter", parameter_allowed_style_), context));
      }
    }
  }
}

absl::Status ParameterNameStyleRule::Configure(
    absl::string_view configuration) {
  // TODO(issue #133) include bitmap choices in generated documentation.
  static const std::vector<absl::string_view> choices = {
      "CamelCase", "ALL_CAPS"};  // same sequence as enum StyleChoicesBits
  using verible::config::SetNamedBits;
  return verible::ParseNameValues(
      configuration,
      {{"localparam_style", SetNamedBits(&localparam_allowed_style_, choices)},
       {"parameter_style", SetNamedBits(&parameter_allowed_style_, choices)}});
}

LintRuleStatus ParameterNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
