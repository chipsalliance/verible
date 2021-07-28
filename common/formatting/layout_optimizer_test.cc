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

#include "common/formatting/unwrapped_line_test_utils.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

static bool TokenRangeEqual(const UnwrappedLine& left,
                            const UnwrappedLine& right) {
  return left.TokensRange() == right.TokensRange();
}

class LayoutTestFixture : public ::testing::Test,
                          public UnwrappedLineMemoryHandler {
 public:
  LayoutTestFixture()
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

TEST_F(LayoutTestFixture, TestLineLength) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine short_line(0, begin);
  short_line.SpanUpToToken(begin + 1);
  EXPECT_EQ(Layout::UnwrappedLineLength(short_line), 10);

  UnwrappedLine long_line(0, begin + 1);
  long_line.SpanUpToToken(begin + 2);
  EXPECT_EQ(Layout::UnwrappedLineLength(long_line), 14);
}

TEST_F(LayoutTestFixture, TestLineLayoutAsUnwrappedLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine short_line(0, begin);
  short_line.SpanUpToToken(begin + 1);

  Layout layout_short(short_line);

  const auto uwline = layout_short.AsUnwrappedLine();
  EXPECT_EQ(uwline.IndentationSpaces(), 0);
  EXPECT_EQ(uwline.PartitionPolicy(), PartitionPolicyEnum::kAlwaysExpand);

  EXPECT_EQ(uwline.TokensRange().begin(), short_line.TokensRange().begin());
  EXPECT_EQ(uwline.TokensRange().end(), short_line.TokensRange().end());
}

TEST_F(LayoutTestFixture, TestLineLayout) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine short_line(0, begin);
  short_line.SpanUpToToken(begin + 1);

  Layout layout_short(short_line);
  EXPECT_EQ(layout_short.GetType(), LayoutType::kLayoutLine);
  EXPECT_EQ(layout_short.GetIndentationSpaces(), 0);
  EXPECT_EQ(layout_short.SpacesBeforeLayout(), 0);
  EXPECT_EQ(layout_short.SpacingOptionsLayout(), SpacingOptions::Undecided);
  EXPECT_FALSE(layout_short.MustWrapLayout());
  EXPECT_FALSE(layout_short.MustAppendLayout());
  EXPECT_EQ(layout_short.Length(), 10);
  EXPECT_EQ(layout_short.Text(), "short_line");
}

TEST_F(LayoutTestFixture, TestIndentedLayout) {
  const auto indent = 7;
  Layout indent_layout(indent);
  EXPECT_EQ(indent_layout.GetType(), LayoutType::kLayoutIndent);
  EXPECT_EQ(indent_layout.GetIndentationSpaces(), indent);
  EXPECT_EQ(indent_layout.SpacesBeforeLayout(), 0);
}

TEST_F(LayoutTestFixture, TestHorizontalAndVerticalLayouts) {
  const auto spaces_before = 3;

  Layout horizontal_layout(LayoutType::kLayoutHorizontalMerge, spaces_before);
  EXPECT_EQ(horizontal_layout.GetType(), LayoutType::kLayoutHorizontalMerge);
  EXPECT_EQ(horizontal_layout.SpacesBeforeLayout(), spaces_before);

  Layout vertical_layout(LayoutType::kLayoutVerticalMerge, spaces_before);
  EXPECT_EQ(vertical_layout.GetType(), LayoutType::kLayoutVerticalMerge);
  EXPECT_EQ(vertical_layout.SpacesBeforeLayout(), spaces_before);
}

class KnotTest : public ::testing::Test {};

TEST_F(KnotTest, TestAccessors) {
  const auto column = 3;
  const auto span = 7;
  const auto intercept = 11.13f;
  const auto gradient = 17;
  const auto layout_tree = LayoutTree{Layout{LayoutType::kLayoutLine, 0}};
  const auto before_spaces = 19;
  const auto spacing_options = SpacingOptions::Undecided;

  Knot knot(column, span, intercept, gradient, layout_tree, before_spaces,
            spacing_options);
  EXPECT_EQ(knot.GetColumn(), column);
  EXPECT_EQ(knot.GetSpan(), span);
  EXPECT_EQ(knot.GetIntercept(), intercept);
  EXPECT_EQ(knot.GetGradient(), gradient);
  {
    const auto tree = knot.GetLayout();
    EXPECT_EQ(tree.Children().size(), 0);
    EXPECT_EQ(tree.Value().GetType(), LayoutType::kLayoutLine);
  }
  EXPECT_EQ(knot.GetSpacesBefore(), before_spaces);
  EXPECT_EQ(knot.GetSpacingOptions(), spacing_options);
  EXPECT_FALSE(knot.MustWrap());
  EXPECT_FALSE(knot.MustAppend());
}

