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

#include "verilog/analysis/checkers/enum_name_style_rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(EnumNameStyleRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

absl::string_view EnumNameStyleRule::Name() { return "enum-name-style"; }
const char EnumNameStyleRule::kTopic[] = "enumerations";
const char EnumNameStyleRule::kMessage[] =
    "Enum names must use lower_snake_case naming convention "
    "and end with _t or _e.";

std::string EnumNameStyleRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that ", Codify("enum", description_type),
                      " names use lower_snake_case naming convention"
                      " and end with '_t' or '_e'. See ",
                      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& TypedefMatcher() {
  static const Matcher matcher(NodekTypeDeclaration());
  return matcher;
}

void EnumNameStyleRule::HandleSymbol(const verible::Symbol& symbol,
                                     const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (TypedefMatcher().Matches(symbol, &manager)) {
    // TODO: This can be changed to checking type of child (by index) when we
    // have consistent shape for all kTypeDeclaration nodes.
    if (!FindAllEnumTypes(symbol).empty()) {
      const auto* identifier_leaf = GetIdentifierFromTypeDeclaration(symbol);
      const auto name = ABSL_DIE_IF_NULL(identifier_leaf)->get().text();
      if (!verible::IsLowerSnakeCaseWithDigits(name) ||
          !(absl::EndsWith(name, "_t") || absl::EndsWith(name, "_e"))) {
        violations_.insert(
            LintViolation(identifier_leaf->get(), kMessage, context));
      }
    } else {
      // Not an enum definition
      return;
    }
  }
}

LintRuleStatus EnumNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
