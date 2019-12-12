// Copyright 2017-2019 The Verible Authors.
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

#include "common/formatting/unwrapped_line.h"

#include <iterator>
#include <sstream>
#include <vector>

#include "gtest/gtest.h"
#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line_test_utils.h"
#include "common/util/container_iterator_range.h"

namespace verible {
namespace {

TEST(PartitionPolicyTest, Printing) {
  {
    std::ostringstream stream;
    stream << PartitionPolicyEnum::kAlwaysExpand;
    EXPECT_EQ(stream.str(), "always-expand");
  }
  {
    std::ostringstream stream;
    stream << PartitionPolicyEnum::kFitOnLineElseExpand;
    EXPECT_EQ(stream.str(), "fit-else-expand");
  }
}

// This test fixture inherits from UnwrappedLineMemoryHandler so that
// UnwrappedLine's internal references can safely point to backed storage.
class UnwrappedLineTest : public UnwrappedLineMemoryHandler,
                          public testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

// Testing IsEmpty() and initialization of UnwrappedLine with no FormatTokens
TEST_F(UnwrappedLineTest, EmptySuccess) {
  UnwrappedLine uwline(0, pre_format_tokens_.begin());
  EXPECT_TRUE(uwline.IsEmpty());
  EXPECT_EQ(uwline.Size(), 0);
}

// Testing IndentationSpaces()
TEST_F(UnwrappedLineTest, DepthTests) {
  UnwrappedLine uwline_no_depth(0, pre_format_tokens_.begin());
  UnwrappedLine uwline_depth(500, pre_format_tokens_.begin());
  EXPECT_EQ(uwline_no_depth.IndentationSpaces(), 0);
  EXPECT_EQ(uwline_depth.IndentationSpaces(), 500);
  uwline_depth.SetIndentationSpaces(22);
  EXPECT_EQ(uwline_depth.IndentationSpaces(), 22);
}

// Testing PartitionPolicy()
TEST_F(UnwrappedLineTest, PartitionPolicyTests) {
  UnwrappedLine uwline(0, pre_format_tokens_.begin(),
                       PartitionPolicyEnum::kFitOnLineElseExpand);
  EXPECT_EQ(uwline.PartitionPolicy(),
            PartitionPolicyEnum::kFitOnLineElseExpand);
  uwline.SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);
  EXPECT_EQ(uwline.PartitionPolicy(), PartitionPolicyEnum::kAlwaysExpand);
}

// Testing SpanNextToken()
TEST_F(UnwrappedLineTest, SpanNextToken) {
  const std::vector<TokenInfo> tokens = {{1, "test_token1"},
                                         {2, "test_token2"}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(0, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);

  const auto& front_token = tokens.front();
  const auto& back_token = tokens.back();
  const auto range = uwline.TokensRange();
  EXPECT_EQ(range.front().TokenEnum(), front_token.token_enum);
  EXPECT_EQ(range.back().TokenEnum(), back_token.token_enum);
  EXPECT_EQ(uwline.Size(), 2);

  uwline.SpanUpToToken(range.begin());  // clear range
  EXPECT_TRUE(uwline.IsEmpty());

  uwline.SpanNextToken();
  EXPECT_FALSE(uwline.IsEmpty());
  EXPECT_EQ(uwline.Size(), 1);
}

// Testing that SpanUpToToken resets the upper-bound.
TEST_F(UnwrappedLineTest, SpanUpToToken) {
  const std::vector<TokenInfo> tokens = {
      {0, "test_token1"}, {1, "test_token2"}, {2, "test_token3"}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(0, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);

  EXPECT_EQ(uwline.Size(), 3);
  auto range = uwline.TokensRange();
  EXPECT_EQ(range.size(), 3);
  const auto end = range.end();

  uwline.SpanUpToToken(range.begin());  // clear range
  EXPECT_TRUE(uwline.IsEmpty());
  range = uwline.TokensRange();
  EXPECT_TRUE(range.empty());

  uwline.SpanUpToToken(end - 1);
  range = uwline.TokensRange();
  EXPECT_EQ(range.size(), 2);
  EXPECT_EQ(range.end(), end - 1);
}

// Testing AddToken(PreFormatToken& token) with multiple tokens
TEST_F(UnwrappedLineTest, AddMultipleTokens) {
  const std::vector<TokenInfo> tokens = {
      {0, "test_token1"}, {1, "test_token2"}, {2, "test_token3"}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(0, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);

  EXPECT_EQ(uwline.Size(), 3);
  const auto range = uwline.TokensRange();
  EXPECT_EQ(range.front().Text(), tokens.front().text);
  EXPECT_EQ(range.back().Text(), tokens.back().text);
}

// Testing final formatting of FormattedText, empty contents.
TEST_F(UnwrappedLineTest, FormattedTextEmpty) {
  const std::vector<TokenInfo> tokens;
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(
      0, pre_format_tokens_.begin());  // indentation level doesn't matter
  FormattedExcerpt output(uwline);
  std::ostringstream stream;
  stream << output;
  EXPECT_TRUE(stream.str().empty());
  EXPECT_EQ(output.Render(), "");
}

// Testing final formatting of FormattedText, with contents.
TEST_F(UnwrappedLineTest, FormattedTextNonEmpty) {
  const std::vector<TokenInfo> tokens = {
      {0, "test_token1"}, {1, "test_token2"}, {2, "test_token3"}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(4, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  auto& ftokens = pre_format_tokens_;
  // Pretend we've committed formatting decisions from an optimizer.
  ftokens[0].before.spaces_required = 4;
  ftokens[1].before.spaces_required = 1;
  ftokens[2].before.spaces_required = 2;
  ftokens[2].before.break_decision = SpacingOptions::MustWrap;
  FormattedExcerpt output(uwline);
  std::ostringstream stream;
  stream << output;
  const char expected[] = R"(    test_token1 test_token2
  test_token3)";
  EXPECT_EQ(expected, stream.str());
  EXPECT_EQ(expected, output.Render());
}

// Make sure that formatting methods all handle the empty tokens case.
TEST_F(UnwrappedLineTest, FormattedTextPreserveSpacesNoTokens) {
  const std::vector<TokenInfo> tokens;
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(4, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  FormattedExcerpt output(uwline);
  {
    std::ostringstream stream;
    stream << output;
    EXPECT_TRUE(stream.str().empty());
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingSpace(stream);
    EXPECT_TRUE(stream.str().empty());
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, true);
    EXPECT_TRUE(stream.str().empty());
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, false);
    EXPECT_TRUE(stream.str().empty());
  }
}

TEST_F(UnwrappedLineTest, FormattedTextPreserveSpacesWithTokens) {
  const absl::string_view text("  aaa  bbb   cc");
  const std::vector<TokenInfo> tokens = {// "aaa", "bbb", "cc"
                                         {0, text.substr(2, 3)},
                                         {1, text.substr(7, 3)},
                                         {2, text.substr(13, 2)}};
  CreateTokenInfosExternalStringBuffer(tokens);  // use 'text' buffer
  UnwrappedLine uwline(4, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  auto& ftokens = pre_format_tokens_;
  // Don't care about other before.* fields when preserving
  ftokens[0].before.preserved_space_start = text.begin() + 0;
  ftokens[0].before.break_decision = SpacingOptions::Preserve;
  ftokens[1].before.preserved_space_start = text.begin() + 5;
  ftokens[1].before.break_decision = SpacingOptions::Preserve;
  ftokens[2].before.preserved_space_start = text.begin() + 10;
  ftokens[2].before.break_decision = SpacingOptions::Preserve;
  FormattedExcerpt output(uwline);
  {
    std::ostringstream stream;
    stream << output;
    EXPECT_EQ(stream.str(), text);
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingSpace(stream);
    EXPECT_EQ(stream.str(), text);
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, true);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "    aaa  bbb   cc");
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, false);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "\n    aaa  bbb   cc");
  }
}

TEST_F(UnwrappedLineTest, FormattedTextPreserveSpacesIgnoreDecision) {
  const absl::string_view text("  aaa  bbb   cc");
  const std::vector<TokenInfo> tokens = {// "aaa", "bbb", "cc"
                                         {0, text.substr(2, 3)},
                                         {1, text.substr(7, 3)},
                                         {2, text.substr(13, 2)}};
  CreateTokenInfosExternalStringBuffer(tokens);  // use 'text' buffer
  UnwrappedLine uwline(4, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  auto& ftokens = pre_format_tokens_;
  // Don't care about other before.* fields when preserving
  ftokens[0].before.preserved_space_start = text.begin() + 0;
  // When calling the Preserve* variants, the spacing decision of the
  // first format token should be ignored.
  ftokens[0].before.break_decision = SpacingOptions::MustAppend;
  ftokens[1].before.preserved_space_start = text.begin() + 5;
  ftokens[1].before.break_decision = SpacingOptions::Preserve;
  ftokens[2].before.preserved_space_start = text.begin() + 10;
  ftokens[2].before.break_decision = SpacingOptions::Preserve;
  FormattedExcerpt output(uwline);
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, true);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "    aaa  bbb   cc");
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, false);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "\n    aaa  bbb   cc");
  }
}

