// Copyright 2017-2021 The Verible Authors.
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

#include "common/formatting/layout_optimizer.h"

#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/formatting/basic_format_style.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/formatting/unwrapped_line_test_utils.h"
#include "common/strings/split.h"
#include "common/util/spacer.h"
#include "gtest/gtest.h"

namespace verible {

using layout_optimizer_internal::LayoutItem;
using layout_optimizer_internal::LayoutTree;
using layout_optimizer_internal::LayoutType;

using layout_optimizer_internal::LayoutFunction;
using layout_optimizer_internal::LayoutFunctionFactory;
using layout_optimizer_internal::LayoutFunctionSegment;

using layout_optimizer_internal::TreeReconstructor;

namespace {

bool TokenRangeEqual(const UnwrappedLine& left, const UnwrappedLine& right) {
  return left.TokensRange() == right.TokensRange();
}

class LayoutTest : public ::testing::Test, public UnwrappedLineMemoryHandler {
 public:
  LayoutTest()
      : sample_("short_line loooooong_line"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

TEST_F(LayoutTest, TestLineLayoutAsUnwrappedLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine short_line(0, begin);
  short_line.SpanUpToToken(begin + 1);

  LayoutItem layout_short(short_line);

  const auto uwline = layout_short.ToUnwrappedLine();
  EXPECT_EQ(uwline.IndentationSpaces(), 0);
  EXPECT_EQ(uwline.PartitionPolicy(), PartitionPolicyEnum::kAlwaysExpand);

  EXPECT_EQ(uwline.TokensRange().begin(), short_line.TokensRange().begin());
  EXPECT_EQ(uwline.TokensRange().end(), short_line.TokensRange().end());
}

TEST_F(LayoutTest, TestLineLayout) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine short_line(0, begin);
  short_line.SpanUpToToken(begin + 1);

  LayoutItem layout_short(short_line);
  EXPECT_EQ(layout_short.Type(), LayoutType::kLine);
  EXPECT_EQ(layout_short.IndentationSpaces(), 0);
  EXPECT_EQ(layout_short.SpacesBefore(), 0);
  EXPECT_EQ(layout_short.MustWrap(), false);
  EXPECT_EQ(layout_short.Length(), 10);
  EXPECT_EQ(layout_short.Text(), "short_line");
}

TEST_F(LayoutTest, TestHorizontalAndVerticalLayouts) {
  const auto spaces_before = 3;

  LayoutItem horizontal_layout(LayoutType::kJuxtaposition, spaces_before,
                               false);
  EXPECT_EQ(horizontal_layout.Type(), LayoutType::kJuxtaposition);
  EXPECT_EQ(horizontal_layout.SpacesBefore(), spaces_before);
  EXPECT_EQ(horizontal_layout.MustWrap(), false);

  LayoutItem vertical_layout(LayoutType::kStack, spaces_before, true);
  EXPECT_EQ(vertical_layout.Type(), LayoutType::kStack);
  EXPECT_EQ(vertical_layout.SpacesBefore(), spaces_before);
  EXPECT_EQ(vertical_layout.MustWrap(), true);
}

std::ostream& PrintIndented(std::ostream& stream, absl::string_view str,
                            int indentation) {
  for (const auto& line : verible::SplitLinesKeepLineTerminator(str))
    stream << verible::Spacer(indentation) << line;
  return stream;
}

class LayoutFunctionFactoryTest : public ::testing::Test,
                                  public UnwrappedLineMemoryHandler {
 public:
  LayoutFunctionFactoryTest()
      : sample_(
            //   :    |10  :    |20  :    |30  :    |40
            "This line is short.\n"
            "This line is so long that it exceeds column limit.\n"
            "        Indented  line  with  many  spaces .\n"

            "One under 40 column limit (39 columns).\n"
            "Exactly at 40 column limit (40 columns).\n"
            "One over 40 column limit (41 characters).\n"

            "One under 30 limit (29 cols).\n"
            "Exactly at 30 limit (30 cols).\n"
            "One over 30 limit (31 columns).\n"

            "10 columns"),
        tokens_(
            absl::StrSplit(sample_, absl::ByAnyChar(" \n"), absl::SkipEmpty())),
        style_(CreateStyle()),
        factory_(LayoutFunctionFactory(style_)) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfosExternalStringBuffer(ftokens_);
    ConnectPreFormatTokensPreservedSpaceStarts(sample_.data(),
                                               &pre_format_tokens_);

    // Create UnwrappedLine for each sample text's line and set token properties
    uwlines_.emplace_back(0, pre_format_tokens_.begin());
    for (auto token_it = pre_format_tokens_.begin();
         token_it != pre_format_tokens_.end(); ++token_it) {
      const auto leading_spaces = token_it->OriginalLeadingSpaces();

      // First token in a line
      if (absl::StrContains(leading_spaces, '\n')) {
        token_it->before.break_decision = SpacingOptions::MustWrap;

        uwlines_.back().SpanUpToToken(token_it);
        uwlines_.emplace_back(0, token_it);
      }

      // Count spaces preceding the token and set spaces_required accordingly
      auto last_non_space_offset = leading_spaces.find_last_not_of(' ');
      if (last_non_space_offset != absl::string_view::npos) {
        token_it->before.spaces_required =
            leading_spaces.size() - 1 - last_non_space_offset;
      } else {
        token_it->before.spaces_required = leading_spaces.size();
      }
    }
    uwlines_.back().SpanUpToToken(pre_format_tokens_.end());
  }

 protected:
  // Readable names for each line
  static constexpr int kShortLineId = 0;
  static constexpr int kLongLineId = 1;
  static constexpr int kIndentedLineId = 2;

