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

// Unit tests for TokenInfo

#include "verible/common/text/token-info.h"

#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/text/constants.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

// Test construction with a token enum and text.
TEST(TokenInfoTest, EnumTextConstruction) {
  constexpr std::string_view text("string of length 19");
  TokenInfo token_info(143, text);
  EXPECT_EQ(token_info.token_enum(), 143);
  EXPECT_EQ(token_info.left(text), 0);
  EXPECT_EQ(token_info.right(text), 19);
  EXPECT_EQ(token_info.text(), text);
}

// Test updating text.
TEST(TokenInfoTest, AdvanceText) {
  constexpr std::string_view text = "This quick brown fox...";
  TokenInfo token_info(1, text.substr(0, 0));
  EXPECT_TRUE(BoundsEqual(token_info.text(), text.substr(0, 0)));
  token_info.AdvanceText(3);
  EXPECT_TRUE(BoundsEqual(token_info.text(), text.substr(0, 3)));
  EXPECT_EQ(token_info.text(), "Thi");
  token_info.AdvanceText(4);
  EXPECT_TRUE(BoundsEqual(token_info.text(), text.substr(3, 4)));
  EXPECT_EQ(token_info.text(), "s qu");
}

// Test operator ==.
TEST(TokenInfoTest, Equality) {
  constexpr char text[] = "string of length 21";
  TokenInfo token_info_1(143, text);
  TokenInfo token_info_2(143, text);
  EXPECT_EQ(token_info_1, token_info_2);
  EXPECT_EQ(token_info_2, token_info_1);
}

// Test operator == EOF.
TEST(TokenInfoTest, EOFEquality) {
  TokenInfo token_info_0 = TokenInfo::EOFToken();
  TokenInfo token_info_1(TK_EOF, "foo");
  TokenInfo token_info_2(TK_EOF, "bar");
  EXPECT_EQ(token_info_0, token_info_1);
  EXPECT_EQ(token_info_1, token_info_0);
  EXPECT_EQ(token_info_0, token_info_2);
  EXPECT_EQ(token_info_2, token_info_0);
  EXPECT_EQ(token_info_1, token_info_2);
  EXPECT_EQ(token_info_2, token_info_1);
}

TEST(TokenInfoTest, EOFWithBuffer) {
  constexpr std::string_view text("string of length 21");
  TokenInfo token_info = TokenInfo::EOFToken(text);
  EXPECT_EQ(token_info.token_enum(), TK_EOF);
  EXPECT_EQ(token_info.text().begin(), text.end());
  EXPECT_EQ(token_info.text().end(), text.end());
}

// Test operator !=.
TEST(TokenInfoTest, Inequality) {
  constexpr std::string_view text("string of length 21");
  const std::vector<TokenInfo> token_infos = {
      TokenInfo(143, text),
      TokenInfo(43, text),
      TokenInfo(143, text.substr(0, 1)),  // substring length 1
      TokenInfo(143, "different string"),
  };
  const auto N = token_infos.size();
  for (size_t i = 0; i < N - 1; ++i) {
    for (size_t j = i + 1; j < N; ++j) {
      EXPECT_NE(token_infos[i], token_infos[j]);
      EXPECT_NE(token_infos[j], token_infos[i]);
    }
  }
}

TEST(TokenInfoTest, EquivalentWithoutLocation) {
  const std::string_view foo1("foo"), foo2("foo");
  TokenInfo token_0(1, foo1);   // reference token
  TokenInfo token_1(1, "bar");  // different text
  TokenInfo token_2(1, foo2);   // different location
  TokenInfo token_3(2, foo1);   // different enum
  EXPECT_FALSE(token_0.EquivalentWithoutLocation(token_1));
  EXPECT_FALSE(token_1.EquivalentWithoutLocation(token_0));
  EXPECT_TRUE(token_0.EquivalentWithoutLocation(token_2));
  EXPECT_TRUE(token_2.EquivalentWithoutLocation(token_0));
  EXPECT_FALSE(token_0.EquivalentWithoutLocation(token_3));
  EXPECT_FALSE(token_3.EquivalentWithoutLocation(token_0));
}

