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

// Defines various adapters to yield a TokenInfo generator.

#ifndef VERIBLE_COMMON_LEXER_TOKEN_STREAM_ADAPTER_H_
#define VERIBLE_COMMON_LEXER_TOKEN_STREAM_ADAPTER_H_

#include <functional>
#include <iterator>
#include <string_view>
#include <type_traits>

#include "absl/status/status.h"
#include "verible/common/lexer/lexer.h"
#include "verible/common/lexer/token-generator.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"

namespace verible {

// Creates a TokenInfo generator from a Lexer object.
TokenGenerator MakeTokenGenerator(Lexer *l);

// Populates a TokenSequence with lexed tokens.
absl::Status MakeTokenSequence(
    Lexer *lexer, std::string_view text, TokenSequence *tokens,
    const std::function<void(const TokenInfo &)> &error_token_handler);

// Generic container-to-iterator-generator adapter.
// Once the end is reached, keep returning the end iterator.
template <class Container>
std::function<typename Container::const_iterator()> MakeConstIteratorStreamer(
    const Container &c) {
  auto iter = c.begin();
  const auto end = c.end();
  return [=]() mutable {
    // Retain iterator state between calls (mutable lambda).
    return (iter != end) ? iter++ : end;
  };
}

// Creates a TokenInfo generator from a sequence of TokenInfo.
template <class Container>
TokenGenerator MakeTokenStreamer(const Container &c) {
  using value_type = typename Container::value_type;
  static_assert(std::is_same_v<value_type, TokenInfo>,
                "Container must have TokenInfo elements.");
  const auto end = c.end();
  auto streamer = MakeConstIteratorStreamer(c);
  return [=]() {
    const auto iter = streamer();
    return (iter != end) ? *iter : TokenInfo::EOFToken();
  };
}

// Creates a TokenInfo generator from a sequence of TokenInfo iterators.
template <class Container>
TokenGenerator MakeTokenViewer(const Container &c) {
  using value_type = typename Container::value_type;
  using iter_type = std::iterator_traits<value_type>;
  using element_type = typename iter_type::value_type;
  static_assert(std::is_same_v<element_type, TokenInfo>,
                "Container must have iterators to TokenInfo.");
  const auto end = c.end();
  auto streamer = MakeConstIteratorStreamer(c);
  return [=]() {
    const auto iter = streamer();
    return (iter != end) ? **iter : TokenInfo::EOFToken();
  };
}

}  // namespace verible

#endif  // VERIBLE_COMMON_LEXER_TOKEN_STREAM_ADAPTER_H_
