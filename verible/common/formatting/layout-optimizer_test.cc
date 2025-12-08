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
//
#include "verible/common/formatting/layout-optimizer.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/basic-format-style.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/layout-optimizer-internal.h"
#include "verible/common/formatting/token-partition-tree-test-utils.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/formatting/unwrapped-line-test-utils.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/strings/split.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/spacer.h"
#include "verible/common/util/tree-operations.h"

namespace verible {

namespace {

template <typename T>
std::string ToString(const T &value) {
  std::ostringstream s;
  s << value;
  return s.str();
}

std::ostream &PrintIndented(std::ostream &stream, std::string_view str,
                            int indentation) {
  for (const auto &line : verible::SplitLinesKeepLineTerminator(str)) {
    stream << verible::Spacer(indentation) << line;
  }
  return stream;
}

template <typename Value>
void PrintInvalidValueMessage(std::ostream &stream, std::string_view value_name,
                              const Value &actual, const Value &expected,
                              int indentation = 0, bool multiline = false) {
  using ::testing::PrintToString;
  using verible::Spacer;

  stream << Spacer(indentation) << "invalid " << value_name << ":\n";
  if (multiline) {
    stream << Spacer(indentation) << "  actual:\n";
    PrintIndented(stream, PrintToString(actual), indentation + 4) << "\n";
    stream << Spacer(indentation) << "  expected:\n";
    PrintIndented(stream, PrintToString(expected), indentation + 4) << "\n";
  } else {
    stream << Spacer(indentation) << "  actual:   " << actual << "\n"
           << Spacer(indentation) << "  expected: " << expected << "\n\n";
  }
}

void ExpectLayoutFunctionsEqual(const LayoutFunction &actual,
                                const LayoutFunction &expected, int line_no) {
  using ::testing::PrintToString;
  std::ostringstream msg;
  if (actual.size() != expected.size()) {
    PrintInvalidValueMessage(msg, "size()", actual.size(), expected.size());
  }

  for (int i = 0; i < std::min(actual.size(), expected.size()); ++i) {
    std::ostringstream segment_msg;

    if (actual[i].column != expected[i].column) {
      PrintInvalidValueMessage(segment_msg, "column", actual[i].column,
                               expected[i].column, 2);
    }
    if (actual[i].intercept != expected[i].intercept) {
      PrintInvalidValueMessage(segment_msg, "intercept", actual[i].intercept,
                               expected[i].intercept, 2);
    }
    if (actual[i].gradient != expected[i].gradient) {
      PrintInvalidValueMessage(segment_msg, "gradient", actual[i].gradient,
                               expected[i].gradient, 2);
    }
    if (actual[i].span != expected[i].span) {
      PrintInvalidValueMessage(segment_msg, "span", actual[i].span,
                               expected[i].span, 2);
    }
    auto layout_diff = DeepEqual(actual[i].layout, expected[i].layout);
    if (layout_diff.left != nullptr) {
      PrintInvalidValueMessage(segment_msg, "layout (fragment)",
                               *layout_diff.left, *layout_diff.right, 2, true);
    }

    if (auto str = segment_msg.str(); !str.empty()) {
      msg << "segment[" << i << "]:\n" << str << "\n";
    }
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

BasicFormatStyle CreateStyle() {
  BasicFormatStyle style;
  // Hardcode everything to prevent failures when defaults change.
  style.indentation_spaces = 2;
  style.wrap_spaces = 4;
  style.column_limit = 40;
  style.over_column_limit_penalty = 100;
  style.line_break_penalty = 2;
  return style;
}

TEST(LayoutTypeTest, ToString) {
  EXPECT_EQ(ToString(LayoutType::kLine), "line");
  EXPECT_EQ(ToString(LayoutType::kJuxtaposition), "juxtaposition");
  EXPECT_EQ(ToString(LayoutType::kStack), "stack");
  EXPECT_EQ(ToString(static_cast<LayoutType>(-1)), "???");
}

bool TokenRangeEqual(const UnwrappedLine &left, const UnwrappedLine &right) {
  return left.TokensRange() == right.TokensRange();
}

class LayoutTest : public ::testing::Test, public UnwrappedLineMemoryHandler {
 public:
  LayoutTest()
      : sample_(
            "short_line\n"
            "loooooong_line"),
        tokens_(absl::StrSplit(sample_, '\n')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(1, token);
    }
    CreateTokenInfos(ftokens_);
  }

 protected:
  const std::string sample_;
  const std::vector<std::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

TEST_F(LayoutTest, LineLayoutItemToString) {
  const auto begin = pre_format_tokens_.begin();

  UnwrappedLine short_line(0, begin);
  short_line.SpanUpToToken(begin + 1);
  UnwrappedLine long_line(0, begin + 1);
  long_line.SpanUpToToken(begin + 2);
  UnwrappedLine empty_line(0, begin);

  pre_format_tokens_[0].before.spaces_required = 1;
  pre_format_tokens_[1].before.break_decision = SpacingOptions::kMustWrap;

  {
    LayoutItem item(short_line, 0);
    EXPECT_EQ(ToString(item),
              "[ short_line ], length: 10, indentation: 0, spacing: 1, must "
              "wrap: no");
  }
  {
    LayoutItem item(short_line, 3);
    EXPECT_EQ(ToString(item),
              "[ short_line ], length: 10, indentation: 3, spacing: 1, must "
              "wrap: no");
  }
  {
    LayoutItem item(long_line, 5);
    EXPECT_EQ(
        ToString(item),
        "[ loooooong_line ], length: 14, indentation: 5, spacing: 0, must "
        "wrap: YES");
  }
  {
    LayoutItem item(long_line, 7);
    EXPECT_EQ(
        ToString(item),
        "[ loooooong_line ], length: 14, indentation: 7, spacing: 0, must "
        "wrap: YES");
  }
  {
    LayoutItem item(empty_line, 11);
    EXPECT_EQ(ToString(item),
              "[  ], length: 0, indentation: 11, spacing: 0, must wrap: no");
  }
  {
    LayoutItem item(empty_line, 13);
    EXPECT_EQ(ToString(item),
              "[  ], length: 0, indentation: 13, spacing: 0, must wrap: no");
  }
}

TEST_F(LayoutTest, JuxtapositionLayoutItemToString) {
  {
    LayoutItem item(LayoutType::kJuxtaposition, 3, false, 5);
    EXPECT_EQ(ToString(item),
              "[<juxtaposition>], indentation: 5, spacing: 3, must wrap: no");
  }
  {
    LayoutItem item(LayoutType::kJuxtaposition, 7, true, 11);
    EXPECT_EQ(ToString(item),
              "[<juxtaposition>], indentation: 11, spacing: 7, must wrap: YES");
  }
}

TEST_F(LayoutTest, StackLayoutItemToString) {
  {
    LayoutItem item(LayoutType::kStack, 3, false, 5);
    EXPECT_EQ(ToString(item),
              "[<stack>], indentation: 5, spacing: 3, must wrap: no");
  }
  {
    LayoutItem item(LayoutType::kStack, 7, true, 11);
    EXPECT_EQ(ToString(item),
              "[<stack>], indentation: 11, spacing: 7, must wrap: YES");
  }
}

TEST_F(LayoutTest, TokensRange) {
  const auto begin = pre_format_tokens_.begin();

  UnwrappedLine short_line(0, begin);
  short_line.SpanUpToToken(begin + 1);

  LayoutItem layout_short(short_line);

  const auto tokens = layout_short.TokensRange();
  EXPECT_EQ(tokens.begin(), short_line.TokensRange().begin());
  EXPECT_EQ(tokens.end(), short_line.TokensRange().end());
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
    EXPECT_EQ(layout.TokensRange().begin(), begin);
    EXPECT_EQ(layout.TokensRange().end(), begin + 1);
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
    EXPECT_EQ(layout.TokensRange().begin(), begin);
    EXPECT_EQ(layout.TokensRange().end(), begin);
  }
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

TEST_F(LayoutFunctionTest, LayoutFunctionSegmentToString) {
  EXPECT_EQ(
      ToString(layout_function_[0]),
      "[  0] (101.000 + 11*x), span: 10, layout:\n"
      "      { ([  ], length: 0, indentation: 0, spacing: 0, must wrap: no) }");
  EXPECT_EQ(
      ToString(layout_function_[5]),
      "[ 50] (606.000 + 66*x), span: 60, layout:\n"
      "      { ([  ], length: 0, indentation: 0, spacing: 0, must wrap: no) }");
}

TEST_F(LayoutFunctionTest, LayoutFunctionToString) {
  EXPECT_EQ(ToString(layout_function_),
            "{\n"
            "  [  0] ( 101.000 +   11*x), span:  10, layout:\n"
            "        { ([  ], length: 0, indentation: 0, spacing: 0, must "
            "wrap: no) }\n"
            "  [  1] ( 202.000 +   22*x), span:  20, layout:\n"
            "        { ([  ], length: 0, indentation: 0, spacing: 0, must "
            "wrap: no) }\n"
            "  [  2] ( 303.000 +   33*x), span:  30, layout:\n"
            "        { ([  ], length: 0, indentation: 0, spacing: 0, must "
            "wrap: no) }\n"
            "  [  3] ( 404.000 +   44*x), span:  40, layout:\n"
            "        { ([  ], length: 0, indentation: 0, spacing: 0, must "
            "wrap: no) }\n"
            "  [ 40] ( 505.000 +   55*x), span:  50, layout:\n"
            "        { ([  ], length: 0, indentation: 0, spacing: 0, must "
            "wrap: no) }\n"
            "  [ 50] ( 606.000 +   66*x), span:  60, layout:\n"
            "        { ([  ], length: 0, indentation: 0, spacing: 0, must "
            "wrap: no) }\n"
            "}");
  EXPECT_EQ(ToString(LayoutFunction{}), "{}");
}

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
    for (auto &segment : layout_function_) {
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
    for (auto &segment : const_layout_function_) {
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
    for (auto &segment [[maybe_unused]] : empty_layout_function) {
      EXPECT_FALSE(true);
    }
  }
}

TEST_F(LayoutFunctionTest, AtOrToTheLeftOf) {
  {
    EXPECT_EQ(layout_function_.AtOrToTheLeftOf(0), layout_function_.begin());
    EXPECT_EQ(layout_function_.AtOrToTheLeftOf(1),
              layout_function_.begin() + 1);
    EXPECT_EQ(layout_function_.AtOrToTheLeftOf(2),
              layout_function_.begin() + 2);
    for (int i = 3; i < 40; ++i) {
      EXPECT_EQ(layout_function_.AtOrToTheLeftOf(i),
                layout_function_.begin() + 3)
          << "i: " << i;
    }
    for (int i = 40; i < 50; ++i) {
      EXPECT_EQ(layout_function_.AtOrToTheLeftOf(i),
                layout_function_.begin() + 4)
          << "i: " << i;
    }
    for (int i = 50; i < 70; ++i) {
      EXPECT_EQ(layout_function_.AtOrToTheLeftOf(i),
                layout_function_.begin() + 5)
          << "i: " << i;
    }
    EXPECT_EQ(layout_function_.AtOrToTheLeftOf(std::numeric_limits<int>::max()),
              layout_function_.begin() + 5);
  }
  {
    LayoutFunction empty_layout_function{};
    EXPECT_EQ(empty_layout_function.AtOrToTheLeftOf(0),
              empty_layout_function.end());
    EXPECT_EQ(empty_layout_function.AtOrToTheLeftOf(1),
              empty_layout_function.end());
    EXPECT_EQ(
        empty_layout_function.AtOrToTheLeftOf(std::numeric_limits<int>::max()),
        empty_layout_function.end());
  }
  {
    EXPECT_EQ(const_layout_function_.AtOrToTheLeftOf(0),
              const_layout_function_.begin());
    EXPECT_EQ(const_layout_function_.AtOrToTheLeftOf(1),
              const_layout_function_.begin() + 1);
    EXPECT_EQ(const_layout_function_.AtOrToTheLeftOf(2),
              const_layout_function_.begin() + 2);
    for (int i = 3; i < 40; ++i) {
      EXPECT_EQ(const_layout_function_.AtOrToTheLeftOf(i),
                const_layout_function_.begin() + 3)
          << "i: " << i;
    }
    for (int i = 40; i < 50; ++i) {
      EXPECT_EQ(const_layout_function_.AtOrToTheLeftOf(i),
                const_layout_function_.begin() + 4)
          << "i: " << i;
    }
    for (int i = 50; i < 70; ++i) {
      EXPECT_EQ(const_layout_function_.AtOrToTheLeftOf(i),
                const_layout_function_.begin() + 5)
          << "i: " << i;
    }
    EXPECT_EQ(
        const_layout_function_.AtOrToTheLeftOf(std::numeric_limits<int>::max()),
        const_layout_function_.begin() + 5);
  }
  {
    const LayoutFunction empty_layout_function{};
    EXPECT_EQ(empty_layout_function.AtOrToTheLeftOf(0),
              empty_layout_function.end());
    EXPECT_EQ(empty_layout_function.AtOrToTheLeftOf(1),
              empty_layout_function.end());
    EXPECT_EQ(
        empty_layout_function.AtOrToTheLeftOf(std::numeric_limits<int>::max()),
        empty_layout_function.end());
  }
}

TEST_F(LayoutFunctionTest, Insertion) {
  layout_function_.push_back({60, layout_, 1, 6.0F, 6});
  ASSERT_EQ(layout_function_.size(), 7);
  EXPECT_EQ(layout_function_[6].column, 60);

  layout_function_.push_back({70, layout_, 1, 6.0F, 6});
  ASSERT_EQ(layout_function_.size(), 8);
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

class LayoutFunctionIteratorTest : public LayoutFunctionTest {};

TEST_F(LayoutFunctionIteratorTest, ToString) {
  {
    std::ostringstream addr;
    addr << &layout_function_;
    EXPECT_EQ(ToString(layout_function_.begin()),
              absl::StrCat(addr.str(), "[0/6]"));
    EXPECT_EQ(ToString(layout_function_.end()),
              absl::StrCat(addr.str(), "[6/6]"));
  }
  {
    std::ostringstream addr;
    addr << &const_layout_function_;
    EXPECT_EQ(ToString(const_layout_function_.begin()),
              absl::StrCat(addr.str(), "[0/6]"));
    EXPECT_EQ(ToString(const_layout_function_.end()),
              absl::StrCat(addr.str(), "[6/6]"));
  }
  {
    LayoutFunction empty_layout_function{};
    std::ostringstream addr;
    addr << &empty_layout_function;
    EXPECT_EQ(ToString(empty_layout_function.begin()),
              absl::StrCat(addr.str(), "[0/0]"));
    EXPECT_EQ(ToString(empty_layout_function.end()),
              absl::StrCat(addr.str(), "[0/0]"));
  }
}

TEST_F(LayoutFunctionIteratorTest, MoveToKnotAtOrToTheLeftOf) {
  {
    auto it = layout_function_.begin();

    it.MoveToKnotAtOrToTheLeftOf(2);
    EXPECT_EQ(it, layout_function_.begin() + 2);
    it.MoveToKnotAtOrToTheLeftOf(0);
    EXPECT_EQ(it, layout_function_.begin());
    it.MoveToKnotAtOrToTheLeftOf(std::numeric_limits<int>::max());
    EXPECT_EQ(it, layout_function_.begin() + 5);
    it.MoveToKnotAtOrToTheLeftOf(1);
    EXPECT_EQ(it, layout_function_.begin() + 1);
    it.MoveToKnotAtOrToTheLeftOf(1);
    EXPECT_EQ(it, layout_function_.begin() + 1);
    for (int i = 3; i < 40; ++i) {
      it.MoveToKnotAtOrToTheLeftOf(i);
      EXPECT_EQ(it, layout_function_.begin() + 3) << "i: " << i;
    }
    for (int i = 50; i < 70; ++i) {
      it.MoveToKnotAtOrToTheLeftOf(i);
      EXPECT_EQ(it, layout_function_.begin() + 5) << "i: " << i;
    }
    for (int i = 49; i >= 40; --i) {
      it.MoveToKnotAtOrToTheLeftOf(i);
      EXPECT_EQ(it, layout_function_.begin() + 4) << "i: " << i;
    }
  }
  {
    LayoutFunction empty_layout_function{};
    auto it = empty_layout_function.begin();

    it.MoveToKnotAtOrToTheLeftOf(0);
    EXPECT_EQ(it, empty_layout_function.end());
    it.MoveToKnotAtOrToTheLeftOf(std::numeric_limits<int>::max());
    EXPECT_EQ(it, empty_layout_function.end());
    it.MoveToKnotAtOrToTheLeftOf(1);
    EXPECT_EQ(it, empty_layout_function.end());
  }

  {
    auto it = const_layout_function_.begin();

    it.MoveToKnotAtOrToTheLeftOf(2);
    EXPECT_EQ(it, const_layout_function_.begin() + 2);
    it.MoveToKnotAtOrToTheLeftOf(0);
    EXPECT_EQ(it, const_layout_function_.begin());
    it.MoveToKnotAtOrToTheLeftOf(std::numeric_limits<int>::max());
    EXPECT_EQ(it, const_layout_function_.begin() + 5);
    it.MoveToKnotAtOrToTheLeftOf(1);
    EXPECT_EQ(it, const_layout_function_.begin() + 1);
    it.MoveToKnotAtOrToTheLeftOf(1);
    EXPECT_EQ(it, const_layout_function_.begin() + 1);
    for (int i = 3; i < 40; ++i) {
      it.MoveToKnotAtOrToTheLeftOf(i);
      EXPECT_EQ(it, const_layout_function_.begin() + 3) << "i: " << i;
    }
    for (int i = 50; i < 70; ++i) {
      it.MoveToKnotAtOrToTheLeftOf(i);
      EXPECT_EQ(it, const_layout_function_.begin() + 5) << "i: " << i;
    }
    for (int i = 49; i >= 40; --i) {
      it.MoveToKnotAtOrToTheLeftOf(i);
      EXPECT_EQ(it, const_layout_function_.begin() + 4) << "i: " << i;
    }
  }
  {
    const LayoutFunction empty_layout_function{};
    auto it = empty_layout_function.begin();

    it.MoveToKnotAtOrToTheLeftOf(0);
    EXPECT_EQ(it, empty_layout_function.end());
    it.MoveToKnotAtOrToTheLeftOf(std::numeric_limits<int>::max());
    EXPECT_EQ(it, empty_layout_function.end());
    it.MoveToKnotAtOrToTheLeftOf(1);
    EXPECT_EQ(it, empty_layout_function.end());
  }
}

TEST_F(LayoutFunctionIteratorTest, ContainerRelatedMethods) {
  {
    int i = 0;
    auto it = layout_function_.begin();
    while (it != layout_function_.end()) {
      EXPECT_EQ(&it.Container(), &layout_function_);
      EXPECT_EQ(it.Index(), i);
      EXPECT_FALSE(it.IsEnd());
      ++it;
      ++i;
    }
    EXPECT_EQ(&it.Container(), &layout_function_);
    EXPECT_EQ(it.Index(), i);
    EXPECT_TRUE(it.IsEnd());
  }
  {
    int i = 0;
    auto it = const_layout_function_.begin();
    while (it != const_layout_function_.end()) {
      EXPECT_EQ(&it.Container(), &const_layout_function_);
      EXPECT_EQ(it.Index(), i);
      EXPECT_FALSE(it.IsEnd());
      ++it;
      ++i;
    }
    EXPECT_EQ(&it.Container(), &const_layout_function_);
    EXPECT_EQ(it.Index(), i);
    EXPECT_TRUE(it.IsEnd());
  }
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
    // Create two sets of tokens
    const size_t number_of_tokens_in_set = tokens_.size();
    ftokens_.reserve(number_of_tokens_in_set * 2);
    for (const auto token : tokens_) ftokens_.emplace_back(1, token);
    for (const auto token : tokens_) ftokens_.emplace_back(1, token);

    CreateTokenInfosExternalStringBuffer(ftokens_);

    auto must_wrap_pre_format_tokens = verible::make_range(
        pre_format_tokens_.begin(),
        pre_format_tokens_.begin() + number_of_tokens_in_set);
    auto joinable_pre_format_tokens = verible::make_range(
        pre_format_tokens_.begin() + number_of_tokens_in_set,
        pre_format_tokens_.end());

    // Setup pointers for OriginalLeadingSpaces()
    auto must_wrap_token = must_wrap_pre_format_tokens.begin();
    auto joinable_token = joinable_pre_format_tokens.begin();
    std::string_view sample_view(sample_);
    std::string_view::const_iterator buffer_start = sample_view.begin();
    for (size_t i = 0; i < number_of_tokens_in_set; ++i) {
      must_wrap_token->before.preserved_space_start = buffer_start;
      joinable_token->before.preserved_space_start = buffer_start;
      ++must_wrap_token;
      ++joinable_token;
      buffer_start = tokens_[i].end();
    }

    ProcessTokensAndCreateUnwrappedLines(true, &must_wrap_pre_format_tokens,
                                         &lines_.uwlines_);
    ProcessTokensAndCreateUnwrappedLines(false, &joinable_pre_format_tokens,
                                         &joinable_lines_.uwlines_);
  }

 protected:
  using PreFormatTokensIterator = decltype(pre_format_tokens_)::iterator;

  static void ProcessTokensAndCreateUnwrappedLines(
      bool first_on_line_must_wrap,
      verible::iterator_range<PreFormatTokensIterator> *pftokens,
      std::vector<UnwrappedLine> *uwlines) {
    uwlines->emplace_back(0, pftokens->begin());
    for (auto token = pftokens->begin(); token != pftokens->end(); ++token) {
      const auto leading_spaces = token->OriginalLeadingSpaces();

      // First token in a line
      if (absl::StrContains(leading_spaces, '\n')) {
        if (first_on_line_must_wrap) {
          token->before.break_decision = SpacingOptions::kMustWrap;
        }
        uwlines->back().SpanUpToToken(token);
        uwlines->emplace_back(0, token);
      }

      // Count spaces preceding the token and set spaces_required accordingly
      auto last_non_space_offset = leading_spaces.find_last_not_of(' ');
      if (last_non_space_offset != std::string_view::npos) {
        token->before.spaces_required =
            leading_spaces.size() - 1 - last_non_space_offset;
      } else {
        token->before.spaces_required = leading_spaces.size();
      }
    }
    uwlines->back().SpanUpToToken(pftokens->end());
  }

  // Wrapper for UnwrappedLine vector with readable getter for each line
  struct NamedUnwrappedLines {
    const UnwrappedLine &Short() const { return uwlines_.at(0); }
    const UnwrappedLine &Long() const { return uwlines_.at(1); }
    const UnwrappedLine &Indented() const { return uwlines_.at(2); }

    const UnwrappedLine &OneUnder40Limit() const { return uwlines_.at(3); }
    const UnwrappedLine &ExactlyAt40Limit() const { return uwlines_.at(4); }
    const UnwrappedLine &OneOver40Limit() const { return uwlines_.at(5); }

    const UnwrappedLine &OneUnder30Limit() const { return uwlines_.at(6); }
    const UnwrappedLine &ExactlyAt30Limit() const { return uwlines_.at(7); }
    const UnwrappedLine &OneOver30Limit() const { return uwlines_.at(8); }

    const UnwrappedLine &Exactly10Columns() const { return uwlines_.at(9); }

    std::vector<UnwrappedLine> uwlines_;
  };

  const std::string sample_;
  const std::vector<std::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;

  NamedUnwrappedLines lines_;
  NamedUnwrappedLines joinable_lines_;

  const BasicFormatStyle style_;
  const LayoutFunctionFactory factory_;
};

TEST_F(LayoutFunctionFactoryTest, Line) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf = factory_.Line(lines_.Short());
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(lines_.Short())), 19, 0.0F, 0},
        {21, LT(LI(lines_.Short())), 19, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }

