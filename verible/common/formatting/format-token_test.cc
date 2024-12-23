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

#include "verible/common/formatting/format-token.h"

#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/unwrapped-line-test-utils.h"
#include "verible/common/strings/position.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

// Tests the string representation of the SpacingOptions enum.
TEST(BreakDecisionTest, StringRep) {
  {
    std::ostringstream stream;
    stream << SpacingOptions::kUndecided;
    EXPECT_EQ(stream.str(), "undecided");
  }
  {
    std::ostringstream stream;
    stream << SpacingOptions::kMustAppend;
    EXPECT_EQ(stream.str(), "must-append");
  }
  {
    std::ostringstream stream;
    stream << SpacingOptions::kMustWrap;
    EXPECT_EQ(stream.str(), "must-wrap");
  }
  {
    std::ostringstream stream;
    stream << SpacingOptions::kPreserve;
    EXPECT_EQ(stream.str(), "preserve");
  }
  {
    std::ostringstream stream;
    stream << SpacingOptions::kAppendAligned;
    EXPECT_EQ(stream.str(), "append-aligned");
  }
}

// Tests the string representation of the GroupBalancing enum.
TEST(GroupBalancingTest, StringRep) {
  {
    std::ostringstream stream;
    stream << GroupBalancing::kNone;
    EXPECT_EQ(stream.str(), "none");
  }
  {
    std::ostringstream stream;
    stream << GroupBalancing::kOpen;
    EXPECT_EQ(stream.str(), "open");
  }
  {
    std::ostringstream stream;
    stream << GroupBalancing::kClose;
    EXPECT_EQ(stream.str(), "close");
  }
}

// Test that InterTokenInfo initializes to reasonable values.
TEST(InterTokenInfoTest, Initialization) {
  InterTokenInfo info;
  EXPECT_EQ(0, info.spaces_required);
  EXPECT_EQ(0, info.break_penalty);
  EXPECT_EQ(info.break_decision, SpacingOptions::kUndecided);
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
    info4.break_decision = SpacingOptions::kMustAppend;
    EXPECT_NE(info1, info4);
  }
  {
    InterTokenInfo info5;
    info5.break_decision = SpacingOptions::kMustWrap;
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
    p2.before.preserved_space_start = tok1.text().end();
    EXPECT_TRUE(BoundsEqual(p1.OriginalLeadingSpaces(), text.substr(0, 1)));
    EXPECT_TRUE(BoundsEqual(p2.OriginalLeadingSpaces(), text.substr(4, 1)));
  }
}

TEST(PreFormatTokenTest, ExcessSpacesNoNewline) {
  const absl::string_view text("abcdefgh");
  const TokenInfo tok(1, text.substr(1, 3));
  PreFormatToken p(&tok);  // before.preserved_space_start == nullptr
  EXPECT_EQ(p.ExcessSpaces(), 0);

  p.before.preserved_space_start = text.begin();
  p.before.spaces_required = 0;
  EXPECT_EQ(p.ExcessSpaces(), 1);

  p.before.spaces_required = 2;
  EXPECT_EQ(p.ExcessSpaces(), -1);
}

TEST(PreFormatTokenTest, ExcessSpacesNewline) {
  const absl::string_view text("\nbcdefgh");
  const TokenInfo tok(1, text.substr(1, 3));
  PreFormatToken p(&tok);  // before.preserved_space_start == nullptr
  EXPECT_EQ(p.ExcessSpaces(), 0);

  p.before.preserved_space_start = text.begin();
  p.before.spaces_required = 0;
  EXPECT_EQ(p.ExcessSpaces(), 0);

  p.before.spaces_required = 2;
  EXPECT_EQ(p.ExcessSpaces(), 0);
}

