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

#include "verible/common/formatting/format-token.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>  // pragma IWYU: keep  // for ostringstream
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/match.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/strings/position.h"
#include "verible/common/strings/range.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/interval.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"
#include "verible/common/util/spacer.h"

namespace verible {

std::ostream &operator<<(std::ostream &stream, SpacingOptions b) {
  switch (b) {
    case SpacingOptions::kUndecided:
      stream << "undecided";
      break;
    case SpacingOptions::kMustAppend:
      stream << "must-append";
      break;
    case SpacingOptions::kMustWrap:
      stream << "must-wrap";
      break;
    case SpacingOptions::kAppendAligned:
      stream << "append-aligned";
      break;
    case SpacingOptions::kPreserve:
      stream << "preserve";
      break;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, GroupBalancing b) {
  switch (b) {
    case GroupBalancing::kNone:
      stream << "none";
      break;
    case GroupBalancing::kOpen:
      stream << "open";
      break;
    case GroupBalancing::kClose:
      stream << "close";
      break;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const InterTokenInfo &t) {
  stream << "{\n  spaces_required: " << t.spaces_required
         << "\n  break_penalty: " << t.break_penalty
         << "\n  break_decision: " << t.break_decision
         << "\n  preserve_space?: "
         << (t.preserved_space_start != string_view_null_iterator()) << "\n}";
  return stream;
}

std::ostream &InterTokenInfo::CompactNotation(std::ostream &stream) const {
  stream << '<';
  // break_penalty is irrelevant when the options are constrained,
  // so don't bother showing it in those cases.
  switch (break_decision) {
    case SpacingOptions::kUndecided:
      stream << '_' << spaces_required << ',' << break_penalty;
      break;
    case SpacingOptions::kMustAppend:
      stream << "+_" << spaces_required;
      break;
    case SpacingOptions::kMustWrap:
      // spaces_required is irrelevant
      stream << "\\n";
      break;
    case SpacingOptions::kAppendAligned:
      stream << "|_" << spaces_required;
      break;
    case SpacingOptions::kPreserve:
      stream << "pre";
      break;
  }
  return stream << '>';
}

std::ostream &operator<<(std::ostream &stream, SpacingDecision d) {
  switch (d) {
    case SpacingDecision::kAppend:
      stream << "append";
      break;
    case SpacingDecision::kWrap:
      stream << "wrap";
      break;
    case SpacingDecision::kAlign:
      stream << "align";
      break;
    case SpacingDecision::kPreserve:
      stream << "preserve";
      break;
  }
  return stream;
}

static SpacingDecision ConvertSpacing(SpacingOptions opt) {
  switch (opt) {
    case SpacingOptions::kMustWrap:
      return SpacingDecision::kWrap;
    case SpacingOptions::kMustAppend:
      return SpacingDecision::kAppend;
    case SpacingOptions::kAppendAligned:
      return SpacingDecision::kAlign;
    default:  // Undecided, Preserve
      return SpacingDecision::kPreserve;
  }
}

InterTokenDecision::InterTokenDecision(const InterTokenInfo &info)
    : spaces(info.spaces_required),
      action(ConvertSpacing(info.break_decision)),
      preserved_space_start(info.preserved_space_start) {}

static std::string_view OriginalLeadingSpacesRange(
    std::string_view::const_iterator begin,
    std::string_view::const_iterator end) {
  if (begin == string_view_null_iterator()) {
    VLOG(4) << "no original space range";
    return make_string_view_range(end, end);  // empty range
  }
  // The original spacing points into the original string buffer, and may span
  // multiple whitespace tokens.
  VLOG(4) << "non-null original space range";
  return make_string_view_range(begin, end);
}

std::string_view FormattedToken::OriginalLeadingSpaces() const {
  return OriginalLeadingSpacesRange(before.preserved_space_start,
                                    token->text().begin());
}

std::ostream &FormattedToken::FormattedText(std::ostream &stream) const {
  switch (before.action) {
    case SpacingDecision::kPreserve: {
      if (before.preserved_space_start != string_view_null_iterator()) {
        // Calculate string_view range of pre-existing spaces, and print that.
        stream << OriginalLeadingSpaces();
      } else {
        // During testing, we are less interested in Preserve mode due to lack
        // of "original spacing", so fall-back to safe behavior.
        stream << Spacer(before.spaces);
      }
      break;
    }
    case SpacingDecision::kWrap:
      // Never print spaces before a newline.
      stream << '\n';
      ABSL_FALLTHROUGH_INTENDED;
    case SpacingDecision::kAlign:
    case SpacingDecision::kAppend:
      stream << Spacer(before.spaces);
      break;
  }
  return stream << token->text();
}

std::ostream &operator<<(std::ostream &stream, const FormattedToken &token) {
  return token.FormattedText(stream);
}

std::string_view PreFormatToken::OriginalLeadingSpaces() const {
  return OriginalLeadingSpacesRange(before.preserved_space_start,
                                    token->text().begin());
}

size_t PreFormatToken::LeadingSpacesLength() const {
  if (before.break_decision == SpacingOptions::kPreserve &&
      before.preserved_space_start != string_view_null_iterator()) {
    return OriginalLeadingSpaces().length();
  }
  // in other cases (append, wrap), take the spaces_required value.
  return before.spaces_required;
}

int PreFormatToken::ExcessSpaces() const {
  if (before.preserved_space_start == string_view_null_iterator()) return 0;
  const std::string_view leading_spaces = OriginalLeadingSpaces();
  int delta = 0;
  if (!absl::StrContains(leading_spaces, "\n")) {
    delta = static_cast<int>(leading_spaces.length()) - before.spaces_required;
  }
  return delta;
}

std::string PreFormatToken::ToString() const {
  std::ostringstream output_stream;
  output_stream << *this;
  return output_stream.str();
}

// Human readable token information
std::ostream &operator<<(std::ostream &stream, const PreFormatToken &t) {
  // don't care about byte offsets
  return t.token->ToStream(stream << "TokenInfo: ")
         << "\nenum: " << t.format_token_enum << "\nbefore: " << t.before
         << "\nbalance: " << t.balancing << std::endl;
}

void ConnectPreFormatTokensPreservedSpaceStarts(
    std::string_view::const_iterator buffer_start,
    std::vector<PreFormatToken> *format_tokens) {
  VLOG(4) << __FUNCTION__;
  CHECK(buffer_start != string_view_null_iterator());
  for (auto &ftoken : *format_tokens) {
    ftoken.before.preserved_space_start = buffer_start;
    VLOG(4) << "space: " << VisualizeWhitespace(ftoken.OriginalLeadingSpaces());
    buffer_start = ftoken.Text().end();
  }
  // This does not cover the spacing between the last token and EOF.
}

// Finds the span of format tokens covered by the 'byte_offset_range'.
// Run-time: O(lg N) due to binary search
static MutableFormatTokenRange FindFormatTokensInByteOffsetRange(
    std::vector<PreFormatToken>::iterator begin,
    std::vector<PreFormatToken>::iterator end,
    const std::pair<int, int> &byte_offset_range, std::string_view base_text) {
  const auto tokens_begin =
      std::lower_bound(begin, end, byte_offset_range.first,
                       [=](const PreFormatToken &t, int position) {
                         return t.token->left(base_text) < position;
                       });
  const auto tokens_end =
      std::upper_bound(tokens_begin, end, byte_offset_range.second,
                       [=](int position, const PreFormatToken &t) {
                         return position < t.token->right(base_text);
                       });
  return {tokens_begin, tokens_end};
}

void PreserveSpacesOnDisabledTokenRanges(
    std::vector<PreFormatToken> *ftokens,
    const ByteOffsetSet &disabled_byte_ranges, std::string_view base_text) {
  VLOG(2) << __FUNCTION__;
  // saved_iter: shrink bounds of binary search with every iteration,
  // due to monotonic, non-overlapping intervals.
  auto saved_iter = ftokens->begin();
  for (const auto &byte_range : disabled_byte_ranges) {
    // 'disable_range' marks the range of format tokens to be
    // marked as preserving original spacing (i.e. not formatted).
    VLOG(2) << "disabling bytes: " << AsInterval(byte_range);
    const auto disable_range = FindFormatTokensInByteOffsetRange(
        saved_iter, ftokens->end(), byte_range, base_text);
    const std::pair<int, int> disabled_token_indices(SubRangeIndices(
        disable_range, make_range(ftokens->begin(), ftokens->end())));
    VLOG(2) << "disabling tokens: " << AsInterval(disabled_token_indices);

    // kludge: When the disabled range immediately follows a //-style
    // comment, skip past the trailing '\n' (not included in the comment
    // token), which will be printed by the Emit() method, and preserve the
    // whitespaces *beyond* that point up to the start of the following
    // token's text.  This way, rendering the start of the format-disabled
    // excerpt won't get redundant '\n's.
    if (!disable_range.empty()) {
      auto &first = disable_range.front();
      VLOG(3) << "checking whether first ftoken in range is a must-wrap.";
      if (first.before.break_decision == SpacingOptions::kMustWrap) {
        VLOG(3) << "checking if spaces before first ftoken starts with \\n.";
        const std::string_view leading_space = first.OriginalLeadingSpaces();
        // consume the first '\n' from the preceding inter-token spaces
        if (absl::StartsWith(leading_space, "\n")) {
          VLOG(3) << "consuming leading \\n.";
          ++first.before.preserved_space_start;
        }
      }
    }

    // Mark tokens in the disabled range as preserving original spaces.
    for (auto &ft : disable_range) {
      VLOG(2) << "disable-format preserve spaces before: " << *ft.token;
      ft.before.break_decision = SpacingOptions::kPreserve;
    }

    // start next iteration search from previous iteration's end
    saved_iter = disable_range.end();
  }
}

}  // namespace verible
