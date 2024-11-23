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

#include "verible/verilog/analysis/checkers/suggest-parentheses-rule.h"

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/expression.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(SuggestParenthesesRule);

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;

static constexpr absl::string_view kMessage =
    "Parenthesize condition expressions that appear in the true-clause of "
    "another condition expression.";

const LintRuleDescriptor &SuggestParenthesesRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "suggest-parentheses",
      .topic = "parentheses",
      .desc =
          "Recommend extra parentheses around subexpressions where it "
          "helps readability.",
  };
  return d;
}

void SuggestParenthesesRule::HandleNode(
    const verible::SyntaxTreeNode &node,
    const verible::SyntaxTreeContext &context) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  // TODO: evaluate other types of expressions
  switch (tag) {
    case NodeEnum::kConditionExpression: {
      const verible::Symbol *trueCase = GetConditionExpressionTrueCase(node);

      const verible::Symbol *trueCaseChild = UnwrapExpression(*trueCase);

      const auto trueCaseChildtag =
          static_cast<verilog::NodeEnum>(trueCaseChild->Tag().tag);

      if (trueCaseChildtag == NodeEnum::kConditionExpression) {
        const verible::TokenInfo token(SymbolIdentifier,
                                       verible::StringSpanOfSymbol(*trueCase));

        violations_.insert(LintViolation(
            token, kMessage, context,
            {AutoFix("Add parenthesis for readability",
                     {{token.text().substr(0, 0), "("},
                      {token.text().substr(token.text().length(), 0), ")"}})}));
      }
      break;
    }
    default:
      break;
  }
}

LintRuleStatus SuggestParenthesesRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
