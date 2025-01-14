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

#include "verible/verilog/analysis/checkers/macro-string-concatenation-rule.h"

#include <cstddef>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/value-saver.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-lexer.h"
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(MacroStringConcatenationRule);

static constexpr std::string_view kMessage =
    "Token concatenation (``) used inside plain string literal.";

const LintRuleDescriptor &MacroStringConcatenationRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "macro-string-concatenation",
      .topic = "defines",
      .desc =
          "Concatenation will not be evaluated here. Use `\"...`\" instead.",
  };
  return d;
}

void MacroStringConcatenationRule::HandleToken(const TokenInfo &token) {
  const auto token_enum = static_cast<verilog_tokentype>(token.token_enum());
  const std::string_view text(token.text());

  // Search only in `define tokens. Ignore state as `defines can be nested.
  if (token_enum == PP_define_body) {
    if (IsUnlexed(token_enum)) {
      verible::ValueSaver<State> state_saver(&state_, State::kInsideDefineBody);
      RecursiveLexText(
          text, [this](const TokenInfo &subtoken) { HandleToken(subtoken); });
    }
  } else if (state_ == State::kInsideDefineBody &&
             token_enum == TK_StringLiteral) {
    size_t pos = 0;
    while ((pos = text.find("``", pos)) != std::string_view::npos) {
      violations_.insert(
          LintViolation(TokenInfo(token_enum, text.substr(pos, 2)), kMessage));
      pos += 2;
    }
  }
}

LintRuleStatus MacroStringConcatenationRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
