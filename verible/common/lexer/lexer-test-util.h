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

// lexer_test_util.h defines some templates for testing lexers by
// comparing individual tokens or sequences of tokens.
// By declaring test data as one of the following structures:
//   SimpleTestData, GenericTestDataSequence, SynthesizedLexerTestData
// the TestLexer function template will select the appropriate
// implementation to run the tests (using function overloading).

#ifndef VERIBLE_COMMON_LEXER_LEXER_TEST_UTIL_H_
#define VERIBLE_COMMON_LEXER_LEXER_TEST_UTIL_H_

#include <cstddef>
#include <initializer_list>
#include <iosfwd>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/common/text/token-info.h"

namespace verible {

// Modeled after the Lexer base class.
class FakeLexer {
 protected:
  explicit FakeLexer() = default;

  void SetTokensData(const std::vector<TokenInfo> &tokens);

 public:
  const TokenInfo &DoNextToken();

 protected:
  std::vector<TokenInfo> tokens_;
  std::vector<TokenInfo>::const_iterator tokens_iter_;
};

// Streamable adaptor for displaying code on error.
// Usage: stream << ShowCode{text};
// Consider this private, only intended for use in this library.
struct ShowCode {
  std::string_view text;
};

std::ostream &operator<<(std::ostream &, const ShowCode &);

// SimpleTestData is used to verify single token values.
struct SimpleTestData {
  const char *code;

  // Check for ignored token, that is, EOF.
  template <class Lexer>
  void testIgnored() const {
    Lexer lexer(code);
    auto token = lexer.DoNextToken();
    EXPECT_EQ(TK_EOF, token.token_enum) << ShowCode{code};
  }

  // Check for a single-character token, then EOF.
  template <class Lexer>
  void testSingleChar() const {
    Lexer lexer(code);
    EXPECT_EQ(code[0], lexer.DoNextToken().token_enum()) << ShowCode{code};
    EXPECT_EQ(TK_EOF, lexer.DoNextToken().token_enum()) << ShowCode{code};
  }

  // Check for a single token, then EOF.
  template <class Lexer>
  void testSingleToken(const int expected_token) const {
    Lexer lexer(code);
    const TokenInfo &next_token(lexer.DoNextToken());
    EXPECT_EQ(expected_token, next_token.token_enum()) << ShowCode{code};
    const TokenInfo &last_token(lexer.DoNextToken());
    EXPECT_EQ(TK_EOF, last_token.token_enum()) << ShowCode{code};
  }
};

// GenericTestDataSequence tests multiple tokens in single string.
// This is useful for testing tokens sensitive to lexer start-conditions.
// TODO(b/139743437): phase this out in favor of SynthesizedLexerTestData.
struct GenericTestDataSequence {
  const char *code;
  const std::initializer_list<int> expected_tokens;

  template <class Lexer>
  void test() const {
    Lexer lexer(code);
    int i = 0;
    for (const auto &expected_token_enum : expected_tokens) {
      const TokenInfo &next_token(lexer.DoNextToken());
      EXPECT_EQ(expected_token_enum, next_token.token_enum())
          << "    Code[" << i << "]:" << ShowCode{code}
          << "\n    Last token text: \"" << next_token.text() << "\"";
      ++i;
    }
    const TokenInfo &last_token(lexer.DoNextToken());
    EXPECT_EQ(TK_EOF, last_token.token_enum())
        << "    expecting " << (expected_tokens.size() - i)
        << " more tokens: " << ShowCode{code};
  }
};

// Encapsulates both input code and expected tokens by concatenating
// expected tokens' text into a single string.
struct SynthesizedLexerTestData : public TokenInfoTestData {
  SynthesizedLexerTestData(std::initializer_list<ExpectedTokenInfo> fragments)
      : TokenInfoTestData(fragments) {}

  // Runs the given Lexer on the synthesized code of this test case.
  template <class Lexer>
  void test() const {
    Lexer lexer(code);
    int i = 0;
    for (const auto &expected_token : expected_tokens) {
      VerifyExpectedToken(&lexer, expected_token);
      ++i;
    }
    const TokenInfo &final_token(lexer.DoNextToken());
    EXPECT_EQ(TK_EOF, final_token.token_enum())
        << " expecting " << (expected_tokens.size() - i) << " more tokens"
        << ShowCode{code};
  }

 private:
  // A single expected_text can span multiple tokens, when we're only checking
  // string contents, and not checking *how* this excerpt is tokenized.
  template <class Lexer>
  void DontCareMultiTokens(Lexer *lexer, std::string_view expected_text) const {
    // Consume tokens and compare string fragments against the
    // expected_text until the text is fully matched.
    while (!expected_text.empty()) {
      const TokenInfo &next_token = lexer->DoNextToken();
      const size_t token_length = next_token.text().length();
      ASSERT_LE(token_length, expected_text.length())
          << "\nlast token: " << next_token << ShowCode{code};
      // Verify that the remaining expected_text starts with token's text.
      EXPECT_EQ(expected_text.substr(0, token_length), next_token.text())
          << ShowCode{code};

      // Trim from the front the token that was just consumed.
      expected_text.remove_prefix(token_length);
    }
  }

  // Check lexer output against a single expected_token.
  template <class Lexer>
  void VerifyExpectedToken(Lexer *lexer,
                           const TokenInfo &expected_token) const {
    switch (expected_token.token_enum()) {
      case ExpectedTokenInfo::kDontCare:
        DontCareMultiTokens(lexer, expected_token.text());
        break;
      case ExpectedTokenInfo::kNoToken:
        return;
      default:
        // Compare full TokenInfo, enum, text (exact range).
        const TokenInfo &next_token = lexer->DoNextToken();
        EXPECT_EQ(expected_token, next_token) << ShowCode{code};
    }
  }
};

// These types and objects help dispatch the right overload of TestLexer.
struct IgnoredText {};
inline constexpr IgnoredText Ignored{};
struct SingleCharTok {};
inline constexpr SingleCharTok SingleChar{};

// Test for ignored tokens.
template <class Lexer>
void TestLexer(std::initializer_list<SimpleTestData> test_data,
               const IgnoredText &not_used) {
  for (const auto &test_case : test_data) {
    test_case.testIgnored<Lexer>();
  }
}

// Test for single-character tokens (returned value == that character).
template <class Lexer>
void TestLexer(std::initializer_list<SimpleTestData> test_data,
               const SingleCharTok &not_used) {
  for (const auto &test_case : test_data) {
    test_case.testSingleChar<Lexer>();
  }
}

// Test for the same kind of token (passed in arg).
template <class Lexer>
void TestLexer(std::initializer_list<SimpleTestData> test_data,
               const int expected_token) {
  for (const auto &test_case : test_data) {
    test_case.testSingleToken<Lexer>(expected_token);
  }
}

// Test for sequences of expected tokens.
template <class Lexer>
void TestLexer(std::initializer_list<GenericTestDataSequence> test_data) {
  for (const auto &test_case : test_data) {
    test_case.test<Lexer>();
  }
}

// Test for sequences of expected tokens.
template <class Lexer>
void TestLexer(std::initializer_list<SynthesizedLexerTestData> test_data) {
  for (const auto &test_case : test_data) {
    test_case.test<Lexer>();
  }
}

}  // namespace verible

#endif  // VERIBLE_COMMON_LEXER_LEXER_TEST_UTIL_H_