TEST_F(KnotTest, TestValueAt) {
  const auto column = 3;
  const auto span = 7;
  const auto intercept = 11.13f;
  const auto gradient = 17;
  const auto layout_tree = LayoutTree{Layout{LayoutType::kLayoutLine, 0}};
  const auto before_spaces = 19;
  const auto spacing_options = SpacingOptions::Undecided;

  Knot knot(column, span, intercept, gradient, layout_tree, before_spaces,
            spacing_options);
  EXPECT_EQ(knot.ValueAt(3), intercept);
  EXPECT_EQ(knot.ValueAt(5), intercept + gradient * 2);
  EXPECT_EQ(knot.ValueAt(11), intercept + gradient * 8);
}

TEST_F(KnotTest, TestWrapping) {
  Knot wrap_knot(0, 0, 0, 0, LayoutTree{Layout{LayoutType::kLayoutLine, 0}}, 0,
                 SpacingOptions::MustWrap);
  EXPECT_TRUE(wrap_knot.MustWrap());
  EXPECT_FALSE(wrap_knot.MustAppend());

  Knot append_knot(0, 0, 0, 0, LayoutTree{Layout{LayoutType::kLayoutLine, 0}},
                   0, SpacingOptions::MustAppend);
  EXPECT_FALSE(append_knot.MustWrap());
  EXPECT_TRUE(append_knot.MustAppend());
}

class KnotSetTestFixture : public ::testing::Test,
                           public UnwrappedLineMemoryHandler {
 public:
  KnotSetTestFixture()
      : sample_("regular_line must_wrap_line"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    pre_format_tokens_[1].before.break_decision = SpacingOptions::MustWrap;
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

TEST_F(KnotSetTestFixture, EmptySet) {
  KnotSet knot_set;
  EXPECT_EQ(knot_set.Size(), 0);
  EXPECT_FALSE(knot_set.MustWrap());
}

TEST_F(KnotSetTestFixture, KnotSetFromUnwrappedLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine regular_line(0, begin);
  regular_line.SpanUpToToken(begin + 1);

  const auto knot_set_regular = KnotSet::FromUnwrappedLine(regular_line, style);
  EXPECT_FALSE(knot_set_regular.MustWrap());
  EXPECT_EQ(knot_set_regular.Size(), 2);
  {
    // first knot
    const auto& knot = knot_set_regular[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(), 12);  // Length of 'regular_line'
    EXPECT_EQ(knot.GetIntercept(), 0);
    EXPECT_EQ(knot.GetGradient(), 0);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);

      // Testing only information that _every_ layout contains/carries
      // (information needed to easily reconstruct TokenPartitionTree)
      // Information specific to kLayoutLine is tested in 'TestLineLayout'
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 0);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 0);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    // second knot
    const auto& knot = knot_set_regular[1];
    EXPECT_EQ(knot.GetColumn(), style.column_limit - 12);
    EXPECT_EQ(knot.GetSpan(), 12);
    EXPECT_EQ(knot.GetIntercept(), 0);
    EXPECT_EQ(knot.GetGradient(), style.over_column_limit_penalty);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 0);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 0);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }

  UnwrappedLine must_wrap_line(0, begin + 1);
  must_wrap_line.SpanUpToToken(begin + 2);

  const auto knot_set_must_wrap =
      KnotSet::FromUnwrappedLine(must_wrap_line, style);
  EXPECT_TRUE(knot_set_must_wrap.MustWrap());
  EXPECT_EQ(knot_set_must_wrap.Size(), 2);
  {
    // first knot
    const auto& knot = knot_set_must_wrap[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(), 14);
    EXPECT_EQ(knot.GetIntercept(), 0);
    EXPECT_EQ(knot.GetGradient(), 0);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 0);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 0);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::MustWrap);
  }
  {
    // second knot
    const auto& knot = knot_set_must_wrap[1];
    // column limit - length of 'must_wrap_line'
    EXPECT_EQ(knot.GetColumn(), style.column_limit - 14);
    EXPECT_EQ(knot.GetSpan(), 14);
    EXPECT_EQ(knot.GetIntercept(), 0);
    EXPECT_EQ(knot.GetGradient(), style.over_column_limit_penalty);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 0);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 0);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::MustWrap);
  }
}

