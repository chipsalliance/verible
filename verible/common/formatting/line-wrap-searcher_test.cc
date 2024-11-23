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

#include "verible/common/formatting/line-wrap-searcher.h"

#include <cstddef>
#include <sstream>
#include <vector>

#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line-test-utils.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/token-info.h"

namespace verible {
namespace {

// This test class just binds a set of style parameters.
class SearchLineWrapsTestFixture : public UnwrappedLineMemoryHandler,
                                   public ::testing::Test {
 public:
  SearchLineWrapsTestFixture() {
    // Shorter column limit makes test case examples shorter.
    style_.column_limit = 20;
    style_.indentation_spaces = 3;
    style_.wrap_spaces = 6;
    style_.over_column_limit_penalty = 80;
  }

  int LevelsToSpaces(int levels) const {
    return levels * style_.indentation_spaces;
  }

  FormattedExcerpt SearchLineWraps(const UnwrappedLine &uwline,
                                   const BasicFormatStyle &style) {
    // Bound the size of search for unit testing.
    const auto results = verible::SearchLineWraps(uwline, style, 1000);
    EXPECT_FALSE(results.empty());
    EXPECT_TRUE(results.front().CompletedFormatting());
    return results.front();
  }

 protected:
  BasicFormatStyle style_;
};

// Test that wrap search works on an empty input.
TEST_F(SearchLineWrapsTestFixture, EmptyRange) {
  const std::vector<TokenInfo> tokens;
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(0), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_TRUE(uwline_in.TokensRange().empty());
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_TRUE(formatted_line.Tokens().empty());
  EXPECT_EQ(formatted_line.IndentationSpaces(), 0);
}

// Test that wrap search works on a single token.
TEST_F(SearchLineWrapsTestFixture, OneToken) {
  // The enums of these tokens don't matter.
  const std::vector<TokenInfo> tokens = {{0, "aaa"}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(0), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 0;
  ftokens_in[0].before.spaces_required = 99;  // should be ignored
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), 1);
  const auto &ftokens_out = formatted_line.Tokens();
  // First token should never break.
  EXPECT_EQ(ftokens_out.front().before.action, SpacingDecision::kAppend);
  EXPECT_EQ(formatted_line.Render(), "aaa");
}

// Test that wrap search works on a single token, starting indented.
TEST_F(SearchLineWrapsTestFixture, OneTokenIndented) {
  // The enums of these tokens don't matter.
  const std::vector<TokenInfo> tokens = {{0, "bbbb"}};
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(2), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 0;
  ftokens_in[0].before.spaces_required = 77;  // should be ignored
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), 1);
  const auto &ftokens_out = formatted_line.Tokens();
  // First token should never break.
  EXPECT_EQ(ftokens_out.front().before.action, SpacingDecision::kAppend);
  // 2 indentation levels, 3 spaces each = 6 spaces
  EXPECT_EQ(formatted_line.Render(), "      bbbb");
}

// Test that wrap search works tokens that fit on one line.
TEST_F(SearchLineWrapsTestFixture, FitsOnOneLine) {
  const std::vector<TokenInfo> tokens = {
      {0, "zz"},
      {0, "yyy"},
      {0, "xxxx"},
      {0, "wwwww"},
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 77;  // should be ignored
  ftokens_in[1].before.break_penalty = 1;
  ftokens_in[1].before.spaces_required = 1;
  ftokens_in[2].before.break_penalty = 1;
  ftokens_in[2].before.spaces_required = 1;
  ftokens_in[3].before.break_penalty = 1;
  ftokens_in[3].before.spaces_required = 1;
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), tokens.size());
  const auto &ftokens_out = formatted_line.Tokens();
  // Since all tokens fit on one line, expect no breaks.
  EXPECT_EQ(ftokens_out[0].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[1].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[2].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[3].before.action, SpacingDecision::kAppend);
  // Exactly 20 columns, which is the limit.
  EXPECT_EQ(formatted_line.Render(), "   zz yyy xxxx wwwww");
}

// Test that wrapping keeps formatted tokens under the column limit.
TEST_F(SearchLineWrapsTestFixture, WrapsToNextLine) {
  const std::vector<TokenInfo> tokens = {
      {0, "zz"},
      {0, "yyy"},
      {0, "xxxx"},
      {0, "wwwwww"},  // One character more than the previous test case.
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 77;  // should be ignored
  ftokens_in[1].before.break_penalty = 1;
  ftokens_in[1].before.spaces_required = 1;
  ftokens_in[2].before.break_penalty = 1;
  ftokens_in[2].before.spaces_required = 1;
  ftokens_in[3].before.break_penalty = 1;
  ftokens_in[3].before.spaces_required = 1;
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), tokens.size());
  const auto &ftokens_out = formatted_line.Tokens();
  // First token should never break.
  EXPECT_EQ(ftokens_out[0].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[1].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[2].before.action, SpacingDecision::kAppend);
  // Last token must wrap.
  EXPECT_EQ(ftokens_out[3].before.action, SpacingDecision::kWrap);
  // Would be 21 columns without wrapping.
  EXPECT_EQ(formatted_line.Render(),
            "   zz yyy xxxx\n"
            "         wwwwww");
}

