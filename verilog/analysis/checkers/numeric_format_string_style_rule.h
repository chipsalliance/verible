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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_NUMERIC_FORMAT_STRING_STYLE_RULE_H
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_NUMERIC_FORMAT_STRING_STYLE_RULE_H

#include <set>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/text/token_info.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// NumericFormatStringStyleRule checks that every string literal contains
// proper formatting strings
class NumericFormatStringStyleRule : public verible::TokenStreamLintRule {
 public:
  using rule_type = verible::TokenStreamLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  NumericFormatStringStyleRule() {}

  void HandleToken(const verible::TokenInfo& token) override;

  verible::LintRuleStatus Report() const override;

 private:
  void CheckAndReportViolation(const verible::TokenInfo& token, size_t position,
                               size_t length,
                               std::initializer_list<unsigned char> prefixes);

  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_NUMERIC_FORMAT_STRING_STYLE_RULE_H