TEST_F(KnotSetTestFixture, TestIndentBlockTrivialKnot) {
  const auto indent = 7;

  KnotSet knot_set;
  knot_set.AppendKnot(Knot(0, 0, 0, 0,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 0}}, 0,
                           SpacingOptions::Undecided));

  BasicFormatStyle style;
  const auto knot_set_indent = KnotSet::IndentBlock(knot_set, indent, style);
  EXPECT_FALSE(knot_set_indent.MustWrap());
  EXPECT_EQ(knot_set_indent.Size(), 1);
  {
    const auto& knot = knot_set_indent[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(), indent);
    EXPECT_EQ(knot.GetIntercept(), 0);
    EXPECT_EQ(knot.GetGradient(), 0);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 1);
      const auto& layout_subtree = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutIndent);
      EXPECT_EQ(layout.GetIndentationSpaces(), indent);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 0);

      const auto& sublayout = layout_subtree.Value();
      EXPECT_EQ(sublayout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(sublayout.GetIndentationSpaces(), 0);
      EXPECT_EQ(sublayout.SpacesBeforeLayout(), 0);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 0);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
}

TEST_F(KnotSetTestFixture, TestIndentBlockFromUnwrappedLine) {
  const auto indent = 7;

  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine regular_line(0, begin);
  regular_line.SpanUpToToken(begin + 1);

  const auto knot_set_regular = KnotSet::FromUnwrappedLine(regular_line, style);
  const auto knot_set_indent =
      KnotSet::IndentBlock(knot_set_regular, indent, style);
  EXPECT_FALSE(knot_set_indent.MustWrap());
  EXPECT_EQ(knot_set_indent.Size(), 2);
  {
    // first knot
    const auto& knot = knot_set_indent[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(),
              12 + indent);  // length of 'regular_line' + indent
    EXPECT_EQ(knot.GetIntercept(), 0);
    EXPECT_EQ(knot.GetGradient(), 0);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 1);
      const auto& layout_subtree = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutIndent);
      EXPECT_EQ(layout.GetIndentationSpaces(), indent);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 0);

      const auto& sublayout = layout_subtree.Value();
      EXPECT_EQ(sublayout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(sublayout.GetIndentationSpaces(), 0);
      EXPECT_EQ(sublayout.SpacesBeforeLayout(), 0);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 0);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    // second knot
    const auto& knot = knot_set_indent[1];
    EXPECT_EQ(knot.GetColumn(), style.column_limit - (12 + indent));
    EXPECT_EQ(knot.GetSpan(), 12 + indent);
    EXPECT_EQ(knot.GetIntercept(), 0);
    EXPECT_EQ(knot.GetGradient(), style.over_column_limit_penalty);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 1);
      const auto& layout_subtree = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutIndent);
      EXPECT_EQ(layout.GetIndentationSpaces(), indent);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 0);

      const auto& sublayout = layout_subtree.Value();
      EXPECT_EQ(sublayout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(sublayout.GetIndentationSpaces(), 0);
      EXPECT_EQ(sublayout.SpacesBeforeLayout(), 0);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 0);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
}

TEST_F(KnotSetTestFixture, TestInterceptPlusConst) {
  KnotSet knot_set;

  knot_set.AppendKnot(Knot(1, 3, 5, 7,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 111}},
                           111, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(11, 13, 17, 23,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 231}},
                           231, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(17, 31, 51, 73,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 957}},
                           957, SpacingOptions::Undecided));

  EXPECT_EQ(knot_set.Size(), 3);

  const auto plusconst = 11;
  const auto knot_set_plusconst = knot_set.InterceptPlusConst(plusconst);
  {
    // first knot
    const auto& knot = knot_set_plusconst[0];
    EXPECT_EQ(knot.GetColumn(), 1);
    EXPECT_EQ(knot.GetSpan(), 3);
    EXPECT_EQ(knot.GetIntercept(), 5 + plusconst);
    EXPECT_EQ(knot.GetGradient(), 7);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 111);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 111);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    // second knot
    const auto& knot = knot_set_plusconst[1];
    EXPECT_EQ(knot.GetColumn(), 11);
    EXPECT_EQ(knot.GetSpan(), 13);
    EXPECT_EQ(knot.GetIntercept(), 17 + plusconst);
    EXPECT_EQ(knot.GetGradient(), 23);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 231);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 231);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    // third knot
    const auto& knot = knot_set_plusconst[2];
    EXPECT_EQ(knot.GetColumn(), 17);
    EXPECT_EQ(knot.GetSpan(), 31);
    EXPECT_EQ(knot.GetIntercept(), 51 + plusconst);
    EXPECT_EQ(knot.GetGradient(), 73);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
      EXPECT_EQ(layout.SpacesBeforeLayout(), 957);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 957);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
}

class KnotSetIteratorTest : public ::testing::Test {};