TEST(PreFormatTokenTest, LeadingSpacesLength) {
  const absl::string_view text("abcdefgh");
  const TokenInfo tok1(1, text.substr(1, 3)), tok2(2, text.substr(5, 2));
  {
    PreFormatToken p1(&tok1), p2(&tok2);
    p1.before.spaces_required = 0;
    p2.before.spaces_required = 3;
    // original spacing not set
    EXPECT_EQ(p1.LeadingSpacesLength(), 0);
    EXPECT_EQ(p2.LeadingSpacesLength(), 3);
  }
  {
    PreFormatToken p1(&tok1), p2(&tok2);
    // set original spacing, but not preserve mode.
    p1.before.preserved_space_start = text.begin();
    p1.before.break_decision = SpacingOptions::kUndecided;
    p1.before.spaces_required = 1;
    p2.before.preserved_space_start = tok1.text().end();
    p2.before.break_decision = SpacingOptions::kUndecided;
    p2.before.spaces_required = 2;
    EXPECT_EQ(p1.LeadingSpacesLength(), 1);
    EXPECT_EQ(p2.LeadingSpacesLength(), 2);
  }
  {
    PreFormatToken p1(&tok1), p2(&tok2);
    // set original spacing and preserve mode.
    p1.before.preserved_space_start = text.begin();
    p1.before.break_decision = SpacingOptions::kPreserve;
    p1.before.spaces_required = 2;
    p2.before.preserved_space_start = tok1.text().end();
    p2.before.break_decision = SpacingOptions::kPreserve;
    p2.before.spaces_required = 4;
    EXPECT_EQ(p1.LeadingSpacesLength(), 1);  // "a"
    EXPECT_EQ(p2.LeadingSpacesLength(), 1);  // "d"
  }
}

class ConnectPreFormatTokensPreservedSpaceStartsTest
    : public ::testing::Test,
      public UnwrappedLineMemoryHandler {};

TEST_F(ConnectPreFormatTokensPreservedSpaceStartsTest, Empty) {
  const char *text = "";
  CreateTokenInfosExternalStringBuffer({});
  ConnectPreFormatTokensPreservedSpaceStarts(text, &pre_format_tokens_);
  EXPECT_TRUE(pre_format_tokens_.empty());
}

TEST_F(ConnectPreFormatTokensPreservedSpaceStartsTest, OneToken) {
  constexpr absl::string_view text("xyz");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 3)},
  });
  ConnectPreFormatTokensPreservedSpaceStarts(text.begin(), &pre_format_tokens_);
  EXPECT_TRUE(BoundsEqual(pre_format_tokens_.front().OriginalLeadingSpaces(),
                          text.substr(0, 0)));
}

TEST_F(ConnectPreFormatTokensPreservedSpaceStartsTest, OneTokenLeadingSpace) {
  constexpr absl::string_view text("  xyz");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(2, 3)},  // "xyz"
  });
  ConnectPreFormatTokensPreservedSpaceStarts(text.begin(), &pre_format_tokens_);
  EXPECT_TRUE(BoundsEqual(pre_format_tokens_.front().OriginalLeadingSpaces(),
                          text.substr(0, 2)));  // "  "
}

TEST_F(ConnectPreFormatTokensPreservedSpaceStartsTest, MultipleTokens) {
  constexpr absl::string_view text("  xyz\t\t\nabc");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(2, 3)},  // "xyz"
      {2, text.substr(8, 3)},  // "abc"
  });
  ConnectPreFormatTokensPreservedSpaceStarts(text.begin(), &pre_format_tokens_);
  EXPECT_TRUE(BoundsEqual(pre_format_tokens_.front().OriginalLeadingSpaces(),
                          text.substr(0, 2)));  // "  "
  EXPECT_TRUE(BoundsEqual(pre_format_tokens_.back().OriginalLeadingSpaces(),
                          text.substr(5, 3)));  // "\t\t\n"
}

class PreserveSpacesOnDisabledTokenRangesTest
    : public ::testing::Test,
      public UnwrappedLineMemoryHandler {};

