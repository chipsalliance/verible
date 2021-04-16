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

#include "verilog/analysis/checkers/constraint_name_style_rule.h"

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
#include "common/text/config_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/token_info.h"
#include "verilog/CST/constraints.h"
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

// Register ConstraintNameStyleRule.
VERILOG_REGISTER_LINT_RULE(ConstraintNameStyleRule);

absl::string_view ConstraintNameStyleRule::Name() {
  return "constraint-name-style";
}
const char ConstraintNameStyleRule::kTopic[] = "constraints";
const char ConstraintNameStyleRule::kMessage[] =
    "Constraint names must by styled with lower_snake_case and end with _c.";

std::string ConstraintNameStyleRule::GetDescription(
    DescriptionType description_type) {
  static std::string basic_desc = absl::StrCat(
      "Check that constraint names follow the lower_snake_case convention and"
      " end with _c or match the optional regular expression format. See ",
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

static const Matcher& ConstraintMatcher() {
  static const Matcher matcher(NodekConstraintDeclaration());
  return matcher;
}

void ConstraintNameStyleRule::HandleSymbol(const verible::Symbol& symbol,
                                           const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (ConstraintMatcher().Matches(symbol, &manager)) {
    // Since an out-of-line definition is always followed by a forward
    // declaration somewhere else (in this case inside a class), we can just
    // ignore all out-of-line definitions to  avoid duplicate lint errors on
    // the same name.
    if (IsOutOfLineConstraintDefinition(symbol)) {
      return;
    }

    const auto& identifier_token =
        GetSymbolIdentifierFromConstraintDeclaration(symbol);

    const auto constraint_name = identifier_token.text();
    if (name_regex_.has_value()) {
      if (!std::regex_match(std::string(constraint_name), *name_regex_)) {
        violations_.insert(LintViolation(identifier_token,
                                         "Regex rule does not match", context));
      }
      return;
    }

    if (!verible::IsLowerSnakeCaseWithDigits(constraint_name) ||
        !absl::EndsWith(constraint_name, "_c"))
      violations_.insert(LintViolation(identifier_token, kMessage, context));
  }
}

absl::Status ConstraintNameStyleRule::Configure(
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

LintRuleStatus ConstraintNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