  {
    const auto lf = factory_.Line(lines_.Long());
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(lines_.Long())), 50, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(lines_.Indented());
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(lines_.Indented())), 36, 0.0F, 0},
        {4, LT(LI(lines_.Indented())), 36, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(lines_.OneUnder40Limit());
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(lines_.OneUnder40Limit())), 39, 0.0F, 0},
        {1, LT(LI(lines_.OneUnder40Limit())), 39, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(lines_.ExactlyAt40Limit());
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(lines_.ExactlyAt40Limit())), 40, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Line(lines_.OneOver40Limit());
    const auto expected_lf = LayoutFunction{
        {0, LT(LI(lines_.OneOver40Limit())), 41, 100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, Stack) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf = factory_.Stack({});
    const auto expected_lf = LayoutFunction{};
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto line = factory_.Line(lines_.Short());
    const auto lf = factory_.Stack({line});
    const auto &expected_lf = line;
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Exactly10Columns()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.Exactly10Columns())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 2.0F, 0},
        {21, expected_layout, 10, 2.0F, 100},
        {30, expected_layout, 10, 902.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.Short())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 2.0F, 0},
        {21, expected_layout, 19, 2.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Long()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.Long())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 50, 1002.0F, 100},
        {21, expected_layout, 50, 3102.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Long()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),  //
                                    LT(LI(lines_.Long())),            //
                                    LT(LI(lines_.Short())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 1002.0F, 100},
        {21, expected_layout, 19, 3102.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Long()),
        factory_.Line(lines_.Exactly10Columns()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.Long())),             //
                                    LT(LI(lines_.Exactly10Columns())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 1004.0F, 100},
        {21, expected_layout, 10, 3104.0F, 200},
        {30, expected_layout, 10, 4904.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Indented()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.Indented())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 36, 2.0F, 0},
        {4, expected_layout, 36, 2.0F, 100},
        {21, expected_layout, 36, 1702.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.OneUnder40Limit()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.OneUnder40Limit())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 2.0F, 0},
        {1, expected_layout, 39, 2.0F, 100},
        {21, expected_layout, 39, 2002.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.OneOver40Limit()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.OneOver40Limit())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 102.0F, 100},
        {21, expected_layout, 41, 2202.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.ExactlyAt40Limit()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),  //
                                    LT(LI(lines_.Short())),            //
                                    LT(LI(lines_.ExactlyAt40Limit())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 2.0F, 100},
        {21, expected_layout, 40, 2102.0F, 100},
    };
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.OneUnder40Limit()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),   //
                                    LT(LI(lines_.OneUnder40Limit())),  //
                                    LT(LI(lines_.Short())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 2.0F, 0},
        {1, expected_layout, 19, 2.0F, 100},
        {21, expected_layout, 19, 2002.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.OneOver40Limit()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),  //
                                    LT(LI(lines_.OneOver40Limit())),  //
                                    LT(LI(lines_.Short())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 19, 102.0F, 100},
        {21, expected_layout, 19, 2202.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.ExactlyAt40Limit()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),    //
                                    LT(LI(lines_.ExactlyAt40Limit())),  //
                                    LT(LI(lines_.Short())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 2.0F, 100},
        {21, expected_layout, 40, 2102.0F, 100},
    };
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Long()),
        factory_.Stack({
            factory_.Line(lines_.Indented()),
            factory_.Line(lines_.OneUnder40Limit()),
            factory_.Line(lines_.ExactlyAt40Limit()),
            factory_.Line(lines_.OneOver40Limit()),
            factory_.Line(lines_.Exactly10Columns()),
        }),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),   //
                                    LT(LI(lines_.Short())),             //
                                    LT(LI(lines_.Long())),              //
                                    LT(LI(lines_.Indented())),          //
                                    LT(LI(lines_.OneUnder40Limit())),   //
                                    LT(LI(lines_.ExactlyAt40Limit())),  //
                                    LT(LI(lines_.OneOver40Limit())),    //
                                    LT(LI(lines_.Exactly10Columns())));
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
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Long()),
        factory_.Line(lines_.Indented()),
        factory_.Stack({
            factory_.Line(lines_.OneUnder40Limit()),
            factory_.Line(lines_.ExactlyAt40Limit()),
            factory_.Line(lines_.OneOver40Limit()),
        }),
        factory_.Line(lines_.Exactly10Columns()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, false),   //
                                    LT(LI(lines_.Short())),             //
                                    LT(LI(lines_.Long())),              //
                                    LT(LI(lines_.Indented())),          //
                                    LT(LI(lines_.OneUnder40Limit())),   //
                                    LT(LI(lines_.ExactlyAt40Limit())),  //
                                    LT(LI(lines_.OneOver40Limit())),    //
                                    LT(LI(lines_.Exactly10Columns())));
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
         LT(LI(lines_.Short())),            //
         LT(LI(lines_.Long())),             //
         LT(LI(lines_.Exactly10Columns())));
  // Result of:
  // factory_.Stack({
  //     factory_.Line(lines_.Short()),
  //     factory_.Line(lines_.Long()),
  //     factory_.Line(lines_.Exactly10Columns()),
  // });
  static const auto kSampleStackLayoutFunction = LayoutFunction{
      {0, kSampleStackLayout, 10, 1004.0F, 100},
      {21, kSampleStackLayout, 10, 3104.0F, 200},
      {30, kSampleStackLayout, 10, 4904.0F, 300},
  };

  {
    const auto lf = factory_.Juxtaposition({});
    const auto expected_lf = LayoutFunction{};
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto line = factory_.Line(lines_.Short());
    const auto lf = factory_.Juxtaposition({line});
    const auto &expected_lf = line;
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(lines_.Short()),
        factory_.Line(joinable_lines_.Exactly10Columns()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(lines_.Short())),                    //
           LT(LI(joinable_lines_.Exactly10Columns())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 29, 0.0F, 0},
        {11, expected_layout, 29, 0.0F, 100},
        {21, expected_layout, 29, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(lines_.Short()),
        factory_.Line(joinable_lines_.Exactly10Columns()),
        factory_.Line(joinable_lines_.Exactly10Columns()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),    //
           LT(LI(lines_.Short())),                      //
           LT(LI(joinable_lines_.Exactly10Columns())),  //
           LT(LI(joinable_lines_.Exactly10Columns())));
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
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Line(joinable_lines_.Short()),
    });
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LT(LI(lines_.Exactly10Columns())),        //
                                    LT(LI(joinable_lines_.Short())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 29, 0.0F, 0},
        {11, expected_layout, 29, 0.0F, 100},
        {30, expected_layout, 29, 1900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(lines_.Short()),
        factory_.Line(joinable_lines_.Indented()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(lines_.Short())),                    //
           LT(LI(joinable_lines_.Indented())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 63, 2300.0F, 100},
        {21, expected_layout, 63, 3600.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(lines_.Indented()),
        factory_.Line(joinable_lines_.Short()),
    });
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 8, true),  //
                                    LT(LI(lines_.Indented())),                //
                                    LT(LI(joinable_lines_.Short())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 55, 1500.0F, 100},
        {4, expected_layout, 55, 1900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        kSampleStackLayoutFunction,
        factory_.Line(joinable_lines_.Short()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           kSampleStackLayout,                        //
           LT(LI(joinable_lines_.Short())));
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
        factory_.Line(lines_.Short()),
        kSampleStackLayoutFunction,
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(lines_.Short())),                    //
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
    const auto lf = factory_.Juxtaposition(
        {factory_.Line(lines_.OneUnder30Limit()),
         factory_.Line(joinable_lines_.Exactly10Columns())});
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LT(LI(lines_.OneUnder30Limit())),         //
                                    LT(LI(joinable_lines_.Exactly10Columns())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 0.0F, 0},
        {1, expected_layout, 39, 0.0F, 100},
        {11, expected_layout, 39, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition(
        {factory_.Line(lines_.ExactlyAt30Limit()),
         factory_.Line(joinable_lines_.Exactly10Columns())});
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LT(LI(lines_.ExactlyAt30Limit())),        //
                                    LT(LI(joinable_lines_.Exactly10Columns())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 0.0F, 100},
        {10, expected_layout, 40, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition(
        {factory_.Line(lines_.OneOver30Limit()),
         factory_.Line(joinable_lines_.Exactly10Columns())});
    const auto expected_layout = LT(LI(LayoutType::kJuxtaposition, 0, true),  //
                                    LT(LI(lines_.OneOver30Limit())),          //
                                    LT(LI(joinable_lines_.Exactly10Columns())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 100.0F, 100},
        {9, expected_layout, 41, 1000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(lines_.Short()),
        factory_.Line(joinable_lines_.Long()),
        factory_.Juxtaposition({
            factory_.Line(joinable_lines_.Indented()),
            factory_.Line(joinable_lines_.OneUnder40Limit()),
            factory_.Line(joinable_lines_.ExactlyAt40Limit()),
            factory_.Line(joinable_lines_.OneOver40Limit()),
            factory_.Line(joinable_lines_.Exactly10Columns()),
        }),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),    //
           LT(LI(lines_.Short())),                      //
           LT(LI(joinable_lines_.Long())),              //
           LT(LI(joinable_lines_.Indented())),          //
           LT(LI(joinable_lines_.OneUnder40Limit())),   //
           LT(LI(joinable_lines_.ExactlyAt40Limit())),  //
           LT(LI(joinable_lines_.OneOver40Limit())),    //
           LT(LI(joinable_lines_.Exactly10Columns())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 243, 19500.0F, 100},
        {21, expected_layout, 243, 21600.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    // Expected result here is the same as in the test case above
    const auto lf = factory_.Juxtaposition({
        factory_.Line(lines_.Short()),
        factory_.Line(joinable_lines_.Long()),
        factory_.Line(joinable_lines_.Indented()),
        factory_.Juxtaposition({
            factory_.Line(joinable_lines_.OneUnder40Limit()),
            factory_.Line(joinable_lines_.ExactlyAt40Limit()),
            factory_.Line(joinable_lines_.OneOver40Limit()),
        }),
        factory_.Line(joinable_lines_.Exactly10Columns()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, false),    //
           LT(LI(lines_.Short())),                      //
           LT(LI(joinable_lines_.Long())),              //
           LT(LI(joinable_lines_.Indented())),          //
           LT(LI(joinable_lines_.OneUnder40Limit())),   //
           LT(LI(joinable_lines_.ExactlyAt40Limit())),  //
           LT(LI(joinable_lines_.OneOver40Limit())),    //
           LT(LI(joinable_lines_.Exactly10Columns())));
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
      {__LINE__, {}, LayoutFunction{}},
      {__LINE__,
       {
           LayoutFunction{{0, layout, 10, 100.0F, 10}},
       },
       LayoutFunction{{0, layout, 10, 100.0F, 10}}},
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

  for (const auto &test_case : kTestCases) {
    const LayoutFunction choice_result = factory_.Choice(test_case.choices);
    ExpectLayoutFunctionsEqual(choice_result, test_case.expected,
                               test_case.line_no);
  }
}

TEST_F(LayoutFunctionFactoryTest, Wrap) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf = factory_.Wrap({});
    const auto expected_lf = LayoutFunction{};
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(lines_.Short()),
    });
    const auto expected_lf = factory_.Line(lines_.Short());
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout_vh =
        LT(LI(LayoutType::kStack, 0, true),             //
           LT(LI(LayoutType::kJuxtaposition, 0, true),  //
              LI(lines_.Exactly10Columns()),            //
              LI(lines_.Short())),                      //
           LI(lines_.Short()));
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(lines_.Exactly10Columns()),            //
           LI(lines_.Short()),                       //
           LI(lines_.Short()));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),  //
                                      LI(lines_.Exactly10Columns()),    //
                                      LI(lines_.Short()),               //
                                      LI(lines_.Short()));
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
        factory_.Line(lines_.Short()),
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout_hv =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LT(LI(LayoutType::kStack, 0, false),       //
              LI(lines_.Short()),                     //
              LI(lines_.Exactly10Columns())),         //
           LI(lines_.Short()));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(lines_.Short()),                //
                                      LI(lines_.Exactly10Columns()),     //
                                      LI(lines_.Short()));
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
        factory_.Line(lines_.OneUnder40Limit()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(lines_.OneUnder40Limit()),             //
           LI(lines_.Short()));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),  //
                                      LI(lines_.OneUnder40Limit()),     //
                                      LI(lines_.Short()));
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
        factory_.Line(lines_.ExactlyAt40Limit()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(lines_.ExactlyAt40Limit()),            //
           LI(lines_.Short()));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),  //
                                      LI(lines_.ExactlyAt40Limit()),    //
                                      LI(lines_.Short()));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 19, 2.0F, 100},
        {21, expected_layout_v, 19, 2102.0F, 200},
        {40, expected_layout_h, 59, 5900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap({
        factory_.Line(lines_.OneOver40Limit()),
        factory_.Line(lines_.Short()),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, true),  //
           LI(lines_.OneOver40Limit()),              //
           LI(lines_.Short()));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, true),  //
                                      LI(lines_.OneOver40Limit()),      //
                                      LI(lines_.Short()));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 19, 102.0F, 100},
        {21, expected_layout_v, 19, 2202.0F, 200},
        {40, expected_layout_h, 60, 6000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Wrap(
        {
            factory_.Line(lines_.OneOver40Limit()),
            factory_.Line(lines_.Short()),
            factory_.Line(lines_.Indented()),
        },
        false, 7);
    const auto expected_layout_vv =
        LT(LI(LayoutType::kStack, 0, true),         //
           LI(lines_.OneOver40Limit()),             //
           LT(LI(LayoutType::kStack, 0, false, 7),  //
              LI(lines_.Short(), 0),                //
              LI(lines_.Indented(), 0)));
    const auto expected_layout_vh =
        LT(LI(LayoutType::kStack, 0, true),             //
           LT(LI(LayoutType::kJuxtaposition, 0, true),  //
              LI(lines_.OneOver40Limit(), 0),           //
              LI(lines_.Short(), 0)),
           LI(lines_.Indented(), 7));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_vv, 43, 404.0F, 200},
        {14, expected_layout_vv, 43, 3204.0F, 300},
        {33, expected_layout_vh, 43, 8902.0F, 200},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, Indent) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  {
    const auto lf =
        factory_.Indent(factory_.Line(lines_.Exactly10Columns()), 29);
    const auto expected_layout = LT(LI(lines_.Exactly10Columns(), 29));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 39, 0.0F, 0},
        {1, expected_layout, 39, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf =
        factory_.Indent(factory_.Line(lines_.Exactly10Columns()), 30);
    const auto expected_layout = LT(LI(lines_.Exactly10Columns(), 30));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 0.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf =
        factory_.Indent(factory_.Line(lines_.Exactly10Columns()), 31);
    const auto expected_layout = LT(LI(lines_.Exactly10Columns(), 31));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Indent(factory_.Line(lines_.Long()), 5);
    const auto expected_layout = LT(LI(lines_.Long(), 5));
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
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Indent(factory_.Line(joinable_lines_.Exactly10Columns()), 9),
        factory_.Line(joinable_lines_.Exactly10Columns()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, true),    //
           LI(lines_.Exactly10Columns(), 0),           //
           LI(joinable_lines_.Exactly10Columns(), 9),  //
           LI(joinable_lines_.Exactly10Columns(), 0));
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
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Indent(factory_.Line(joinable_lines_.Exactly10Columns()), 10),
        factory_.Line(joinable_lines_.Exactly10Columns()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, true),     //
           LI(lines_.Exactly10Columns(), 0),            //
           LI(joinable_lines_.Exactly10Columns(), 10),  //
           LI(joinable_lines_.Exactly10Columns(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 40, 0.0F, 100},
        {10, expected_layout, 40, 1000.0F, 100},
        {30, expected_layout, 40, 3000.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Juxtaposition({
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Indent(factory_.Line(joinable_lines_.Exactly10Columns()), 11),
        factory_.Line(joinable_lines_.Exactly10Columns()),
    });
    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 0, true),     //
           LI(lines_.Exactly10Columns(), 0),            //
           LI(joinable_lines_.Exactly10Columns(), 11),  //
           LI(joinable_lines_.Exactly10Columns(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 41, 100.0F, 100},
        {9, expected_layout, 41, 1000.0F, 100},
        {30, expected_layout, 41, 3100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }

  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Indent(factory_.Line(lines_.Exactly10Columns()), 29),
        factory_.Line(lines_.Exactly10Columns()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),    //
                                    LI(lines_.Exactly10Columns(), 0),   //
                                    LI(lines_.Exactly10Columns(), 29),  //
                                    LI(lines_.Exactly10Columns(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 4.0F, 0},
        {1, expected_layout, 10, 4.0F, 100},
        {30, expected_layout, 10, 2904.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Indent(factory_.Line(lines_.Exactly10Columns()), 30),
        factory_.Line(lines_.Exactly10Columns()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),    //
                                    LI(lines_.Exactly10Columns(), 0),   //
                                    LI(lines_.Exactly10Columns(), 30),  //
                                    LI(lines_.Exactly10Columns(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 4.0F, 100},
        {30, expected_layout, 10, 3004.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(lines_.Exactly10Columns()),
        factory_.Indent(factory_.Line(lines_.Exactly10Columns()), 31),
        factory_.Line(lines_.Exactly10Columns()),
    });
    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),    //
                                    LI(lines_.Exactly10Columns(), 0),   //
                                    LI(lines_.Exactly10Columns(), 31),  //
                                    LI(lines_.Exactly10Columns(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 10, 104.0F, 100},
        {30, expected_layout, 10, 3104.0F, 300},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }

  {
    const auto lf = factory_.Wrap({
        factory_.Line(lines_.Short()),
        factory_.Indent(factory_.Line(joinable_lines_.Short()), 1),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(lines_.Short(), 0),                     //
           LI(joinable_lines_.Short(), 1));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(lines_.Short(), 0),             //
                                      LI(joinable_lines_.Short(), 1));
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
        factory_.Line(lines_.Short()),
        factory_.Indent(factory_.Line(joinable_lines_.Short()), 2),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(lines_.Short(), 0),                     //
           LI(joinable_lines_.Short(), 2));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(lines_.Short(), 0),             //
                                      LI(joinable_lines_.Short(), 2));
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
        factory_.Line(lines_.Short()),
        factory_.Indent(factory_.Line(joinable_lines_.Short()), 3),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(lines_.Short(), 0),                     //
           LI(joinable_lines_.Short(), 3));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(lines_.Short(), 0),             //
                                      LI(joinable_lines_.Short(), 3));
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
        factory_.Indent(factory_.Line(lines_.Short()), 1),
        factory_.Line(joinable_lines_.Short()),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(lines_.Short(), 1),                     //
           LI(joinable_lines_.Short(), 0));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(lines_.Short(), 1),             //
                                      LI(joinable_lines_.Short(), 0));
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
        factory_.Indent(factory_.Line(lines_.Short()), 2),
        factory_.Line(joinable_lines_.Short()),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(lines_.Short(), 2),                     //
           LI(joinable_lines_.Short(), 0));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(lines_.Short(), 2),             //
                                      LI(joinable_lines_.Short(), 0));
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
        factory_.Indent(factory_.Line(lines_.Short()), 3),
        factory_.Line(joinable_lines_.Short()),
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(lines_.Short(), 3),                     //
           LI(joinable_lines_.Short(), 0));
    const auto expected_layout_v = LT(LI(LayoutType::kStack, 0, false),  //
                                      LI(lines_.Short(), 3),             //
                                      LI(joinable_lines_.Short(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_v, 19, 2.0F, 0},
        {18, expected_layout_v, 19, 2.0F, 100},
        {21, expected_layout_v, 19, 302.0F, 200},
        {40, expected_layout_h, 41, 4100.0F, 100},
    };
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

TEST_F(LayoutFunctionFactoryTest, DifferentLayoutsInDifferentSegments) {
  using LT = LayoutTree;
  using LI = LayoutItem;

  const auto juxtaposition_or_wrap = factory_.Choice({
      factory_.Juxtaposition({
          factory_.Line(joinable_lines_.OneUnder30Limit()),
          factory_.Line(joinable_lines_.Exactly10Columns()),
      }),
      factory_.Stack({
          factory_.Line(joinable_lines_.OneUnder30Limit()),
          factory_.Line(joinable_lines_.Exactly10Columns()),
      }),
  });

  {
    const auto expected_layout_h =
        LT(LI(LayoutType::kJuxtaposition, 0, false),  //
           LI(joinable_lines_.OneUnder30Limit(), 0),  //
           LI(joinable_lines_.Exactly10Columns(), 0));
    const auto expected_layout_v =
        LT(LI(LayoutType::kStack, 0, false),          //
           LI(joinable_lines_.OneUnder30Limit(), 0),  //
           LI(joinable_lines_.Exactly10Columns(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_h, 39, 0.0F, 0},
        {1, expected_layout_h, 39, 0.0F, 100},
        {2, expected_layout_v, 10, 2.0F, 0},
        {11, expected_layout_v, 10, 2.0F, 100},
        {30, expected_layout_v, 10, 1902.0F, 200},
        {40, expected_layout_h, 39, 3900.0F, 100},
    };
    ExpectLayoutFunctionsEqual(juxtaposition_or_wrap, expected_lf, __LINE__);
  }
  {
    const auto lf = factory_.Stack({
        factory_.Line(joinable_lines_.Short()),
        juxtaposition_or_wrap,
    });
    const auto expected_layout_h =
        LT(LI(LayoutType::kStack, 0, false),             //
           LI(joinable_lines_.Short(), 0),               //
           LT(LI(LayoutType::kJuxtaposition, 0, false),  //
              LI(joinable_lines_.OneUnder30Limit(), 0),  //
              LI(joinable_lines_.Exactly10Columns(), 0)));
    const auto expected_layout_v =
        LT(LI(LayoutType::kStack, 0, false),          //
           LI(joinable_lines_.Short(), 0),            //
           LI(joinable_lines_.OneUnder30Limit(), 0),  //
           LI(joinable_lines_.Exactly10Columns(), 0));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout_h, 39, 2.0F, 0},
        {1, expected_layout_h, 39, 2.0F, 100},
        {2, expected_layout_v, 10, 4.0F, 0},
        {11, expected_layout_v, 10, 4.0F, 100},
        {21, expected_layout_v, 10, 1004.0F, 200},
        {30, expected_layout_v, 10, 2804.0F, 300},
        {40, expected_layout_h, 39, 5802.0F, 200},
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
      ftokens_.emplace_back(1, token);
    }
    CreateTokenInfos(ftokens_);
  }

 protected:
  const std::string sample_;
  const std::vector<std::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

TEST_F(TreeReconstructorTest, SingleLine) {
  using TPT = TokenPartitionTreeBuilder;
  using LT = LayoutTree;
  using LI = LayoutItem;

  const auto tree_expected =
      TPT(0, {0, 1}, PartitionPolicyEnum::kAlreadyFormatted)
          .build(pre_format_tokens_);

  const auto layout_tree = LT(LI(tree_expected.Value()));

  auto optimized_tree = TokenPartitionTree();
  TreeReconstructor tree_reconstructor(0);
  tree_reconstructor.TraverseTree(layout_tree);
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  EXPECT_PRED_FORMAT2(TokenPartitionTreesEqualPredFormat, optimized_tree,
                      tree_expected);
}

TEST_F(TreeReconstructorTest, HorizontalLayoutWithOneLine) {
  using TPT = TokenPartitionTreeBuilder;
  using LT = LayoutTree;
  using LI = LayoutItem;

  const auto tree_expected =
      TPT(0, {0, 1}, PartitionPolicyEnum::kAlreadyFormatted)
          .build(pre_format_tokens_);

  const auto layout_tree = LT(LI(LayoutType::kJuxtaposition, 0, false),
                              LT(LI(tree_expected.Value())));

  auto optimized_tree = TokenPartitionTree();
  TreeReconstructor tree_reconstructor(0);
  tree_reconstructor.TraverseTree(layout_tree);
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  EXPECT_PRED_FORMAT2(TokenPartitionTreesEqualPredFormat, optimized_tree,
                      tree_expected);
}

TEST_F(TreeReconstructorTest, HorizontalLayoutSingleLines) {
  const auto begin = pre_format_tokens_.begin();

  UnwrappedLine left_line(0, begin);
  left_line.SpanUpToToken(begin + 1);
  UnwrappedLine right_line(0, begin + 1);
  right_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, left_line.TokensRange().begin());
  all.SpanUpToToken(right_line.TokensRange().end());

  const auto layout_tree = LayoutTree(
      LayoutItem(LayoutType::kJuxtaposition, 0,
                 left_line.TokensRange().front().before.break_decision ==
                     SpacingOptions::kMustWrap),
      LayoutTree(LayoutItem(left_line)), LayoutTree(LayoutItem(right_line)));

  TreeReconstructor tree_reconstructor(0);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree(UnwrappedLine(0, begin));
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected(all);
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, EmptyHorizontalLayout) {
  const auto begin = pre_format_tokens_.begin();

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

  TreeReconstructor tree_reconstructor(0);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{all};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, VerticalLayoutWithOneLine) {
  const auto begin = pre_format_tokens_.begin();

  UnwrappedLine uwline(0, begin);
  uwline.SpanUpToToken(begin + 1);

  const auto layout_tree = LayoutTree(LayoutItem(LayoutType::kStack, 0, false),
                                      LayoutTree(LayoutItem(uwline)));

  TreeReconstructor tree_reconstructor(0);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{uwline};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTest, VerticalLayoutSingleLines) {
  const auto begin = pre_format_tokens_.begin();

  UnwrappedLine upper_line(0, begin);
  upper_line.SpanUpToToken(begin + 1);
  UnwrappedLine lower_line(0, begin + 1);
  lower_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, upper_line.TokensRange().begin());
  all.SpanUpToToken(lower_line.TokensRange().end());

  const auto layout_tree = LayoutTree(
      LayoutItem(LayoutType::kStack, 0,
                 upper_line.TokensRange().front().before.break_decision ==
                     SpacingOptions::kMustWrap),
      LayoutTree(LayoutItem(upper_line)), LayoutTree(LayoutItem(lower_line)));

  TreeReconstructor tree_reconstructor(0);
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
  const auto begin = pre_format_tokens_.begin();

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

  TreeReconstructor tree_reconstructor(0);
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
  const auto begin = pre_format_tokens_.begin();

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

  TreeReconstructor tree_reconstructor(0);
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
  const auto begin = pre_format_tokens_.begin();

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

  TreeReconstructor tree_reconstructor(0);
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
  const auto begin = pre_format_tokens_.begin();

  UnwrappedLine single_line(0, begin);
  single_line.SpanUpToToken(begin + 1);

  const auto indent = 7;
  LayoutTree layout_tree{LayoutItem(single_line)};
  layout_tree.Value().SetIndentationSpaces(indent);

  TreeReconstructor tree_reconstructor(0);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = TokenPartitionTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using Tree = TokenPartitionTree;
  const Tree tree_expected{single_line};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";

  EXPECT_EQ(optimized_tree.Value().IndentationSpaces(), indent);
}

