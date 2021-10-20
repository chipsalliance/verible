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

namespace {

class LayoutTest : public ::testing::Test, public UnwrappedLineMemoryHandler {
 public:
  LayoutTest()
      : sample_(
            "short_line\n"
            "loooooong_line"),
        tokens_(absl::StrSplit(sample_, '\n')) {
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

TEST_F(LayoutTest, AsUnwrappedLine) {
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

TEST_F(LayoutTest, LineLayout) {
  const auto begin = pre_format_tokens_.begin();

  {
    UnwrappedLine short_line(0, begin);
    short_line.SpanUpToToken(begin + 1);

    LayoutItem layout(short_line);
    EXPECT_EQ(layout.Type(), LayoutType::kLine);
    EXPECT_EQ(layout.IndentationSpaces(), 0);
    EXPECT_EQ(layout.SpacesBefore(), 0);
    EXPECT_EQ(layout.MustWrap(), false);
    EXPECT_EQ(layout.Length(), 10);
    EXPECT_EQ(layout.Text(), "short_line");
  }
  {
    UnwrappedLine empty_line(0, begin);

    LayoutItem layout(empty_line);
    EXPECT_EQ(layout.Type(), LayoutType::kLine);
    EXPECT_EQ(layout.IndentationSpaces(), 0);
    EXPECT_EQ(layout.SpacesBefore(), 0);
    EXPECT_EQ(layout.MustWrap(), false);
    EXPECT_EQ(layout.Length(), 0);
    EXPECT_EQ(layout.Text(), "");
  }
}

class LayoutFunctionTest : public ::testing::Test {
 public:
  LayoutFunctionTest()
      : layout_function_(LayoutFunction{{0, layout_, 10, 101.0F, 11},
                                        {1, layout_, 20, 202.0F, 22},
                                        {2, layout_, 30, 303.0F, 33},
                                        {3, layout_, 40, 404.0F, 44},
                                        {40, layout_, 50, 505.0F, 55},
                                        {50, layout_, 60, 606.0F, 66}}),
        const_layout_function_(layout_function_) {}

 protected:
  static const LayoutTree layout_;
  LayoutFunction layout_function_;
  const LayoutFunction const_layout_function_;
};
const LayoutTree LayoutFunctionTest::layout_ =
    LayoutTree(LayoutItem(LayoutType::kLine, 0, false));

TEST_F(LayoutFunctionTest, Size) {
  EXPECT_EQ(layout_function_.size(), 6);
  EXPECT_FALSE(layout_function_.empty());

  EXPECT_EQ(const_layout_function_.size(), 6);
  EXPECT_FALSE(const_layout_function_.empty());

  LayoutFunction empty_layout_function{};
  EXPECT_EQ(empty_layout_function.size(), 0);
  EXPECT_TRUE(empty_layout_function.empty());
}

TEST_F(LayoutFunctionTest, Iteration) {
  static const auto columns = {0, 1, 2, 3, 40, 50};

  {
    auto it = layout_function_.begin();
    EXPECT_NE(it, layout_function_.end());
    EXPECT_EQ(it + 6, layout_function_.end());
    EXPECT_EQ(it->column, 0);

    auto column_it = columns.begin();
    for (auto& segment : layout_function_) {
      EXPECT_EQ(segment.column, *column_it);
      EXPECT_NE(column_it, columns.end());
      ++column_it;
    }
    EXPECT_EQ(column_it, columns.end());
  }
  {
    auto it = const_layout_function_.begin();
    EXPECT_NE(it, const_layout_function_.end());
    EXPECT_EQ(it + 6, const_layout_function_.end());
    EXPECT_EQ(it->column, 0);

    auto column_it = columns.begin();
    for (auto& segment : const_layout_function_) {
      EXPECT_EQ(segment.column, *column_it);
      EXPECT_NE(column_it, columns.end());
      ++column_it;
    }
    EXPECT_EQ(column_it, columns.end());
  }
  {
    LayoutFunction empty_layout_function{};

    auto it = empty_layout_function.begin();
    EXPECT_EQ(it, empty_layout_function.end());
    for (auto& segment [[maybe_unused]] : empty_layout_function) {
      EXPECT_FALSE(true);
    }
  }
}

TEST_F(LayoutFunctionTest, AtOrToTheLeftOf) {
  EXPECT_EQ(layout_function_.AtOrToTheLeftOf(0), layout_function_.begin());
  EXPECT_EQ(layout_function_.AtOrToTheLeftOf(1), layout_function_.begin() + 1);
  EXPECT_EQ(layout_function_.AtOrToTheLeftOf(2), layout_function_.begin() + 2);
  for (int i = 3; i < 40; ++i) {
    EXPECT_EQ(layout_function_.AtOrToTheLeftOf(i), layout_function_.begin() + 3)
        << "i: " << i;
  }
  for (int i = 40; i < 50; ++i) {
    EXPECT_EQ(layout_function_.AtOrToTheLeftOf(i), layout_function_.begin() + 4)
        << "i: " << i;
  }
  for (int i = 50; i < 70; ++i) {
    EXPECT_EQ(layout_function_.AtOrToTheLeftOf(i), layout_function_.begin() + 5)
        << "i: " << i;
  }
  EXPECT_EQ(layout_function_.AtOrToTheLeftOf(std::numeric_limits<int>::max()),
            layout_function_.begin() + 5);

  LayoutFunction empty_layout_function{};
  EXPECT_EQ(empty_layout_function.AtOrToTheLeftOf(0),
            empty_layout_function.end());
  EXPECT_EQ(empty_layout_function.AtOrToTheLeftOf(1),
            empty_layout_function.end());
  EXPECT_EQ(
      empty_layout_function.AtOrToTheLeftOf(std::numeric_limits<int>::max()),
      empty_layout_function.end());
}

TEST_F(LayoutFunctionTest, Insertion) {
  layout_function_.push_back({60, layout_, 1, 6.0F, 6});
  EXPECT_EQ(layout_function_.size(), 7);
  EXPECT_EQ(layout_function_[6].column, 60);

  layout_function_.push_back({70, layout_, 1, 6.0F, 6});
  EXPECT_EQ(layout_function_.size(), 8);
  EXPECT_EQ(layout_function_[6].column, 60);
  EXPECT_EQ(layout_function_[7].column, 70);

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(layout_function_[i].column, const_layout_function_[i].column)
        << "i: " << i;
  }
}

TEST_F(LayoutFunctionTest, Subscript) {
  EXPECT_EQ(layout_function_[0].column, 0);
  EXPECT_EQ(layout_function_[1].column, 1);
  EXPECT_EQ(layout_function_[2].column, 2);
  EXPECT_EQ(layout_function_[3].column, 3);
  EXPECT_EQ(layout_function_[4].column, 40);
  EXPECT_EQ(layout_function_[5].column, 50);
  layout_function_[5].column += 5;
  EXPECT_EQ(layout_function_[5].column, 55);

  EXPECT_EQ(const_layout_function_[0].column, 0);
  EXPECT_EQ(const_layout_function_[1].column, 1);
  EXPECT_EQ(const_layout_function_[2].column, 2);
  EXPECT_EQ(const_layout_function_[3].column, 3);
  EXPECT_EQ(const_layout_function_[4].column, 40);
  EXPECT_EQ(const_layout_function_[5].column, 50);
}

TEST_F(LayoutTest, TestHorizontalAndVerticalLayouts) {
  const auto spaces_before = 3;

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

}  // namespace
}  // namespace verible
