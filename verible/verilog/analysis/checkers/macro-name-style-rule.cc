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

#include "verible/verilog/analysis/checkers/macro-name-style-rule.h"

#include <memory>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "re2/re2.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(MacroNameStyleRule);

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;

static constexpr absl::string_view kUVMLowerCaseMessage =
    "'uvm_*' named macros must follow 'lower_snake_case' format.";

static constexpr absl::string_view kUVMUpperCaseMessage =
    "'UVM_*' named macros must follow 'UPPER_SNAKE_CASE' format.";

static constexpr absl::string_view kLowerSnakeCaseRegex = "[a-z_0-9]+";
static constexpr absl::string_view kUpperSnakeCaseRegex = "[A-Z_0-9]+";

MacroNameStyleRule::MacroNameStyleRule()
    : style_regex_(
          std::make_unique<re2::RE2>(kUpperSnakeCaseRegex, re2::RE2::Quiet)),
      style_lower_snake_case_regex_(
          std::make_unique<re2::RE2>(kLowerSnakeCaseRegex, re2::RE2::Quiet)),
      style_upper_snake_case_regex_(
          std::make_unique<re2::RE2>(kUpperSnakeCaseRegex, re2::RE2::Quiet)) {}

const LintRuleDescriptor &MacroNameStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "macro-name-style",
      .topic = "defines",
      .desc =
          "Checks that macro names conform to a naming convention defined by a "
          "RE2 regular expression. The default regex pattern expects "
          "\"UPPER_SNAKE_CASE\". Exceptions are made for UVM like macros, "
          "where macros named 'uvm_*' and 'UVM_*' follow \"lower_snake_case\" "
          "and \"UPPER_SNAKE_CASE\" naming conventions respectively. Refer to "
          "https://github.com/chipsalliance/verible/tree/master/verilog/tools/"
          "lint#readme for more detail on verible regex patterns.",
      .param = {{"style_regex", std::string(kUpperSnakeCaseRegex),
                 "A regex used to check macro names style."}},
  };
  return d;
}

std::string MacroNameStyleRule::CreateViolationMessage() {
  return absl::StrCat("Macro name does not match the naming convention ",
                      "defined by regex pattern: ", style_regex_->pattern());
}

void MacroNameStyleRule::HandleToken(const TokenInfo &token) {
  const auto token_enum = static_cast<verilog_tokentype>(token.token_enum());
  const absl::string_view text(token.text());
  if (IsUnlexed(verilog_tokentype(token.token_enum()))) {
    // recursively lex to examine inside macro definition bodies, etc.
    RecursiveLexText(
        text, [this](const TokenInfo &subtoken) { HandleToken(subtoken); });
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
            if (!RE2::FullMatch(text, *style_lower_snake_case_regex_)) {
              violations_.insert(LintViolation(token, kUVMLowerCaseMessage));
            }
          } else if (absl::StartsWith(text, "UVM_")) {
            // Special case for UVM_* macros
            if (!RE2::FullMatch(text, *style_upper_snake_case_regex_)) {
              violations_.insert(LintViolation(token, kUVMUpperCaseMessage));
            }
          } else {
            // General case for everything else
            if (!RE2::FullMatch(text, *style_regex_)) {
              violations_.insert(
                  LintViolation(token, CreateViolationMessage()));
            }
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

absl::Status MacroNameStyleRule::Configure(absl::string_view configuration) {
  using verible::config::SetRegex;
  absl::Status s = verible::ParseNameValues(
      configuration, {{"style_regex", SetRegex(&style_regex_)}});
  return s;
}

LintRuleStatus MacroNameStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