TEST_F(PreserveSpacesOnDisabledTokenRangesTest, DisableNone) {
  constexpr absl::string_view text("a b c d e");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(2, 1)},
      {1, text.substr(4, 1)},
      {3, text.substr(6, 1)},
      {2, text.substr(8, 1)},
  });
  ByteOffsetSet disabled_bytes;  // empty
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  for (const auto &ftoken : pre_format_tokens_) {
    EXPECT_EQ(ftoken.before.break_decision, SpacingOptions::kUndecided);
  }
}

TEST_F(PreserveSpacesOnDisabledTokenRangesTest, DisableSpaceBeforeText) {
  constexpr absl::string_view text("a b c d e");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(2, 1)},
      {1, text.substr(4, 1)},
      {3, text.substr(6, 1)},
      {2, text.substr(8, 1)},
  });
  ByteOffsetSet disabled_bytes{{5, 7}};  // substring " d"
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  const auto &ftokens = pre_format_tokens_;
  EXPECT_EQ(ftokens[0].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[1].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[2].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[3].before.break_decision,
            SpacingOptions::kPreserve);  // before "d"
  EXPECT_EQ(ftokens[4].before.break_decision, SpacingOptions::kUndecided);
}

TEST_F(PreserveSpacesOnDisabledTokenRangesTest, DisableSpaceAfterText) {
  constexpr absl::string_view text("a b c d e");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(2, 1)},
      {1, text.substr(4, 1)},
      {3, text.substr(6, 1)},
      {2, text.substr(8, 1)},
  });
  ByteOffsetSet disabled_bytes{{4, 6}};  // substring "c "
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  const auto &ftokens = pre_format_tokens_;
  EXPECT_EQ(ftokens[0].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[1].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[2].before.break_decision,
            SpacingOptions::kPreserve);  // "c"
  EXPECT_EQ(ftokens[3].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[4].before.break_decision, SpacingOptions::kUndecided);
}

TEST_F(PreserveSpacesOnDisabledTokenRangesTest, DisableSpanningTwoTokens) {
  constexpr absl::string_view text("a b c d e");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(2, 1)},
      {1, text.substr(4, 1)},
      {3, text.substr(6, 1)},
      {2, text.substr(8, 1)},
  });
  ByteOffsetSet disabled_bytes{{4, 7}};  // substring "c d"
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  const auto &ftokens = pre_format_tokens_;
  EXPECT_EQ(ftokens[0].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[1].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(ftokens[2].before.break_decision,
            SpacingOptions::kPreserve);  // before "c"
  EXPECT_EQ(ftokens[3].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(ftokens[4].before.break_decision, SpacingOptions::kUndecided);
}

TEST_F(PreserveSpacesOnDisabledTokenRangesTest, DisableSpanningMustWrap) {
  constexpr absl::string_view text("a b c d e");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(2, 1)},
      {1, text.substr(4, 1)},
      {3, text.substr(6, 1)},
      {2, text.substr(8, 1)},
  });
  ConnectPreFormatTokensPreservedSpaceStarts(text.begin(), &pre_format_tokens_);
  ByteOffsetSet disabled_bytes{{2, 5}};  // substring "b c"
  pre_format_tokens_[2].before.break_decision = SpacingOptions::kMustWrap;
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  const auto &ftokens = pre_format_tokens_;
  EXPECT_EQ(ftokens[0].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_TRUE(
      BoundsEqual(ftokens[0].OriginalLeadingSpaces(), text.substr(0, 0)));
  EXPECT_EQ(ftokens[1].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_TRUE(
      BoundsEqual(ftokens[1].OriginalLeadingSpaces(), text.substr(1, 1)));
  EXPECT_EQ(ftokens[2].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_TRUE(
      BoundsEqual(ftokens[2].OriginalLeadingSpaces(), text.substr(3, 1)));
  EXPECT_EQ(ftokens[3].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_TRUE(
      BoundsEqual(ftokens[3].OriginalLeadingSpaces(), text.substr(5, 1)));
  EXPECT_EQ(ftokens[4].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_TRUE(
      BoundsEqual(ftokens[4].OriginalLeadingSpaces(), text.substr(7, 1)));
}

TEST_F(PreserveSpacesOnDisabledTokenRangesTest,
       DisableSpanningMustWrapWithNewline) {
  constexpr absl::string_view text("a\nb\nc d e");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(2, 1)},
      {1, text.substr(4, 1)},
      {3, text.substr(6, 1)},
      {2, text.substr(8, 1)},
  });
  ConnectPreFormatTokensPreservedSpaceStarts(text.begin(), &pre_format_tokens_);
  ByteOffsetSet disabled_bytes{{2, 5}};  // substring "b\nc"
  pre_format_tokens_[1].before.break_decision = SpacingOptions::kMustWrap;
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  const auto &ftokens = pre_format_tokens_;
  auto indices = [&text](const absl::string_view &range) {
    return SubRangeIndices(range, text);
  };
  EXPECT_EQ(ftokens[0].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[0].OriginalLeadingSpaces()),
            indices(text.substr(0, 0)));
  EXPECT_EQ(ftokens[1].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(  // \n was consumed
      indices(ftokens[1].OriginalLeadingSpaces()), indices(text.substr(2, 0)));
  EXPECT_EQ(ftokens[2].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(indices(ftokens[2].OriginalLeadingSpaces()),
            indices(text.substr(3, 1)));
  EXPECT_EQ(ftokens[3].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[3].OriginalLeadingSpaces()),
            indices(text.substr(5, 1)));
  EXPECT_EQ(ftokens[4].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[4].OriginalLeadingSpaces()),
            indices(text.substr(7, 1)));
}

