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

#include "verible/common/formatting/tree-annotator.h"

#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

std::vector<int> ExtractSyntaxTreeContextEnums(
    const SyntaxTreeContext &context) {
  std::vector<int> result;
  for (const auto *node : context) {
    result.push_back(ABSL_DIE_IF_NULL(node)->Tag().tag);
  }
  return result;
}

static void DoNothing(const PreFormatToken &, PreFormatToken *,
                      const SyntaxTreeContext &, const SyntaxTreeContext &) {}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, EmptyFormatTokens) {
  std::vector<PreFormatToken> ftokens;
  AnnotateFormatTokensUsingSyntaxContext(nullptr, TokenInfo::EOFToken(),
                                         ftokens.begin(), ftokens.end(),
                                         DoNothing);
  // Reaching here is success.
}

constexpr int kForcedSpaces = 5;

void ForceSpaces(const PreFormatToken &, PreFormatToken *right,
                 const SyntaxTreeContext & /* left_context */,
                 const SyntaxTreeContext & /* right_context */) {
  right->before.spaces_required = kForcedSpaces;
}

// The first format token never gets annotated.
template <class T>
iterator_range<typename T::const_iterator> ExcludeFirst(const T &t) {
  return make_range(t.begin() + 1, t.end());
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, UnusedContext) {
  const std::string_view text("abc");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)},
      {5, text.substr(1, 1)},
      {6, text.substr(2, 1)},
      {verible::TK_EOF, text.substr(3, 0)},  // EOF
  };
  std::vector<PreFormatToken> ftokens;
  for (const auto &t : tokens) {
    ftokens.emplace_back(&t);
  }
  AnnotateFormatTokensUsingSyntaxContext(nullptr, tokens[3], ftokens.begin(),
                                         ftokens.end(), ForceSpaces);
  for (const auto &ftoken : ExcludeFirst(ftokens)) {
    EXPECT_EQ(ftoken.before.spaces_required, kForcedSpaces);
  }
}

void LeftIsB(const PreFormatToken &left, PreFormatToken *right,
             const SyntaxTreeContext & /* left_context */,
             const SyntaxTreeContext & /* right_context */) {
  if (left.token->text() == "b") {
    right->before.spaces_required = kForcedSpaces;
  } else {
    right->before.spaces_required = kForcedSpaces + 1;
  }
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, UnusedContextBasedOnLeft) {
  const std::string_view text("abc");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)},
      {5, text.substr(1, 1)},
      {6, text.substr(2, 1)},
      {verible::TK_EOF, text.substr(3, 0)},  // EOF
  };
  std::vector<PreFormatToken> ftokens;
  for (const auto &t : tokens) {
    ftokens.emplace_back(&t);
  }
  AnnotateFormatTokensUsingSyntaxContext(nullptr, tokens[3], ftokens.begin(),
                                         ftokens.end(), LeftIsB);
  EXPECT_EQ(ftokens[1].before.spaces_required, kForcedSpaces + 1);
  EXPECT_EQ(ftokens[2].before.spaces_required, kForcedSpaces);
}

void RightContextDirectParentIsNine(const PreFormatToken &,
                                    PreFormatToken *right,
                                    const SyntaxTreeContext &left_context,
                                    const SyntaxTreeContext &right_context) {
  if (right_context.DirectParentIs(9)) {
    right->before.spaces_required = kForcedSpaces;
  } else {
    right->before.spaces_required = kForcedSpaces + 2;
  }
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, UsingRightContext) {
  const std::string_view text("abc");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)},
      {5, text.substr(1, 1)},
      {6, text.substr(2, 1)},
      {verible::TK_EOF, text.substr(3, 0)},  // EOF
  };
  std::vector<PreFormatToken> ftokens;
  for (const auto &t : tokens) {
    ftokens.emplace_back(&t);
  }
  const auto tree = TNode(6,                          // synthesized syntax tree
                          TNode(7, Leaf(tokens[0])),  //
                          TNode(8, Leaf(tokens[1])),  //
                          TNode(9, Leaf(tokens[2]))   //
  );
  AnnotateFormatTokensUsingSyntaxContext(&*tree, tokens[3], ftokens.begin(),
                                         ftokens.end(),
                                         RightContextDirectParentIsNine);
  EXPECT_EQ(ftokens[1].before.spaces_required, kForcedSpaces + 2);
  EXPECT_EQ(ftokens[2].before.spaces_required, kForcedSpaces);
}

