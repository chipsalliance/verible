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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_PROPER_PARAMETER_DECLARATION_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_PROPER_PARAMETER_DECLARATION_RULE_H_

#include <set>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// ProperParameterDeclarationRule checks that parameter declarations are only
// inside packages or in formal parameter list of modules/classes, and
// localparam declarations are only inside modules and classes.
class ProperParameterDeclarationRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  static const LintRuleDescriptor &GetDescriptor();

  ProperParameterDeclarationRule();

  void AddParameterViolation(const verible::Symbol &symbol,
                             const verible::SyntaxTreeContext &context);

  void AddLocalparamViolation(const verible::Symbol &symbol,
                              const verible::SyntaxTreeContext &context);

  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;

  verible::LintRuleStatus Report() const final;

  absl::Status Configure(std::string_view configuration) final;

 private:
  void ChooseMessagesForConfiguration();

  std::set<verible::LintViolation> violations_;

  bool package_allow_parameter_ = false;
  bool package_allow_localparam_ = true;

  std::string_view parameter_message_;
  std::string_view local_parameter_message_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_PROPER_PARAMETER_DECLARATION_RULE_H_