TEST_F(TreeReconstructorTest, InlineSpacingReconstruction) {
  using TPT = TokenPartitionTreeBuilder;
  using LT = LayoutTree;
  using LI = LayoutItem;

  const auto tree_expected =
      TPT(1, PartitionPolicyEnum::kAlreadyFormatted,
          {
              TPT(0, {0, 1}, PartitionPolicyEnum::kInline),
              TPT(2, {1, 2}, PartitionPolicyEnum::kInline),
              TPT(3, {2, 3}, PartitionPolicyEnum::kInline),
          })
          .build(pre_format_tokens_);

  const auto layout_tree =
      LT(LI(LayoutType::kJuxtaposition, 1, true),               //
         LT(LI(tree_expected.Children()[0].Value(), true, 1)),  //
         LT(LI(tree_expected.Children()[1].Value())),           //
         LT(LI(tree_expected.Children()[2].Value())));

  auto optimized_tree = TokenPartitionTree();

  TreeReconstructor tree_reconstructor(0);
  tree_reconstructor.TraverseTree(layout_tree);
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  EXPECT_PRED_FORMAT2(TokenPartitionTreesEqualPredFormat, optimized_tree,
                      tree_expected);
}

class OptimizeTokenPartitionTreeTest : public ::testing::Test,
                                       public UnwrappedLineMemoryHandler {
 public:
  OptimizeTokenPartitionTreeTest()
      : sample_(
            "function_fffffffffff( type_a_aaaa, "
            "type_b_bbbbb, type_c_cccccc, "
            "type_d_dddddddd, type_e_eeeeeeee, type_f_ffff); "
            "seven eight nine ten eleven twelve"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(1, token);
    }
    CreateTokenInfos(ftokens_);
  }

 protected:
  const std::string sample_;
  const std::vector<std::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

