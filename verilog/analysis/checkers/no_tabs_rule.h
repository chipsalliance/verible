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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_NO_TABS_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_NO_TABS_RULE_H_

#include <stddef.h>

#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "common/analysis/line_lint_rule.h"
#include "common/analysis/lint_rule_status.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// NoTabsRule detects whether any Verilog tokens contain tabs.
class NoTabsRule : public verible::LineLintRule {
 public:
  using rule_type = verible::LineLintRule;
  static absl::string_view Name();

  // Returns the description of the rule implemented formatted for either the
  // helper flag or markdown depending on the parameter type.
  static std::string GetDescription(DescriptionType);

  NoTabsRule() {}

  void HandleLine(absl::string_view line) override;

  verible::LintRuleStatus Report() const override;

 private:
  // Link to style guide rule.
  static const char kTopic[];

  // Diagnostic message.
  static const char kMessage[];

  // Collection of found violations.
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_NO_TABS_RULE_H_
