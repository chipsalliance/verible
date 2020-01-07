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

#include "verilog/formatting/comment_controls.h"

#include <iostream>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "common/strings/comment_utils.h"
#include "common/util/logging.h"
#include "verilog/parser/verilog_parser.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace formatter {

void SetRange(std::vector<bool>* disable_set, int start, int end) {
  CHECK_GE(start, 0);
  CHECK_LE(start, end);
  if (end > disable_set->size()) {
    disable_set->resize(end, false);
  }
  for (int i = start; i < end; ++i) {
    (*disable_set)[i] = true;
  }
}

std::vector<bool> DisableFormattingRanges(
    absl::string_view text, const verible::TokenSequence& tokens) {
  static constexpr absl::string_view kTrigger = "verilog_format:";
  static const auto kDelimiters = strings::delimiter::AnyOf(" \t");
  static constexpr int kNullOffset = -1;
  const verible::TokenInfo::Context context(
      text,
      [](std::ostream& stream, int e) { stream << verilog_symbol_name(e); });

  // By default, no text ranges are formatter-disabled.
  int begin_disable_offset = kNullOffset;
  std::vector<bool> disable_set(text.length(), false);
  for (const auto& token : tokens) {
    VLOG(2) << verible::TokenWithContext{token, context};
    switch (token.token_enum) {
      case TK_COMMENT_BLOCK:
      case TK_EOL_COMMENT: {
        // Focus on the space-delimited tokens in the comment text.
        auto commands = verible::StripCommentAndSpacePadding(token.text);
        if (absl::ConsumePrefix(&commands, kTrigger)) {
          const std::vector<absl::string_view> comment_tokens(
              absl::StrSplit(commands, kDelimiters, absl::SkipEmpty()));
          if (!comment_tokens.empty()) {
            // "off" marks the start of a disabling range.
            // "on" marks the end of disabling range.
            if (comment_tokens.front() == "off") {
              if (begin_disable_offset == kNullOffset) {
                begin_disable_offset = token.left(text);
              }  // else ignore
            } else if (comment_tokens.front() == "on") {
              if (begin_disable_offset != kNullOffset) {
                const int end_disable_offset = token.right(text);
                SetRange(&disable_set, begin_disable_offset,
                         end_disable_offset);
                begin_disable_offset = kNullOffset;
              }  // else ignore
            }
          }
        }
        break;
      }
      default:
        break;
    }
  }
  // If the disabling interval remains open, close it (to end-of-buffer).
  if (begin_disable_offset != kNullOffset) {
    SetRange(&disable_set, begin_disable_offset, text.length());
  }
  return disable_set;
}

bool ContainsRange(const std::vector<bool>& intervals, int start, int end) {
  CHECK_GE(start, 0);
  CHECK_LE(start, end);
  if (start == end) return true;  // degenerate case
  if (end > intervals.size()) return false;
  for (int i = start; i < end; ++i) {
    if (!intervals[i]) return false;
  }
  return true;
}

}  // namespace formatter
}  // namespace verilog
