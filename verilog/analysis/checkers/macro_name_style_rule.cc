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

#include "verilog/analysis/checkers/macro_name_style_rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/strings/naming_utils.h"
#include "common/text/token_info.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_lexer.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;
using verible::TokenStreamLintRule;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(MacroNameStyleRule);

absl::string_view MacroNameStyleRule::Name() { return "macro-name-style"; }
const char MacroNameStyleRule::kTopic[] = "defines";
const char MacroNameStyleRule::kMessage[] =
    "Macro names must contain only CAPITALS, underscores, and digits.  "
    "Exception: UVM-like macros.";

std::string MacroNameStyleRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that every macro name follows ALL_CAPS naming convention.  "
      "Exception: UVM-like macros.  See ",
      GetStyleGuideCitation(kTopic), ".");
}

void MacroNameStyleRule::HandleToken(const TokenInfo& token) {
  const auto token_enum = static_cast<verilog_tokentype>(token.token_enum());
  const absl::string_view text(token.text());
  if (IsUnlexed(verilog_tokentype(token.token_enum()))) {
    // recursively lex to examine inside macro definition bodies, etc.
    RecursiveLexText(
        text, [this](const TokenInfo& subtoken) { HandleToken(subtoken); });
    return;
  }

  switch (state_) {
    case State::kNormal: {
      // Only changes state on `define tokens; all others are ignored in this
      // analysis.
      switch (token_enum) {
        case PP_define:
          state_ = State::kExpectPPIdentifier;
          break;
        default:
          break;
      }
      break;
    }
    case State::kExpectPPIdentifier: {
      switch (token_enum) {
        case TK_SPACE:  // stay in the same state
          break;
        case PP_Identifier: {
          if (absl::StartsWith(text, "uvm_")) {
            // Special case for uvm_* macros
            if (!verible::IsLowerSnakeCaseWithDigits(text))
              violations_.insert(LintViolation(token, kMessage));
          } else if (absl::StartsWith(text, "UVM_")) {
            // Special case for UVM_* macros
            if (!verible::IsNameAllCapsUnderscoresDigits(text))
              violations_.insert(LintViolation(token, kMessage));
          } else {
            // General case for everything else
            // TODO(fangism): make this configurable
            if (!verible::IsNameAllCapsUnderscoresDigits(text))
              violations_.insert(LintViolation(token, kMessage));
          }
          state_ = State::kNormal;
          break;
        }
        default:
          break;
      }
    }
  }  // switch (state_)
}

LintRuleStatus MacroNameStyleRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
