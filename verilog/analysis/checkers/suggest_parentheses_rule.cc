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

#include "verilog/analysis/checkers/suggest_parentheses_rule.h"

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "verilog/CST/expression.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(SuggestParenthesesRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;

absl::string_view SuggestParenthesesRule::Name() {
  return "suggest-parentheses";
}
const char SuggestParenthesesRule::kTopic[] = "parentheses";
const char SuggestParenthesesRule::kMessage[] =
    "Parenthesize condition expressions that appear in the true-clause of "
    "another condition expression.";

std::string SuggestParenthesesRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Recommend extra parentheses around subexpressions where it helps "
      "readability. See ",
      GetStyleGuideCitation(kTopic), ".");
}

void SuggestParenthesesRule::HandleNode(
    const verible::SyntaxTreeNode& node,
    const verible::SyntaxTreeContext& context) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  // TODO: evaluate other types of expressions
  switch (tag) {
    case NodeEnum::kConditionExpression: {
      const verible::Symbol* trueCase = GetConditionExpressionTrueCase(node);

      const verible::Symbol* trueCaseChild = UnwrapExpression(*trueCase);

      const auto trueCaseChildtag =
          static_cast<verilog::NodeEnum>(trueCaseChild->Tag().tag);

      if (trueCaseChildtag == NodeEnum::kConditionExpression) {
        const verible::TokenInfo token(SymbolIdentifier,
                                       verible::StringSpanOfSymbol(*trueCase));

        violations_.insert(LintViolation(token, kMessage, context));
      }
      break;
    }
    default:
      break;
  }
}

LintRuleStatus SuggestParenthesesRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
