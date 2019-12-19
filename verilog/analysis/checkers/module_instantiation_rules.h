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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_MODULE_INSTANTIATION_RULES_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_MODULE_INSTANTIATION_RULES_H_

#include <string>
#include <set>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// ModuleParamRule is an implementation of LintRule that requires named
// positional ports in module instantiations.
class ModuleParameterRule : public verible::SyntaxTreeLintRule {
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
  // Matches against a parameter list that has positional parameters
  // For examples:
  //   foo #(1, 2) bar;
  // Here, the node representing "1, 2" will be bound to "list".
  const verible::matcher::Matcher matcher_ = NodekActualParameterList(
      ActualParameterListHasPositionalParameterList().Bind("list"));

  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  std::set<verible::LintViolation> violations_;
};

// ModuleParamRule is an implementation of LintRule that handles incorrect
// usage of positional ports in module instantiation
class ModulePortRule : public verible::SyntaxTreeLintRule {
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
  // Matches against a gate instance with a port list and bind that port list
  // to "list".
  // For example:
  //   foo bar (port1, port2);
  // Here, the node representing "port1, port2" will be bound to "list"
  const verible::matcher::Matcher matcher_ =
      NodekGateInstance(GateInstanceHasPortList().Bind("list"));

  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  // Returns false if a port list node is in violation of this rule and
  // true if it is not.
  bool IsPortListCompliant(const verible::SyntaxTreeNode& port_list_node) const;

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_MODULE_INSTANTIATION_RULES_H_