TEST(TokenInfoTest, EquivalentEOF) {
  TokenInfo token_0(TK_EOF, "foo");  // reference token
  TokenInfo token_1(TK_EOF, "bar");  // different text
  TokenInfo token_2(TK_EOF, "foo");  // different location
  EXPECT_TRUE(token_0.EquivalentWithoutLocation(token_1));
  EXPECT_TRUE(token_1.EquivalentWithoutLocation(token_0));
  EXPECT_TRUE(token_0.EquivalentWithoutLocation(token_2));
  EXPECT_TRUE(token_2.EquivalentWithoutLocation(token_0));
}

TEST(TokenInfoTest, EquivalentBySpaceEmpty) {
  TokenInfo t0(1, "");
  TokenInfo t1(1, "");
  EXPECT_TRUE(t0.EquivalentBySpace(t1));
  EXPECT_TRUE(t1.EquivalentBySpace(t0));
}

TEST(TokenInfoTest, EquivalentBySpaceDiffEnum) {
  TokenInfo t0(1, "");
  TokenInfo t1(2, "");
  EXPECT_FALSE(t0.EquivalentBySpace(t1));
  EXPECT_FALSE(t1.EquivalentBySpace(t0));
}

TEST(TokenInfoTest, EquivalentBySpaceDiffLengthText) {
  TokenInfo t0(1, "a");
  TokenInfo t1(1, "aa");
  EXPECT_FALSE(t0.EquivalentBySpace(t1));
  EXPECT_FALSE(t1.EquivalentBySpace(t0));
}

TEST(TokenInfoTest, EquivalentBySpaceDiffTextEqualLength) {
  TokenInfo t0(1, "bb");
  TokenInfo t1(1, "aa");
  EXPECT_TRUE(t0.EquivalentBySpace(t1));
  EXPECT_TRUE(t1.EquivalentBySpace(t0));
}

TEST(TokenInfoTest, EquivalentBySpaceEOF) {
  // text is ignored in EOF case
  TokenInfo t0(TK_EOF, "xyz");
  TokenInfo t1(TK_EOF, "12345");
  EXPECT_TRUE(t0.EquivalentBySpace(t1));
  EXPECT_TRUE(t1.EquivalentBySpace(t0));
}

// This test verifies comparison against EOF.
TEST(TokenInfoTest, IsEOF) {
  TokenInfo token_info = TokenInfo::EOFToken();
  EXPECT_TRUE(token_info.isEOF());
}

TEST(TokenInfoTest, IsEOFAnyString) {
  TokenInfo token_info(TK_EOF, "does not matter");
  EXPECT_TRUE(token_info.isEOF());
}

// Test string representation of token_info.
TEST(TokenInfoTest, ToStringEOF) {
  const std::string_view base;  // empty
  const TokenInfo::Context context(base);
  TokenInfo token_info(TK_EOF, base);
  EXPECT_EQ(token_info.ToString(context), "(#0 @0-0: \"\")");
}

TEST(TokenInfoTest, ToStringWithBase) {
  const std::string_view base("basement cat");
  const TokenInfo::Context context(base);
  TokenInfo token_info(7, base.substr(9, 3));
  EXPECT_EQ(token_info.ToString(context), "(#7 @9-12: \"cat\")");
}

void TokenTranslator(std::ostream &stream, int e) {
  switch (e) {
    case 7:
      stream << "lucky-seven";
      break;
    default:
      stream << "???";
  }
}

TEST(TokenInfoTest, ToStringWithBaseAndTranslator) {
  const std::string_view base("basement cat");
  const TokenInfo::Context context(base, TokenTranslator);
  TokenInfo token_info(7, base.substr(9, 3));
  EXPECT_EQ(token_info.ToString(context), "(#lucky-seven @9-12: \"cat\")");
}

TEST(TokenWithContextTest, StreamOutput) {
  const std::string_view base("basement cat");
  const TokenInfo::Context context(base, TokenTranslator);
  TokenInfo token_info(7, base.substr(9, 3));
  std::ostringstream stream;
  stream << TokenWithContext{token_info, context};
  EXPECT_EQ(stream.str(), "(#lucky-seven @9-12: \"cat\")");
}

// RebaseStringView() tests

