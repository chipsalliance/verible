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

#include "verible/verilog/analysis/checkers/always-comb-rule.h"

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/statement.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register AlwaysCombRule
VERILOG_REGISTER_LINT_RULE(AlwaysCombRule);

static constexpr std::string_view kMessage =
    "Use 'always_comb' instead of 'always @*'.";

const LintRuleDescriptor &AlwaysCombRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "always-comb",
      .topic = "combinational-logic",
      .desc =
          "Checks that there are no occurrences of "
          "`always @*`. Use `always_comb` instead.",
  };
  return d;
}

// Matches event control (sensitivity list) for all signals.
// For example:
//   always @* begin
//     f = g + h;
//   end
static const Matcher &AlwaysStarMatcher() {
  static const Matcher matcher(NodekAlwaysStatement(
      AlwaysKeyword(), AlwaysStatementHasEventControlStar()));
  return matcher;
}

static const Matcher &AlwaysStarMatcherWithParentheses() {
  static const Matcher matcher(NodekAlwaysStatement(
      AlwaysKeyword(), AlwaysStatementHasEventControlStarAndParentheses()));
  return matcher;
}

void AlwaysCombRule::HandleSymbol(const verible::Symbol &symbol,
                                  const SyntaxTreeContext &context) {
  // Check for offending use of always @*
  verible::matcher::BoundSymbolManager manager;

  bool always_no_paren = AlwaysStarMatcher().Matches(symbol, &manager);
  bool always_paren =
      AlwaysStarMatcherWithParentheses().Matches(symbol, &manager);
  if (!always_no_paren && !always_paren) {
    return;
  }

  const std::string_view fix_message =
      always_paren ? "Substitute 'always @(*)' for 'always_comb'"
                   : "Substitute 'always @*' for 'always_comb'";

  // kAlwaysStatement node
  //  Leaf @0: 'always'
  //  Node @1: kProceduralTimingControlStatement
  //  Node @0: kEventControl (We want to completely remove this!)
  const verible::SyntaxTreeNode &always_statement_node =
      verible::SymbolCastToNode(symbol);

  // Leaf @0 of kAlwaysStatement node.
  const verible::SyntaxTreeLeaf *always_leaf = verible::GetLeftmostLeaf(symbol);

  // Get to the kEventControl symbol
  const verible::SyntaxTreeNode *proc_ctrl_statement =
      GetProceduralTimingControlFromAlways(always_statement_node);
  const verible::Symbol *event_ctrl =
      GetEventControlFromProceduralTimingControl(*proc_ctrl_statement);

  // always_str will cover the 'always @(*)' (or similar), which we'll
  // substitute for plain 'always_comb'
  std::string_view always_str =
      verible::StringSpanOfSymbol(*always_leaf, *event_ctrl);

  std::vector<AutoFix> autofixes{
      AutoFix(fix_message, {always_str, "always_comb"})};

  violations_.insert(LintViolation(symbol, kMessage, context, autofixes));
}

LintRuleStatus AlwaysCombRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
