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

#include "verible/verilog/analysis/checkers/case-missing-default-rule.h"

#include <set>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher-builders.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::matcher::Matcher;

// Register CaseMissingDefaultRule
VERILOG_REGISTER_LINT_RULE(CaseMissingDefaultRule);

static constexpr std::string_view kMessage =
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
