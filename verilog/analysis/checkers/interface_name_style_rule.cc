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
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/module.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(InterfaceNameStyleRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

absl::string_view InterfaceNameStyleRule::Name() {
  return "interface-name-style";
}
const char InterfaceNameStyleRule::kTopic[] = "interface-conventions";
const char InterfaceNameStyleRule::kMessage[] =
    "Interface names must use lower_snake_case naming convention "
    "and end with _if.";

std::string InterfaceNameStyleRule::GetDescription(
    DescriptionType description_type) {
  static std::string basic_desc = absl::StrCat(
      "Checks that ", Codify("interface", description_type),
      " names use lower_snake_case naming convention and end with "
      "'_if' or match the optional regular expression format. See ",
      GetStyleGuideCitation(kTopic), ".");
  if (description_type == DescriptionType::kHelpRulesFlag) {
    return absl::StrCat(basic_desc, "Parameters: name_regex:regex rule");
  } else {
    return absl::StrCat(
        basic_desc,
        "\n##### Parameters\n"
        "* `name_regex` (The regex rule validating the names. Default: Empty)");
  }
}

static const Matcher& InterfaceMatcher() {
  static const Matcher matcher(NodekInterfaceDeclaration());
  return matcher;
}

void InterfaceNameStyleRule::HandleSymbol(const verible::Symbol& symbol,
                                          const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  absl::string_view name;
  const verible::TokenInfo* identifier_token;
  if (InterfaceMatcher().Matches(symbol, &manager)) {
    identifier_token = &GetInterfaceNameToken(symbol);
    name = identifier_token->text();

    if (name_regex_.has_value()) {
      if (!std::regex_match(std::string(name), *name_regex_)) {
        violations_.insert(LintViolation(*identifier_token,
                                         "Regex rule does not match", context));
      }
      return;
    }

    if (!verible::IsLowerSnakeCaseWithDigits(name) ||
        !absl::EndsWith(name, "_if")) {
      violations_.insert(LintViolation(*identifier_token, kMessage, context));
    }
  }
}

absl::Status InterfaceNameStyleRule::Configure(
    absl::string_view configuration) {
  using verible::config::SetString;
  std::string name_regex;
  auto status = verible::ParseNameValues(
      configuration, {{"name_regex", SetString(&name_regex)}});

  if (!status.ok()) return status;

  if (!name_regex.empty()) {
    try {
      name_regex_ = name_regex;
    } catch (const std::regex_error& e) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Invalid regex specified");
    }
  }
  return absl::OkStatus();
}
LintRuleStatus InterfaceNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