void LeftContextDirectParentIsSeven(const PreFormatToken &,
                                    PreFormatToken *right,
                                    const SyntaxTreeContext &left_context,
                                    const SyntaxTreeContext &right_context) {
  if (left_context.DirectParentIs(7)) {
    right->before.spaces_required = kForcedSpaces + 4;
  } else {
    right->before.spaces_required = kForcedSpaces;
  }
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, UsingLeftContext) {
  const std::string_view text("abc");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)},
      {5, text.substr(1, 1)},
      {6, text.substr(2, 1)},
      {verible::TK_EOF, text.substr(3, 0)},  // EOF
  };
  std::vector<PreFormatToken> ftokens;
  for (const auto &t : tokens) {
    ftokens.emplace_back(&t);
  }
  const auto tree = TNode(6,                          // synthesized syntax tree
                          TNode(7, Leaf(tokens[0])),  //
                          TNode(8, Leaf(tokens[1])),  //
                          TNode(9, Leaf(tokens[2]))   //
  );
  AnnotateFormatTokensUsingSyntaxContext(&*tree, tokens[3], ftokens.begin(),
                                         ftokens.end(),
                                         LeftContextDirectParentIsSeven);
  EXPECT_EQ(ftokens[1].before.spaces_required, kForcedSpaces + 4);
  EXPECT_EQ(ftokens[2].before.spaces_required, kForcedSpaces);
}

TEST(AnnotateFormatTokensUsingSyntaxContextTest, VerifySlidingContexts) {
  const std::string_view text("abcdefgh");
  const TokenInfo tokens[] = {
      {4, text.substr(0, 1)}, {5, text.substr(1, 1)},
      {6, text.substr(2, 1)}, {4, text.substr(3, 1)},
      {5, text.substr(4, 1)}, {6, text.substr(5, 1)},
      {4, text.substr(6, 1)}, {verible::TK_EOF, text.substr(7, 0)},  // EOF
  };
  const auto tree = TNode(6,                      // synthesized syntax tree
                          TNode(7,                //
                                Leaf(tokens[0]),  //
                                TNode(10,         //
                                      Leaf(tokens[1]),  //
                                      TNode(12)),       //
                                TNode(11,               //
                                      Leaf(tokens[2]),  //
                                      Leaf(tokens[3]))  //
                                ),                      //
                          TNode(8,                      //
                                Leaf(tokens[4]),        //
                                Leaf(tokens[5])),       //
                          TNode(9, Leaf(tokens[6]))     //
  );
  std::vector<PreFormatToken> ftokens;
  for (const auto &t : tokens) {
    ftokens.emplace_back(&t);
  }
  std::vector<std::vector<int>> saved_contexts;
  saved_contexts.push_back({6, 7});  // first leaf's context
  auto context_listener = [&](const PreFormatToken &, PreFormatToken *,
                              const SyntaxTreeContext &left_context,
                              const SyntaxTreeContext &right_context) {
    // continuity and consistency check
    const auto left_enums = ExtractSyntaxTreeContextEnums(left_context);
    EXPECT_EQ(left_enums, saved_contexts.back());
    saved_contexts.push_back(ExtractSyntaxTreeContextEnums(right_context));
  };
  AnnotateFormatTokensUsingSyntaxContext(&*tree, tokens[7], ftokens.begin(),
                                         ftokens.end(), context_listener);
  using V = std::vector<int>;
  EXPECT_THAT(saved_contexts,
              ElementsAre(  //
                  V({6, 7}), V({6, 7, 10}), V({6, 7, 11}), V({6, 7, 11}),
                  V({6, 8}), V({6, 8}), V({6, 9}), V()));
}

}  // namespace
}  // namespace verible
