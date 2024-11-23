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

// Generic Lexer interface to be implemented by language-specific lexers.

#ifndef VERIBLE_COMMON_LEXER_LEXER_H_
#define VERIBLE_COMMON_LEXER_LEXER_H_

#include "absl/strings/string_view.h"
#include "verible/common/text/token-info.h"

namespace verible {

class Lexer {
 public:
  virtual ~Lexer() = default;

  Lexer(const Lexer &) = delete;
  Lexer &operator=(const Lexer &) = delete;

  // Returns the last token scanned.
  virtual const TokenInfo &GetLastToken() const = 0;

  // Scan next token and return it.
  virtual const TokenInfo &DoNextToken() = 0;

  // Reset lexer to new input.  Overrides should discard all previous state.
  virtual void Restart(absl::string_view) = 0;

  // Return true if token is a lexical error.
  virtual bool TokenIsError(const TokenInfo &) const = 0;

 protected:
  Lexer() = default;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_LEXER_LEXER_H_
