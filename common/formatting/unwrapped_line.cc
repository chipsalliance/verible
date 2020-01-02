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

#include "common/formatting/unwrapped_line.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/formatting/format_token.h"
#include "common/util/container_iterator_range.h"
#include "common/util/container_util.h"
#include "common/util/spacer.h"

namespace verible {

using verible::container::FindOrDie;

std::ostream& operator<<(std::ostream& stream, PartitionPolicyEnum p) {
  static const auto* enum_names =
      new std::map<PartitionPolicyEnum, const char*>{
          {PartitionPolicyEnum::kAlwaysExpand, "always-expand"},
          {PartitionPolicyEnum::kFitOnLineElseExpand, "fit-else-expand"},
      };
  return stream << FindOrDie(*enum_names, p);
}

static void TokenFormatter(std::string* out, const PreFormatToken& token) {
  absl::StrAppend(out, token.Text());
}

std::ostream* UnwrappedLine::AsCode(std::ostream* stream) const {
  *stream << Spacer(indentation_spaces_, kIndentationMarker) << '['
          << absl::StrJoin(tokens_, " ", TokenFormatter) << ']';
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const UnwrappedLine& line) {
  return *line.AsCode(&stream);
}

FormattedExcerpt::FormattedExcerpt(const UnwrappedLine& uwline)
    : indentation_spaces_(uwline.IndentationSpaces()), tokens_() {
  tokens_.reserve(uwline.Size());
  // Convert working PreFormatTokens (computed from wrap optimization) into
  // decision-bound representation.
  const auto range = uwline.TokensRange();
  std::transform(range.begin(), range.end(), std::back_inserter(tokens_),
                 [](const PreFormatToken& t) { return FormattedToken(t); });
  if (!tokens_.empty()) {
    // Translate indentation depth into first token's before.spaces.
    tokens_.front().before.spaces = indentation_spaces_;
  }
}

static size_t NewlineCount(absl::string_view s) {
  return std::count(s.begin(), s.end(), '\n');
}

// Preserve only the newlines of a string of whitespaces.
size_t FormattedExcerpt::PreservedNewlinesCount(absl::string_view text,
                                                bool is_first_line) {
  // There is a minimum of 1 because this is being printed before
  // a formatter partition that starts on a new line.
  // The very first line, however, is already at the start of a newline,
  // so the minimum need not apply.
  const size_t original_newlines = NewlineCount(text);
  return is_first_line ? original_newlines
                       : std::max(original_newlines, size_t(1));
  // TODO(fangism): max of 1 blank line, even if count>2?
}

std::ostream& FormattedExcerpt::FormatLinePreserveLeadingSpace(
    std::ostream& stream) const {
  if (tokens_.empty()) return stream;

  // Explicitly preserve spaces before first token in each line.
  {
    auto replaced_first_token(tokens_.front());  // copy, then modify
    replaced_first_token.before.action = SpacingDecision::Preserve;
    stream << replaced_first_token;
  }

  const auto remaining_tokens = make_range(tokens_.begin() + 1, tokens_.end());
  for (const auto& ftoken : remaining_tokens) {
    stream << ftoken;
    // Don't print newline here, let next line print pre-existing space.
  }
  return stream;
}

std::ostream& FormattedExcerpt::FormatLinePreserveLeadingNewlines(
    std::ostream& stream, bool is_first_line) const {
  if (tokens_.empty()) return stream;

  // Explicitly preserve newlines before first token in each line.
  {
    auto replaced_first_token(tokens_.front());  // copy, then modify
    const auto original_spacing = replaced_first_token.OriginalLeadingSpaces();
    replaced_first_token.before.action = SpacingDecision::Append;
    // Print preserved newlines, then indentation spaces, then token text.
    stream << Spacer(PreservedNewlinesCount(original_spacing, is_first_line),
                     '\n')
           << replaced_first_token;
  }

  const auto remaining_tokens = make_range(tokens_.begin() + 1, tokens_.end());
  for (const auto& ftoken : remaining_tokens) {
    stream << ftoken;
    // Don't print newline here, let next line print pre-existing space.
  }
  return stream;
}

std::ostream& FormattedExcerpt::FormattedText(std::ostream& stream) const {
  // Let caller print the preceding/trailing newline.
  // Indentation is expected to be accounted for in the first token.
  for (const auto& ftoken : tokens_) {
    stream << ftoken;
  }
  // TODO(fangism): if (!enabled_) print out original formatting.
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const FormattedExcerpt& excerpt) {
  return excerpt.FormattedText(stream);
}

std::string FormattedExcerpt::Render() const {
  std::ostringstream stream;
  FormattedText(stream);
  return stream.str();
}

}  // namespace verible
