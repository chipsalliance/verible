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

#include "verible/verilog/analysis/checkers/numeric-format-string-style-rule.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <set>

#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/text/token-info.h"
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
using verible::TokenStreamLintRule;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(NumericFormatStringStyleRule);

static constexpr absl::string_view kMessage =
    "Formatting string must contain proper style-compilant numeric specifiers.";

const LintRuleDescriptor &NumericFormatStringStyleRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "numeric-format-string-style",
      .topic = "number-formatting",
      .desc =
          "Checks that string literals with numeric format specifiers "
          "have proper prefixes for hex and bin values and no prefixes for "
          "decimal values.",
  };
  return d;
}

template <typename T>
class TD;

void NumericFormatStringStyleRule::CheckAndReportViolation(
    const TokenInfo &token, size_t pos, size_t len,
    std::initializer_list<unsigned char> prefixes) {
  const absl::string_view text(token.text());

  // Check for prefix
  if (pos >= 2 && (text[pos - 2] == '0' || text[pos - 2] == '\'')) {
    const auto ch = text[pos - 1];
    if (std::none_of(prefixes.begin(), prefixes.end(),
                     [&ch](const char &_ch) { return _ch == ch; })) {
      // Report whole prefix with a "0" or "'"
      violations_.insert(LintViolation(
          TokenInfo(token.token_enum(), text.substr(pos - 2, len + 2)),
          kMessage));
    }
  } else {
    // Report just radix
    violations_.insert(LintViolation(
        TokenInfo(token.token_enum(), text.substr(pos, len)), kMessage));
  }
}

void NumericFormatStringStyleRule::HandleToken(const TokenInfo &token) {
  const auto token_enum = static_cast<verilog_tokentype>(token.token_enum());
  const absl::string_view text(token.text());

  if (IsUnlexed(verilog_tokentype(token.token_enum()))) {
    // recursively lex to examine inside macro definition bodies, etc.
    RecursiveLexText(
        text, [this](const TokenInfo &subtoken) { HandleToken(subtoken); });
    return;
  }

  if (token_enum != TK_StringLiteral) return;

  for (size_t pos = 0; pos < text.size();) {
    const auto &ch = text[pos];

    // Skip ordinary characters
    if (ch != '%') {
      ++pos;
      continue;
    }

    // Examine formatting directive
    for (size_t itr = pos + 1; itr < text.size(); ++itr) {
      // Skip field with display data size
      if (absl::ascii_isdigit(text[itr])) continue;

      const auto radix = text[itr];
      const auto len = itr - pos + 1;

      // Format radix
      switch (radix) {
        // binary value
        case 'b':
        case 'B': {
          CheckAndReportViolation(token, pos, len, {'b'});
          break;
        }

        // hexdecimal value
        case 'h':
        case 'H':
        case 'x':
        case 'X': {
          CheckAndReportViolation(token, pos, len, {'h', 'x'});
          break;
        }

        // decimal value
        case 'd':
        case 'D': {
          // Detect prefixes starting with "0" and "'"
          if (pos >= 2 && (text[pos - 2] == '0' || text[pos - 2] == '\'')) {
            // Report whole string with a "0" or "'"
            violations_.insert(LintViolation(
                TokenInfo(token.token_enum(), text.substr(pos - 2, len + 2)),
                kMessage));
          }
          break;
        }

        default:
          break;
      }

      // continue with the following character
      pos = itr + 1;
      break;
    }
  }
}

LintRuleStatus NumericFormatStringStyleRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