TEST_F(UnwrappedLineTest, FormattedTextPreserveNewlines) {
  const absl::string_view text("\n\naaa\n\nbbb\n\n\ncc");
  const std::vector<TokenInfo> tokens = {
      {0, text.substr(2, 3)}, {1, text.substr(7, 3)}, {2, text.substr(13, 2)}};
  CreateTokenInfosExternalStringBuffer(tokens);  // use 'text' buffer
  UnwrappedLine uwline(4, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  auto& ftokens = pre_format_tokens_;
  // Don't care about other before.* fields when preserving
  ftokens[0].before.preserved_space_start = text.begin() + 0;
  ftokens[0].before.break_decision = SpacingOptions::Preserve;
  ftokens[1].before.preserved_space_start = text.begin() + 5;
  ftokens[1].before.break_decision = SpacingOptions::Preserve;
  ftokens[2].before.preserved_space_start = text.begin() + 10;
  ftokens[2].before.break_decision = SpacingOptions::Preserve;
  FormattedExcerpt output(uwline);
  {
    std::ostringstream stream;
    stream << output;
    EXPECT_EQ(stream.str(), text);
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingSpace(stream);
    EXPECT_EQ(stream.str(), text);
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, true);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "\n\n    aaa\n\nbbb\n\n\ncc");
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, false);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "\n\n    aaa\n\nbbb\n\n\ncc");
  }
}

