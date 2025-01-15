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

#include <memory>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/text/tree-builder-test-util.h"

namespace verible {

constexpr int NOT_EOF = 1;  // Fake Token Enumeration
static_assert(NOT_EOF != TK_EOF, "NOT_EOF cannot be TK_EOF");

static TokenInfo Token(std::string_view s) { return TokenInfo(NOT_EOF, s); }

static bool equal_text(TokenInfo t1, TokenInfo t2) {
  return t1.text() == t2.text();
}

TEST(ParserVerifierTest, EmptySuccess) {
  auto root = Node();
  TokenSequence stream = {};

  TokenStreamView view;
  InitTokenStreamView(stream, &view);

  ASSERT_NE(root, nullptr);
  ParserVerifier verifier(*root, view, equal_text);
  auto unmatched = verifier.Verify();
  EXPECT_EQ(unmatched.size(), 0);
}

TEST(ParserVerifierTest, SimpleAllMatchSuccess) {
  auto root = Node(Leaf(NOT_EOF, "foo"));
  TokenSequence stream = {Token("foo")};

  TokenStreamView view;
  InitTokenStreamView(stream, &view);

  ASSERT_NE(root, nullptr);
  ParserVerifier verifier(*root, view, equal_text);
  auto unmatched = verifier.Verify();
  EXPECT_EQ(unmatched.size(), 0);
}

TEST(ParserVerifierTest, MultipleAllMatchSuccess) {
  auto root = Node(Leaf(NOT_EOF, "foo"), Leaf(NOT_EOF, "bar"),
                   Node(Leaf(NOT_EOF, "roo"), Leaf(NOT_EOF, "rar")));
  TokenSequence stream = {Token("foo"), Token("bar"), Token("roo"),
                          Token("rar")};

  TokenStreamView view;
  InitTokenStreamView(stream, &view);

  ASSERT_NE(root, nullptr);
  ParserVerifier verifier(*root, view, equal_text);
  auto unmatched = verifier.Verify();
  EXPECT_EQ(unmatched.size(), 0);
}

TEST(ParserVerifierTest, AllUnmatched) {
  auto root = Node();
  TokenSequence stream = {Token("foo"), Token("bar")};

  TokenStreamView view;
  InitTokenStreamView(stream, &view);

  ASSERT_NE(root, nullptr);
  ParserVerifier verifier(*root, view, equal_text);
  auto unmatched = verifier.Verify();
  EXPECT_EQ(unmatched.size(), 2);
  EXPECT_EQ(unmatched, stream);
}

TEST(ParserVerifierTest, PartialUnmatched) {
  constexpr std::string_view foo("foo");
  constexpr std::string_view bar("bar");
  auto root = Node(Leaf(NOT_EOF, foo));
  TokenSequence stream = {Token(foo), Token(bar)};
  TokenSequence unmatched_expected = {Token(bar)};

  TokenStreamView view;
  InitTokenStreamView(stream, &view);

  ASSERT_NE(root, nullptr);
  ParserVerifier verifier(*root, view, equal_text);
  auto unmatched = verifier.Verify();
  EXPECT_EQ(unmatched.size(), 1);
  EXPECT_EQ(unmatched, unmatched_expected);
}

TEST(ParserVerifierTest, SeveralPartialUnmatched) {
  constexpr std::string_view foo("foo");
  constexpr std::string_view bar1("bar1");
  constexpr std::string_view bar2("bar2");
  constexpr std::string_view mee("mee");

  auto root = Node(Leaf(NOT_EOF, foo), Node(Leaf(NOT_EOF, mee)));
  TokenSequence stream = {Token(foo), Token(bar1), Token(bar2), Token(mee)};
  TokenSequence unmatched_expected = {Token(bar1), Token(bar2)};

  TokenStreamView view;
  InitTokenStreamView(stream, &view);

  ASSERT_NE(root, nullptr);
  ParserVerifier verifier(*root, view, equal_text);
  auto unmatched = verifier.Verify();
  EXPECT_EQ(unmatched.size(), 2);
  EXPECT_EQ(unmatched, unmatched_expected);
}

}  // namespace verible
