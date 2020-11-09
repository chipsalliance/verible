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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_ANONYMOUS_STRUCTS_UNIONS_RULE_H_  // NOLINT
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_ANONYMOUS_STRUCTS_UNIONS_RULE_H_  // NOLINT

#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// Upon encountering a struct or union keyword, detects whether it falls
// under a typedef.
//
// Accepted examples:
//    typedef struct {
//      firstSignal,
//      secondSignal,
//    } type_name_e;
//    type_name_e my_instance;
//
// Rejected examples:
//    struct {
//      firstSignal,
//      secondSignal,
//    } my_instance;
//
// If 'waive_nested' configuration is provided, anonymous structs within
// nested typedefs are allowed
//
// Allowed with 'allow_anonymous_nested'
// typedef struct {
//    struct { logic x; logic y; } foo;
// } outer_t;
//
class ForbiddenAnonymousStructsUnionsRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  absl::Status Configure(absl::string_view configuration) override;

  void HandleSymbol(const verible::Symbol& symbol,
                    const verible::SyntaxTreeContext& context) override;

  verible::LintRuleStatus Report() const override;

 private:
  // Tests if the rule is met, taking waiving condition into account.
  bool IsRuleMet(const verible::SyntaxTreeContext& context) const;

  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessageStruct[];
  static const char kMessageUnion[];

  bool allow_anonymous_nested_type_ = false;

  // Collection of found violations.
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBIDDEN_ANONYMOUS_STRUCTS_UNIONS_RULE_H_ // NOLINT
