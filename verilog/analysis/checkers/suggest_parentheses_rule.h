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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS__SUGGEST_PARENTHESES_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS__SUGGEST_PARENTHESES_RULE_H_

#include "common/analysis/syntax_tree_lint_rule.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// Suggests where extra parentheses can help readability.
// For example:
// a1 ? a2 ? b : c : d
// would be improved as the equivalent:
// a1 ? (a2 ? b : c) : d
class SuggestParenthesesRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  void HandleNode(const verible::SyntaxTreeNode& node,
                  const verible::SyntaxTreeContext& context) override;

  verible::LintRuleStatus Report() const override;

 private:
  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message for rule violations.
  static const char kMessage[];

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS__SUGGEST_PARENTHESES_RULE_H_
