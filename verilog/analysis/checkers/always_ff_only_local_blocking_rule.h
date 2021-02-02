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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_ALWAYS_FF_NON_BLOCKING_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_ALWAYS_FF_NON_BLOCKING_RULE_H_

#include <set>
#include <stack>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

class AlwaysFFOnlyLocalBlockingRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  void HandleSymbol(const verible::Symbol& symbol,
                    const verible::SyntaxTreeContext& context) override;

  verible::LintRuleStatus Report() const override;

 private:
  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  std::set<verible::LintViolation> violations_;

  // Inside an always_ff block
  int inside = 0;

  // Stack of inner begin-end scopes
  using scope_t =
      std::pair<int, int>;  // depth in syntax tree, number of inherited locals
  std::stack<scope_t, std::vector<scope_t>> scopes{
      {{-1, 0}}};  // bottom element -> never empty

  // In-order stack of local variable names
  std::vector<absl::string_view> locals;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_ALWAYS_FF_NON_BLOCKING_RULE_H_