// Test that empty string token rebases correctly.
TEST(RebaseStringViewTest, EmptyStringsZeroOffset) {
  const std::string text;
  // We want another empty string, but we need to trick too smart compilers
  // to give us a different memory address.
  std::string substr = "foo";
  substr.resize(0);  // Force empty string such as 'text' but memory space
  ASSERT_NE(text.c_str(), substr.c_str()) << "Mismatch in memory assumption";

  EXPECT_FALSE(BoundsEqual(text, substr));
  TokenInfo token(0, text);
  EXPECT_EQ(token.left(text), 0);
  token.RebaseStringView(substr);
  EXPECT_EQ(token.left(substr), 0);
  EXPECT_EQ(token.text(), substr);
}

// Test that non-empty whole-string copy rebases correctly.
TEST(RebaseStringViewTest, IdenticalCopy) {
  const std::string text = "hello";
  const std::string substr = "hello";  // different memory space
  EXPECT_FALSE(BoundsEqual(text, substr));
  TokenInfo token(3, text);
  EXPECT_EQ(token.left(text), 0);
  token.RebaseStringView(substr);
  EXPECT_EQ(token.left(substr), 0);
  EXPECT_EQ(token.text(), substr);
}

// Test that substring mismatch between new and old is checked.
TEST(RebaseStringViewDeathTest, SubstringMismatch) {
  const std::string_view text = "hell0";
  const std::string_view substr = "hello";
  TokenInfo token(1, text);
  EXPECT_EQ(token.left(text), 0);
  EXPECT_DEATH(token.RebaseStringView(substr),
               "only valid when the new text referenced matches the old text");
}

TEST(RebaseStringViewDeathTest, SubstringMismatch2) {
  const std::string_view text = "hello";
  const std::string_view substr = "Hello";
  TokenInfo token(1, text);
  EXPECT_EQ(token.left(text), 0);
  EXPECT_DEATH(token.RebaseStringView(substr),
               "only valid when the new text referenced matches the old text");
}

// Test that substring in the middle of old string is rebased correctly.
TEST(RebaseStringViewTest, NewSubstringNotAtFront) {
  const std::string_view text = "hello";
  const std::string_view new_base = "xxxhelloyyy";
  TokenInfo token(1, text);
  token.RebaseStringView(new_base.substr(3, 5));
  EXPECT_EQ(token.left(new_base), 3);
  EXPECT_EQ(token.right(new_base), 8);
  EXPECT_EQ(token.text(), text);
}

// Test that substring in the middle of old string is rebased correctly.
TEST(RebaseStringViewTest, UsingCharPointer) {
  const std::string_view text = "hello";
  const std::string_view new_base = "xxxhelloyyy";
  TokenInfo token(1, text);
  token.RebaseStringView(new_base.begin() + 3);  // assume original length
  EXPECT_EQ(token.left(new_base), 3);
  EXPECT_EQ(token.right(new_base), 8);
  EXPECT_EQ(token.text(), text);
}

// Test integration with substr() function rebases correctly.
TEST(RebaseStringViewTest, RelativeToOldBase) {
  const std::string_view full_text = "xxxxxxhelloyyyyy";
  const std::string_view substr = full_text.substr(6, 5);
  EXPECT_EQ(substr, "hello");
  TokenInfo token(1, substr);
  EXPECT_EQ(token.left(full_text), 6);
  EXPECT_EQ(token.text(), substr);
  const std::string_view new_base = "aahellobbb";
  token.RebaseStringView(new_base.substr(2, substr.length()));
  EXPECT_EQ(token.left(new_base), 2);
  EXPECT_EQ(token.right(new_base), 7);
  EXPECT_EQ(token.text(), substr);
}

// Test rebasing into middle of superstring.
TEST(RebaseStringViewTest, MiddleOfSuperstring) {
  const std::string_view dest_text = "xxxxxxhell0yyyyy";
  const std::string_view src_text = "ccchell0ddd";
  const int dest_offset = 6;
  const std::string_view src_substr = src_text.substr(3, 5);
  EXPECT_EQ(src_substr, "hell0");
  TokenInfo token(2, src_substr);
  // src_text[3] lines up with dest_text[6].
  token.RebaseStringView(dest_text.substr(dest_offset, src_substr.length()));
  EXPECT_EQ(token.left(dest_text), dest_offset);
  EXPECT_EQ(token.text(), src_substr);
}

