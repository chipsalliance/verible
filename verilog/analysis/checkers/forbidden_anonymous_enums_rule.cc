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

#include "verilog/analysis/checkers/forbidden_anonymous_enums_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using Matcher = verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbiddenAnonymousEnumsRule);

absl::string_view ForbiddenAnonymousEnumsRule::Name() {
  return "typedef-enums";
}
const char ForbiddenAnonymousEnumsRule::kTopic[] = "typedef-enums";
const char ForbiddenAnonymousEnumsRule::kMessage[] =
    "enum types always should be named using typedef.";

std::string ForbiddenAnonymousEnumsRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that a Verilog ", Codify("enum", description_type),
      " declaration is named using ", Codify("typedef", description_type),
      ". See ", GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& EnumMatcher() {
  static const Matcher matcher(NodekEnumType());
  return matcher;
}

void ForbiddenAnonymousEnumsRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (EnumMatcher().Matches(symbol, &manager)) {
    // Check if it is preceded by a typedef
    if (!context.DirectParentsAre({NodeEnum::kDataTypePrimitive,
                                   NodeEnum::kDataType,
                                   NodeEnum::kTypeDeclaration})) {
      violations_.insert(LintViolation(symbol, kMessage, context));
    }
  }
}

LintRuleStatus ForbiddenAnonymousEnumsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