  static constexpr int kOneUnder40LimitLineId = 3;
  static constexpr int kExactlyAt40LimitLineId = 4;
  static constexpr int kOneOver40LimitLineId = 5;

  static constexpr int kOneUnder30LimitLineId = 6;
  static constexpr int kExactlyAt30LimitLineId = 7;
  static constexpr int kOneOver30LimitLineId = 8;

  static constexpr int k10ColumnsLineId = 9;

  static BasicFormatStyle CreateStyle() {
    BasicFormatStyle style;
    // Hardcode everything to prevent failures when defaults change.
    style.indentation_spaces = 2;
    style.wrap_spaces = 4;
    style.column_limit = 40;
    style.over_column_limit_penalty = 100;
    style.line_break_penalty = 2;
    return style;
  }

  static void ExpectLayoutFunctionsEqual(const LayoutFunction& actual,
                                         const LayoutFunction& expected,
                                         int line_no) {
    using ::testing::PrintToString;
    std::ostringstream msg;
    if (actual.size() != expected.size()) {
      msg << "invalid value of size():\n"
          << "  actual:   " << actual.size() << "\n"
          << "  expected: " << expected.size() << "\n\n";
    }

    for (int i = 0; i < std::min(actual.size(), expected.size()); ++i) {
      std::ostringstream segment_msg;

      if (actual[i].column != expected[i].column) {
        segment_msg << "  invalid column:\n"
                    << "    actual:   " << actual[i].column << "\n"
                    << "    expected: " << expected[i].column << "\n";
      }
      if (actual[i].intercept != expected[i].intercept) {
        segment_msg << "  invalid intercept:\n"
                    << "    actual:   " << actual[i].intercept << "\n"
                    << "    expected: " << expected[i].intercept << "\n";
      }
      if (actual[i].gradient != expected[i].gradient) {
        segment_msg << "  invalid gradient:\n"
                    << "    actual:   " << actual[i].gradient << "\n"
                    << "    expected: " << expected[i].gradient << "\n";
      }
      if (actual[i].span != expected[i].span) {
        segment_msg << "  invalid span:\n"
                    << "    actual:   " << actual[i].span << "\n"
                    << "    expected: " << expected[i].span << "\n";
      }
      auto layout_diff = DeepEqual(actual[i].layout, expected[i].layout);
      if (layout_diff.left != nullptr) {
        segment_msg << "  invalid layout (fragment):\n"
                    << "    actual:\n";
        PrintIndented(segment_msg, PrintToString(*layout_diff.left), 6) << "\n";
        segment_msg << "    expected:\n";
        PrintIndented(segment_msg, PrintToString(*layout_diff.right), 6)
            << "\n";
      }
      if (auto str = segment_msg.str(); !str.empty())
        msg << "segment[" << i << "]:\n" << str << "\n";
    }

    if (const auto str = msg.str(); !str.empty()) {
      ADD_FAILURE_AT(__FILE__, line_no) << "LayoutFunctions differ.\nActual:\n"
                                        << actual << "\nExpected:\n"
                                        << expected << "\n\nDetails:\n\n"
                                        << str;
    } else {
      SUCCEED();
    }
  }

  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
  std::vector<UnwrappedLine> uwlines_;
  const BasicFormatStyle style_;
  const LayoutFunctionFactory factory_;
};

TEST_F(LayoutFunctionFactoryTest, Line) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf = factory_.Line(uwlines_[kShortLineId]);
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(uwlines_[kShortLineId])), 19, 0.0F, 0},
        {21, LT(LI(uwlines_[kShortLineId])), 19, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }

  {
    const auto lf = factory_.Line(uwlines_[kLongLineId]);
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(uwlines_[kLongLineId])), 50, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(uwlines_[kIndentedLineId]);
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(uwlines_[kIndentedLineId])), 36, 0.0F, 0},
        {4, LT(LI(uwlines_[kIndentedLineId])), 36, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(uwlines_[kOneUnder40LimitLineId]);
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(uwlines_[kOneUnder40LimitLineId])), 39, 0.0F, 0},
        {1, LT(LI(uwlines_[kOneUnder40LimitLineId])), 39, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(uwlines_[kExactlyAt40LimitLineId]);
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(uwlines_[kExactlyAt40LimitLineId])), 40, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(uwlines_[kOneOver40LimitLineId]);
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(uwlines_[kOneOver40LimitLineId])), 41, 100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, Stack) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 2.0F, 0},
        {21, expected_layout, 10, 2.0F, 100},
        {30, expected_layout, 10, 902.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 2.0F, 0},
        {21, expected_layout, 19, 2.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kLongLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[kLongLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 50, 1002.0F, 100},
        {21, expected_layout, 50, 3102.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kLongLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),  //
                                    LT(LI(uwlines_[kLongLineId])),    //
                                    LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 1002.0F, 100},
        {21, expected_layout, 19, 3102.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kLongLineId]),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[kLongLineId])),     //
                                    LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 1004.0F, 100},
        {21, expected_layout, 10, 3104.0F, 200},
        {30, expected_layout, 10, 4904.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kIndentedLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[kIndentedLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 36, 2.0F, 0},
        {4, expected_layout, 36, 2.0F, 100},
        {21, expected_layout, 36, 1702.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kOneUnder40LimitLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[kOneUnder40LimitLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 2.0F, 0},
        {1, expected_layout, 39, 2.0F, 100},
        {21, expected_layout, 39, 2002.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kOneOver40LimitLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[kOneOver40LimitLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 102.0F, 100},
        {21, expected_layout, 41, 2202.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kExactlyAt40LimitLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(uwlines_[kShortLineId])),    //
                                    LT(LI(uwlines_[kExactlyAt40LimitLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 2.0F, 100},
        {21, expected_layout, 40, 2102.0F, 100},
    };
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kOneUnder40LimitLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kStack, 0, true),           //
           LT(LI(uwlines_[kOneUnder40LimitLineId])),  //
           LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 2.0F, 0},
        {1, expected_layout, 19, 2.0F, 100},
        {21, expected_layout, 19, 2002.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kOneOver40LimitLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),          //
                                    LT(LI(uwlines_[kOneOver40LimitLineId])),  //
                                    LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 102.0F, 100},
        {21, expected_layout, 19, 2202.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kExactlyAt40LimitLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kStack, 0, true),            //
           LT(LI(uwlines_[kExactlyAt40LimitLineId])),  //
           LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 2.0F, 100},
        {21, expected_layout, 40, 2102.0F, 100},
    };
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kLongLineId]),
        factory_.Stack({
            factory_.Line(uwlines_[kIndentedLineId]),
            factory_.Line(uwlines_[kOneUnder40LimitLineId]),
            factory_.Line(uwlines_[kExactlyAt40LimitLineId]),
            factory_.Line(uwlines_[kOneOver40LimitLineId]),
            factory_.Line(uwlines_[k10ColumnsLineId]),
        }),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kStack, 0, false),           //
           LT(LI(uwlines_[kShortLineId])),             //
           LT(LI(uwlines_[kLongLineId])),              //
           LT(LI(uwlines_[kIndentedLineId])),          //
           LT(LI(uwlines_[kOneUnder40LimitLineId])),   //
           LT(LI(uwlines_[kExactlyAt40LimitLineId])),  //
           LT(LI(uwlines_[kOneOver40LimitLineId])),    //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 1112.0F, 300},
        {1, expected_layout, 10, 1412.0F, 400},
        {4, expected_layout, 10, 2612.0F, 500},
        {21, expected_layout, 10, 11112.0F, 600},
        {30, expected_layout, 10, 16512.0F, 700},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    // Expected result here is the same as in the test case above
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kLongLineId]),
        factory_.Line(uwlines_[kIndentedLineId]),
        factory_.Stack({
            factory_.Line(uwlines_[kOneUnder40LimitLineId]),
            factory_.Line(uwlines_[kExactlyAt40LimitLineId]),
            factory_.Line(uwlines_[kOneOver40LimitLineId]),
        }),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kStack, 0, false),           //
           LT(LI(uwlines_[kShortLineId])),             //
           LT(LI(uwlines_[kLongLineId])),              //
           LT(LI(uwlines_[kIndentedLineId])),          //
           LT(LI(uwlines_[kOneUnder40LimitLineId])),   //
           LT(LI(uwlines_[kExactlyAt40LimitLineId])),  //
           LT(LI(uwlines_[kOneOver40LimitLineId])),    //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 1112.0F, 300},
        {1, expected_layout, 10, 1412.0F, 400},
        {4, expected_layout, 10, 2612.0F, 500},
        {21, expected_layout, 10, 11112.0F, 600},
        {30, expected_layout, 10, 16512.0F, 700},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, Juxtaposition) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  static const auto kSampleStackLayout =
      LT(LI(LayoutType::kStack, 0, false),  //
         LT(LI(uwlines_[kShortLineId])),    //
         LT(LI(uwlines_[kLongLineId])),     //
         LT(LI(uwlines_[k10ColumnsLineId])));
  // Result of:
  // factory_.Stack({
  //     factory_.Line(uwlines_[kShortLineId]),
  //     factory_.Line(uwlines_[kLongLineId]),
  //     factory_.Line(uwlines_[k10ColumnsLineId]),
  // });
  static const auto kSampleStackLayoutFunction = LayoutFunction{
      {0, kSampleStackLayout, 10, 1004.0F, 100},
      {21, kSampleStackLayout, 10, 3104.0F, 200},
      {30, kSampleStackLayout, 10, 4904.0F, 300},
  };

  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(uwlines_[kShortLineId])),            //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 29, 0.0F, 0},
        {11, expected_layout, 29, 0.0F, 100},
        {21, expected_layout, 29, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(uwlines_[kShortLineId])),            //
           LT(LI(uwlines_[k10ColumnsLineId])),        //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 0.0F, 0},
        {1, expected_layout, 39, 0.0F, 100},
        {11, expected_layout, 39, 1000.0F, 100},
        {21, expected_layout, 39, 2000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LT(LI(uwlines_[k10ColumnsLineId])),       //
                                    LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 29, 0.0F, 0},
        {11, expected_layout, 29, 0.0F, 100},
        {30, expected_layout, 29, 1900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kIndentedLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(uwlines_[kShortLineId])),            //
           LT(LI(uwlines_[kIndentedLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 63, 2300.0F, 100},
        {21, expected_layout, 63, 3600.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[kIndentedLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 8, true),  //
                                    LT(LI(uwlines_[kIndentedLineId])),        //
                                    LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 55, 1500.0F, 100},
        {4, expected_layout, 55, 1900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        kSampleStackLayoutFunction,
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           kSampleStackLayout,                        //
           LT(LI(uwlines_[kShortLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 29, 1004.0F, 100},
        {11, expected_layout, 29, 2104.0F, 200},
        {21, expected_layout, 29, 4104.0F, 300},
        {30, expected_layout, 29, 6804.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[kShortLineId]),
        kSampleStackLayoutFunction,
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(uwlines_[kShortLineId])),            //
           kSampleStackLayout);
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 29, 2904.0F, 100},
        {2, expected_layout, 29, 3104.0F, 200},
        {11, expected_layout, 29, 4904.0F, 300},
        {21, expected_layout, 29, 7904.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf =
        factory_.Juxtaposition({factory_.Line(uwlines_[kOneUnder30LimitLineId]),
                                factory_.Line(uwlines_[k10ColumnsLineId])});
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, true),   //
           LT(LI(uwlines_[kOneUnder30LimitLineId])),  //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 0.0F, 0},
        {1, expected_layout, 39, 0.0F, 100},
        {11, expected_layout, 39, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition(
        {factory_.Line(uwlines_[kExactlyAt30LimitLineId]),
         factory_.Line(uwlines_[k10ColumnsLineId])});
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, true),    //
           LT(LI(uwlines_[kExactlyAt30LimitLineId])),  //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 0.0F, 100},
        {10, expected_layout, 40, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf =
        factory_.Juxtaposition({factory_.Line(uwlines_[kOneOver30LimitLineId]),
                                factory_.Line(uwlines_[k10ColumnsLineId])});
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LT(LI(uwlines_[kOneOver30LimitLineId])),  //
                                    LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 100.0F, 100},
        {9, expected_layout, 41, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kLongLineId]),
        factory_.Juxtaposition({
            factory_.Line(uwlines_[kIndentedLineId]),
            factory_.Line(uwlines_[kOneUnder40LimitLineId]),
            factory_.Line(uwlines_[kExactlyAt40LimitLineId]),
            factory_.Line(uwlines_[kOneOver40LimitLineId]),
            factory_.Line(uwlines_[k10ColumnsLineId]),
        }),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),   //
           LT(LI(uwlines_[kShortLineId])),             //
           LT(LI(uwlines_[kLongLineId])),              //
           LT(LI(uwlines_[kIndentedLineId])),          //
           LT(LI(uwlines_[kOneUnder40LimitLineId])),   //
           LT(LI(uwlines_[kExactlyAt40LimitLineId])),  //
           LT(LI(uwlines_[kOneOver40LimitLineId])),    //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 243, 19500.0F, 100},
        {21, expected_layout, 243, 21600.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    // Expected result here is the same as in the test case above
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kLongLineId]),
        factory_.Line(uwlines_[kIndentedLineId]),
        factory_.Juxtaposition({
            factory_.Line(uwlines_[kOneUnder40LimitLineId]),
            factory_.Line(uwlines_[kExactlyAt40LimitLineId]),
            factory_.Line(uwlines_[kOneOver40LimitLineId]),
        }),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),   //
           LT(LI(uwlines_[kShortLineId])),             //
           LT(LI(uwlines_[kLongLineId])),              //
           LT(LI(uwlines_[kIndentedLineId])),          //
           LT(LI(uwlines_[kOneUnder40LimitLineId])),   //
           LT(LI(uwlines_[kExactlyAt40LimitLineId])),  //
           LT(LI(uwlines_[kOneOver40LimitLineId])),    //
           LT(LI(uwlines_[k10ColumnsLineId])));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 243, 19500.0F, 100},
        {21, expected_layout, 243, 21600.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, Choice) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  struct ChoiceTestCase {
    int line_no;
    const std::initializer_list<LayoutFunction> choices;
    const LayoutFunction expected;
  };

  // Layout doesn't really matter in this test
  static const auto layout = LT(LI(LayoutType::kLine, 0, false));

  static const ChoiceTestCase kTestCases[] = {
      {__LINE__,
       {
           LayoutFunction{{0, layout, 10, 100.0F, 10}},
           LayoutFunction{{0, layout, 10, 200.0F, 10}},
       },
       LayoutFunction{{0, layout, 10, 100.0F, 10}}},
      {__LINE__,
       {
           LayoutFunction{{0, layout, 10, 200.0F, 10}},
           LayoutFunction{{0, layout, 10, 100.0F, 10}},
       },
       LayoutFunction{{0, layout, 10, 100.0F, 10}}},
      {__LINE__,
       {
           LayoutFunction{{0, layout, 10, 100.0F, 10}},
           LayoutFunction{{0, layout, 10, 100.0F, 10}},
       },
       LayoutFunction{{0, layout, 10, 100.0F, 10}}},
      {__LINE__,
       {
           LayoutFunction{{0, layout, 10, 100.0F, 1}},
           LayoutFunction{{0, layout, 10, 0.0F, 3}},
       },
       LayoutFunction{
           {0, layout, 10, 0.0F, 3},
           {50, layout, 10, 150.0F, 1},
       }},
      {__LINE__,
       {
           LayoutFunction{
               {0, layout, 10, 100.0F, 1},
           },
           LayoutFunction{
               {0, layout, 10, 0.0F, 3},
               {50, layout, 10, 150.0F, 0},
           },
       },
       LayoutFunction{
           {0, layout, 10, 0.0F, 3},
           {50, layout, 10, 150.0F, 0},
       }},
      {__LINE__,
       {
           LayoutFunction{
               {0, layout, 10, 100.0F, 1},
           },
           LayoutFunction{
               {0, layout, 10, 0.0F, 3},
               {50, layout, 10, 160.0F, 0},
           },
       },
       LayoutFunction{
           {0, layout, 10, 0.0F, 3},
           {50, layout, 10, 150.0F, 1},
           {60, layout, 10, 160.0F, 0},
       }},
      {__LINE__,
       {
           LayoutFunction{
               {0, layout, 10, 100.0F, 1},
           },
           LayoutFunction{
               {0, layout, 10, 0.0F, 3},
               {50, layout, 10, 160.0F, 0},
           },
       },
       LayoutFunction{
           {0, layout, 10, 0.0F, 3},
           {50, layout, 10, 150.0F, 1},
           {60, layout, 10, 160.0F, 0},
       }},
      {__LINE__,
       {
           LayoutFunction{
               {0, layout, 10, 100.0F, 1},
               {50, layout, 10, 150.0F, 0},
           },
           LayoutFunction{
               {0, layout, 10, 125.0F, 0},
               {75, layout, 10, 125.0F, 1},
           },
       },
       LayoutFunction{
           {0, layout, 10, 100.0F, 1},
           {25, layout, 10, 125.0F, 0},
           {75, layout, 10, 125.0F, 1},
           {100, layout, 10, 150.0F, 0},
       }},
      {__LINE__,
       {
           LayoutFunction{
               {0, layout, 1, 50.0F, 0},
           },
           LayoutFunction{
               {0, layout, 2, 0.0F, 10},
           },
           LayoutFunction{
               {0, layout, 3, 999.0F, 0},
               {10, layout, 3, 0.0F, 10},
           },
           LayoutFunction{
               {0, layout, 4, 999.0F, 0},
               {20, layout, 4, 0.0F, 10},
           },
       },
       LayoutFunction{
           {0, layout, 2, 0.0F, 10},
           {5, layout, 1, 50.0F, 0},
           {10, layout, 3, 0.0F, 10},
           {15, layout, 1, 50.0F, 0},
           {20, layout, 4, 0.0F, 10},
           {25, layout, 1, 50.0F, 0},
       }},
  };

  for (const auto& test_case : kTestCases) {
    const LayoutFunction choice_result = factory_.Choice(test_case.choices);
    ExpectLayoutFunctionsEqual(choice_result, test_case.expected,
                               test_case.line_no);
  }
}

