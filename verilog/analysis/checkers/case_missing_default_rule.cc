// Copyright 2017-2023 The Verible Authors.
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

#include "verilog/analysis/checkers/case_missing_default_rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::matcher::Matcher;

// Register CaseMissingDefaultRule
VERILOG_REGISTER_LINT_RULE(CaseMissingDefaultRule);

static constexpr absl::string_view kMessage =
    "Explicitly define a default case for every case statement or add `unique` "
    "qualifier to the case statement.";

const LintRuleDescriptor &CaseMissingDefaultRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "case-missing-default",
      .topic = "case-statements",
      .desc =
          "Checks that a default case-item is always defined unless the case "
          "statement has the `unique` qualifier.",
  };
  return d;
}

void CaseMissingDefaultRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;

  // Ensure that symbol is a kCaseStatement node
  if (symbol.Kind() != verible::SymbolKind::kNode) return;
  const verible::SyntaxTreeNode &node = verible::SymbolCastToNode(symbol);
  if (!node.MatchesTag(NodeEnum::kCaseStatement)) return;

  static const Matcher uniqueCaseMatcher(
      NodekCaseStatement(HasUniqueQualifier()));

  static const Matcher caseMatcherWithDefaultCase(
      NodekCaseStatement(HasDefaultCase()));

  // If the case statement doesn't have the "unique" qualifier and
  // it is missing the "default" case, insert the violation
  if (!uniqueCaseMatcher.Matches(symbol, &manager) &&
      !caseMatcherWithDefaultCase.Matches(symbol, &manager)) {
    violations_.insert(LintViolation(symbol, kMessage, context));
  }
}

LintRuleStatus CaseMissingDefaultRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
