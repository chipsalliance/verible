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

#ifndef VERIBLE_COMMON_TEXT_PARSER_VERIFIER_H_
#define VERIBLE_COMMON_TEXT_PARSER_VERIFIER_H_

#include <vector>

#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/text/tree-compare.h"
#include "verible/common/text/visitors.h"

namespace verible {

// ParserVerifier is used to compare a parse tree and a token stream view. It
// Iterates through both, and checks that every token appearing in the view
// also appears in the same order in the tree.
// Unmatched tokens are reported
//
// Usage:
//   const Symbol& root = ...
//   const TokenStreamView& view = ...
//   ParserVerifier verifier(root, view);
//   auto unmatched_tokens = verifier.Verify();
//   ... process unmatched tokens ...

class ParserVerifier : public TreeVisitorRecursive {
 public:
  ParserVerifier(const Symbol &root, const TokenStreamView &view)
      : root_(root), view_(view), view_iterator_(view.begin()) {}

  ParserVerifier(const Symbol &root, const TokenStreamView &view,
                 const TokenComparator &token_comparator)
      : root_(root),
        view_(view),
        view_iterator_(view.begin()),
        token_comparator_(token_comparator) {}

  // Iterates through tree and stream view provided in constructor
  std::vector<TokenInfo> Verify();

  // Do call these methods directly. Use Verify instead
  // TODO(jeremycs): changed these to protected and make SyntaxTreeLeaf
  //                 and SyntaxTreeNode friend classes
  void Visit(const SyntaxTreeLeaf &leaf) final;
  void Visit(const SyntaxTreeNode &node) final{};

 private:
  const Symbol &root_;
  const TokenStreamView &view_;

  // Current position in view. Ensures visit once behavior for each token
  TokenStreamView::const_iterator view_iterator_;

  // List of tokens contained in view that were not found in tree
  std::vector<TokenInfo> unmatched_tokens_;

  TokenComparator token_comparator_ = default_comparator;

  static bool default_comparator(const TokenInfo &t1, const TokenInfo &t2) {
    return t1 == t2;
  }
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_PARSER_VERIFIER_H_
