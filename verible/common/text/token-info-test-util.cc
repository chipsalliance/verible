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

#include "verible/common/text/token-info-test-util.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <string_view>
#include <vector>

#include "verible/common/text/token-info.h"
#include "verible/common/util/logging.h"

namespace verible {

ExpectedTokenInfo::ExpectedTokenInfo(char token_enum_and_text)
    : TokenInfo(static_cast<int>(token_enum_and_text),
                // Point the string_view to the sub-byte of token_enum that
                // holds the single character value (which also acts as the
                // enum). View the (int) token_enum as a sequence of characters.
                // Depending on the endianness of the underlying machine,
                // the desired byte will be (relative to the reinterpreted base
                // address) at offset 0 or sizeof(int) -1.
                // Note: This constructor using a self-pointer makes this struct
                // non-default-copy/move/assign-able.
                std::string_view(reinterpret_cast<const char *>(&token_enum_)
#ifdef IS_BIG_ENDIAN
                                     + (sizeof(typeid(token_enum_)) - 1)
#endif
                                     ,
                                 1)) {
}

static std::vector<TokenInfo> ComposeExpectedTokensFromFragments(
    std::initializer_list<ExpectedTokenInfo> fragments) {
  std::vector<TokenInfo> expected_tokens;
  expected_tokens.resize(fragments.size(), TokenInfo::EOFToken());
  std::copy(fragments.begin(), fragments.end(), expected_tokens.begin());
  return expected_tokens;  // move
}

TokenInfoTestData::TokenInfoTestData(
    std::initializer_list<ExpectedTokenInfo> fragments)
    :  // Trivially convert ExpectedTokenInfo into TokenInfo.
       // TODO(fangism): For tokens that use the single (char) construction,
       // this copying operation will leave new TokenInfos' text pointing into
       // the source ExpectedTokenInfos' token_enums.  (Deleting the standard
       // interfaces in ExpectedTokenInfo alone does not prevent this.)
       // Under most circumstances, this leakage would be deemed dangerous
       // and unintended, however, since fragments is passed in as an
       // initializer_list (a const-T proxy to a longer-lived memory),
       // this operates safely.  The alternative would be to perform a fix-up
       // over the copy destination array.
      expected_tokens(ComposeExpectedTokensFromFragments(fragments)) {
  // Construct the whole text to lex from fragments.
  TokenInfo::Concatenate(&code, &expected_tokens);
}

std::vector<TokenInfo> TokenInfoTestData::FindImportantTokens() const {
  std::vector<TokenInfo> return_tokens;
  std::copy_if(expected_tokens.begin(), expected_tokens.end(),
               std::back_inserter(return_tokens), [](const TokenInfo &t) {
                 return t.token_enum() != ExpectedTokenInfo::kDontCare;
               });
  return return_tokens;
}

std::vector<TokenInfo> TokenInfoTestData::FindImportantTokens(
    std::string_view base) const {
  std::vector<TokenInfo> return_tokens = FindImportantTokens();
  RebaseToCodeCopy(&return_tokens, base);
  return return_tokens;
}

void TokenInfoTestData::RebaseToCodeCopy(std::vector<TokenInfo> *tokens,
                                         std::string_view base) const {
  CHECK_EQ(code, base);  // verify content match
  // Another analyzer object may have made its own copy of 'code', so
  // we need to translate the expected error token into a rebased version
  // before directly comparing against the rejected tokens.
  for (TokenInfo &token : *tokens) {
    const auto offset =
        std::distance(std::string_view(code).begin(), token.text().begin());
    token.RebaseStringView(base.begin() + offset);
  }
}

}  // namespace verible
