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

#include "verilog/analysis/checkers/always_comb_blocking_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register AlwaysCombBlockingRule
VERILOG_REGISTER_LINT_RULE(AlwaysCombBlockingRule);

static const char kMessage[] =
    "Use only blocking assignments in \'always_comb\' combinational blocks.";

const LintRuleDescriptor& AlwaysCombBlockingRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "always-comb-blocking",
      .topic = "combinational-logic",
      .desc =
          "Checks that there are no occurrences of "
          "non-blocking assignment in combinational logic.",
  };
  return d;
}

// Matches always_comb blocks.
static const Matcher& AlwaysCombMatcher() {
  static const Matcher matcher(NodekAlwaysStatement(AlwaysCombKeyword()));
  return matcher;
}

void AlwaysCombBlockingRule::HandleSymbol(const verible::Symbol& symbol,
                                          const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;

  if (AlwaysCombMatcher().Matches(symbol, &manager)) {
    for (const auto& match :
         SearchSyntaxTree(symbol, NodekNonblockingAssignmentStatement())) {
      const auto* node =
          dynamic_cast<const verible::SyntaxTreeNode*>(match.match);

      if (node == nullptr) continue;

      const verible::SyntaxTreeLeaf* leaf = verible::GetSubtreeAsLeaf(
          *node, NodeEnum::kNonblockingAssignmentStatement, 1);

      if (leaf && leaf->get().token_enum() == TK_LE)
        violations_.insert(LintViolation(*leaf, kMessage, match.context));
    }
  }
}

LintRuleStatus AlwaysCombBlockingRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
