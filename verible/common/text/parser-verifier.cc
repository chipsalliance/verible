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

#include "verible/common/text/parser-verifier.h"

#include <functional>
#include <vector>

#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"

namespace verible {

void ParserVerifier::Visit(const SyntaxTreeLeaf &leaf) {
  for (;;) {
    // Check to stop if reached end of stream or end of file
    if (view_iterator_ == view_.end() || (**view_iterator_).isEOF()) return;

    const TokenInfo &view_token = **view_iterator_;
    if (token_comparator_(view_token, leaf.get())) {
      // Found a matching token, continue to next leaf
      view_iterator_++;
      return;
    }
    // Failed to find a matching token.
    unmatched_tokens_.push_back(view_token);
    view_iterator_++;
  }
}

std::vector<TokenInfo> ParserVerifier::Verify() {
  unmatched_tokens_.clear();
  view_iterator_ = view_.begin();

  root_.Accept(this);

  // If a leaf was never visited, add all tokens in view to unmatched tokens
  for (; view_iterator_ != view_.end() && !(**view_iterator_).isEOF();
       view_iterator_++) {
    unmatched_tokens_.push_back(**view_iterator_);
  }

  return unmatched_tokens_;
}

}  // namespace verible
