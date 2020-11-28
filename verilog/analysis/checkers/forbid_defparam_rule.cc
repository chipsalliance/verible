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

#include "verilog/analysis/checkers/forbid_defparam_rule.h"

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
using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbidDefparamRule);

absl::string_view ForbidDefparamRule::Name() { return "forbid-defparam"; }
const char ForbidDefparamRule::kTopic[] = "defparam";
const char ForbidDefparamRule::kMessage[] = "Do not use defparam.";

std::string ForbidDefparamRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Do not use defparam. See:", GetStyleGuideCitation(kTopic), ".");
}

// Matches the defparam construct.
static const Matcher& OverrideMatcher() {
  static const Matcher matcher(NodekParameterOverride());
  return matcher;
}

void ForbidDefparamRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (OverrideMatcher().Matches(symbol, &manager)) {
    const auto& defparam_token =
        GetSubtreeAsLeaf(symbol, NodeEnum::kParameterOverride, 0).get();
    CHECK_EQ(defparam_token.token_enum(), TK_defparam);
    violations_.insert(
        verible::LintViolation(defparam_token, kMessage, context));
  }
}

verible::LintRuleStatus ForbidDefparamRule::Report() const {
  return verible::LintRuleStatus(violations_, Name(),
                                 GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
