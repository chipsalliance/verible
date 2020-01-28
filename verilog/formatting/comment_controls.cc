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

ByteOffsetSet DisableFormattingRanges(absl::string_view text,
                                      const verible::TokenSequence& tokens) {
  static constexpr absl::string_view kTrigger = "verilog_format:";
  static const auto kDelimiters = absl::ByAnyChar(" \t");
  static constexpr int kNullOffset = -1;
  const verible::TokenInfo::Context context(
      text,
      [](std::ostream& stream, int e) { stream << verilog_symbol_name(e); });

  // By default, no text ranges are formatter-disabled.
  int begin_disable_offset = kNullOffset;
  ByteOffsetSet disable_set;
  for (const auto& token : tokens) {
    VLOG(2) << verible::TokenWithContext{token, context};
    switch (token.token_enum) {
      case TK_EOL_COMMENT:
      case TK_COMMENT_BLOCK: {
        // Focus on the space-delimited tokens in the comment text.
        auto commands = verible::StripCommentAndSpacePadding(token.text);
        if (absl::ConsumePrefix(&commands, kTrigger)) {
          const std::vector<absl::string_view> comment_tokens(
              absl::StrSplit(commands, kDelimiters, absl::SkipEmpty()));
          if (!comment_tokens.empty()) {
            // "off" marks the start of a disabling range, at end of comment.
            // "on" marks the end of disabling range, up to the end of comment.
            if (comment_tokens.front() == "off") {
              if (begin_disable_offset == kNullOffset) {
                begin_disable_offset = token.right(text);
                if (token.token_enum == TK_EOL_COMMENT) {
                  ++begin_disable_offset;  // to cover the trailing '\n'
                }
              }  // else ignore
            } else if (comment_tokens.front() == "on") {
              if (begin_disable_offset != kNullOffset) {
                const int end_disable_offset = token.right(text);
                if (begin_disable_offset != end_disable_offset) {
                  disable_set.Add({begin_disable_offset, end_disable_offset});
                }
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
    disable_set.Add({begin_disable_offset, static_cast<int>(text.length())});
  }
  return disable_set;
}

ByteOffsetSet EnabledLinesToDisabledByteRanges(
    const LineNumberSet& line_numbers,
    const verible::LineColumnMap& line_column_map) {
  // Interpret empty line numbers as enabling all lines for formatting.
  if (line_numbers.empty()) return ByteOffsetSet();
  // Translate lines to byte offsets (strictly monotonic).
  const int max_line = line_column_map.GetBeginningOfLineOffsets().size() + 1;
  ByteOffsetSet byte_offsets(
      line_numbers.MonotonicTransform<int>([&](int line_number) {
        // line_numbers are 1-based, while OffsetAtLine is 0-based
        const int n = std::max(std::min(line_number, max_line), 1);
        return line_column_map.OffsetAtLine(n - 1);
      }));
  // Invert set to get disabled ranges.
  const int end_byte = line_column_map.EndOffset();
  byte_offsets.Complement({0, end_byte});
  return byte_offsets;
}

}  // namespace formatter
}  // namespace verilog