TEST_F(OptimizeTokenPartitionTreeTest, AppendToLineWithInlinePartitions) {
  using TPT = TokenPartitionTreeBuilder;
  using PP = PartitionPolicyEnum;

  auto tree = TPT(PP::kFitOnLineElseExpand,
                  {
                      TPT(PP::kAlreadyFormatted,
                          {
                              TPT(1, {7, 8}, PP::kInline),
                              TPT(3, {8, 9}, PP::kInline),
                              TPT(5, {9, 10}, PP::kInline),
                          }),
                      TPT(PP::kAlwaysExpand,
                          {
                              TPT({10, 11}, PP::kFitOnLineElseExpand),
                              TPT({11, 12}, PP::kFitOnLineElseExpand),
                              TPT({12, 13}, PP::kFitOnLineElseExpand),
                          }),
                  })
                  .build(pre_format_tokens_);

  const auto expected_tree = TPT(PP::kAlwaysExpand,
                                 {
                                     TPT(1, PP::kAlreadyFormatted,
                                         {
                                             TPT(0, {7, 8}, PP::kInline),
                                             TPT(3, {8, 9}, PP::kInline),
                                             TPT(5, {9, 11}, PP::kInline),
                                         }),
                                     TPT(23, {11, 12}, PP::kAlreadyFormatted),
                                     TPT(23, {12, 13}, PP::kAlreadyFormatted),
                                 })
                                 .build(pre_format_tokens_);

  static const BasicFormatStyle style = CreateStyle();
  OptimizeTokenPartitionTree(style, &tree);

  EXPECT_PRED_FORMAT2(TokenPartitionTreesEqualPredFormat, tree, expected_tree);
}

