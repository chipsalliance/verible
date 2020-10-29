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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_UNDERSIZED_BINARY_LITERAL_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_UNDERSIZED_BINARY_LITERAL_RULE_H_

#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// UndersizedBinaryLiteralRule checks binary literals match their declared
// width.  This needs a syntax tree analysis because the numbers of
// interest are composed of a constant width and based numeric literal.
// See the 'number' rule in verilog.y.
class UndersizedBinaryLiteralRule : public verible::SyntaxTreeLintRule {
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
  // Generate string representation of why lint error occurred at leaf
  static std::string FormatReason(absl::string_view width,
                                  absl::string_view base,
                                  absl::string_view literal);

  // Link to style guide rule.
  static const char kTopic[];

  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_UNDERSIZED_BINARY_LITERAL_RULE_H_
