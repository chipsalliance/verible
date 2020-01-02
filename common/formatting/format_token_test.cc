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

#include "common/formatting/format_token.h"

#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "common/text/token_info.h"
#include "common/util/range.h"

namespace verible {
namespace {

// Tests the string representation of the SpacingOptions enum.
TEST(BreakDecisionTest, StringRep) {
  {
    std::ostringstream stream;
    stream << SpacingOptions::Undecided;
    EXPECT_EQ(stream.str(), "undecided");
  }
  {
    std::ostringstream stream;
    stream << SpacingOptions::MustAppend;
    EXPECT_EQ(stream.str(), "must-append");
  }
  {
    std::ostringstream stream;
    stream << SpacingOptions::MustWrap;
    EXPECT_EQ(stream.str(), "must-wrap");
  }
  {
    std::ostringstream stream;
    stream << SpacingOptions::Preserve;
    EXPECT_EQ(stream.str(), "preserve");
  }
}

// Tests the string representation of the GroupBalancing enum.
TEST(GroupBalancingTest, StringRep) {
  {
    std::ostringstream stream;
    stream << GroupBalancing::None;
    EXPECT_EQ(stream.str(), "none");
  }
  {
    std::ostringstream stream;
    stream << GroupBalancing::Open;
    EXPECT_EQ(stream.str(), "open");
  }
  {
    std::ostringstream stream;
    stream << GroupBalancing::Close;
    EXPECT_EQ(stream.str(), "close");
  }
}

// Test that InterTokenInfo initializes to reasonable values.
TEST(InterTokenInfoTest, Initialization) {
  InterTokenInfo info;
  EXPECT_EQ(0, info.spaces_required);
  EXPECT_EQ(0, info.break_penalty);
  EXPECT_EQ(info.break_decision, SpacingOptions::Undecided);
}

// Test for InterTokenInfo equality.
TEST(InterTokenInfoTest, Equality) {
  InterTokenInfo info1, info2;
  EXPECT_EQ(info1, info1);
  EXPECT_EQ(info1, info2);
}

// Test for InterTokenInfo inequality.
TEST(InterTokenInfoTest, Inequality) {
  InterTokenInfo info1;
  {
    InterTokenInfo info2;
    info2.spaces_required = 66;
    EXPECT_NE(info1, info2);
  }
  {
    InterTokenInfo info3;
    info3.break_penalty = 44;
    EXPECT_NE(info1, info3);
  }
  {
    InterTokenInfo info4;
    info4.break_decision = SpacingOptions::MustAppend;
    EXPECT_NE(info1, info4);
  }
  {
    InterTokenInfo info5;
    info5.break_decision = SpacingOptions::MustWrap;
    EXPECT_NE(info1, info5);
  }
}

// Test that PreFormatToken initializes correctly.
TEST(PreFormatTokenTest, DefaultCtor) {
  PreFormatToken ftoken;
  EXPECT_EQ(ftoken.token, nullptr);
}

// Test that vector of PreFormatToken is resizable.
TEST(PreFormatTokenTest, VectorResizeable) {
  std::vector<PreFormatToken> ftokens;
  ftokens.resize(4);
  EXPECT_EQ(ftokens.size(), 4);
}

TEST(PreFormatTokenTest, OriginalLeadingSpaces) {
  const absl::string_view text("abcdefgh");
  const TokenInfo tok1(1, text.substr(1, 3)), tok2(2, text.substr(5, 2));
  {
    PreFormatToken p1(&tok1), p2(&tok2);
    // original spacing not set
    EXPECT_TRUE(p1.OriginalLeadingSpaces().empty());
    EXPECT_TRUE(p2.OriginalLeadingSpaces().empty());
  }
  {
    PreFormatToken p1(&tok1), p2(&tok2);
    // set original spacing
    p1.before.preserved_space_start = text.begin();
    p2.before.preserved_space_start = tok1.text.end();
    EXPECT_TRUE(BoundsEqual(p1.OriginalLeadingSpaces(), text.substr(0, 1)));
    EXPECT_TRUE(BoundsEqual(p2.OriginalLeadingSpaces(), text.substr(4, 1)));
  }
}

// Test that FormattedText prints correctly.
TEST(FormattedTokenTest, FormattedText) {
  TokenInfo token(0, "roobar");
  PreFormatToken ptoken(&token);
  {
    FormattedToken ftoken(ptoken);
    std::ostringstream stream;
    stream << ftoken;
    EXPECT_EQ("roobar", stream.str());
  }
  {
    FormattedToken ftoken(ptoken);
    ftoken.before.spaces = 3;
    std::ostringstream stream;
    stream << ftoken;
    EXPECT_EQ("   roobar", stream.str());
  }
  {
    FormattedToken ftoken(ptoken);
    ftoken.before.action = SpacingDecision::Wrap;
    std::ostringstream stream;
    stream << ftoken;
    EXPECT_EQ("\nroobar", stream.str());
  }
  {
    FormattedToken ftoken(ptoken);
    ftoken.before.action = SpacingDecision::Wrap;
    ftoken.before.spaces = 2;
    std::ostringstream stream;
    stream << ftoken;
    EXPECT_EQ("\n  roobar", stream.str());
  }
}

TEST(FormattedTokenTest, OriginalLeadingSpaces) {
  const absl::string_view text("abcdefgh");
  const TokenInfo tok1(1, text.substr(1, 3)), tok2(2, text.substr(5, 2));
  const PreFormatToken p1(&tok1), p2(&tok2);
  {
    FormattedToken ft1(p1), ft2(p2);
    // original spacing not set
    EXPECT_TRUE(ft1.OriginalLeadingSpaces().empty());
    EXPECT_TRUE(ft2.OriginalLeadingSpaces().empty());
  }
  {
    FormattedToken ft1(p1), ft2(p2);
    // set original spacing
    ft1.before.preserved_space_start = text.begin();
    ft2.before.preserved_space_start = tok1.text.end();
    EXPECT_TRUE(BoundsEqual(ft1.OriginalLeadingSpaces(), text.substr(0, 1)));
    EXPECT_TRUE(BoundsEqual(ft2.OriginalLeadingSpaces(), text.substr(4, 1)));
  }
}

TEST(FormattedTokenTest, PreservedSpaces) {
  const absl::string_view text("abcdefgh");
  const TokenInfo tok1(1, text.substr(1, 3)), tok2(2, text.substr(5, 2));
  const PreFormatToken p1(&tok1), p2(&tok2);
  {
    FormattedToken ft1(p1), ft2(p2);
    ft1.before.spaces = 2;
    ft2.before.spaces = 3;
    std::ostringstream stream;
    stream << ft1 << ft2;
    EXPECT_EQ(stream.str(), "  bcd   fg");
  }
  {
    FormattedToken ft1(p1), ft2(p2);
    ft1.before.spaces = 2;  // ignored
    ft1.before.action = SpacingDecision::Preserve;
    ft2.before.spaces = 3;  // ignored
    ft2.before.action = SpacingDecision::Preserve;
    // preserved_space_start takes precedence over the other attributes
    ft1.before.preserved_space_start = text.begin();
    ft2.before.preserved_space_start = tok1.text.end();
    std::ostringstream stream;
    stream << ft1 << ft2;
    // For testing purposes, it doesn't matter what text was in the gap between
    // the tokens, need not be space.
    EXPECT_EQ(stream.str(), "abcdefg");
  }
}

// Test for InterTokenInfo string representation.
TEST(InterTokenInfoTest, StringRep) {
  std::ostringstream stream;
  InterTokenInfo info;
  stream << info;
  EXPECT_EQ(
      R"str({
  spaces_required: 0
  break_penalty: 0
  break_decision: undecided
  preserve_space?: 0
})str",
      stream.str());
}

// Test that Length() returns the correct distance between L and R location of
// an Empty TokenInfo
TEST(PreFormatTokenTest, FormatTokenLengthEmptyTest) {
  // Empty
  TokenInfo empty_token_info(0, "");
  PreFormatToken empty_format_token(&empty_token_info);
  EXPECT_EQ(0, empty_format_token.Length());
}

// Test that Length() returns the correct distance between L and R location of
// TokenInfo
TEST(PreFormatTokenTest, FormatTokenLengthTest) {
  // Valid Location
  TokenInfo token_info(1, "Hello World!");
  PreFormatToken format_token(&token_info);
  EXPECT_EQ(12, format_token.Length());
}

// Test for PreFormatToken's string representation.
TEST(PreFormatTokenTest, StringRep) {
  TokenInfo token_info(1, "Hello");
  PreFormatToken format_token(&token_info);
  std::string str(format_token.ToString());
  absl::string_view strv(str);
  EXPECT_NE(strv.find("TokenInfo:"), strv.npos);
  EXPECT_NE(strv.find("before:"), strv.npos);
  EXPECT_NE(strv.find("break_decision:"), strv.npos);
}

}  // namespace
}  // namespace verible