class TokenPartitionsLayoutOptimizerTest : public ::testing::Test,
                                           public UnwrappedLineMemoryHandler {
 public:
  TokenPartitionsLayoutOptimizerTest()
      : sample_backing_(
            //   :    |10  :    |20  :    |30  :    |40
            "one two three four\n"
            "eleven twelve thirteen fourteen\n"),
        sample_(sample_backing_),
        tokens_(
            absl::StrSplit(sample_, absl::ByAnyChar(" \n"), absl::SkipEmpty())),
        style_(CreateStyle()),
        factory_(LayoutFunctionFactory(style_)) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(1, token);
    }
    CreateTokenInfosExternalStringBuffer(ftokens_);
    ConnectPreFormatTokensPreservedSpaceStarts(sample_.begin(),
                                               &pre_format_tokens_);

    // Set token properties
    for (auto &token : pre_format_tokens_) {
      const auto leading_spaces = token.OriginalLeadingSpaces();

      // First token in a line
      if (absl::StrContains(leading_spaces, '\n')) {
        token.before.break_decision = SpacingOptions::kMustWrap;
        auto last_non_space_offset = leading_spaces.find_last_not_of(' ');
        if (last_non_space_offset != std::string_view::npos) {
          token.before.spaces_required =
              leading_spaces.size() - 1 - last_non_space_offset;
        }
      } else {
        token.before.spaces_required = leading_spaces.size();
      }
    }
    pre_format_tokens_.front().before.break_decision =
        SpacingOptions::kMustWrap;
  }

 protected:
  const std::string sample_backing_;
  const std::string_view sample_;
  const std::vector<std::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
  const BasicFormatStyle style_;
  const LayoutFunctionFactory factory_;
};

