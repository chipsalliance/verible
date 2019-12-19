// Copyright 2017-2019 The Verible Authors.
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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_CREATE_OBJECT_NAME_MATCH_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_CREATE_OBJECT_NAME_MATCH_RULE_H_

#include <string>
#include <set>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// CreateObjectNameMatchRule checks that the name of a create()'d object
// matches the name of the variable to which it is assigned.
//
// Good:
//   foo_h = mytype::type_id::create("foo_h");
// Bad:
//   foo_h = mytype::type_id::create("zoo_h");
//
class CreateObjectNameMatchRule : public verible::SyntaxTreeLintRule {
 public:
  // This policy determines how this lint rule is registered in
  // verilog/analysis/lint_rule_registry.h.
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

  using Matcher = verible::matcher::Matcher;

  // Matches against assignments to typename::type_id::create() calls.
  //
  // For example:
  //   var_h = mytype::type_id::create("var_h", ...);
  //
  // Here, the LHS var_h will be bound to "lval" (only for simple references),
  // the qualified function call (mytype::type_id::create) will be bound to
  // "func", and the list of function call arguments will be bound to "args".
  const Matcher create_assignment_matcher_ = NodekAssignmentStatement(
      LValueOfAssignment(
          PathkReference(UnqualifiedReferenceHasId().Bind("lval")),
          verible::matcher::Unless(ReferenceHasHierarchy()),
          verible::matcher::Unless(ReferenceHasIndex())),
      RValueIsFunctionCall(FunctionCallIsQualified().Bind("func"),
                           FunctionCallArguments().Bind("args")));

  // Record of found violations.
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_CREATE_OBJECT_NAME_MATCH_RULE_H_
