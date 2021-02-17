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

class AlwaysFFNonBlockingRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  absl::Status Configure(const absl::string_view configuration) override;
  void HandleSymbol(const verible::Symbol& symbol,
                    const verible::SyntaxTreeContext& context) override;

  verible::LintRuleStatus Report() const override;

 private:
  // Detects entering and leaving relevant code inside always_ff
  bool InsideBlock(const verible::Symbol& symbol, const int depth);
  // Processes local declarations
  bool LocalDeclaration(const verible::Symbol& symbol);

 private:
  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  // Collected violations.
  std::set<verible::LintViolation> violations_;

  //- Configuration ---------------------
  bool catch_modifying_assignments_ = false;
  bool waive_for_locals_ = false;

  //- Processing State ------------------
  // Inside an always_ff block
  int inside_ = 0;

  // Stack of inner begin-end scopes
  struct Scope {
    int syntax_tree_depth;
    size_t inherited_local_count;
  };
  std::stack<Scope, std::vector<Scope>> scopes_{
      {{-1, 0}}  // bottom element -> the stack is never empty
  };

  // In-order stack of local variable names
  std::vector<absl::string_view> locals_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_ALWAYS_FF_NON_BLOCKING_RULE_H_