TEST_F(UnwrappedLineTest, FormattedTextPreserveNewlinesDropSpaces) {
  const absl::string_view text("   \n   aaa  bbb   cc");
  const std::vector<TokenInfo> tokens = {
      {0, text.substr(7, 3)}, {1, text.substr(12, 3)}, {2, text.substr(18, 2)}};
  CreateTokenInfosExternalStringBuffer(tokens);  // use 'text' buffer
  UnwrappedLine uwline(4, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  auto& ftokens = pre_format_tokens_;
  // Don't care about other before.* fields when preserving
  ftokens[0].before.preserved_space_start = text.begin() + 0;
  ftokens[0].before.break_decision = SpacingOptions::Preserve;
  ftokens[1].before.preserved_space_start = text.begin() + 10;
  ftokens[1].before.break_decision = SpacingOptions::Preserve;
  ftokens[2].before.preserved_space_start = text.begin() + 15;
  ftokens[2].before.break_decision = SpacingOptions::Preserve;
  FormattedExcerpt output(uwline);
  {
    std::ostringstream stream;
    stream << output;
    EXPECT_EQ(stream.str(), text);
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingSpace(stream);
    EXPECT_EQ(stream.str(), text);
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, true);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "\n    aaa  bbb   cc");
  }
  {
    std::ostringstream stream;
    output.FormatLinePreserveLeadingNewlines(stream, false);
    // This uses the indentation from the unwrapped lines, which is 4 spaces.
    EXPECT_EQ(stream.str(), "\n    aaa  bbb   cc");
  }
}

// Testing AsCode() with no tokens and no indentation
TEST_F(UnwrappedLineTest, AsCodeEmptyNoIndent) {
  const std::vector<TokenInfo> tokens;
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(0, pre_format_tokens_.begin());
  std::ostringstream stream;
  stream << uwline;
  EXPECT_EQ(stream.str(), "[]");
}

// Testing AsCode() with no tokens and indentation
TEST_F(UnwrappedLineTest, AsCodeEmptyIndent) {
  const std::vector<TokenInfo> tokens;
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(1, pre_format_tokens_.begin());
  std::ostringstream stream;
  stream << uwline;
  EXPECT_EQ(stream.str(), ">[]");
}

// Testing AsCode() with one token and no indentation
TEST_F(UnwrappedLineTest, AsCodeOneTokenNoIndent) {
  const char test[] = "endmodule";
  const std::vector<TokenInfo> tokens = {{0, test}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(0, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  std::ostringstream stream;
  stream << uwline;
  EXPECT_EQ(stream.str(), "[endmodule]");
}

// Testing AsCode() with tokens and no indentation
TEST_F(UnwrappedLineTest, AsCodeTextNoIndent) {
  const std::vector<TokenInfo> tokens = {{0, "module"}, {1, "foo"}, {2, "#("}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(0, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  const char expected[] = "[module foo #(]";
  std::ostringstream stream;
  stream << uwline;
  EXPECT_EQ(stream.str(), expected);
}

// Testing AsCode() with tokens and indentation
TEST_F(UnwrappedLineTest, AsCodeTextIndent) {
  const std::vector<TokenInfo> tokens = {{0, "const"}, {1, "void"}, {2, "foo"},
                                         {3, "("},     {4, ")"},    {5, ";"}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline(5, pre_format_tokens_.begin());
  AddFormatTokens(&uwline);
  const char expected[] = ">>>>>[const void foo ( ) ;]";
  std::ostringstream stream;
  stream << uwline;
  EXPECT_EQ(stream.str(), expected);
}

TEST(PreservedNewlinesCountTest, EmptyString) {
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("", false), 1);
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("", true), 0);
}

TEST(PreservedNewlinesCountTest, Spaces) {
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("  ", false), 1);
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("  ", true), 0);
}

TEST(PreservedNewlinesCountTest, Newlines) {
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("\n", false), 1);
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("\n", true), 1);
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("\t\n\t", false), 1);
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("\t\n\t", true), 1);
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("\n\n", false), 2);
  EXPECT_EQ(FormattedExcerpt::PreservedNewlinesCount("\n\n", true), 2);
}

}  // namespace
}  // namespace verible
