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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_PLUSARG_ASSIGNMENT_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_PLUSARG_ASSIGNMENT_RULE_H_

#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// PlusargAssignmentRule checks that plusargs are always assigned a value, by
// ensuring that plusargs are never accessed using $test$plusargs system task.
//
// Example violation:
// class Bad;
//   function int Foo();
//     if($test$plusargs("foo")) return 1;
//   endfunction
// endclass
// TODO(b/137326925): Refactor to merge with ForbiddenSystemTaskFunctionRule.
class PlusargAssignmentRule : public verible::SyntaxTreeLintRule {
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
  std::string FormatReason() const;

  // Link to style guide rule.
  static const char kTopic[];

  static const char kForbiddenFunctionName[];
  static const char kCorrectFunctionName[];

 private:
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_PLUSARG_ASSIGNMENT_RULE_H_
