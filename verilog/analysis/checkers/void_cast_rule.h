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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_VOID_CAST_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_VOID_CAST_RULE_H_

#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// VoidCastRule checks that void casts do not contain certain function/method
// calls.
class VoidCastRule : public verible::SyntaxTreeLintRule {
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
  // Generate diagnostic message of why lint error occurred.
  std::string FormatReason(const verible::SyntaxTreeLeaf& leaf) const;

  // Link to style guide rule.
  static const char kTopic[];

  using Matcher = verible::matcher::Matcher;

  static const std::set<std::string>& BlacklistedFunctionsSet();

  // Matches against top level function calls within void casts
  // For example:
  //   void'(foo());
  // Here, the leaf representing "foo" will be bound to id
  const Matcher blacklisted_function_matcher_ =
      NodekVoidcast(VoidcastHasExpression(
          ExpressionHasFunctionCall(FunctionCallHasId().Bind("id"))));

  // Matches against both calls to randomize and randomize methods within
  // voidcasts.
  // For example:
  //   void'(obj.randomize(...));
  // Here, the node representing "randomize(...)" will be bound to "id"
  //
  // For example:
  //   void'(randomize(obj));
  // Here, the node representing "randomize(obj)" will be bound to "id"
  //
  const Matcher randomize_matcher_ = NodekVoidcast(VoidcastHasExpression(
      verible::matcher::AnyOf(ExpressionHasRandomizeCallExtension().Bind("id"),
                              ExpressionHasRandomizeFunction().Bind("id"))));

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_VOID_CAST_RULE_H_