TEST_F(PreserveSpacesOnDisabledTokenRangesTest,
       DisableSpanningMustWrapWithNewlineKeepIndentation) {
  constexpr absl::string_view text("a\n  b\n  c d e");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(4, 1)},
      {1, text.substr(8, 1)},
      {3, text.substr(10, 1)},
      {2, text.substr(12, 1)},
  });
  ConnectPreFormatTokensPreservedSpaceStarts(text.begin(), &pre_format_tokens_);
  ByteOffsetSet disabled_bytes{{4, 9}};  // substring "b\n  c"
  pre_format_tokens_[1].before.break_decision = SpacingOptions::kMustWrap;
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  const auto &ftokens = pre_format_tokens_;
  auto indices = [&text](const absl::string_view &range) {
    return SubRangeIndices(range, text);
  };
  EXPECT_EQ(ftokens[0].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[0].OriginalLeadingSpaces()),
            indices(text.substr(0, 0)));
  EXPECT_EQ(ftokens[1].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(  // \n was consumed, "  " remains
      indices(ftokens[1].OriginalLeadingSpaces()), indices(text.substr(2, 2)));
  EXPECT_EQ(ftokens[2].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(indices(ftokens[2].OriginalLeadingSpaces()),
            indices(text.substr(5, 3)));
  EXPECT_EQ(ftokens[3].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[3].OriginalLeadingSpaces()),
            indices(text.substr(9, 1)));
  EXPECT_EQ(ftokens[4].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[4].OriginalLeadingSpaces()),
            indices(text.substr(11, 1)));
}

