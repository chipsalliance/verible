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

#include "common/formatting/tree_annotator.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/formatting/format_token.h"
#include "common/text/constants.h"
#include "common/text/token_info.h"
#include "common/text/tree_builder_test_util.h"
#include "common/util/iterator_range.h"

namespace verible {
namespace {

void DoNothing(const PreFormatToken&, PreFormatToken*,
               const SyntaxTreeContext&) {}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, EmptyFormatTokens) {
  std::vector<PreFormatToken> ftokens;
  AnnotateFormatTokensUsingSyntaxContext(nullptr, TokenInfo::EOFToken(),
                                         ftokens.begin(), ftokens.end(),
                                         DoNothing);
  // Reaching here is success.
}

constexpr int kForcedSpaces = 5;

void ForceSpaces(const PreFormatToken&, PreFormatToken* right,
                 const SyntaxTreeContext&) {
  right->before.spaces_required = kForcedSpaces;
}

// The first format token never gets annotated.
template <class T>
iterator_range<typename T::const_iterator> ExcludeFirst(const T& t) {
  return make_range(t.begin() + 1, t.end());
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, UnusedContext) {
  const absl::string_view text("abc");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)},
      {5, text.substr(1, 1)},
      {6, text.substr(2, 1)},
      {verible::TK_EOF, text.substr(3, 0)},  // EOF
  };
  std::vector<PreFormatToken> ftokens;
  for (const auto& t : tokens) {
    ftokens.emplace_back(&t);
  }
  AnnotateFormatTokensUsingSyntaxContext(nullptr, tokens[3], ftokens.begin(),
                                         ftokens.end(), ForceSpaces);
  for (const auto& ftoken : ExcludeFirst(ftokens)) {
    EXPECT_EQ(ftoken.before.spaces_required, kForcedSpaces);
  }
}

void LeftIsB(const PreFormatToken& left, PreFormatToken* right,
             const SyntaxTreeContext&) {
  if (left.token->text == "b") {
    right->before.spaces_required = kForcedSpaces;
  } else {
    right->before.spaces_required = kForcedSpaces + 1;
  }
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, UnusedContextBasedOnLeft) {
  const absl::string_view text("abc");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)},
      {5, text.substr(1, 1)},
      {6, text.substr(2, 1)},
      {verible::TK_EOF, text.substr(3, 0)},  // EOF
  };
  std::vector<PreFormatToken> ftokens;
  for (const auto& t : tokens) {
    ftokens.emplace_back(&t);
  }
  AnnotateFormatTokensUsingSyntaxContext(nullptr, tokens[3], ftokens.begin(),
                                         ftokens.end(), LeftIsB);
  EXPECT_EQ(ftokens[1].before.spaces_required, kForcedSpaces + 1);
  EXPECT_EQ(ftokens[2].before.spaces_required, kForcedSpaces);
}

void ContextDirectParentIsNine(const PreFormatToken&, PreFormatToken* right,
                               const SyntaxTreeContext& context) {
  if (context.DirectParentIs(9)) {
    right->before.spaces_required = kForcedSpaces;
  } else {
    right->before.spaces_required = kForcedSpaces + 2;
  }
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, UsingContext) {
  const absl::string_view text("abc");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)},
      {5, text.substr(1, 1)},
      {6, text.substr(2, 1)},
      {verible::TK_EOF, text.substr(3, 0)},  // EOF
  };
  std::vector<PreFormatToken> ftokens;
  for (const auto& t : tokens) {
    ftokens.emplace_back(&t);
  }
  const auto tree = TNode(6,                          // synthesized syntax tree
                          TNode(7, Leaf(tokens[0])),  //
                          TNode(8, Leaf(tokens[1])),  //
                          TNode(9, Leaf(tokens[2]))   //
  );
  AnnotateFormatTokensUsingSyntaxContext(&*tree, tokens[3], ftokens.begin(),
                                         ftokens.end(),
                                         ContextDirectParentIsNine);
  EXPECT_EQ(ftokens[1].before.spaces_required, kForcedSpaces + 2);
  EXPECT_EQ(ftokens[2].before.spaces_required, kForcedSpaces);
}

}  // namespace
}  // namespace verible