TEST_F(KnotSetIteratorTest, TestIteration) {
  KnotSet knot_set;

  knot_set.AppendKnot(Knot(1, 3, 5, 7,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 111}},
                           111, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(11, 13, 17, 23,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 231}},
                           231, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(17, 31, 51, 73,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 957}},
                           957, SpacingOptions::Undecided));

  EXPECT_EQ(knot_set.Size(), 3);

  KnotSetIterator knot_set_iterator(knot_set);
  EXPECT_EQ(knot_set_iterator.Size(), 3);

  EXPECT_FALSE(knot_set_iterator.Done());
  EXPECT_EQ(knot_set_iterator.GetIndex(), 0);

  knot_set_iterator.Advance();
  EXPECT_FALSE(knot_set_iterator.Done());
  EXPECT_EQ(knot_set_iterator.GetIndex(), 1);

  knot_set_iterator.Advance();
  EXPECT_FALSE(knot_set_iterator.Done());
  EXPECT_EQ(knot_set_iterator.GetIndex(), 2);

  knot_set_iterator.Advance();
  EXPECT_TRUE(knot_set_iterator.Done());

  knot_set_iterator.Reset();
  EXPECT_FALSE(knot_set_iterator.Done());
  EXPECT_EQ(knot_set_iterator.GetIndex(), 0);
}

TEST_F(KnotSetIteratorTest, TestKnotsColumns) {
  KnotSet knot_set;

  knot_set.AppendKnot(Knot(1, 3, 5, 7,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 111}},
                           111, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(11, 13, 17, 23,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 231}},
                           231, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(17, 31, 51, 73,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 957}},
                           957, SpacingOptions::Undecided));

  KnotSetIterator knot_set_iterator(knot_set);
  EXPECT_EQ(knot_set_iterator.Size(), 3);

  const auto infinity = std::numeric_limits<int>::max();

  EXPECT_EQ(knot_set_iterator.CurrentColumn(), 1);
  EXPECT_EQ(knot_set_iterator.NextKnotColumn(), 11);
  knot_set_iterator.Advance();
  EXPECT_EQ(knot_set_iterator.CurrentColumn(), 11);
  EXPECT_EQ(knot_set_iterator.NextKnotColumn(), 17);
  knot_set_iterator.Advance();
  EXPECT_EQ(knot_set_iterator.CurrentColumn(), 17);
  EXPECT_EQ(knot_set_iterator.NextKnotColumn(), infinity);
  knot_set_iterator.Advance();
  EXPECT_EQ(knot_set_iterator.CurrentColumn(), infinity);
  EXPECT_EQ(knot_set_iterator.NextKnotColumn(), infinity);
}

TEST_F(KnotSetIteratorTest, TestKnotsValues) {
  KnotSet knot_set;

  knot_set.AppendKnot(Knot(1, 3, 5, 7,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 111}},
                           111, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(11, 13, 17, 23,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 231}},
                           231, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(17, 31, 51, 73,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 957}},
                           957, SpacingOptions::Undecided));

  KnotSetIterator knot_set_iterator(knot_set);
  EXPECT_EQ(knot_set_iterator.Size(), 3);

  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(1), 5);
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(5), 5 + (5 - 1) * 7);
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(10), 5 + (10 - 1) * 7);

  knot_set_iterator.Advance();
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(11), 17);
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(13), 17 + (13 - 11) * 23);
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(15), 17 + (15 - 11) * 23);

  knot_set_iterator.Advance();
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(17), 51);
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(18), 51 + 73);
  EXPECT_EQ(knot_set_iterator.CurrentKnotValueAt(31), 51 + (31 - 17) * 73);
}

TEST_F(KnotSetIteratorTest, TestMoveToMargin) {
  KnotSet knot_set;

  knot_set.AppendKnot(Knot(1, 3, 5, 7,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 111}},
                           111, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(11, 13, 17, 23,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 231}},
                           231, SpacingOptions::Undecided));
  knot_set.AppendKnot(Knot(17, 31, 51, 73,
                           LayoutTree{Layout{LayoutType::kLayoutLine, 957}},
                           957, SpacingOptions::Undecided));

  KnotSetIterator knot_set_iterator(knot_set);
  EXPECT_EQ(knot_set_iterator.Size(), 3);

  // Three knots at @1, @11 and @17
  // which gives us three ranges: {[1-10], [11-16], [17-inf]}
  EXPECT_EQ(knot_set_iterator.GetIndex(), 0);

  // Shouldn't change index
  knot_set_iterator.MoveToMargin(5);
  EXPECT_EQ(knot_set_iterator.GetIndex(), 0);

  // Select second (middle) knot
  knot_set_iterator.MoveToMargin(15);
  EXPECT_EQ(knot_set_iterator.GetIndex(), 1);

  knot_set_iterator.MoveToMargin(30);
  EXPECT_EQ(knot_set_iterator.GetIndex(), 2);

  knot_set_iterator.MoveToMargin(50);
  EXPECT_EQ(knot_set_iterator.GetIndex(), 2);

  knot_set_iterator.MoveToMargin(3);
  EXPECT_EQ(knot_set_iterator.GetIndex(), 0);
}