// Test that wrapping keeps formatted tokens under the column limit.
// Extended to test multiple wraps.
TEST_F(SearchLineWrapsTestFixture, WrapsToNextLineMultiple) {
  const std::vector<TokenInfo> tokens = {
      {0, "zz"},
      {0, "yyy"},
      {0, "xxxx"},
      {0, "wwwwww"},  // Expect this one to wrap (from previous test case)
      {0, "a"},
      {0, "1"},
      {0, "2"},  // Expect to break before this token.
      {0, "3"},
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 33;  // should be ignored
  for (size_t i = 1; i < tokens.size(); ++i) {
    ftokens_in[i].before.break_penalty = 1;
    ftokens_in[i].before.spaces_required = 1;
  }
  // forced tie-breaker among otherwise equally good formattings:
  ftokens_in[5].before.break_penalty = 2;
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), tokens.size());
  const auto &ftokens_out = formatted_line.Tokens();
  EXPECT_EQ(ftokens_out[0].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[1].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[2].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[3].before.action, SpacingDecision::kWrap);
  EXPECT_EQ(ftokens_out[4].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[5].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[6].before.action, SpacingDecision::kWrap);
  EXPECT_EQ(ftokens_out[7].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(formatted_line.Render(),
            "   zz yyy xxxx\n"
            "         wwwwww a 1\n"
            "         2 3");
}

// Test that wrapping keeps formatted tokens under the column limit.
// Testing different before.spaces_required.
TEST_F(SearchLineWrapsTestFixture, WrapsToNextLineMultipleDifferentSpaces) {
  const std::vector<TokenInfo> tokens = {
      {0, "zz"},     {0, "==="}, {0, "xxxx"},
      {0, "wwwwww"}, {0, "a"},  // Expect to break before this token.
      {0, "1"},      {0, "2"},   {0, "3"},
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 33;  // should be ignored
  ftokens_in[1].before.break_penalty = 1;
  ftokens_in[1].before.spaces_required = 0;
  ftokens_in[2].before.break_penalty = 1;
  ftokens_in[2].before.spaces_required = 0;
  for (size_t i = 3; i < tokens.size(); ++i) {
    ftokens_in[i].before.break_penalty = 1;
    ftokens_in[i].before.spaces_required = 2;  // more spacing
  }
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), tokens.size());
  const auto &ftokens_out = formatted_line.Tokens();
  EXPECT_EQ(ftokens_out[0].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[1].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[2].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[3].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[4].before.action, SpacingDecision::kWrap);
  EXPECT_EQ(ftokens_out[5].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[6].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[7].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(formatted_line.Render(),
            "   zz===xxxx  wwwwww\n"
            "         a  1  2  3");
}

// Test that wrapping search honors SpacingOptions::No as a constraint.
TEST_F(SearchLineWrapsTestFixture, ForcedJoins) {
  const std::vector<TokenInfo> tokens = {
      {0, "aaaaaa"},
      {0, "bbbbb"},  // Normally this would fit on first line, but being forced
      {0, "ccccc"},  // to adhere to this token, it must break earlier.
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 11;  // should be ignored
  ftokens_in[1].before.break_penalty = 1;
  ftokens_in[1].before.spaces_required = 1;
  ftokens_in[2].before.break_penalty = 1;
  ftokens_in[2].before.spaces_required = 1;
  ftokens_in[2].before.break_decision =
      SpacingOptions::kMustAppend;  // This causes search to break earlier.
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), tokens.size());
  const auto &ftokens_out = formatted_line.Tokens();
  EXPECT_EQ(ftokens_out[0].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[1].before.action, SpacingDecision::kWrap);
  EXPECT_EQ(ftokens_out[2].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(formatted_line.Render(),
            "   aaaaaa\n"
            "         bbbbb ccccc");
}