TEST_F(LayoutFunctionFactoryTest, Wrap) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_vh =
        LT(LI(LayoutType::kStack, 0, true),             //
           LT(LI(LayoutType::kJuxtaposition, 0, true),  //
              LI(uwlines_[k10ColumnsLineId]),           //
              LI(uwlines_[kShortLineId])),              //
           LI(uwlines_[kShortLineId]));
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(uwlines_[k10ColumnsLineId]),           //
           LI(uwlines_[kShortLineId]),               //
           LI(uwlines_[kShortLineId]));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),  //
                                      LI(uwlines_[k10ColumnsLineId]),   //
                                      LI(uwlines_[kShortLineId]),       //
                                      LI(uwlines_[kShortLineId]));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_vh, 19, 2.0F, 0},
        {11, expected_layout_vh, 19, 2.0F, 100},
        {12, expected_layout_v, 19, 4.0F, 0},
        {21, expected_layout_v, 19, 4.0F, 200},
        {30, expected_layout_v, 19, 1804.0F, 300},
        {40, expected_layout_h, 48, 4800.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_hv =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(LayoutType::kStack, 0, false),       //
              LI(uwlines_[kShortLineId]),             //
              LI(uwlines_[k10ColumnsLineId])),        //
           LI(uwlines_[kShortLineId]));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(uwlines_[kShortLineId]),        //
                                      LI(uwlines_[k10ColumnsLineId]),    //
                                      LI(uwlines_[kShortLineId]));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_hv, 29, 2.0F, 0},
        {11, expected_layout_hv, 29, 2.0F, 100},
        {12, expected_layout_v, 19, 4.0F, 0},
        {21, expected_layout_v, 19, 4.0F, 200},
        {30, expected_layout_v, 19, 1804.0F, 300},
        {40, expected_layout_hv, 29, 4802.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[kOneUnder40LimitLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(uwlines_[kOneUnder40LimitLineId]),     //
           LI(uwlines_[kShortLineId]));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),       //
                                      LI(uwlines_[kOneUnder40LimitLineId]),  //
                                      LI(uwlines_[kShortLineId]));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 19, 2.0F, 0},
        {1, expected_layout_v, 19, 2.0F, 100},
        {21, expected_layout_v, 19, 2002.0F, 200},
        {40, expected_layout_h, 58, 5800.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[kExactlyAt40LimitLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(uwlines_[kExactlyAt40LimitLineId]),    //
           LI(uwlines_[kShortLineId]));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),        //
                                      LI(uwlines_[kExactlyAt40LimitLineId]),  //
                                      LI(uwlines_[kShortLineId]));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 19, 2.0F, 100},
        {21, expected_layout_v, 19, 2102.0F, 200},
        {40, expected_layout_h, 59, 5900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[kOneOver40LimitLineId]),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(uwlines_[kOneOver40LimitLineId]),      //
           LI(uwlines_[kShortLineId]));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),      //
                                      LI(uwlines_[kOneOver40LimitLineId]),  //
                                      LI(uwlines_[kShortLineId]));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 19, 102.0F, 100},
        {21, expected_layout_v, 19, 2202.0F, 200},
        {40, expected_layout_h, 60, 6000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, Indent) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf =
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 29);
    const auto expected_layout = LT(LI(uwlines_[k10ColumnsLineId], 29));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 0.0F, 0},
        {1, expected_layout, 39, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf =
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 30);
    const auto expected_layout = LT(LI(uwlines_[k10ColumnsLineId], 30));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf =
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 31);
    const auto expected_layout = LT(LI(uwlines_[k10ColumnsLineId], 31));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Indent(factory_.Line(uwlines_[kLongLineId]), 5);
    const auto expected_layout = LT(LI(uwlines_[kLongLineId], 5));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 55, 1500.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, IndentWithOtherCombinators) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 9),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LI(uwlines_[k10ColumnsLineId], 0),        //
                                    LI(uwlines_[k10ColumnsLineId], 9),        //
                                    LI(uwlines_[k10ColumnsLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 0.0F, 0},
        {1, expected_layout, 39, 0.0F, 100},
        {11, expected_layout, 39, 1000.0F, 100},
        {30, expected_layout, 39, 2900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 10),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LI(uwlines_[k10ColumnsLineId], 0),        //
                                    LI(uwlines_[k10ColumnsLineId], 10),       //
                                    LI(uwlines_[k10ColumnsLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 0.0F, 100},
        {10, expected_layout, 40, 1000.0F, 100},
        {30, expected_layout, 40, 3000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 11),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LI(uwlines_[k10ColumnsLineId], 0),        //
                                    LI(uwlines_[k10ColumnsLineId], 11),       //
                                    LI(uwlines_[k10ColumnsLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 100.0F, 100},
        {9, expected_layout, 41, 1000.0F, 100},
        {30, expected_layout, 41, 3100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }

  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 29),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),     //
                                    LI(uwlines_[k10ColumnsLineId], 0),   //
                                    LI(uwlines_[k10ColumnsLineId], 29),  //
                                    LI(uwlines_[k10ColumnsLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 4.0F, 0},
        {1, expected_layout, 10, 4.0F, 100},
        {30, expected_layout, 10, 2904.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 30),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),     //
                                    LI(uwlines_[k10ColumnsLineId], 0),   //
                                    LI(uwlines_[k10ColumnsLineId], 30),  //
                                    LI(uwlines_[k10ColumnsLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 4.0F, 100},
        {30, expected_layout, 10, 3004.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(uwlines_[k10ColumnsLineId]),
        factory_.Indent(factory_.Line(uwlines_[k10ColumnsLineId]), 31),
        factory_.Line(uwlines_[k10ColumnsLineId]),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),     //
                                    LI(uwlines_[k10ColumnsLineId], 0),   //
                                    LI(uwlines_[k10ColumnsLineId], 31),  //
                                    LI(uwlines_[k10ColumnsLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 104.0F, 100},
        {30, expected_layout, 10, 3104.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }

  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Indent(factory_.Line(uwlines_[kShortLineId]), 1),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(uwlines_[kShortLineId], 0),             //
           LI(uwlines_[kShortLineId], 1));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(uwlines_[kShortLineId], 0),     //
                                      LI(uwlines_[kShortLineId], 1));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_h, 39, 0.0F, 0},
        {1, expected_layout_h, 39, 0.0F, 100},
        {2, expected_layout_v, 20, 2.0F, 0},
        {20, expected_layout_v, 20, 2.0F, 100},
        {21, expected_layout_v, 20, 102.0F, 200},
        {40, expected_layout_h, 39, 3900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Indent(factory_.Line(uwlines_[kShortLineId]), 2),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(uwlines_[kShortLineId], 0),             //
           LI(uwlines_[kShortLineId], 2));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(uwlines_[kShortLineId], 0),     //
                                      LI(uwlines_[kShortLineId], 2));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_h, 40, 0.0F, 100},
        {1, expected_layout_v, 21, 2.0F, 0},
        {19, expected_layout_v, 21, 2.0F, 100},
        {21, expected_layout_v, 21, 202.0F, 200},
        {40, expected_layout_h, 40, 4000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(uwlines_[kShortLineId]),
        factory_.Indent(factory_.Line(uwlines_[kShortLineId]), 3),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(uwlines_[kShortLineId], 0),             //
           LI(uwlines_[kShortLineId], 3));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(uwlines_[kShortLineId], 0),     //
                                      LI(uwlines_[kShortLineId], 3));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 22, 2.0F, 0},
        {18, expected_layout_v, 22, 2.0F, 100},
        {21, expected_layout_v, 22, 302.0F, 200},
        {40, expected_layout_h, 41, 4100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }

  {
    const auto lf = factory_.Wrap({
        factory_.Indent(factory_.Line(uwlines_[kShortLineId]), 1),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(uwlines_[kShortLineId], 1),             //
           LI(uwlines_[kShortLineId], 0));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(uwlines_[kShortLineId], 1),     //
                                      LI(uwlines_[kShortLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_h, 39, 0.0F, 0},
        {1, expected_layout_h, 39, 0.0F, 100},
        {2, expected_layout_v, 19, 2.0F, 0},
        {20, expected_layout_v, 19, 2.0F, 100},
        {21, expected_layout_v, 19, 102.0F, 200},
        {40, expected_layout_h, 39, 3900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Indent(factory_.Line(uwlines_[kShortLineId]), 2),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(uwlines_[kShortLineId], 2),             //
           LI(uwlines_[kShortLineId], 0));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(uwlines_[kShortLineId], 2),     //
                                      LI(uwlines_[kShortLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_h, 40, 0.0F, 100},
        {1, expected_layout_v, 19, 2.0F, 0},
        {19, expected_layout_v, 19, 2.0F, 100},
        {21, expected_layout_v, 19, 202.0F, 200},
        {40, expected_layout_h, 40, 4000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Indent(factory_.Line(uwlines_[kShortLineId]), 3),
        factory_.Line(uwlines_[kShortLineId]),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(uwlines_[kShortLineId], 3),             //
           LI(uwlines_[kShortLineId], 0));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(uwlines_[kShortLineId], 3),     //
                                      LI(uwlines_[kShortLineId], 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 19, 2.0F, 0},
        {18, expected_layout_v, 19, 2.0F, 100},
        {21, expected_layout_v, 19, 302.0F, 200},
        {40, expected_layout_h, 41, 4100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

class TreeReconstructorTest : public ::testing::Test,
                              public UnwrappedLineMemoryHandler {
 public:
  TreeReconstructorTest()
      : sample_("first_line second_line third_line fourth_line"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

TEST_F(TreeReconstructorTest, SingleLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine single_line(0, begin);
  single_line.SpanUpToToken(begin + 1);

  const auto layout_tree = LayoutTree(LayoutItem(single_line));
  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree(UnwrappedLine(0, begin));
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{single_line,  //
                           Tree{single_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, HorizontalLayoutWithOneLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine uwline(0, begin);
  uwline.SpanUpToToken(begin + 1);

  const auto layout_tree =
      LayoutTree(LayoutItem(LayoutType::kJuxtaposition, 0, false),
                 LayoutTree(LayoutItem(uwline)));

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{uwline,  //
                           Tree{uwline}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, HorizontalLayoutSingleLines) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine left_line(0, begin);
  left_line.SpanUpToToken(begin + 1);
  UnwrappedLine right_line(0, begin + 1);
  right_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, left_line.TokensRange().begin());
  all.SpanUpToToken(right_line.TokensRange().end());

  const auto layout_tree = LayoutTree(
      LayoutItem(LayoutType::kJuxtaposition, 0,
                 left_line.TokensRange().front().before.break_decision ==
                     SpacingOptions::MustWrap),
      LayoutTree(LayoutItem(left_line)), LayoutTree(LayoutItem(right_line)));

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree(UnwrappedLine(0, begin));
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected(all,  //
                           Tree(all));
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, EmptyHorizontalLayout) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine upper_line(0, begin);
  upper_line.SpanUpToToken(begin + 1);
  UnwrappedLine lower_line(0, begin + 1);
  lower_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, upper_line.TokensRange().begin());
  all.SpanUpToToken(lower_line.TokensRange().end());

  const auto layout_tree =
      LayoutTree(LayoutItem(LayoutType::kJuxtaposition, 0, false),
                 LayoutTree(LayoutItem(upper_line)),
                 LayoutTree(LayoutItem(LayoutType::kJuxtaposition, 0, false)),
                 LayoutTree(LayoutItem(lower_line)));

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{all,  //
                           Tree{all}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, VerticalLayoutWithOneLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine uwline(0, begin);
  uwline.SpanUpToToken(begin + 1);

  const auto layout_tree = LayoutTree(LayoutItem(LayoutType::kStack, 0, false),
                                      LayoutTree(LayoutItem(uwline)));

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{uwline,  //
                           Tree{uwline}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, VerticalLayoutSingleLines) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine upper_line(0, begin);
  upper_line.SpanUpToToken(begin + 1);
  UnwrappedLine lower_line(0, begin + 1);
  lower_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, upper_line.TokensRange().begin());
  all.SpanUpToToken(lower_line.TokensRange().end());

  const auto layout_tree = LayoutTree(
      LayoutItem(LayoutType::kStack, 0,
                 upper_line.TokensRange().front().before.break_decision ==
                     SpacingOptions::MustWrap),
      LayoutTree(LayoutItem(upper_line)), LayoutTree(LayoutItem(lower_line)));

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{all,               //
                           Tree{upper_line},  //
                           Tree{lower_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, EmptyVerticalLayout) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine upper_line(0, begin);
  upper_line.SpanUpToToken(begin + 1);
  UnwrappedLine lower_line(0, begin + 1);
  lower_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, upper_line.TokensRange().begin());
  all.SpanUpToToken(lower_line.TokensRange().end());

  const auto layout_tree =
      LayoutTree(LayoutItem(LayoutType::kStack, 0, false),
                 LayoutTree(LayoutItem(upper_line)),
                 LayoutTree(LayoutItem(LayoutType::kStack, 0, false)),
                 LayoutTree(LayoutItem(lower_line)));

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{all,               //
                           Tree{upper_line},  //
                           Tree{lower_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, VerticallyJoinHorizontalLayouts) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine first_line(0, begin);
  first_line.SpanUpToToken(begin + 1);
  UnwrappedLine second_line(0, begin + 1);
  second_line.SpanUpToToken(begin + 2);
  UnwrappedLine third_line(0, begin + 2);
  third_line.SpanUpToToken(begin + 3);
  UnwrappedLine fourth_line(0, begin + 3);
  fourth_line.SpanUpToToken(begin + 4);

  UnwrappedLine upper_line(0, first_line.TokensRange().begin());
  upper_line.SpanUpToToken(second_line.TokensRange().end());
  UnwrappedLine lower_line(0, third_line.TokensRange().begin());
  lower_line.SpanUpToToken(fourth_line.TokensRange().end());

  UnwrappedLine all(0, upper_line.TokensRange().begin());
  all.SpanUpToToken(lower_line.TokensRange().end());
  const LayoutTree layout_tree{
      LayoutItem(LayoutType::kStack, 0, false),
      LayoutTree(LayoutItem(LayoutType::kJuxtaposition, 0, false),
                 LayoutTree(LayoutItem(first_line)),
                 LayoutTree(LayoutItem(second_line))),
      LayoutTree(LayoutItem(LayoutType::kJuxtaposition, 0, false),
                 LayoutTree(LayoutItem(third_line)),
                 LayoutTree(LayoutItem(fourth_line)))};

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{all,               //
                           Tree{upper_line},  //
                           Tree{lower_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, HorizontallyJoinVerticalLayouts) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine first_line(0, begin);
  first_line.SpanUpToToken(begin + 1);
  UnwrappedLine second_line(0, begin + 1);
  second_line.SpanUpToToken(begin + 2);
  UnwrappedLine third_line(0, begin + 2);
  third_line.SpanUpToToken(begin + 3);
  UnwrappedLine fourth_line(0, begin + 3);
  fourth_line.SpanUpToToken(begin + 4);

  UnwrappedLine upper_line(0, first_line.TokensRange().begin());
  upper_line.SpanUpToToken(first_line.TokensRange().end());
  UnwrappedLine middle_line(0, second_line.TokensRange().begin());
  middle_line.SpanUpToToken(third_line.TokensRange().end());
  UnwrappedLine bottom_line(0, fourth_line.TokensRange().begin());
  bottom_line.SpanUpToToken(fourth_line.TokensRange().end());

  UnwrappedLine all(0, upper_line.TokensRange().begin());
  all.SpanUpToToken(bottom_line.TokensRange().end());

  const LayoutTree layout_tree{
      LayoutItem(LayoutType::kJuxtaposition, 0, false),
      LayoutTree(LayoutItem(LayoutType::kStack, 0, false),
                 LayoutTree(LayoutItem(first_line)),
                 LayoutTree(LayoutItem(second_line))),
      LayoutTree(LayoutItem(LayoutType::kStack, 0, false),
                 LayoutTree(LayoutItem(third_line)),
                 LayoutTree(LayoutItem(fourth_line)))};

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{all,                //
                           Tree{upper_line},   //
                           Tree{middle_line},  //
                           Tree{bottom_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, IndentSingleLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine single_line(0, begin);
  single_line.SpanUpToToken(begin + 1);

  const auto indent = 7;
  LayoutTree layout_tree{LayoutItem(single_line)};
  layout_tree.Value().SetIndentationSpaces(indent);

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{single_line,  //
                           Tree{single_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";

  EXPECT_EQ(optimized_tree.Children()[0].Value().IndentationSpaces(), indent);
}

class OptimizeTokenPartitionTreeTest : public ::testing::Test,
                                       public UnwrappedLineMemoryHandler {
 public:
  OptimizeTokenPartitionTreeTest()
      : sample_(
            "function_fffffffffff( type_a_aaaa, "
            "type_b_bbbbb, type_c_cccccc, "
            "type_d_dddddddd, type_e_eeeeeeee, type_f_ffff);"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

TEST_F(OptimizeTokenPartitionTreeTest, OneLevelFunctionCall) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine function_name(0, begin);
  function_name.SpanUpToToken(begin + 1);
  UnwrappedLine arg_a(0, begin + 1);
  arg_a.SpanUpToToken(begin + 2);
  UnwrappedLine arg_b(0, begin + 2);
  arg_b.SpanUpToToken(begin + 3);
  UnwrappedLine arg_c(0, begin + 3);
  arg_c.SpanUpToToken(begin + 4);
  UnwrappedLine arg_d(0, begin + 4);
  arg_d.SpanUpToToken(begin + 5);
  UnwrappedLine arg_e(0, begin + 5);
  arg_e.SpanUpToToken(begin + 6);
  UnwrappedLine arg_f(0, begin + 6);
  arg_f.SpanUpToToken(begin + 7);

  function_name.SetPartitionPolicy(PartitionPolicyEnum::kAlwaysExpand);
  arg_a.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  arg_b.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  arg_c.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  arg_d.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  arg_e.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  arg_f.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

  UnwrappedLine header(0, function_name.TokensRange().begin());
  header.SpanUpToToken(function_name.TokensRange().end());
  UnwrappedLine args(0, arg_a.TokensRange().begin());
  args.SpanUpToToken(arg_f.TokensRange().end());

  header.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  args.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

  UnwrappedLine all(0, header.TokensRange().begin());
  all.SpanUpToToken(args.TokensRange().end());
  all.SetPartitionPolicy(PartitionPolicyEnum::kOptimalLayout);

  using Tree = TokenPartitionTree;
  Tree tree_under_test{all,               //
                       Tree{header},      //
                       Tree{args,         //
                            Tree{arg_a},  //
                            Tree{arg_b},  //
                            Tree{arg_c},  //
                            Tree{arg_d},  //
                            Tree{arg_e},  //
                            Tree{arg_f}}};

  BasicFormatStyle style;
  style.column_limit = 40;
  OptimizeTokenPartitionTree(&tree_under_test, style);

  UnwrappedLine args_top_line(0, arg_a.TokensRange().begin());
  args_top_line.SpanUpToToken(arg_b.TokensRange().end());
  UnwrappedLine args_middle_line(0, arg_c.TokensRange().begin());
  args_middle_line.SpanUpToToken(arg_d.TokensRange().end());
  UnwrappedLine args_bottom_line(0, arg_e.TokensRange().begin());
  args_bottom_line.SpanUpToToken(arg_f.TokensRange().end());

  const Tree tree_expected{all,                     //
                           Tree{header},            //
                           Tree{args_top_line},     //
                           Tree{args_middle_line},  //
                           Tree{args_bottom_line}};

  const auto diff = DeepEqual(tree_under_test, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << tree_under_test << "\n";

  // header
  EXPECT_EQ(tree_under_test.Children()[0].Value().IndentationSpaces(), 0);
  // args_top_line (wrapped)
  EXPECT_EQ(tree_under_test.Children()[1].Value().IndentationSpaces(), 4);
  // args_middle_line (wrapped)
  EXPECT_EQ(tree_under_test.Children()[2].Value().IndentationSpaces(), 4);
  // args_bottom_line (wrapped)
  EXPECT_EQ(tree_under_test.Children()[3].Value().IndentationSpaces(), 4);
}

}  // namespace
}  // namespace verible
