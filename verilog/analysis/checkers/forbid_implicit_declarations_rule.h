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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_IMPLICIT_DECLARATIONS_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_IMPLICIT_DECLARATIONS_RULE_H_

#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/text_structure_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_context_visitor.h"
#include "common/util/auto_pop_stack.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/symbol_table.h"
#include "verilog/analysis/verilog_project.h"

namespace verilog {
namespace analysis {

// ForbidImplicitDeclarationsRule detect implicitly declared nets
class ForbidImplicitDeclarationsRule : public verible::TextStructureLintRule {
                                       //public ScopeTreeVisitor {
 public:
  using rule_type = verible::TextStructureLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  // Analyze text structure for violations.
  void Lint(const verible::TextStructureView& text_structure,
            absl::string_view filename) override;

  verible::LintRuleStatus Report() const override;

 private:

 private:
  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_FORBID_IMPLICIT_DECLARATIONS_RULE_H_
