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

#include "verible/common/text/token-info.h"

#include <iterator>
#include <ostream>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>
#include <vector>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/rebase.h"
#include "verible/common/text/constants.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"

namespace verible {

TokenInfo TokenInfo::EOFToken() {
  static constexpr absl::string_view null_text;
  return {TK_EOF, null_text};
}

TokenInfo TokenInfo::EOFToken(absl::string_view buffer) {
  return {TK_EOF, absl::string_view(buffer.end(), 0)};
}

bool TokenInfo::operator==(const TokenInfo &token) const {
  return token_enum_ == token.token_enum_ &&
         (token_enum_ == TK_EOF ||  // All EOF tokens considered equal.
          BoundsEqual(text_, token.text_));
}

TokenInfo::Context::Context(absl::string_view b)
    : base(b),
      // By default, just print the enum integer value, un-translated.
      token_enum_translator([](std::ostream &stream, int e) { stream << e; }) {}

std::ostream &TokenInfo::ToStream(std::ostream &output_stream,
                                  const Context &context) const {
  output_stream << "(#";
  context.token_enum_translator(output_stream, token_enum_);
  output_stream << " @" << left(context.base) << '-' << right(context.base)
                << ": \"" << absl::CEscape(text_) << "\")";
  const auto dist = std::distance(context.base.end(), text_.end());
  CHECK(IsSubRange(text_, context.base)) << "text.end() is off by " << dist;
  return output_stream;
}

std::ostream &TokenInfo::ToStream(std::ostream &output_stream) const {
  return output_stream << "(#" << token_enum_ << ": \"" << absl::CEscape(text_)
                       << "\")";
}

std::string TokenInfo::ToString(const Context &context) const {
  std::ostringstream output_stream;
  ToStream(output_stream, context);
  return output_stream.str();
}

std::string TokenInfo::ToString() const {
  std::ostringstream output_stream;
  ToStream(output_stream);
  return output_stream.str();
}

void TokenInfo::RebaseStringView(absl::string_view new_text) {
  verible::RebaseStringView(&text_, new_text);
}

void TokenInfo::Concatenate(std::string *out, std::vector<TokenInfo> *tokens) {
  ConcatenateTokenInfos(out, tokens->begin(), tokens->end());
}

// Print human-readable token information.
std::ostream &operator<<(std::ostream &stream, const TokenInfo &token) {
  // This will exclude any byte offset information because the base address
  // of the enclosing stream is not known to this function.
  return token.ToStream(stream);
}

std::ostream &operator<<(std::ostream &stream, const TokenWithContext &t) {
  return t.token.ToStream(stream, t.context);
}

}  // namespace verible
