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

#include "verible/common/formatting/unwrapped-line.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/spacer.h"

namespace verible {

std::ostream &operator<<(std::ostream &stream, PartitionPolicyEnum p) {
  switch (p) {
    case PartitionPolicyEnum::kUninitialized:
      return stream << "uninitialized";
    case PartitionPolicyEnum::kAlwaysExpand:
      return stream << "always-expand";
    case PartitionPolicyEnum::kFitOnLineElseExpand:
      return stream << "fit-else-expand";
    case PartitionPolicyEnum::kTabularAlignment:
      return stream << "tabular-alignment";
    case PartitionPolicyEnum::kAlreadyFormatted:
      return stream << "already-formatted";
    case PartitionPolicyEnum::kInline:
      return stream << "inline";
    case PartitionPolicyEnum::kAppendFittingSubPartitions:
      return stream << "append-fitting-sub-partitions";
    case PartitionPolicyEnum::kJuxtaposition:
      return stream << "juxtaposition";
    case PartitionPolicyEnum::kStack:
      return stream << "stack";
    case PartitionPolicyEnum::kWrap:
      return stream << "wrap";
    case PartitionPolicyEnum::kJuxtapositionOrIndentedStack:
      return stream << "juxtaposition-or-indented-stack";
  }
  LOG(FATAL) << "Unknown partition policy " << int(p);
}

static void TokenFormatter(std::string *out, const PreFormatToken &token,
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

void UnwrappedLine::DefaultOriginPrinter(std::ostream &stream,
                                         const verible::Symbol *symbol) {
  static constexpr int kContextLimit = 25;
  stream << '"' << AutoTruncate{StringSpanOfSymbol(*symbol), kContextLimit}
         << '"';
}

std::ostream *UnwrappedLine::AsCode(
    std::ostream *stream, bool verbose,
    const OriginPrinterFunction &origin_printer) const {
  *stream << Spacer(indentation_spaces_, kIndentationMarker) << '['
          << absl::StrJoin(tokens_, " ",
                           [=](std::string *out, const PreFormatToken &token) {
                             TokenFormatter(out, token, verbose);
                           })
          << "], policy: " << partition_policy_;
  if (origin_ != nullptr) {
    *stream << ", (origin: ";
    origin_printer(*stream, origin_);
    *stream << ")";
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const UnwrappedLine &line) {
  return *line.AsCode(&stream);
}

FormattedExcerpt::FormattedExcerpt(const UnwrappedLine &uwline)
    : indentation_spaces_(uwline.IndentationSpaces()) {
  tokens_.reserve(uwline.Size());
  // Convert working PreFormatTokens (computed from wrap optimization) into
  // decision-bound representation.
  const auto range = uwline.TokensRange();
  std::transform(range.begin(), range.end(), std::back_inserter(tokens_),
                 [](const PreFormatToken &t) { return FormattedToken(t); });
}

std::ostream &FormattedExcerpt::FormattedText(
    std::ostream &stream, bool indent,
    const std::function<bool(const TokenInfo &)> &include_token_p) const {
  if (tokens_.empty()) return stream;
  // Let caller print the preceding/trailing newline.
  if (indent) {
    if (tokens_.front().before.action != SpacingDecision::kPreserve) {
      stream << Spacer(IndentationSpaces());
    }
  }
  // We do not want the indentation before the first token, if it was
  // already handled separately.
  const auto &front = tokens_.front();
  if (include_token_p(*front.token)) {
    VLOG(2) << "action: " << front.before.action;
    if (indent && front.before.action == SpacingDecision::kAlign) {
      // When aligning tokens, the first token might be further indented.
      stream << Spacer(front.before.spaces) << front.token->text();
    } else {
      stream << front.token->text();
    }
  }
  for (const auto &ftoken :
       verible::make_range(tokens_.begin() + 1, tokens_.end())) {
    if (include_token_p(*ftoken.token)) stream << ftoken;
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream,
                         const FormattedExcerpt &excerpt) {
  return excerpt.FormattedText(stream, true);
}

std::string FormattedExcerpt::Render() const {
  std::ostringstream stream;
  FormattedText(stream, true);
  return stream.str();
}

}  // namespace verible
