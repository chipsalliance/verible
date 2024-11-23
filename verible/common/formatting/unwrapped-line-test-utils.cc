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

#include "verible/common/formatting/unwrapped-line-test-utils.h"

#include <cstddef>
#include <vector>

#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/token-info.h"

namespace verible {

void UnwrappedLineMemoryHandler::CreateTokenInfosExternalStringBuffer(
    const std::vector<TokenInfo> &tokens) {
  // pre-allocate to guarantee address stability, prevent realloc.
  const size_t N = tokens.size();
  token_infos_.reserve(N);
  pre_format_tokens_.reserve(N);
  for (const auto &token : tokens) {
    token_infos_.emplace_back(token.token_enum(), token.text());
    pre_format_tokens_.emplace_back(&token_infos_.back());
  }
  // joined_token_text_ remains unused
}

void UnwrappedLineMemoryHandler::CreateTokenInfos(
    const std::vector<TokenInfo> &tokens) {
  CreateTokenInfosExternalStringBuffer(tokens);
  // Join the token string_view fragments into a single contiguous string buffer
  // and rebase the ranges to point into the new buffer.
  TokenInfo::Concatenate(&joined_token_text_, &token_infos_);
}

void UnwrappedLineMemoryHandler::AddFormatTokens(UnwrappedLine *uwline) {
  for (size_t i = 0; i < pre_format_tokens_.size(); ++i) {
    uwline->SpanNextToken();
    // Note: this leaves PreFormatToken::format_token_enum unset.
  }
}

}  // namespace verible
