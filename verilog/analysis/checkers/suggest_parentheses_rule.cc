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
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/CST/expression.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(SuggestParenthesesRule);

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::SymbolTag;

absl::string_view SuggestParenthesesRule::Name() {
  return "suggest-parentheses";
}
const char SuggestParenthesesRule::kTopic[] = "parentheses";
const char SuggestParenthesesRule::kMessage[] =
    "Use parentheses around nested expressions";

std::string SuggestParenthesesRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that parentheses are wrapped in nested expressions. See ",
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
      const verible::Symbol* falseCase = GetConditionExpressionFalseCase(node);

      const auto trueCasetag = static_cast<verilog::NodeEnum>(trueCase->Tag().tag);
      const auto falseCasetag = static_cast<verilog::NodeEnum>(falseCase->Tag().tag);

      const verible::Symbol* trueCaseChild = UnwrapExpression(*trueCase);
      const verible::Symbol* falseCaseChild = UnwrapExpression(*falseCase);

      const auto trueCaseChildtag = static_cast<verilog::NodeEnum>(trueCaseChild->Tag().tag);
      const auto falseCaseChildtag = static_cast<verilog::NodeEnum>(falseCaseChild->Tag().tag);
      
      if((trueCasetag == NodeEnum::kExpression && trueCaseChildtag == NodeEnum::kConditionExpression) || trueCasetag == NodeEnum::kConditionExpression){
        violations_.insert(LintViolation(node, kMessage, context));
      }

      if((falseCasetag == NodeEnum::kExpression && falseCaseChildtag == NodeEnum::kConditionExpression) || falseCasetag == NodeEnum::kConditionExpression){
        violations_.insert(LintViolation(node, kMessage, context));
      }
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
