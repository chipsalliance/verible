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

#include "verilog/analysis/checkers/no_trailing_spaces_rule.h"

#include <stddef.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/text/token_info.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(NoTrailingSpacesRule);

absl::string_view NoTrailingSpacesRule::Name() { return "no-trailing-spaces"; }
const char NoTrailingSpacesRule::kTopic[] = "trailing-spaces";
const char NoTrailingSpacesRule::kMessage[] = "Remove trailing spaces.";

std::string NoTrailingSpacesRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that there are no trailing spaces on any lines. See ",
      GetStyleGuideCitation(kTopic), ".");
}

void NoTrailingSpacesRule::HandleLine(absl::string_view line) {
  // Searches each line in reverse for spaces.
  const auto rbegin = line.crbegin();
  const auto rend = line.crend();
  if (rbegin != rend) {
    // Lines already exclude \n, so we can check using std::isspace.
    const auto reverse_iter =
        std::find_if(rbegin, rend, [](char c) { return !std::isspace(c); });
    const int trailing = std::distance(rbegin, reverse_iter);
    if (trailing != 0) {
      const int column = line.length() - trailing;
      const TokenInfo token(TK_SPACE, line.substr(column));
      violations_.insert(LintViolation(token, kMessage));
    }
  }
}

LintRuleStatus NoTrailingSpacesRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
