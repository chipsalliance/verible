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

#ifndef VERIBLE_COMMON_TEXT_TOKEN_INFO_TEST_UTIL_H_
#define VERIBLE_COMMON_TEXT_TOKEN_INFO_TEST_UTIL_H_

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "verible/common/text/token-info.h"

namespace verible {

// Proxy class of a TokenInfo for lexer test case construction.
// This class exists for the sole purpose of being able to easily construct
// lexer test cases from a mixture of plain string literals and TokenInfos.
// There are no new data members in this class, so it is expected to be
// copyable into a TokenInfo without slicing out any information.
// Use this class in containers of TokenInfos that require default
// constructibility.
struct ExpectedTokenInfo : public TokenInfo {
  // This pseudo token_enum signals to the test harness to not bother
  // checking the token enum, and check only the string contents.
  enum {
    kDontCare = -2,
    kNoToken  // kNoToken skips the token
  };

  ExpectedTokenInfo() : TokenInfo(TokenInfo::EOFToken()) {}

  // Arbitrary text constructor for cases where one does not care about
  // the token enum of the string.
  // Implicit construction intentional.
  ExpectedTokenInfo(  // NOLINT(google-explicit-constructor)
      std::string_view token_text)
      : TokenInfo(kDontCare, token_text) {}

  // Arbitrary text constructor for cases where one does not care about
  // the token enum of the string.  This constructor is provided to bypass
  // conversion through string_view, to work directly with string literals.
  // Implicit construction intentional.
  ExpectedTokenInfo(  // NOLINT(google-explicit-constructor)
      const char *token_text)
      : ExpectedTokenInfo(std::string_view(token_text)) {}  // delegating

  // Single-character token constructor, for the cases where the
  // only character of the text **is** the token enum.
  // The internal text string_view set to point to the location of the
  // (internally stored) character literal.
  // This internal storage is necessary because unlike a string literal,
  // a character literal is not guaranteed permanent storage.
  // Implicit construction intentional.
  ExpectedTokenInfo(  // NOLINT(google-explicit-constructor)
      char token_enum_and_text);

  ExpectedTokenInfo(int expected_token_enum, std::string_view expected_text)
      : TokenInfo(expected_token_enum, expected_text) {}

  // Deleted interfaces.
  // Deleted because the (char) constructor points the text string_view member
  // to the internal token_enum member, creating a self-pointer.
  ExpectedTokenInfo(const ExpectedTokenInfo &) = delete;
  ExpectedTokenInfo(ExpectedTokenInfo &&) = delete;
  ExpectedTokenInfo &operator=(const ExpectedTokenInfo &) = delete;
};

static_assert(
    sizeof(ExpectedTokenInfo) == sizeof(TokenInfo),
    "class ExpectedTokenInfo must not introduce any new data members.");

// Encapsulates both input code and expected tokens by concatenating
// expected tokens' text into a single string.
struct TokenInfoTestData {
  // Sequence of expected tokens to find that point into 'code'.
  // Publicly, this should to be const, but cannot be due to initialization
  // details.
  std::vector<TokenInfo> expected_tokens;

  // This needs to own the memory for a newly concatenated string.
  // Publicly, this should to be const, but cannot be due to initialization
  // details.
  std::string code;

  TokenInfoTestData(std::initializer_list<ExpectedTokenInfo> fragments);

  // disallow copy/assign because of relationship between expected_tokens'
  // string_view and code string buffer.
  TokenInfoTestData(const TokenInfoTestData &) = delete;
  TokenInfoTestData &operator=(const TokenInfoTestData &) = delete;
  // No need for moveability (yet).
  TokenInfoTestData(TokenInfoTestData &&) = delete;
  TokenInfoTestData &operator=(TokenInfoTestData &&) = delete;

  // Returns subset of expected_tokens that are *not* enumerated
  // ExpectedTokenInfo::kDontCare.
  std::vector<TokenInfo> FindImportantTokens() const;

  // This variant rebases tokens to a copy of the same 'code' that lives
  // in a different buffer.  This combines FindImportantTokens() with
  // RebaseToCodeCopy().
  std::vector<TokenInfo> FindImportantTokens(std::string_view base) const;

  // Moves the locations of tokens into the range spanned by the 'base' buffer.
  // 'base' is another copy of (this) 'code' (content match is verified).
  void RebaseToCodeCopy(std::vector<TokenInfo> *tokens,
                        std::string_view base) const;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TOKEN_INFO_TEST_UTIL_H_