// Test rebasing into prefix superstring.
TEST(RebaseStringViewTest, PrefixSuperstring) {
  const std::string_view dest_text = "xxxhell0yyyyyzzzzzzz";
  const std::string_view src_text = "ccchell0ddd";
  const int dest_offset = 3;
  const std::string_view src_substr = src_text.substr(3, 5);
  EXPECT_EQ(src_substr, "hell0");
  TokenInfo token(1, src_substr);
  // src_text[3] lines up with dest_text[3].
  token.RebaseStringView(dest_text.substr(dest_offset, src_substr.length()));
  EXPECT_EQ(token.left(dest_text), dest_offset);
  EXPECT_EQ(token.text(), src_substr);
}

// Test that concatenation works on the degenerate case of no-tokens.
TEST(TokenInfoConcatenateTest, EmptyTokens) {
  std::string joined;
  std::vector<TokenInfo> tokens;
  TokenInfo::Concatenate(&joined, &tokens);
  EXPECT_TRUE(joined.empty());
  EXPECT_TRUE(tokens.empty());
}

// Test that concatenation works on a single token.
TEST(TokenInfoConcatenateTest, OneToken) {
  std::string joined;
  std::vector<TokenInfo> tokens{
      {3, "foo"},
  };
  TokenInfo::Concatenate(&joined, &tokens);
  EXPECT_EQ(joined, "foo");
  ASSERT_EQ(tokens.size(), 1);
  EXPECT_EQ(tokens[0].token_enum(), 3);
  EXPECT_EQ(tokens[0].text(), "foo");
  EXPECT_EQ(tokens[0].left(joined), 0);
  EXPECT_EQ(tokens[0].right(joined), 3);
}

// Test that concatenation works on multiple tokens.
TEST(TokenInfoConcatenateTest, MultipleTokens) {
  std::string joined;
  std::vector<TokenInfo> tokens{
      {3, "foo"},
      {4, "  "},
      {5, "bar"},
  };
  TokenInfo::Concatenate(&joined, &tokens);
  EXPECT_EQ(joined, "foo  bar");
  EXPECT_EQ(joined.size(), 8);  // 3 + 2 + 3
  ASSERT_EQ(tokens.size(), 3);

  EXPECT_EQ(tokens[0].token_enum(), 3);
  EXPECT_EQ(tokens[0].text(), "foo");
  EXPECT_EQ(tokens[0].left(joined), 0);
  EXPECT_EQ(tokens[0].right(joined), 3);

  EXPECT_EQ(tokens[1].token_enum(), 4);
  EXPECT_EQ(tokens[1].text(), "  ");
  EXPECT_EQ(tokens[1].left(joined), 3);
  EXPECT_EQ(tokens[1].right(joined), 5);

  EXPECT_EQ(tokens[2].token_enum(), 5);
  EXPECT_EQ(tokens[2].text(), "bar");
  EXPECT_EQ(tokens[2].left(joined), 5);
  EXPECT_EQ(tokens[2].right(joined), 8);
}

// Test that concatenation works on multiple tokens, even empty strings.
TEST(TokenInfoConcatenateTest, MultipleTokensWithEmptyString) {
  std::string joined;
  std::vector<TokenInfo> tokens{
      {3, "foo"},
      {6, ""},
      {5, "barr"},
  };
  TokenInfo::Concatenate(&joined, &tokens);
  EXPECT_EQ(joined, "foobarr");
  EXPECT_EQ(joined.size(), 7);  // 3 + 0 + 4
  ASSERT_EQ(tokens.size(), 3);

  EXPECT_EQ(tokens[0].token_enum(), 3);
  EXPECT_EQ(tokens[0].text(), "foo");
  EXPECT_EQ(tokens[0].left(joined), 0);
  EXPECT_EQ(tokens[0].right(joined), 3);

  EXPECT_EQ(tokens[1].token_enum(), 6);
  EXPECT_EQ(tokens[1].text(), "");
  EXPECT_EQ(tokens[1].left(joined), 3);
  EXPECT_EQ(tokens[1].right(joined), 3);

  EXPECT_EQ(tokens[2].token_enum(), 5);
  EXPECT_EQ(tokens[2].text(), "barr");
  EXPECT_EQ(tokens[2].left(joined), 3);
  EXPECT_EQ(tokens[2].right(joined), 7);
}

}  // namespace
}  // namespace verible
