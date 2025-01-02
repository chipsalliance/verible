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

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/analysis/descriptions.h"

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

  const RE2 *localparam_style_regex() const {
    return localparam_style_regex_.get();
  }
  const RE2 *parameter_style_regex() const {
    return parameter_style_regex_.get();
  }

 private:
  absl::Status AppendRegex(std::unique_ptr<re2::RE2> *rule_regex,
                           absl::string_view regex_str);
  absl::Status ConfigureRegex(std::unique_ptr<re2::RE2> *rule_regex,
                              uint32_t config_style,
                              std::unique_ptr<re2::RE2> *config_style_regex);

  enum StyleChoicesBits {
    kUpperCamelCase = (1 << 0),
    kAllCaps = (1 << 1),
  };

  std::set<verible::LintViolation> violations_;

  // Regex's to check the style against
  std::unique_ptr<re2::RE2> localparam_style_regex_;
  std::unique_ptr<re2::RE2> parameter_style_regex_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_PARAMETER_NAME_STYLE_RULE_H_