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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_MODULE_INSTANTIATION_RULES_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_MODULE_INSTANTIATION_RULES_H_

#include <set>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// ModuleParamRule is an implementation of LintRule that requires named
// positional ports in module instantiations.
class ModuleParameterRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static const LintRuleDescriptor &GetDescriptor();

  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;
  verible::LintRuleStatus Report() const final;

 private:
  std::set<verible::LintViolation> violations_;
};

// ModuleParamRule is an implementation of LintRule that handles incorrect
// usage of positional ports in module instantiation
class ModulePortRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;
  static const LintRuleDescriptor &GetDescriptor();

  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;
  verible::LintRuleStatus Report() const final;

 private:
  // Returns false if a port list node is in violation of this rule and
  // true if it is not.
  static bool IsPortListCompliant(
      const verible::SyntaxTreeNode &port_list_node);

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_MODULE_INSTANTIATION_RULES_H_