TEST_F(PreserveSpacesOnDisabledTokenRangesTest, MultipleOffsetRanges) {
  constexpr absl::string_view text("a\nb\nc d e ff gg");
  CreateTokenInfosExternalStringBuffer({
      {1, text.substr(0, 1)},
      {2, text.substr(2, 1)},
      {1, text.substr(4, 1)},
      {3, text.substr(6, 1)},
      {2, text.substr(8, 1)},
      {2, text.substr(10, 2)},
      {2, text.substr(13, 2)},
  });
  ConnectPreFormatTokensPreservedSpaceStarts(text.begin(), &pre_format_tokens_);
  ByteOffsetSet disabled_bytes{{2, 5}, {8, 12}};  // substrings "b\nc", "e ff"
  pre_format_tokens_[1].before.break_decision = SpacingOptions::kMustWrap;
  PreserveSpacesOnDisabledTokenRanges(&pre_format_tokens_, disabled_bytes,
                                      text);
  const auto &ftokens = pre_format_tokens_;
  auto indices = [&text](const absl::string_view &range) {
    return SubRangeIndices(range, text);
  };
  EXPECT_EQ(ftokens[0].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[0].OriginalLeadingSpaces()),
            indices(text.substr(0, 0)));
  EXPECT_EQ(ftokens[1].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(  // \n was consumed
      indices(ftokens[1].OriginalLeadingSpaces()), indices(text.substr(2, 0)));
  EXPECT_EQ(ftokens[2].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(indices(ftokens[2].OriginalLeadingSpaces()),
            indices(text.substr(3, 1)));
  EXPECT_EQ(ftokens[3].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[3].OriginalLeadingSpaces()),
            indices(text.substr(5, 1)));
  EXPECT_EQ(ftokens[4].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(indices(ftokens[4].OriginalLeadingSpaces()),
            indices(text.substr(7, 1)));
  EXPECT_EQ(ftokens[5].before.break_decision, SpacingOptions::kPreserve);
  EXPECT_EQ(indices(ftokens[5].OriginalLeadingSpaces()),
            indices(text.substr(9, 1)));
  EXPECT_EQ(ftokens[6].before.break_decision, SpacingOptions::kUndecided);
  EXPECT_EQ(indices(ftokens[6].OriginalLeadingSpaces()),
            indices(text.substr(12, 1)));
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
    ftoken.before.action = SpacingDecision::kWrap;
    std::ostringstream stream;
    stream << ftoken;
    EXPECT_EQ("\nroobar", stream.str());
  }
  {
    FormattedToken ftoken(ptoken);
    ftoken.before.action = SpacingDecision::kWrap;
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
    ft2.before.preserved_space_start = tok1.text().end();
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
    ft1.before.action = SpacingDecision::kPreserve;
    ft2.before.spaces = 3;  // ignored
    ft2.before.action = SpacingDecision::kPreserve;
    // preserved_space_start takes precedence over the other attributes
    ft1.before.preserved_space_start = text.begin();
    ft2.before.preserved_space_start = tok1.text().end();
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

TEST(InterTokenInfoTest, CompactNotationUndecided) {
  std::ostringstream stream;
  InterTokenInfo info;
  info.break_decision = SpacingOptions::kUndecided;
  info.spaces_required = 3;
  info.break_penalty = 25;
  info.CompactNotation(stream);
  EXPECT_EQ(stream.str(), "<_3,25>");
}

TEST(InterTokenInfoTest, CompactNotationMustAppend) {
  std::ostringstream stream;
  InterTokenInfo info;
  info.break_decision = SpacingOptions::kMustAppend;
  info.spaces_required = 2;
  info.CompactNotation(stream);
  EXPECT_EQ(stream.str(), "<+_2>");
}

TEST(InterTokenInfoTest, CompactNotationMustWrap) {
  std::ostringstream stream;
  InterTokenInfo info;
  info.break_decision = SpacingOptions::kMustWrap;
  info.CompactNotation(stream);
  EXPECT_EQ(stream.str(), "<\\n>");
}

TEST(InterTokenInfoTest, CompactNotationAppendAligned) {
  std::ostringstream stream;
  InterTokenInfo info;
  info.break_decision = SpacingOptions::kAppendAligned;
  info.spaces_required = 3;
  info.CompactNotation(stream);
  EXPECT_EQ(stream.str(), "<|_3>");
}

TEST(InterTokenInfoTest, CompactNotationPreserve) {
  std::ostringstream stream;
  InterTokenInfo info;
  info.break_decision = SpacingOptions::kPreserve;
  info.CompactNotation(stream);
  EXPECT_EQ(stream.str(), "<pre>");
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
