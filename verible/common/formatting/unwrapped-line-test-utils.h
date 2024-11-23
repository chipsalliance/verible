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

#ifndef VERIBLE_COMMON_FORMATTING_UNWRAPPED_LINE_TEST_UTILS_H_
#define VERIBLE_COMMON_FORMATTING_UNWRAPPED_LINE_TEST_UTILS_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/token-info.h"

namespace verible {

// Used to handle the memory lifespan of string_views, TokenInfos, and
// PreFormatTokens for UnwrappedLine creation.  UnwrappedLines do not own the
// memory referenced by their internal ranges.
// TODO(fangism): refactor with TokenInfoTestData, both are using similar
// token string concatenation techniques.
class UnwrappedLineMemoryHandler {
 public:
  // Creates TokenInfo objects from the stored token strings and stores them
  // in joined_token_text_ and token_infos_ to be used by FormatTokens.
  // This variant considers the string_views in tokens as disjoint,
  // and automatically joins them into an internal buffer.
  void CreateTokenInfos(const std::vector<TokenInfo> &tokens);

  // Same as CreateTokenInfos(), except that string_views are already owned
  // externally, and do not need to be joined into an internal buffer.
  void CreateTokenInfosExternalStringBuffer(
      const std::vector<TokenInfo> &tokens);

  // Creates format tokens for each of the token info objects passed and
  // spans the entire array in the UnwrappedLine.
  // Call this after CreateTokenInfos().
  void AddFormatTokens(UnwrappedLine *uwline);

  std::vector<PreFormatToken>::iterator GetPreFormatTokensBegin() {
    return pre_format_tokens_.begin();
  }

  // Points to the end of joined_token_text_ string buffer.
  // Same concept as TextStructureView::EOFToken().
  TokenInfo EOFToken() const {
    const absl::string_view s(joined_token_text_);
    return TokenInfo(verible::TK_EOF, absl::string_view(s.end(), 0));
  }

 protected:
  // When joining incoming token texts, store the concatenated result
  // into this buffer, which will ensure that token_infos_ (rebased)
  // string_views will point to valid memory for this object's lifetime.
  std::string joined_token_text_;

  // The TokenInfo objects to be wrapped by the PreFormatTokens.
  // The individual token's .text string_views point into joined_token_text_.
  std::vector<TokenInfo> token_infos_;

 public:
  // PreFormatTokens storage
  std::vector<PreFormatToken> pre_format_tokens_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_UNWRAPPED_LINE_TEST_UTILS_H_
