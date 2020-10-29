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

#include "verilog/analysis/checkers/explicit_task_lifetime_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/context_functions.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/tasks.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Register ExplicitTaskLifetimeRule
VERILOG_REGISTER_LINT_RULE(ExplicitTaskLifetimeRule);

absl::string_view ExplicitTaskLifetimeRule::Name() {
  return "explicit-task-lifetime";
}
const char ExplicitTaskLifetimeRule::kTopic[] =
    "function-task-explicit-lifetime";
const char ExplicitTaskLifetimeRule::kMessage[] =
    "Explicitly define static or automatic lifetime for non-class tasks";

std::string ExplicitTaskLifetimeRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that every task declared outside of a class is declared with "
      "an explicit lifetime (static or automatic). See ",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& TaskMatcher() {
  static const Matcher matcher(NodekTaskDeclaration());
  return matcher;
}

void ExplicitTaskLifetimeRule::HandleSymbol(const verible::Symbol& symbol,
                                            const SyntaxTreeContext& context) {
  // Don't need to check for lifetime declaration if context is inside a class
  if (ContextIsInsideClass(context)) return;

  verible::matcher::BoundSymbolManager manager;
  if (TaskMatcher().Matches(symbol, &manager)) {
    // If task id is qualified, it is an out-of-line
    // class task definition, which is also exempt.
    const auto* task_id = GetTaskId(symbol);
    if (IdIsQualified(*task_id)) return;

    // Make sure the lifetime was set
    if (GetTaskLifetime(symbol) == nullptr) {
      // Point to the task id.
      const verible::TokenInfo token(SymbolIdentifier,
                                     verible::StringSpanOfSymbol(*task_id));
      violations_.insert(LintViolation(token, kMessage, context));
    }
  }
}

LintRuleStatus ExplicitTaskLifetimeRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
