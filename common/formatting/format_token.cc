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

#include "common/formatting/format_token.h"

#include <iostream>
#include <sstream>  // pragma IWYU: keep  // for ostringstream
#include <string>

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "common/strings/range.h"
#include "common/text/token_info.h"
#include "common/util/logging.h"
#include "common/util/spacer.h"

namespace verible {

std::ostream& operator<<(std::ostream& stream, SpacingOptions b) {
  switch (b) {
    case SpacingOptions::Undecided:
      stream << "undecided";
      break;
    case SpacingOptions::MustAppend:
      stream << "must-append";
      break;
    case SpacingOptions::MustWrap:
      stream << "must-wrap";
      break;
    case SpacingOptions::Preserve:
      stream << "preserve";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, GroupBalancing b) {
  switch (b) {
    case GroupBalancing::None:
      stream << "none";
      break;
    case GroupBalancing::Open:
      stream << "open";
      break;
    case GroupBalancing::Close:
      stream << "close";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const InterTokenInfo& t) {
  stream << "{\n  spaces_required: " << t.spaces_required
         << "\n  break_penalty: " << t.break_penalty
         << "\n  break_decision: " << t.break_decision
         << "\n  preserve_space?: " << (t.preserved_space_start != nullptr)
         << "\n}";
  return stream;
}

std::ostream& InterTokenInfo::CompactNotation(std::ostream& stream) const {
  stream << '<';
  // break_penalty is irrelevant when the options are constrained,
  // so don't bother showing it in those cases.
  switch (break_decision) {
    case SpacingOptions::Undecided:
      stream << '_' << spaces_required << ',' << break_penalty;
      break;
    case SpacingOptions::MustAppend:
      stream << "+_" << spaces_required;
      break;
    case SpacingOptions::MustWrap:
      // spaces_required is irrelevant
      stream << "\\n";
      break;
    case SpacingOptions::Preserve:
      stream << "pre";
      break;
  }
  return stream << '>';
}

std::ostream& operator<<(std::ostream& stream, SpacingDecision d) {
  switch (d) {
    case SpacingDecision::Append:
      stream << "append";
      break;
    case SpacingDecision::Wrap:
      stream << "wrap";
      break;
    case SpacingDecision::Preserve:
      stream << "preserve";
      break;
  }
  return stream;
}

static SpacingDecision ConvertSpacing(SpacingOptions opt) {
  switch (opt) {
    case SpacingOptions::MustWrap:
      return SpacingDecision::Wrap;
    case SpacingOptions::MustAppend:
      return SpacingDecision::Append;
    default:  // Undecided, Preserve
      return SpacingDecision::Preserve;
  }
}

InterTokenDecision::InterTokenDecision(const InterTokenInfo& info)
    : spaces(info.spaces_required),
      action(ConvertSpacing(info.break_decision)),
      preserved_space_start(info.preserved_space_start) {}

static absl::string_view OriginalLeadingSpacesRange(const char* begin,
                                                    const char* end) {
  if (begin == nullptr) {
    VLOG(4) << "no original space range";
    return make_string_view_range(end, end);  // empty range
  }
  // The original spacing points into the original string buffer, and may span
  // multiple whitespace tokens.
  VLOG(4) << "non-null original space range";
  return make_string_view_range(begin, end);
}

absl::string_view FormattedToken::OriginalLeadingSpaces() const {
  return OriginalLeadingSpacesRange(before.preserved_space_start,
                                    token->text().begin());
}

std::ostream& FormattedToken::FormattedText(std::ostream& stream) const {
  switch (before.action) {
    case SpacingDecision::Preserve: {
      if (before.preserved_space_start != nullptr) {
        // Calculate string_view range of pre-existing spaces, and print that.
        stream << OriginalLeadingSpaces();
      } else {
        // During testing, we are less interested in Preserve mode due to lack
        // of "original spacing", so fall-back to safe behavior.
        stream << verible::Spacer(before.spaces);
      }
      break;
    }
    case SpacingDecision::Wrap:
      // Never print spaces before a newline.
      stream << '\n';
      ABSL_FALLTHROUGH_INTENDED;
    case SpacingDecision::Append:
      stream << verible::Spacer(before.spaces);
      break;
  }
  return stream << token->text();
}

std::ostream& operator<<(std::ostream& stream, const FormattedToken& token) {
  return token.FormattedText(stream);
}

absl::string_view PreFormatToken::OriginalLeadingSpaces() const {
  return OriginalLeadingSpacesRange(before.preserved_space_start,
                                    token->text().begin());
}

size_t PreFormatToken::LeadingSpacesLength() const {
  if (before.break_decision == SpacingOptions::Preserve &&
      before.preserved_space_start != nullptr) {
    return OriginalLeadingSpaces().length();
  }
  // in other cases (append, wrap), take the spaces_required value.
  return before.spaces_required;
}

std::string PreFormatToken::ToString() const {
  std::ostringstream output_stream;
  output_stream << *this;
  return output_stream.str();
}

// Human readable token information
std::ostream& operator<<(std::ostream& stream, const PreFormatToken& t) {
  // don't care about byte offsets
  return t.token->ToStream(stream << "TokenInfo: ")
         << "\nenum: " << t.format_token_enum << "\nbefore: " << t.before
         << "\nbalance: " << t.balancing << std::endl;
}

}  // namespace verible
