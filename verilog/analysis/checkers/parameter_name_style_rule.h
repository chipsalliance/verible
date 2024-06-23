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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_PARAMETER_NAME_STYLE_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_PARAMETER_NAME_STYLE_RULE_H_

#include <memory>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "re2/re2.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// ParameterNameStyleRule checks that each non-type parameter/localparam
// follows the correct naming convention matching a regex pattern.
class ParameterNameStyleRule : public verible::SyntaxTreeLintRule {
 public:
  using rule_type = verible::SyntaxTreeLintRule;

  ParameterNameStyleRule();

  static const LintRuleDescriptor &GetDescriptor();

  std::string CreateLocalparamViolationMessage();
  std::string CreateParameterViolationMessage();

  void HandleSymbol(const verible::Symbol &symbol,
                    const verible::SyntaxTreeContext &context) final;

  verible::LintRuleStatus Report() const final;

  absl::Status Configure(absl::string_view configuration) final;

 private:
  std::set<verible::LintViolation> violations_;

  // A regex to check the style against
  std::unique_ptr<re2::RE2> localparam_style_regex_;
  std::unique_ptr<re2::RE2> parameter_style_regex_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_PARAMETER_NAME_STYLE_RULE_H_
