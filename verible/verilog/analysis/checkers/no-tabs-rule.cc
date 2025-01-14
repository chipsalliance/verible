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

#include "verible/verilog/analysis/checkers/no-tabs-rule.h"

#include <set>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(NoTabsRule);

static constexpr std::string_view kMessage = "Use spaces, not tabs.";

const LintRuleDescriptor &NoTabsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "no-tabs",
      .topic = "tabs",
      .desc =
          "Checks that no tabs are used. Spaces should be used instead of "
          "tabs. ",
  };
  return d;
}

void NoTabsRule::HandleLine(std::string_view line) {
  // Finds first tab in each line, if there is one.
  // This reports only the first violation on each line.
  const auto tab_pos = line.find('\t');
  if (tab_pos != std::string_view::npos) {
    TokenInfo token(TK_SPACE, line.substr(tab_pos, 1));
    violations_.insert(LintViolation(token, kMessage));
  }
}

LintRuleStatus NoTabsRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
