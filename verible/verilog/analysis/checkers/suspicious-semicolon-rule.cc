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

#include "verible/verilog/analysis/checkers/suspicious-semicolon-rule.h"

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

VERILOG_REGISTER_LINT_RULE(SuspiciousSemicolon);

static constexpr absl::string_view kMessage =
    "Potentially unintended semicolon";

const LintRuleDescriptor &SuspiciousSemicolon::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "suspicious-semicolon",
      .topic = "bugprone",
      .desc =
          "Checks that there are no suspicious semicolons that might affect "
          "code behaviour but escape quick visual inspection"};
  return d;
}

static const Matcher &NullStatementMatcher() {
  static const Matcher matcher(NodekNullStatement());
  return matcher;
}

void SuspiciousSemicolon::HandleNode(
    const verible::SyntaxTreeNode &node,
    const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (!NullStatementMatcher().Matches(node, &manager)) return;

  // Waive @(posedge clk);
  // But catch always_ff @(posedge clk);
  const bool parent_is_proc_timing_ctrl_statement =
      context.DirectParentIs(NodeEnum::kProceduralTimingControlStatement);
  if (!context.IsInside(NodeEnum::kAlwaysStatement) &&
      parent_is_proc_timing_ctrl_statement) {
    return;
  }

  if (!parent_is_proc_timing_ctrl_statement &&
      !context.DirectParentIsOneOf(
          {NodeEnum::kForeachLoopStatement, NodeEnum::kWhileLoopStatement,
           NodeEnum::kForLoopStatement, NodeEnum::kForeverLoopStatement,
           NodeEnum::kIfBody, NodeEnum::kElseBody})) {
    return;
  }

  violations_.insert(verible::LintViolation(
      node, kMessage, context,
      {verible::AutoFix("Remove ';'",
                        {verible::StringSpanOfSymbol(node), ""})}));
}

verible::LintRuleStatus SuspiciousSemicolon::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