// Test that wrapping search honors SpacingOptions::MustWrap as a constraint.
TEST_F(SearchLineWrapsTestFixture, ForcedWraps) {
  const std::vector<TokenInfo> tokens = {
      {0, "aaaaaa"},
      {0, "bbbbb"},  // Force a break here.
      {0, "ccccc"},
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 11;  // should be ignored
  ftokens_in[1].before.break_penalty = 1;
  ftokens_in[1].before.spaces_required = 1;
  ftokens_in[1].before.break_decision = SpacingOptions::kMustWrap;
  ftokens_in[2].before.break_penalty = 1;
  ftokens_in[2].before.spaces_required = 1;
  const FormattedExcerpt formatted_line = SearchLineWraps(uwline_in, style_);
  EXPECT_EQ(formatted_line.Tokens().size(), tokens.size());
  const auto &ftokens_out = formatted_line.Tokens();
  EXPECT_EQ(ftokens_out[0].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(ftokens_out[1].before.action, SpacingDecision::kWrap);
  EXPECT_EQ(ftokens_out[2].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(formatted_line.Render(),
            "   aaaaaa\n"  // Force break before 'bbbbb'
            "         bbbbb ccccc");
}

// Test multiple equally good wrapping solutions can be found and diagnosed.
TEST_F(SearchLineWrapsTestFixture, DisplayEquallyOptimalWrappings) {
  const std::vector<TokenInfo> tokens = {
      {0, "aaaaaaaaaa"},
      {0, "bbbbb"},
      {0, "ccccc"},
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 11;  // should be ignored
  ftokens_in[1].before.break_penalty = 3;
  ftokens_in[1].before.spaces_required = 1;
  ftokens_in[2].before.break_penalty = ftokens_in[1].before.break_penalty;
  ftokens_in[2].before.spaces_required = 1;
  const auto formatted_lines = verible::SearchLineWraps(uwline_in, style_, 10);
  EXPECT_EQ(formatted_lines.size(), 1);
  // By total cost alone, expected solutions are:
  //   break before token[1] and
  //   break before token[2].
  // However, using a tie-breaker like terminal-column position, will favor
  // equally good solutions that break earlier.
  const auto &first = formatted_lines.front();
  EXPECT_EQ(first.Tokens()[1].before.action, SpacingDecision::kAppend);
  EXPECT_EQ(first.Tokens()[2].before.action, SpacingDecision::kWrap);
  std::ostringstream stream;
  DisplayEquallyOptimalWrappings(stream, uwline_in, formatted_lines);
  // Limited output checking.
  EXPECT_TRUE(absl::StrContains(stream.str(), "Found 1 equally good"));
  EXPECT_TRUE(absl::StrContains(stream.str(), "============"));
}

TEST_F(SearchLineWrapsTestFixture, FitsOnLine) {
  const std::vector<TokenInfo> tokens = {
      {0, "aaaaaa"},
      {0, "bbbbb"},
      {0, "ccccc"},
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(0), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.spaces_required = 99;  // irrelevant
  ftokens_in[1].before.spaces_required = 1;
  ftokens_in[2].before.spaces_required = 1;
  EXPECT_TRUE(FitsOnLine(uwline_in, style_).fits);
  EXPECT_EQ(FitsOnLine(uwline_in, style_).final_column, 18);

  uwline_in.SetIndentationSpaces(2);
  // fits: 2 + 6 + 1 + 5 + 1 + 5 = 20
  EXPECT_TRUE(FitsOnLine(uwline_in, style_).fits);
  EXPECT_EQ(FitsOnLine(uwline_in, style_).final_column, 20);

  uwline_in.SetIndentationSpaces(3);
  // not fits: 3 + 6 + 1 + 5 + 1 + 5 = 21
  EXPECT_FALSE(FitsOnLine(uwline_in, style_).fits);
  EXPECT_EQ(FitsOnLine(uwline_in, style_).final_column, 21);

  uwline_in.SetIndentationSpaces(2);
  ftokens_in[1].before.spaces_required = 2;
  // not fits: 2 + 6 + 2 + 5 + 1 + 5 = 21
  EXPECT_FALSE(FitsOnLine(uwline_in, style_).fits);
  EXPECT_EQ(FitsOnLine(uwline_in, style_).final_column, 21);

  ftokens_in[1].before.spaces_required = 1;
  // fits: 2 + 6 + 1 + 5 + 1 + 5 = 20
  EXPECT_TRUE(FitsOnLine(uwline_in, style_).fits);
  EXPECT_EQ(FitsOnLine(uwline_in, style_).final_column, 20);

  ftokens_in[2].before.break_decision =
      SpacingOptions::kMustWrap;  // forced break
  EXPECT_FALSE(FitsOnLine(uwline_in, style_).fits);
  EXPECT_EQ(FitsOnLine(uwline_in, style_).final_column, 14);
}

// Test that aborted wrap search works returns a result marked as incomplete.
TEST_F(SearchLineWrapsTestFixture, AbortedSearch) {
  const std::vector<TokenInfo> tokens = {
      {0, "zz"},
      {0, "yyy"},
      {0, "xxxx"},
  };
  CreateTokenInfos(tokens);
  UnwrappedLine uwline_in(LevelsToSpaces(1), pre_format_tokens_.begin());
  AddFormatTokens(&uwline_in);
  EXPECT_EQ(uwline_in.Size(), tokens.size());
  auto &ftokens_in = pre_format_tokens_;
  ftokens_in[0].before.break_penalty = 1;
  ftokens_in[0].before.spaces_required = 77;  // should be ignored
  ftokens_in[1].before.break_penalty = 1;
  ftokens_in[1].before.spaces_required = 1;
  ftokens_in[2].before.break_penalty = 1;
  ftokens_in[2].before.spaces_required = 1;
  // Intentionally limit search space to a small count to force early abort.
  const auto formatted_lines = verible::SearchLineWraps(uwline_in, style_, 2);
  const FormattedExcerpt &formatted_line = formatted_lines.front();
  EXPECT_EQ(formatted_line.Tokens().size(), tokens.size());
  EXPECT_FALSE(formatted_line.CompletedFormatting());
  // The resulting state is unpredictable, because the search terminated early.
  // So we don't check any other properties of the formatted_line.
}

}  // namespace
}  // namespace verible