class SolutionSetTest : public ::testing::Test {};

TEST_F(SolutionSetTest, VerticalJoinTest2Solutions) {
  KnotSet knot_set_up;
  knot_set_up.AppendKnot(Knot(0, 13, 5, 7,
                              LayoutTree{Layout{LayoutType::kLayoutLine, 111}},
                              111, SpacingOptions::Undecided));
  knot_set_up.AppendKnot(Knot(11, 13, 17, 23,
                              LayoutTree{Layout{LayoutType::kLayoutLine, 231}},
                              231, SpacingOptions::Undecided));

  KnotSet knot_set_down;
  knot_set_down.AppendKnot(
      Knot(0, 31, 11, 1, LayoutTree{Layout{LayoutType::kLayoutLine, 957}}, 957,
           SpacingOptions::Undecided));
  knot_set_down.AppendKnot(
      Knot(5, 31, 51, 73, LayoutTree{Layout{LayoutType::kLayoutLine, 957}}, 957,
           SpacingOptions::Undecided));

  BasicFormatStyle style;
  const auto knot_set_vertical =
      SolutionSet{knot_set_up, knot_set_down}.VerticalJoin(style);

  EXPECT_EQ(knot_set_vertical.Size(), 3);

  {
    // first knot
    const auto& knot = knot_set_vertical[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(), 31);  // 'last' KnotSet span
    EXPECT_EQ(knot.GetIntercept(), 5 + 11 + style.line_break_penalty);
    EXPECT_EQ(knot.GetGradient(), 7 + 1);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto layout_subtree_upper = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree_upper.Children().size(), 0);
      const auto layout_subtree_lower = layout_tree.Children()[1];
      EXPECT_EQ(layout_subtree_lower.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutVerticalMerge);
      const auto& layout_upper = layout_subtree_upper.Value();
      EXPECT_EQ(layout_upper.GetType(), LayoutType::kLayoutLine);
      const auto& layout_lower = layout_subtree_lower.Value();
      EXPECT_EQ(layout_lower.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 111);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    // second knot
    const auto& knot = knot_set_vertical[1];
    EXPECT_EQ(knot.GetColumn(), 5);
    EXPECT_EQ(knot.GetSpan(), 31);
    EXPECT_EQ(knot.GetIntercept(), 5 + 51 + style.line_break_penalty + (5 * 7));
    EXPECT_EQ(knot.GetGradient(), 7 + 73);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto layout_subtree_upper = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree_upper.Children().size(), 0);
      const auto layout_subtree_lower = layout_tree.Children()[1];
      EXPECT_EQ(layout_subtree_lower.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutVerticalMerge);
      const auto& layout_upper = layout_subtree_upper.Value();
      EXPECT_EQ(layout_upper.GetType(), LayoutType::kLayoutLine);
      const auto& layout_lower = layout_subtree_lower.Value();
      EXPECT_EQ(layout_lower.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 111);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    // third knot
    const auto& knot = knot_set_vertical[2];
    EXPECT_EQ(knot.GetColumn(), 11);
    EXPECT_EQ(knot.GetSpan(), 31);
    EXPECT_EQ(knot.GetIntercept(),
              17 + 51 + style.line_break_penalty + (11 - 5) * 73);
    EXPECT_EQ(knot.GetGradient(), 23 + 73);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto layout_subtree_upper = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree_upper.Children().size(), 0);
      const auto layout_subtree_lower = layout_tree.Children()[1];
      EXPECT_EQ(layout_subtree_lower.Children().size(), 0);

      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutVerticalMerge);
      const auto& layout_upper = layout_subtree_upper.Value();
      EXPECT_EQ(layout_upper.GetType(), LayoutType::kLayoutLine);
      const auto& layout_lower = layout_subtree_lower.Value();
      EXPECT_EQ(layout_lower.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 111);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
}

