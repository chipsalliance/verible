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

#include <initializer_list>
#include <string_view>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "verible/common/lexer/lexer-test-util.h"
#include "verible/common/lexer/lexer.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"

namespace verible {
namespace {

class FakeTokenSequenceLexer : public Lexer, public FakeLexer {
 public:
  using FakeLexer::SetTokensData;

  const TokenInfo &GetLastToken() const final { return *tokens_iter_; }

  const TokenInfo &DoNextToken() final { return FakeLexer::DoNextToken(); }

  void Restart(std::string_view) final {}

  bool TokenIsError(const TokenInfo &) const override {  // not yet final.
    return false;
  }
};

TEST(MakeTokenGeneratorTest, Generate) {
  static constexpr std::string_view abc("abc");
  static constexpr std::string_view xyz("xyz");
  FakeTokenSequenceLexer lexer;
  std::initializer_list<TokenInfo> tokens = {
      {1, abc},
      {2, xyz},
      {TK_EOF, ""},
  };
  lexer.SetTokensData(tokens);
  auto generator = MakeTokenGenerator(&lexer);
  EXPECT_EQ(generator(), TokenInfo(1, abc));
  EXPECT_EQ(generator(), TokenInfo(2, xyz));
  EXPECT_TRUE(generator().isEOF());
  // cannot call this generator any further after an EOF
}

TEST(MakeTokenSequenceTest, Sequencer) {
  FakeTokenSequenceLexer lexer;
  constexpr std::string_view text("abcxyz");
  std::initializer_list<TokenInfo> tokens = {
      {1, text.substr(0, 3)},
      {2, text.substr(3, 3)},
      {TK_EOF, text.substr(6, 0)},
  };
  lexer.SetTokensData(tokens);
  TokenSequence receiver;
  const auto lex_status =
      MakeTokenSequence(&lexer, text, &receiver, [](const TokenInfo &) {});
  EXPECT_TRUE(lex_status.ok());
  EXPECT_EQ(receiver, TokenSequence(tokens));
}

class TheNumberTwoIsErrorLexer : public FakeTokenSequenceLexer {
  bool TokenIsError(const TokenInfo &token) const final {
    return token.token_enum() == 2;
  }
};

TEST(MakeTokenSequenceTest, SequencerWithError) {
  TheNumberTwoIsErrorLexer lexer;
  constexpr std::string_view text("abcxyz");
  std::initializer_list<TokenInfo> tokens = {
      {1, text.substr(0, 3)},
      {2, text.substr(3, 3)},  // error token
      {TK_EOF, text.substr(6, 0)},
  };
  lexer.SetTokensData(tokens);
  TokenSequence receiver;
  TokenSequence errors;
  const auto lex_status = MakeTokenSequence(
      &lexer, text, &receiver,
      [&](const TokenInfo &error_token) { errors.push_back(error_token); });
  EXPECT_FALSE(lex_status.ok());
  ASSERT_EQ(receiver.size(), 2);  // includes error token
  ASSERT_EQ(errors.size(), 1);
  EXPECT_EQ(receiver.front(), *tokens.begin());
  EXPECT_EQ(receiver.back(), *(tokens.begin() + 1));
  EXPECT_EQ(errors.front().token_enum(), 2);
  EXPECT_EQ(errors.front(), receiver.back());
}

}  // namespace
}  // namespace verible
