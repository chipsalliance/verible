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
#include "common/strings/display_utils.h"
#include "common/text/tree_utils.h"
#include "common/util/container_iterator_range.h"
#include "common/util/container_util.h"
#include "common/util/spacer.h"

namespace verible {

using verible::container::FindOrDie;

std::ostream& operator<<(std::ostream& stream, PartitionPolicyEnum p) {
  static const auto* enum_names =
      new std::map<PartitionPolicyEnum, const char*>{
          {PartitionPolicyEnum::kUninitialized, "uninitialized"},
          {PartitionPolicyEnum::kAlwaysExpand, "always-expand"},
          {PartitionPolicyEnum::kFitOnLineElseExpand, "fit-else-expand"},
          {PartitionPolicyEnum::kTabularAlignment, "tabular-alignment"},
          {PartitionPolicyEnum::kAppendFittingSubPartitions,
           "append-fitting-sub-partitions"},
      };
  return stream << FindOrDie(*enum_names, p);
}

static void TokenFormatter(std::string* out, const PreFormatToken& token,
                           bool verbose) {
  if (verbose) {
    std::ostringstream oss;
    token.before.CompactNotation(oss);
    absl::StrAppend(out, oss.str());
  }
  absl::StrAppend(out, token.Text());
}

void UnwrappedLine::SetIndentationSpaces(int spaces) {
  CHECK_GE(spaces, 0);
  indentation_spaces_ = spaces;
}

std::ostream* UnwrappedLine::AsCode(std::ostream* stream, bool verbose) const {
  constexpr int kContextLimit = 25;  // length limit for displaying spanned text
  *stream << Spacer(indentation_spaces_, kIndentationMarker) << '['
          << absl::StrJoin(tokens_, " ",
                           [=](std::string* out, const PreFormatToken& token) {
                             TokenFormatter(out, token, verbose);
                           })
          << "], policy: " << partition_policy_;
  if (origin_ != nullptr) {
    *stream << ", (origin: \""
            << AutoTruncate{StringSpanOfSymbol(*origin_), kContextLimit}
            << "\")";
  }
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
    // Beware, however, that other attempts to adjust before.spaces (e.g.
    // tabular alignment) may get clobbered by this.
    // The workaround in those cases is to compute this first spacing as a sum
    // of indentation and left-padding.
  }
}

std::ostream& FormattedExcerpt::FormattedText(std::ostream& stream,
                                              bool indent) const {
  // Let caller print the preceding/trailing newline.
  if (indent) {
    // Indentation is expected to be accounted for in the first token.
    for (const auto& ftoken : tokens_) {
      stream << ftoken;
    }
  } else {
    // We do not want the indentation before the first token, if it was
    // already handled separately.
    stream << tokens_.front().token->text;
    for (const auto& ftoken :
         verible::make_range(tokens_.begin() + 1, tokens_.end())) {
      stream << ftoken;
    }
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const FormattedExcerpt& excerpt) {
  return excerpt.FormattedText(stream, true);
}

std::string FormattedExcerpt::Render() const {
  std::ostringstream stream;
  FormattedText(stream, true);
  return stream.str();
}

}  // namespace verible