TEST_F(SolutionSetTest, HorizontalJoin2Solutions) {
  KnotSet knot_set_left;
  knot_set_left.AppendKnot(Knot(0, 13, 5, 7,
                                LayoutTree{Layout{LayoutType::kLayoutLine, 1}},
                                1, SpacingOptions::Undecided));
  knot_set_left.AppendKnot(Knot(11, 13, 17, 23,
                                LayoutTree{Layout{LayoutType::kLayoutLine, 1}},
                                1, SpacingOptions::Undecided));

  KnotSet knot_set_right;
  knot_set_right.AppendKnot(Knot(0, 7, 11, 1,
                                 LayoutTree{Layout{LayoutType::kLayoutLine, 2}},
                                 2, SpacingOptions::Undecided));
  knot_set_right.AppendKnot(Knot(5, 7, 51, 73,
                                 LayoutTree{Layout{LayoutType::kLayoutLine, 2}},
                                 2, SpacingOptions::Undecided));

  BasicFormatStyle style;
  const auto knot_set_horizontal =
      SolutionSet{knot_set_left, knot_set_right}.HorizontalJoin(style);

  EXPECT_EQ(knot_set_horizontal.Size(), 2);

  {
    const auto& knot = knot_set_horizontal[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(), 13 + 2 + 7);
    EXPECT_EQ(knot.GetIntercept(), 5 + 51 + 73 * (15 - 5));  // overhang == 0
    EXPECT_EQ(knot.GetGradient(), 7 + 73);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutHorizontalMerge);

      const auto& layout_left_subtree = layout_tree.Children()[0];
      EXPECT_EQ(layout_left_subtree.Children().size(), 0);
      const auto& layout_left = layout_left_subtree.Value();
      EXPECT_EQ(layout_left.GetType(), LayoutType::kLayoutLine);

      const auto& layout_right_subtree = layout_tree.Children()[1];
      EXPECT_EQ(layout_right_subtree.Children().size(), 0);
      const auto& layout_right = layout_right_subtree.Value();
      EXPECT_EQ(layout_right.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 1);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    const auto& knot = knot_set_horizontal[1];
    EXPECT_EQ(knot.GetColumn(), 11);
    EXPECT_EQ(knot.GetSpan(), 13 + 2 + 7);
    EXPECT_EQ(knot.GetIntercept(),
              17 + 51 + 73 * ((11 + 13 + 2) - 5));  // overhang == 0
    EXPECT_EQ(knot.GetGradient(), 23 + 73);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutHorizontalMerge);

      const auto& layout_left_subtree = layout_tree.Children()[0];
      EXPECT_EQ(layout_left_subtree.Children().size(), 0);
      const auto& layout_left = layout_left_subtree.Value();
      EXPECT_EQ(layout_left.GetType(), LayoutType::kLayoutLine);

      const auto& layout_right_subtree = layout_tree.Children()[1];
      EXPECT_EQ(layout_right_subtree.Children().size(), 0);
      const auto& layout_right = layout_right_subtree.Value();
      EXPECT_EQ(layout_right.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 1);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
}

TEST_F(SolutionSetTest, MinimalSetFrom2Solutions) {
  KnotSet knot_set_a;
  knot_set_a.AppendKnot(Knot(0, 13, 5, 7,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 1}}, 1,
                             SpacingOptions::Undecided));
  knot_set_a.AppendKnot(Knot(11, 13, 17, 23,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 1}}, 1,
                             SpacingOptions::Undecided));

  KnotSet knot_set_b;
  knot_set_b.AppendKnot(Knot(0, 7, 11, 1,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 2}}, 2,
                             SpacingOptions::Undecided));
  knot_set_b.AppendKnot(Knot(5, 7, 51, 73,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 2}}, 2,
                             SpacingOptions::Undecided));

  BasicFormatStyle style;
  const auto knot_set_minimal =
      SolutionSet{knot_set_a, knot_set_b}.MinimalSet(style);

  EXPECT_EQ(knot_set_minimal.Size(), 2);

  {
    const auto& knot = knot_set_minimal[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(), 13);
    EXPECT_EQ(knot.GetIntercept(), 5);
    EXPECT_EQ(knot.GetGradient(), 7);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 1);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    const auto& knot = knot_set_minimal[1];
    EXPECT_EQ(knot.GetColumn(), 11);
    EXPECT_EQ(knot.GetSpan(), 13);
    EXPECT_EQ(knot.GetIntercept(), 17);
    EXPECT_EQ(knot.GetGradient(), 23);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 0);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 1);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
}

