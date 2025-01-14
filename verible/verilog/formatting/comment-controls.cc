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

#include "verible/verilog/formatting/comment-controls.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "verible/common/strings/comment-utils.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"
#include "verible/common/util/spacer.h"
#include "verible/verilog/parser/verilog-parser.h"
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace formatter {

using verible::ByteOffsetSet;
using verible::EscapeString;

ByteOffsetSet DisableFormattingRanges(std::string_view text,
                                      const verible::TokenSequence &tokens) {
  static constexpr std::string_view kTrigger = "verilog_format:";
  static const auto kDelimiters = absl::ByAnyChar(" \t");
  static constexpr int kNullOffset = -1;
  const verible::TokenInfo::Context context(
      text,
      [](std::ostream &stream, int e) { stream << verilog_symbol_name(e); });

  // By default, no text ranges are formatter-disabled.
  int begin_disable_offset = kNullOffset;
  ByteOffsetSet disable_set;
  for (const auto &token : tokens) {
    VLOG(2) << verible::TokenWithContext{token, context};
    const auto vtoken_enum = verilog_tokentype(token.token_enum());
    if (IsComment(vtoken_enum)) {
      // Focus on the space-delimited tokens in the comment text.
      auto commands = verible::StripCommentAndSpacePadding(token.text());
      if (absl::ConsumePrefix(&commands, kTrigger)) {
        const std::vector<std::string_view> comment_tokens(
            absl::StrSplit(commands, kDelimiters, absl::SkipEmpty()));
        if (!comment_tokens.empty()) {
          // "off" marks the start of a disabling range, at end of comment.
          // "on" marks the end of disabling range, up to the end of comment.
          if (comment_tokens.front() == "off") {
            if (begin_disable_offset == kNullOffset) {
              begin_disable_offset = token.right(text);
              if (vtoken_enum == TK_EOL_COMMENT) {
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
    }
  }
  // If the disabling interval remains open, close it (to end-of-buffer).
  int text_length = static_cast<int>(text.length());
  if (begin_disable_offset != kNullOffset &&
      begin_disable_offset <= text_length) {
    disable_set.Add({begin_disable_offset, text_length});
  }
  return disable_set;
}

ByteOffsetSet EnabledLinesToDisabledByteRanges(
    const verible::LineNumberSet &line_numbers,
    const verible::LineColumnMap &line_column_map) {
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
  const int end_byte = line_column_map.LastLineOffset();
  byte_offsets.Complement({0, end_byte});
  return byte_offsets;
}

static size_t NewlineCount(std::string_view s) {
  return std::count(s.begin(), s.end(), '\n');
}

void FormatWhitespaceWithDisabledByteRanges(
    std::string_view text_base, std::string_view space_text,
    const ByteOffsetSet &disabled_ranges, bool include_disabled_ranges,
    std::ostream &stream) {
  VLOG(3) << __FUNCTION__;
  CHECK(verible::IsSubRange(space_text, text_base));
  const int start = std::distance(text_base.begin(), space_text.begin());
  const int end = start + space_text.length();
  ByteOffsetSet enabled_ranges{{start, end}};  // initial interval set mask
  enabled_ranges.Difference(disabled_ranges);
  VLOG(3) << "space range: [" << start << ", " << end << ')';
  VLOG(3) << "disabled ranges: " << disabled_ranges;
  VLOG(3) << "enabled ranges: " << enabled_ranges;

  // Special case if space_text is empty.
  if (space_text.empty() && start != 0) {
    if (!disabled_ranges.Contains(start)) {
      VLOG(3) << "output: 1*\"\\n\" (empty space text)";
      stream << '\n';
      return;
    }
  }

  // Traverse alternating disabled and enabled ranges.
  bool partially_enabled = false;
  size_t total_enabled_newlines = 0;
  int next_start = start;  // keep track of last consumed position
  for (const auto &range : enabled_ranges) {
    if (include_disabled_ranges) {  // for disabled intervals, print the
                                    // original spacing
      const std::string_view disabled(
          text_base.substr(next_start, range.first - next_start));
      VLOG(3) << "output: \"" << EscapeString{disabled} << "\" (preserved)";
      stream << disabled;
      total_enabled_newlines += NewlineCount(disabled);
    }
    {  // for enabled intervals, preserve only newlines
      const std::string_view enabled(
          text_base.substr(range.first, range.second - range.first));
      const size_t newline_count = NewlineCount(enabled);
      VLOG(3) << "output: " << newline_count << "*\"\\n\" (formatted)";
      stream << verible::Spacer(newline_count, '\n');
      partially_enabled = true;
      total_enabled_newlines += newline_count;
    }
    next_start = range.second;
  }
  if (include_disabled_ranges) {
    // If there is a disabled interval left over, print that.
    const std::string_view final_disabled(
        text_base.substr(next_start, end - next_start));
    VLOG(3) << "output: \"" << EscapeString(final_disabled)
            << "\" (remaining disabled)";
    stream << final_disabled;
    total_enabled_newlines += NewlineCount(final_disabled);
  }
  // Print at least one newline if some subrange was format-enabled.
  if (partially_enabled && total_enabled_newlines == 0 && start != 0) {
    VLOG(3) << "output: 1*\"\\n\"";
    stream << '\n';
  }
}

}  // namespace formatter
}  // namespace verilog
