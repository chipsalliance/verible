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

#include "verible/common/lexer/token-stream-adapter.h"

#include <functional>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/lexer/lexer.h"
#include "verible/common/lexer/token-generator.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"

namespace verible {

TokenGenerator MakeTokenGenerator(Lexer *l) {
  return [=]() { return l->DoNextToken(); };
}

absl::Status MakeTokenSequence(
    Lexer *lexer, std::string_view text, TokenSequence *tokens,
    const std::function<void(const TokenInfo &)> &error_token_handler) {
  // TODO(fangism): provide a Lexer interface to grab all tokens en masse,
  // which would save virtual function dispatch overhead.
  lexer->Restart(text);
  do {
    const auto &new_token = lexer->DoNextToken();
    tokens->push_back(new_token);
    if (lexer->TokenIsError(new_token)) {  // one more virtual function call
      error_token_handler(new_token);
      // Stop-on-first-error.
      return absl::InvalidArgumentError("Lexical error.");
    }
  } while (!tokens->back().isEOF());
  // Final token is EOF.
  // Force EOF token's text range to be empty, pointing to end of original
  // string.  Otherwise, its range ends up overlapping with the previous token.
  tokens->back() = TokenInfo::EOFToken(text);
  return absl::OkStatus();
}

}  // namespace verible
