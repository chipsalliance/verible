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

#include "verible/verilog/analysis/checkers/always-comb-blocking-rule.h"

#include <set>
#include <string>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::down_cast;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SearchSyntaxTree;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register AlwaysCombBlockingRule
VERILOG_REGISTER_LINT_RULE(AlwaysCombBlockingRule);

static constexpr std::string_view kMessage =
    "Use only blocking assignments in \'always_comb\' combinational blocks.";

const LintRuleDescriptor &AlwaysCombBlockingRule::GetDescriptor() {
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
static const Matcher &AlwaysCombMatcher() {
  static const Matcher matcher(NodekAlwaysStatement(AlwaysCombKeyword()));
  return matcher;
}

void AlwaysCombBlockingRule::HandleSymbol(const verible::Symbol &symbol,
                                          const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;

  if (AlwaysCombMatcher().Matches(symbol, &manager)) {
    for (const auto &match :
         SearchSyntaxTree(symbol, NodekNonblockingAssignmentStatement())) {
      if (match.match->Kind() != verible::SymbolKind::kNode) continue;

      const auto *node =
          down_cast<const verible::SyntaxTreeNode *>(match.match);

      const verible::SyntaxTreeLeaf *leaf = verible::GetSubtreeAsLeaf(
          *node, NodeEnum::kNonblockingAssignmentStatement, 1);

      if (leaf && leaf->get().token_enum() == TK_LE) {
        violations_.insert(
            LintViolation(*leaf, kMessage, match.context,
                          {AutoFix("Use blocking assignment '=' instead of "
                                   "nonblocking assignment '<='",
                                   {leaf->get(), "="})}));
      }
    }
  }
}

LintRuleStatus AlwaysCombBlockingRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