TEST_F(SolutionSetTest, Wrap2Solutions) {
  KnotSet knot_set_a;
  knot_set_a.AppendKnot(Knot(0, 13, 5, 7,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 1}}, 1,
                             SpacingOptions::Undecided));
  knot_set_a.AppendKnot(Knot(11, 13, 17, 23,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 1}}, 1,
                             SpacingOptions::Undecided));

  KnotSet knot_set_b;
  knot_set_b.AppendKnot(Knot(0, 7, 11, 1,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 2}}, 2,
                             SpacingOptions::Undecided));
  knot_set_b.AppendKnot(Knot(5, 7, 51, 73,
                             LayoutTree{Layout{LayoutType::kLayoutLine, 2}}, 2,
                             SpacingOptions::Undecided));

  BasicFormatStyle style;
  const auto knot_set_wrapped =
      SolutionSet{knot_set_a, knot_set_b}.WrapSet(style);

  EXPECT_EQ(knot_set_wrapped.Size(), 3);

  {
    const auto& knot = knot_set_wrapped[0];
    EXPECT_EQ(knot.GetColumn(), 0);
    EXPECT_EQ(knot.GetSpan(), 7);
    EXPECT_EQ(knot.GetIntercept(), 20.002f);
    EXPECT_EQ(knot.GetGradient(), 8);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutVerticalMerge);

      const auto& layout_subtree_upper = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree_upper.Children().size(), 0);
      const auto& layout_upper = layout_subtree_upper.Value();
      EXPECT_EQ(layout_upper.GetType(), LayoutType::kLayoutLine);

      const auto& layout_subtree_lower = layout_tree.Children()[1];
      EXPECT_EQ(layout_subtree_lower.Children().size(), 0);
      const auto& layout_lower = layout_subtree_lower.Value();
      EXPECT_EQ(layout_lower.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 1);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    const auto& knot = knot_set_wrapped[1];
    EXPECT_EQ(knot.GetColumn(), 5);
    EXPECT_EQ(knot.GetSpan(), 7);
    EXPECT_EQ(knot.GetIntercept(), 95.002f);
    EXPECT_EQ(knot.GetGradient(), 80);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutVerticalMerge);

      const auto& layout_subtree_upper = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree_upper.Children().size(), 0);
      const auto& layout_upper = layout_subtree_upper.Value();
      EXPECT_EQ(layout_upper.GetType(), LayoutType::kLayoutLine);

      const auto& layout_subtree_lower = layout_tree.Children()[1];
      EXPECT_EQ(layout_subtree_lower.Children().size(), 0);
      const auto& layout_lower = layout_subtree_lower.Value();
      EXPECT_EQ(layout_lower.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 1);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
  {
    const auto& knot = knot_set_wrapped[2];
    EXPECT_EQ(knot.GetColumn(), 11);
    EXPECT_EQ(knot.GetSpan(), 7);
    EXPECT_EQ(knot.GetIntercept(), 510.002f);
    EXPECT_EQ(knot.GetGradient(), 96);
    {
      const auto layout_tree = knot.GetLayout();
      EXPECT_EQ(layout_tree.Children().size(), 2);
      const auto& layout = layout_tree.Value();
      EXPECT_EQ(layout.GetType(), LayoutType::kLayoutVerticalMerge);

      const auto& layout_subtree_upper = layout_tree.Children()[0];
      EXPECT_EQ(layout_subtree_upper.Children().size(), 0);
      const auto& layout_upper = layout_subtree_upper.Value();
      EXPECT_EQ(layout_upper.GetType(), LayoutType::kLayoutLine);

      const auto& layout_subtree_lower = layout_tree.Children()[1];
      EXPECT_EQ(layout_subtree_lower.Children().size(), 0);
      const auto& layout_lower = layout_subtree_lower.Value();
      EXPECT_EQ(layout_lower.GetType(), LayoutType::kLayoutLine);
    }
    EXPECT_EQ(knot.GetSpacesBefore(), 1);
    EXPECT_EQ(knot.GetSpacingOptions(), SpacingOptions::Undecided);
  }
}

