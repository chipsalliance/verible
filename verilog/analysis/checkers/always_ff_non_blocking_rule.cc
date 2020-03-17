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

#include "verilog/analysis/checkers/always_ff_non_blocking_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;

// Register AlwaysFFNonBlockingRule
VERILOG_REGISTER_LINT_RULE(AlwaysFFNonBlockingRule);

absl::string_view AlwaysFFNonBlockingRule::Name() {
  return "always-ff-non-blocking";
}
const char AlwaysFFNonBlockingRule::kTopic[] = "sequential-logic";
const char AlwaysFFNonBlockingRule::kMessage[] =
    "Use only non-blocking assignments inside \'always_ff\' sequential blocks.";

std::string AlwaysFFNonBlockingRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that there are no occurrences of ",
                      "blocking assignment in sequential logic.");
}

void AlwaysFFNonBlockingRule::HandleSymbol(const verible::Symbol &symbol,
                                           const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;

  if (always_ff_matcher_.Matches(symbol, &manager)) {
    for (const auto &match :
         SearchSyntaxTree(symbol, NodekNetVariableAssignment())) {
      // this is intended to ignore assignments in for loop step statements like
      // i=i+1
      if (match.context.IsInside(NodeEnum::kLoopHeader)) continue;

      auto *node = dynamic_cast<const verible::SyntaxTreeNode *>(match.match);

      if (node == nullptr) continue;

      auto *leaf = dynamic_cast<const verible::SyntaxTreeLeaf *>(
          node->children()[1].get());

      if (leaf == nullptr) continue;

      if (leaf->get().token_enum == '=')
        violations_.insert(LintViolation(*leaf, kMessage, match.context));
    }
  }
}

LintRuleStatus AlwaysFFNonBlockingRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
