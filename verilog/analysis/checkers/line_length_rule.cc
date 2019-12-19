// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/analysis/checkers/line_length_rule.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <set>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/strings/comment_utils.h"
#include "common/text/constants.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_stream_view.h"
#include "common/util/iterator_range.h"
#include "common/util/logging.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/analysis/verilog_linter_constants.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TextStructureView;
using verible::TokenInfo;
using verible::TokenSequence;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(LineLengthRule);

absl::string_view LineLengthRule::Name() { return "line-length"; }
const char LineLengthRule::kTopic[] = "line-length";
const char LineLengthRule::kMessage[] = "Line length exceeds max: ";

static bool ContainsAnyWhitespace(absl::string_view s) {
  for (char c : s) {
    if (absl::ascii_isspace(c)) return true;
  }
  return false;
}

std::string LineLengthRule::GetDescription(DescriptionType description_type) {
  return absl::StrCat(
      "Checks that all lines do not exceed the maximum allowed length, "
      "currently set to ",
      LineLengthRule::kMaxLineLength, " characters. See ",
      GetStyleGuideCitation(kTopic), ".");
}

// Returns true if line is an exceptional case that should allow excessive
// length.
static bool AllowLongLineException(TokenSequence::const_iterator token_begin,
                                   TokenSequence::const_iterator token_end) {
  // There may be no tokens on this line if the lexer skipped them.
  // TODO(b/134180314): Preserve all text in lexer.
  if (token_begin == token_end) return true;  // Conservatively ignore.
  auto last_token = token_end - 1;            // Point to last token.
  if (last_token->token_enum == verible::TK_EOF) --last_token;
  // Point to last non-newline.
  if (last_token->token_enum == TK_NEWLINE) --last_token;

  // Ignore leading whitespace, to find first non-space token.
  while (token_begin->token_enum == TK_SPACE) ++token_begin;

  // Single token case:
  // If there is only one token on this line, forgive non-comment tokens,
  // but examine comment tokens deeper.
  if (token_begin == last_token) {
    switch (token_begin->token_enum) {
      case TK_EOL_COMMENT: {
        const absl::string_view text =
            verible::StripCommentAndSpacePadding(token_begin->text);
        // If comment consist of more than one token, it should be split.
        return !ContainsAnyWhitespace(text);
      }
      // TODO(fangism): examine "long string literals"
      // TODO(fangism): case TK_COMMENT_BLOCK:
      // Multi-line comments need deeper inspection.
      default:
        return true;
    }
  }

  // Multi-token cases:
  switch (token_begin->token_enum) {
    case PP_include:
      // TODO(fangism): Could try to be more specific and inspect this line's
      // tokens further, but it is acceptable to forgive all `include lines.
      return true;
    case PP_ifdef:
    case PP_ifndef:
    case PP_endif:
      // Include guards (if they reflect the full path) can be long.
      // TODO(fangism): Could examine lines further and determine whether or
      // not length could have been reduced, but not bothering for now.
      return true;
    // TODO(fangism): Consider whether or not PP_else and PP_elsif should
    // be exempt from length checks as well.
    default:
      break;
  }

  if (last_token->token_enum == TK_COMMENT_BLOCK ||
      last_token->token_enum == TK_EOL_COMMENT) {
    // Check for end-of-line comment that contain lint waivers.
    const absl::string_view text =
        verible::StripCommentAndSpacePadding(last_token->text);
    if (absl::StartsWith(text, "ri lint_check_waive")) {
      // TODO(fangism): Could make this pattern more space-insensitive
      return true;
    }
    if (absl::StartsWith(text, kLinterTrigger)) {
      // This is the waiver for this linter tool.
      return true;
    }
    // TODO(fangism): add "noformat" formatter directives.
  }

  return false;
}

void LineLengthRule::Lint(const TextStructureView& text_structure,
                          absl::string_view) {
  size_t lineno = 0;
  for (const auto& line : text_structure.Lines()) {
    VLOG(2) << "Examining line: " << lineno + 1;
    if (line.length() > kMaxLineLength) {
      const auto token_range = text_structure.TokenRangeOnLine(lineno);
      // Recall that token_range is *unfiltered* and may contain non-essential
      // whitespace 'tokens'.
      if (!AllowLongLineException(token_range.begin(), token_range.end())) {
        // Fake a token that marks the offending range of text.
        TokenInfo token(TK_OTHER, line.substr(kMaxLineLength));
        violations_.insert(LintViolation(token, kMessage));
      }
    }
    ++lineno;
  }
}

LintRuleStatus LineLengthRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
