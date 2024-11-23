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

#include "verible/verilog/analysis/checkers/no-trailing-spaces-rule.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(NoTrailingSpacesRule);

static constexpr absl::string_view kMessage = "Remove trailing spaces.";

const LintRuleDescriptor &NoTrailingSpacesRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "no-trailing-spaces",
      .topic = "trailing-spaces",
      .desc = "Checks that there are no trailing spaces on any lines.",
  };
  return d;
}

void NoTrailingSpacesRule::HandleLine(absl::string_view line) {
  // Lines may end with \n or \r\n. '\n' is already excluded.
  // Exclude '\r'
  absl::ConsumeSuffix(&line, "\r");
  // Now any line endings (either \n or \r\n) are excluded.
  // Searches each line in reverse for spaces.
  const auto rbegin = line.crbegin();
  const auto rend = line.crend();
  if (rbegin != rend) {
    const auto reverse_iter =
        std::find_if(rbegin, rend, [](char c) { return !std::isspace(c); });
    const int trailing = std::distance(rbegin, reverse_iter);
    if (trailing != 0) {
      const int column = line.length() - trailing;
      const TokenInfo token(TK_SPACE, line.substr(column));

      violations_.insert(LintViolation(
          token, kMessage, {AutoFix("Remove trailing space", {token, ""})}));
    }
  }
}

LintRuleStatus NoTrailingSpacesRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