class TreeReconstructorTestFixture : public ::testing::Test,
                                     public UnwrappedLineMemoryHandler {
 public:
  TreeReconstructorTestFixture()
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

TEST_F(TreeReconstructorTestFixture, SingleLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine single_line(0, begin);
  single_line.SpanUpToToken(begin + 1);

  const auto layout_tree = LayoutTree{Layout{single_line}};
  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = VectorTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using tree_type = TokenPartitionTree;
  const tree_type tree_expected{single_line, tree_type{single_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTestFixture, HorizontalLayoutSingleLines) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine left_line(0, begin);
  left_line.SpanUpToToken(begin + 1);
  UnwrappedLine right_line(0, begin + 1);
  right_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, left_line.TokensRange().begin());
  all.SpanUpToToken(right_line.TokensRange().end());

  const auto layout_tree = LayoutTree{{LayoutType::kLayoutHorizontalMerge, 0},
                                      LayoutTree{Layout{left_line}},
                                      LayoutTree{Layout{right_line}}};

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = VectorTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using tree_type = TokenPartitionTree;
  const tree_type tree_expected{all, tree_type{all}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTestFixture, VerticalLayoutSingleLines) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine upper_line(0, begin);
  upper_line.SpanUpToToken(begin + 1);
  UnwrappedLine lower_line(0, begin + 1);
  lower_line.SpanUpToToken(begin + 2);
  UnwrappedLine all(0, upper_line.TokensRange().begin());
  all.SpanUpToToken(lower_line.TokensRange().end());

  const auto layout_tree = LayoutTree{{LayoutType::kLayoutVerticalMerge, 0},
                                      LayoutTree{Layout{upper_line}},
                                      LayoutTree{Layout{lower_line}}};

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = VectorTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using tree_type = TokenPartitionTree;
  const tree_type tree_expected{all, tree_type{upper_line},
                                tree_type{lower_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTestFixture, VerticallyJoinHorizontalLayouts) {
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
      {LayoutType::kLayoutVerticalMerge, 0},
      LayoutTree{{LayoutType::kLayoutHorizontalMerge, 0},
                 LayoutTree{Layout{first_line}},
                 LayoutTree{Layout{second_line}}},
      LayoutTree{{LayoutType::kLayoutHorizontalMerge, 0},
                 LayoutTree{Layout{third_line}},
                 LayoutTree{Layout{fourth_line}}}};

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = VectorTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using tree_type = TokenPartitionTree;
  const tree_type tree_expected{all, tree_type{upper_line},
                                tree_type{lower_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTestFixture, HorizontallyJoinVerticalLayouts) {
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

  const LayoutTree layout_tree{{LayoutType::kLayoutHorizontalMerge, 0},
                               LayoutTree{{LayoutType::kLayoutVerticalMerge, 0},
                                          LayoutTree{Layout{first_line}},
                                          LayoutTree{Layout{second_line}}},
                               LayoutTree{{LayoutType::kLayoutVerticalMerge, 0},
                                          LayoutTree{Layout{third_line}},
                                          LayoutTree{Layout{fourth_line}}}};

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = VectorTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using tree_type = TokenPartitionTree;
  const tree_type tree_expected{all, tree_type{upper_line},
                                tree_type{middle_line}, tree_type{bottom_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";
}

TEST_F(TreeReconstructorTestFixture, IndentSingleLine) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  BasicFormatStyle style;

  UnwrappedLine single_line(0, begin);
  single_line.SpanUpToToken(begin + 1);

  const auto indent = 7;

  const LayoutTree layout_tree{{indent}, LayoutTree{single_line}};

  TreeReconstructor tree_reconstructor(0, style);
  tree_reconstructor.TraverseTree(layout_tree);

  auto optimized_tree = VectorTree{UnwrappedLine(0, begin)};
  tree_reconstructor.ReplaceTokenPartitionTreeNode(&optimized_tree);

  using tree_type = TokenPartitionTree;
  const tree_type tree_expected{single_line, tree_type{single_line}};
  const auto diff = DeepEqual(optimized_tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n"
                                << optimized_tree << "\n";

  EXPECT_EQ(optimized_tree.Children()[0].Value().IndentationSpaces(), indent);
}

class OptimizeTokenPartitionTreeTestFixture
    : public ::testing::Test,
      public UnwrappedLineMemoryHandler {
 public:
  OptimizeTokenPartitionTreeTestFixture()
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

TEST_F(OptimizeTokenPartitionTreeTestFixture, OneLevelFunctionCall) {
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

  using tree_type = TokenPartitionTree;
  tree_type tree_under_test{
      all, tree_type{header},
      tree_type{args, tree_type{arg_a}, tree_type{arg_b}, tree_type{arg_c},
                tree_type{arg_d}, tree_type{arg_e}, tree_type{arg_f}}};

  BasicFormatStyle style;
  style.column_limit = 40;
  OptimizeTokenPartitionTree(&tree_under_test, style);

  UnwrappedLine args_top_line(0, arg_a.TokensRange().begin());
  args_top_line.SpanUpToToken(arg_b.TokensRange().end());
  UnwrappedLine args_middle_line(0, arg_c.TokensRange().begin());
  args_middle_line.SpanUpToToken(arg_d.TokensRange().end());
  UnwrappedLine args_bottom_line(0, arg_e.TokensRange().begin());
  args_bottom_line.SpanUpToToken(arg_f.TokensRange().end());

  const tree_type tree_expected{
      all, tree_type{header}, tree_type{args_top_line},
      tree_type{args_middle_line}, tree_type{args_bottom_line}};

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
