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

#include "verilog/analysis/checkers/explicit_function_lifetime_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/util/logging.h"
#include "verilog/CST/context_functions.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Register ExplicitFunctionLifetimeRule
VERILOG_REGISTER_LINT_RULE(ExplicitFunctionLifetimeRule);

absl::string_view ExplicitFunctionLifetimeRule::Name() {
  return "explicit-function-lifetime";
}
const char ExplicitFunctionLifetimeRule::kTopic[] =
    "function-task-explicit-lifetime";
const char ExplicitFunctionLifetimeRule::kMessage[] =
    "Explicitly define static or automatic lifetime for non-class functions";

std::string ExplicitFunctionLifetimeRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that every function declared outside of a class is declared "
      "with an explicit lifetime (static or automatic). See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& FunctionMatcher() {
  static const Matcher matcher(NodekFunctionDeclaration());
  return matcher;
}

void ExplicitFunctionLifetimeRule::HandleSymbol(
    const verible::Symbol& symbol, const SyntaxTreeContext& context) {
  // Don't need to check for lifetime declaration if context is inside a class
  if (ContextIsInsideClass(context)) return;

  verible::matcher::BoundSymbolManager manager;
  if (FunctionMatcher().Matches(symbol, &manager)) {
    // If function id is qualified, it is an out-of-line
    // class method definition, which is also exempt.
    const auto* function_id = ABSL_DIE_IF_NULL(GetFunctionId(symbol));
    if (IdIsQualified(*function_id)) return;

    // Make sure the lifetime was set
    if (GetFunctionLifetime(symbol) == nullptr) {
      // Point to the function id.
      const verible::TokenInfo token(SymbolIdentifier,
                                     verible::StringSpanOfSymbol(*function_id));
      violations_.insert(LintViolation(token, kMessage, context));
    }
  }
}

LintRuleStatus ExplicitFunctionLifetimeRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