TEST_F(TokenPartitionsLayoutOptimizerTest, CalculateOptimalLayout) {
  using TPT = TokenPartitionTreeBuilder;
  using PP = PartitionPolicyEnum;
  using LT = LayoutTree;
  using LI = LayoutItem;

  const auto optimizer = TokenPartitionsLayoutOptimizer(style_);

  {
    const auto tree = TPT(4, PP::kAlwaysExpand,
                          {
                              TPT(4, {0, 1}),
                              TPT(4, {1, 2}),
                              TPT(4, {2, 3}),
                          })
                          .build(pre_format_tokens_);

    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),     //
                                    LT(LI(tree.Children()[0].Value())),  //
                                    LT(LI(tree.Children()[1].Value())),  //
                                    LT(LI(tree.Children()[2].Value())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 5, 4.0F, 0},
        {35, expected_layout, 5, 4.0F, 100},
        {37, expected_layout, 5, 204.0F, 300},
    };

    const LayoutFunction lf = optimizer.CalculateOptimalLayout(tree);
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto tree = TPT(PP::kAlwaysExpand,
                          {
                              TPT(1, {0, 1}),
                              TPT(2, {1, 2}),
                              TPT(3, {2, 3}),
                          })
                          .build(pre_format_tokens_);

    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),        //
                                    LT(LI(tree.Children()[0].Value(), 1)),  //
                                    LT(LI(tree.Children()[1].Value(), 2)),  //
                                    LT(LI(tree.Children()[2].Value(), 3)));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 8, 4.0F, 0},
        {32, expected_layout, 8, 4.0F, 100},
        {35, expected_layout, 8, 304.0F, 200},
        {36, expected_layout, 8, 504.0F, 300},
    };

    const LayoutFunction lf = optimizer.CalculateOptimalLayout(tree);
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto tree = TPT(2, PP::kTabularAlignment,
                          {
                              TPT(2, {0, 1}, PP::kAlreadyFormatted),
                              TPT(2, {1, 2}, PP::kAlreadyFormatted),
                              TPT(2, {2, 3}, PP::kAlreadyFormatted),
                          })
                          .build(pre_format_tokens_);

    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),     //
                                    LT(LI(tree.Children()[0].Value())),  //
                                    LT(LI(tree.Children()[1].Value())),  //
                                    LT(LI(tree.Children()[2].Value())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 5, 4.0F, 0},
        {35, expected_layout, 5, 4.0F, 100},
        {37, expected_layout, 5, 204.0F, 300},
    };

    const LayoutFunction lf = optimizer.CalculateOptimalLayout(tree);
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto tree = TPT(PP::kTabularAlignment,
                          {
                              TPT(1, {0, 1}, PP::kAlreadyFormatted),
                              TPT(2, {1, 2}, PP::kAlreadyFormatted),
                              TPT(3, {2, 3}, PP::kAlreadyFormatted),
                          })
                          .build(pre_format_tokens_);

    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),        //
                                    LT(LI(tree.Children()[0].Value(), 1)),  //
                                    LT(LI(tree.Children()[1].Value(), 2)),  //
                                    LT(LI(tree.Children()[2].Value(), 3)));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 8, 4.0F, 0},
        {32, expected_layout, 8, 4.0F, 100},
        {35, expected_layout, 8, 304.0F, 200},
        {36, expected_layout, 8, 504.0F, 300},
    };

    const LayoutFunction lf = optimizer.CalculateOptimalLayout(tree);
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto tree = TPT(PP::kAppendFittingSubPartitions,
                          {
                              TPT(3, {0, 4}),
                              TPT(5, {4, 8}),
                          })
                          .build(pre_format_tokens_);

    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),     //
                                    LT(LI(tree.Children()[0].Value())),  //
                                    LT(LI(tree.Children()[1].Value())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 31, 2.0F, 0},
        {9, expected_layout, 31, 2.0F, 100},
        {22, expected_layout, 31, 1302.0F, 200},
    };

    const LayoutFunction lf = optimizer.CalculateOptimalLayout(tree);
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto tree = TPT(PP::kFitOnLineElseExpand,
                          {
                              TPT(3, {0, 4}),
                              TPT(5, {4, 8}),
                          })
                          .build(pre_format_tokens_);

    const auto expected_layout = LT(LI(LayoutType::kStack, 0, true),     //
                                    LT(LI(tree.Children()[0].Value())),  //
                                    LT(LI(tree.Children()[1].Value())));
    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 31, 2.0F, 0},
        {9, expected_layout, 31, 2.0F, 100},
        {22, expected_layout, 31, 1302.0F, 200},
    };

    const LayoutFunction lf = optimizer.CalculateOptimalLayout(tree);
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
  {
    const auto tree = TPT(PP::kAlreadyFormatted,
                          {
                              TPT(3, {0, 1}, PP::kInline),
                              TPT(5, {1, 2}, PP::kInline),
                              TPT(7, {2, 4}, PP::kInline),
                          })
                          .build(pre_format_tokens_);

    const auto expected_layout =
        LT(LI(LayoutType::kJuxtaposition, 3, true),
           LT(LI(tree.Children()[0].Value(), true, 3)),
           LT(LI(tree.Children()[1].Value(), false, 0)),
           LT(LI(tree.Children()[2].Value(), false, 0)));

    const auto expected_lf = LayoutFunction{
        {0, expected_layout, 31, 0.0, 0},
        {9, expected_layout, 31, 0.0, 100},
        {26, expected_layout, 31, 1000.0, 100},
        {34, expected_layout, 31, 1300.0, 100},
    };

    const LayoutFunction lf = optimizer.CalculateOptimalLayout(tree);
    ExpectLayoutFunctionsEqual(lf, expected_lf, __LINE__);
  }
}

}  // namespace
}  // namespace verible
