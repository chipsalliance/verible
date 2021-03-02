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

#include "common/formatting/tree_reshaper.h"

#include "gtest/gtest.h"
#include "common/formatting/line_wrap_searcher.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line.h"
#include "common/formatting/unwrapped_line_test_utils.h"

namespace verible {
namespace {

//class DynamicLayoutTypeFixture
//    : public ::testing::Test,
//      public UnwrappedLineMemoryHandler {
// public:
//  DynamicLayoutTypeFixture()
//      : sample_("unwrapped_line"),
//        tokens_(absl::StrSplit(sample_, ' ')) {
//    ftokens_.emplace_back(TokenInfo{1, tokens_[0]});
//    CreateTokenInfos(ftokens_);
//  }
//
// protected:
//  const std::string sample_;
//  const std::vector<absl::string_view> tokens_;
//  std::vector<TokenInfo> ftokens_;
//};

//TEST_F(DynamicLayoutTypeFixture, Printing) {
//  {
//    std::ostringstream stream;
//    UnwrappedLine uwline(0, pre_format_tokens_.begin());
//    uwline.SpanUpToToken(pre_format_tokens_.end());
//    uwline.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
//    stream << DynamicLayout{uwline};
//    // FIXME(ldk): Test only part of the line
//    EXPECT_EQ(stream.str(), "[unwrapped_line], "
//                            "policy: fit-else-expand, "
//                            "layout: uninitialized");
//  }
//  {
//    std::ostringstream stream;
//    stream << DynamicLayout{DynamicLayoutType::kDynamicLayoutLine};
//    EXPECT_EQ(stream.str(), "[<horizontal>]");
//  }
//  {
//    std::ostringstream stream;
//    stream << DynamicLayout{DynamicLayoutType::kDynamicLayoutStack};
//    EXPECT_EQ(stream.str(), "[<vertical>]");
//  }
//}

class KnotSetTest : public ::testing::Test {};

TEST_F(KnotSetTest, FourKnots) {
  TreeReshaper::KnotSet knot_set;
  knot_set.AppendKnot(TreeReshaper::Knot(0, 10, 0, 30, nullptr));
  knot_set.AppendKnot(TreeReshaper::Knot(5, 10, 20, 30, nullptr));
  knot_set.AppendKnot(TreeReshaper::Knot(11, 10, 40, 30, nullptr));
  knot_set.AppendKnot(TreeReshaper::Knot(20, 10, 60, 30, nullptr));

  EXPECT_EQ(knot_set.size(), 4);

  // Test iterator
  EXPECT_EQ(std::distance(knot_set.begin(), knot_set.end()), 4);

  auto itr = knot_set.begin();

  EXPECT_EQ(itr->column_, 0);
  EXPECT_EQ(itr->span_, 10);
  EXPECT_EQ(itr->intercept_, 0);
  EXPECT_EQ(itr->gradient_, 30);

  ++itr;
  EXPECT_EQ(itr->column_, 5);
  EXPECT_EQ(itr->span_, 10);
  EXPECT_EQ(itr->intercept_, 20);
  EXPECT_EQ(itr->gradient_, 30);

  ++itr;
  EXPECT_EQ(itr->column_, 11);
  EXPECT_EQ(itr->span_, 10);
  EXPECT_EQ(itr->intercept_, 40);
  EXPECT_EQ(itr->gradient_, 30);

  ++itr;
  EXPECT_EQ(itr->column_, 20);
  EXPECT_EQ(itr->span_, 10);
  EXPECT_EQ(itr->intercept_, 60);
  EXPECT_EQ(itr->gradient_, 30);

  ++itr;
  EXPECT_EQ(itr, knot_set.end());

  {
    auto iter = knot_set.begin();
    EXPECT_EQ(iter.ValueAt(11), 0 + 30 * 11);
    EXPECT_EQ(iter.NextKnot(), 5);
  }

  {
    auto iter = knot_set.begin();
    iter.MoveToMargin(15);
    EXPECT_EQ(iter->column_, 11);
  }

  {
    auto iter = knot_set.begin();
    iter.MoveToMargin(8);
    EXPECT_EQ(iter->column_, 5);
  }
}

TEST_F(KnotSetTest, HPlusSolution) {
  TreeReshaper::KnotSet s1, s2;

  s1.AppendKnot(TreeReshaper::Knot(0, 11, 0, 0,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));
  s1.AppendKnot(TreeReshaper::Knot(5, 11, 20, 3,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));

  s2.AppendKnot(TreeReshaper::Knot(0, 17, 0, 0,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));
  s2.AppendKnot(TreeReshaper::Knot(20, 17, 20, 5,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));

  BasicFormatStyle style;
  auto* sut_ptr = TreeReshaper::HPlusSolution(s1, s2, style);
  auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

  EXPECT_EQ(sut.size(), 3);

  const auto extra_span = 0;
  EXPECT_EQ(sut[0].column_, 0);
  EXPECT_EQ(sut[0].span_, 11 + 17 + extra_span);
  EXPECT_EQ(sut[0].gradient_, 0);
  EXPECT_EQ(sut[0].intercept_, 0);

  EXPECT_EQ(sut[1].column_, 5);
  EXPECT_EQ(sut[1].span_, 11 + 17 + extra_span);
  EXPECT_EQ(sut[1].gradient_, 3);  // gradient from s1[1]
  EXPECT_EQ(sut[1].intercept_, 20 + 0);  // (5 + 11) < 20

  EXPECT_EQ(sut[2].column_, 20 - 11 - extra_span);  // 20 - 11 - extra_span = 8
  EXPECT_EQ(sut[2].span_, 11 + 17 + extra_span);  // 11 + 17
  EXPECT_EQ(sut[2].gradient_, 8);  // sum of s1[1] + s2[1] gradients
  EXPECT_EQ(sut[2].intercept_, 20 + 20 + (20 - 11 - extra_span - 5) * 3);

  delete sut_ptr;
}

class HPlusSolutionFixture
    : public ::testing::Test,
      public UnwrappedLineMemoryHandler {
 public:
  HPlusSolutionFixture()
      : sample_(
            "first_line "
            "second_line"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    pre_format_tokens_[0].before.spaces_required = 1;
    pre_format_tokens_[1].before.spaces_required = 2;
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class HPlusSolutionFixtureTest : public HPlusSolutionFixture {};

TEST_F(HPlusSolutionFixtureTest,
       HPlusSolutionLayoutTest) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine first_line(0, begin);
  first_line.SpanUpToToken(begin + 1);
  UnwrappedLine second_line(0, begin + 1);
  second_line.SpanUpToToken(begin + 2);

  BasicFormatStyle style;
  EXPECT_EQ(UnwrappedLineLength(first_line, style), 10);
  EXPECT_EQ(UnwrappedLineLength(second_line, style), 11);

  TreeReshaper::KnotSet s1, s2;
  s1.AppendKnot(TreeReshaper::Knot(0, 10, 0, 0,
      new TreeReshaper::LayoutTree{first_line}));
  s2.AppendKnot(TreeReshaper::Knot(0, 11, 0, 0,
      new TreeReshaper::LayoutTree{second_line}));
  EXPECT_EQ(s2.begin()->before_spaces_, 2);

  auto* sut_ptr = TreeReshaper::HPlusSolution(s1, s2, style);
  auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

  EXPECT_EQ(sut.size(), 1);
  EXPECT_EQ(sut[0].column_, 0);
  EXPECT_EQ(sut[0].span_, 21 + 2);
  EXPECT_EQ(sut[0].intercept_, 0);
  EXPECT_EQ(sut[0].gradient_, 0);
  EXPECT_EQ(sut[0].before_spaces_, 1);

  const auto* lut = sut[0].layout_;
  EXPECT_FALSE(lut == nullptr);
  //LOG(INFO) << std::endl << *ABSL_DIE_IF_NULL(sut[0].layout_) << std::endl;
  //EXPECT_EQ(lut->Children().size(), 0);
  //auto& uwline = lut->Value();
  //EXPECT_EQ(uwline.TokensRange().begin(), first_line.TokensRange().begin());
  //EXPECT_EQ(uwline.TokensRange().end(), second_line.TokensRange().end());

  delete sut_ptr;
}


TEST_F(KnotSetTest, VSumSolution2Solutions) {
  TreeReshaper::KnotSet s1, s2;

  s1.AppendKnot(TreeReshaper::Knot(0, 11, 0, 0,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));
  s1.AppendKnot(TreeReshaper::Knot(5, 11, 20, 3,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));

  s2.AppendKnot(TreeReshaper::Knot(0, 17, 0, 0,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));
  s2.AppendKnot(TreeReshaper::Knot(20, 17, 20, 5,
      new TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}));

  TreeReshaper::SolutionSet set;
  set.push_back(&s1);
  set.push_back(&s2);

  BasicFormatStyle style;
  auto* sut_ptr = TreeReshaper::VSumSolution(set, style);
  auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
  EXPECT_EQ(sut.size(), 3); // 0, 5, 20

  EXPECT_EQ(sut[0].column_, 0);
  EXPECT_EQ(sut[0].span_, 17);
  EXPECT_EQ(sut[0].intercept_, 0);
  EXPECT_EQ(sut[0].gradient_, 0);

  EXPECT_EQ(sut[1].column_, 5);
  EXPECT_EQ(sut[1].span_, 17);
  EXPECT_EQ(sut[1].intercept_, 20);
  EXPECT_EQ(sut[1].gradient_, 3);

  EXPECT_EQ(sut[2].column_, 20);
  EXPECT_EQ(sut[2].span_, 17);
  EXPECT_EQ(sut[2].intercept_, 40 + 3 * 15);
  EXPECT_EQ(sut[2].gradient_, 8);

  delete sut_ptr;
}

class TextBlockFixture
  : public ::testing::Test,
    public UnwrappedLineMemoryHandler {
 public:
  TextBlockFixture()
      : sample_(
            "short_text "
            "looooooooooooooooooong_text"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    pre_format_tokens_[0].before.spaces_required = 1;
    pre_format_tokens_[1].before.spaces_required = 1;
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class TextBlockFixtureTest : public TextBlockFixture {};

TEST_F(TextBlockFixtureTest,
       TestIndentation) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  BasicFormatStyle style;  // default, column_limit == 100

  UnwrappedLine text_noindent(0, begin);
  text_noindent.SpanUpToToken(begin + 1);
  TreeReshaper::BlockTree block_noindent{
      TreeReshaper::Layout{TreeReshaper::LayoutType::kText}};
  block_noindent.Value().uwline_ = text_noindent;
  auto* noindent_sut_ptr = TreeReshaper::ComputeSolution(
      block_noindent, TreeReshaper::KnotSet{}, style);
  auto& noindent_sut = *ABSL_DIE_IF_NULL(noindent_sut_ptr);
  EXPECT_GE(noindent_sut.size(), 1);
  EXPECT_EQ(noindent_sut[0].span_, 10);
  //{
  //  const auto* lut = noindent_sut[0].layout_;
  //  EXPECT_FALSE(lut == nullptr);
  //  const auto& uwline = lut->Value();
  //  EXPECT_EQ(uwline.TokensRange().begin(), text_noindent.TokensRange().begin());
  //  EXPECT_EQ(uwline.TokensRange().end(), text_noindent.TokensRange().end());
  //}

  UnwrappedLine text_indent(4, begin);
  text_indent.SpanUpToToken(begin + 1);
  TreeReshaper::BlockTree block_indent{
      TreeReshaper::Layout{TreeReshaper::LayoutType::kText}};
  block_indent.Value().uwline_ = text_indent;
  auto* indent_sut_ptr = TreeReshaper::ComputeSolution(
      block_indent, TreeReshaper::KnotSet{}, style);
  auto& indent_sut = *ABSL_DIE_IF_NULL(indent_sut_ptr);
  EXPECT_GE(indent_sut.size(), 1);
  EXPECT_EQ(indent_sut[0].span_, 14);
  //{
  //  const auto* lut = indent_sut[0].layout_;
  //  EXPECT_FALSE(lut == nullptr);
  //  const auto& uwline = lut->Value();
  //  // Test againt text_unindented would pass this test too (same tokens range)
  //  EXPECT_EQ(uwline.TokensRange().begin(), text_indent.TokensRange().begin());
  //  EXPECT_EQ(uwline.TokensRange().end(), text_indent.TokensRange().end());
  //}
}

TEST_F(TextBlockFixtureTest,
       TextBelowColumnLimit) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine text(0, begin);
  text.SpanUpToToken(begin + 1);

  BasicFormatStyle style;
  style.column_limit = 20;

  TreeReshaper::BlockTree block{
      TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}};
  block.Value().uwline_ = text;
  auto* sut_ptr = TreeReshaper::ComputeSolution(block, TreeReshaper::KnotSet{}, style);
  auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

  // two knots
  EXPECT_EQ(sut.size(), 2);

  EXPECT_EQ(sut[0].column_, 0);
  EXPECT_EQ(sut[0].span_, 10);
  EXPECT_EQ(sut[0].intercept_, 0);  // fits on line, no additional cost
  EXPECT_EQ(sut[0].gradient_, 0);  // as above

  EXPECT_EQ(sut[1].column_, style.column_limit - 10);
  EXPECT_EQ(sut[1].span_, 10);
  EXPECT_EQ(sut[1].intercept_, 0); // fits on line, no additional cost
  EXPECT_EQ(sut[1].gradient_,
      style.over_column_limit_penalty); // cost for characters above column limit

  // FIXME(ldk): test layout_
}

TEST_F(TextBlockFixtureTest,
       TextAboveColumnLimit) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine text(0, begin + 1);
  text.SpanUpToToken(preformat_tokens.end());

  BasicFormatStyle style;
  style.column_limit = 20;

  TreeReshaper::BlockTree block{
      TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}};
  block.Value().uwline_ = text;
  auto* sut_ptr = TreeReshaper::ComputeSolution(block, TreeReshaper::KnotSet{}, style);
  auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

  // two knots
  EXPECT_EQ(sut.size(), 1);

  EXPECT_EQ(sut[0].column_, 0);
  EXPECT_EQ(sut[0].span_, 27);
  EXPECT_EQ(sut[0].intercept_, 7 *
      style.over_column_limit_penalty);  // 7 characters over column limit
  EXPECT_EQ(sut[0].gradient_,
      style.over_column_limit_penalty); // cost for characters above column limit
  EXPECT_EQ(sut[0].before_spaces_, 1);

  // FIXME(ldk): test layout_
}

class StackBlockFixture
  : public ::testing::Test,
    public UnwrappedLineMemoryHandler {
 public:
  StackBlockFixture()
      : sample_(
            "text_line "  // 9 chars
            "looonger_text_line "  // 18 chars
            "loooooooooooong_teeeeeeext_lineeeee"),  // 35 chars
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    for (auto& itr : pre_format_tokens_) {
      itr.before.spaces_required = 1;
    }
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class StackBlockFixtureTest : public StackBlockFixture {};

TEST_F(StackBlockFixtureTest,
       StackBlockBelowColumnLimit) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  BasicFormatStyle style;  // default, column_limit == 100
  TreeReshaper::BlockTree stack_block{
    TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kStack}};

  UnwrappedLine text_1(0, begin);
  text_1.SpanUpToToken(begin + 1);
  TreeReshaper::BlockTree block_1{
    TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}};
  block_1.Value().uwline_ = text_1;
  {
    auto* tsut_ptr = TreeReshaper::ComputeSolution(block_1,
        TreeReshaper::KnotSet{}, style);
    auto& tsut = *ABSL_DIE_IF_NULL(tsut_ptr);

    EXPECT_EQ(tsut.size(), 2);

    EXPECT_EQ(tsut[0].column_, 0);
    EXPECT_EQ(tsut[0].span_, 9);
    EXPECT_EQ(tsut[0].intercept_, 0);
    EXPECT_EQ(tsut[0].gradient_, 0);

    EXPECT_EQ(tsut[1].column_, style.column_limit - 9);
    EXPECT_EQ(tsut[1].span_, 9);
    EXPECT_EQ(tsut[1].intercept_, 0);
    EXPECT_EQ(tsut[1].gradient_, style.over_column_limit_penalty);
  }
  stack_block.AdoptSubtree(block_1);

  UnwrappedLine text_2(0, begin + 1);
  text_2.SpanUpToToken(begin + 2);
  TreeReshaper::BlockTree block_2{
      TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}};
  block_2.Value().uwline_ = text_2;
  {
    auto* tsut_ptr = TreeReshaper::ComputeSolution(block_2,
        TreeReshaper::KnotSet{}, style);
    auto& tsut = *ABSL_DIE_IF_NULL(tsut_ptr);

    EXPECT_EQ(tsut.size(), 2);

    EXPECT_EQ(tsut[0].column_, 0);
    EXPECT_EQ(tsut[0].span_, 18);
    EXPECT_EQ(tsut[0].intercept_, 0);
    EXPECT_EQ(tsut[0].gradient_, 0);

    EXPECT_EQ(tsut[1].column_, style.column_limit - 18);
    EXPECT_EQ(tsut[1].span_, 18);
    EXPECT_EQ(tsut[1].intercept_, 0);
    EXPECT_EQ(tsut[1].gradient_, style.over_column_limit_penalty);
  }
  stack_block.AdoptSubtree(block_2);

  UnwrappedLine text_3(0, begin + 2);
  text_3.SpanUpToToken(begin + 3);
  TreeReshaper::BlockTree block_3{
      TreeReshaper::LayoutTree{TreeReshaper::LayoutType::kText}};
  block_3.Value().uwline_ = text_3;
  {
    auto* tsut_ptr = TreeReshaper::ComputeSolution(block_3,
        TreeReshaper::KnotSet{}, style);
    auto& tsut = *ABSL_DIE_IF_NULL(tsut_ptr);

    EXPECT_EQ(tsut.size(), 2);

    EXPECT_EQ(tsut[0].column_, 0);
    EXPECT_EQ(tsut[0].span_, 35);
    EXPECT_EQ(tsut[0].intercept_, 0);
    EXPECT_EQ(tsut[0].gradient_, 0);

    EXPECT_EQ(tsut[1].column_, style.column_limit - 35);
    EXPECT_EQ(tsut[1].span_, 35);
    EXPECT_EQ(tsut[1].intercept_, 0);
    EXPECT_EQ(tsut[1].gradient_, style.over_column_limit_penalty);
  }
  stack_block.AdoptSubtree(block_3);

  auto* sut_ptr = TreeReshaper::ComputeSolution(
      stack_block, TreeReshaper::KnotSet{}, style);
  auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

  EXPECT_EQ(sut.size(), 4);

  EXPECT_EQ(sut[0].column_, 0);
  EXPECT_EQ(sut[0].span_, 35);
  EXPECT_EQ(sut[0].intercept_, (3 - 1) * style.line_break_penalty);  // 3 == no. of lines
  EXPECT_EQ(sut[0].gradient_, 0);
  //EXPECT_TRUE(false) << *ABSL_DIE_IF_NULL(sut[0].layout_);
  //EXPECT_TRUE(false) << TokenPartitionTreePrinter(
  //    *ABSL_DIE_IF_NULL(ABSL_DIE_IF_NULL(sut[0].layout_)->GetTokenPartitionTree()));
  //EXPECT_TRUE(false) <<
  //    *ABSL_DIE_IF_NULL(ABSL_DIE_IF_NULL(sut[0].layout_)->GetTokenPartitionTree());

  EXPECT_EQ(sut[1].column_, 100 - 35);
  EXPECT_EQ(sut[1].span_, 35);
  EXPECT_EQ(sut[1].intercept_, 4);
  EXPECT_EQ(sut[1].gradient_, style.over_column_limit_penalty);

  EXPECT_EQ(sut[2].column_, 100 - 18);
  EXPECT_EQ(sut[2].span_, 35);
  EXPECT_EQ(sut[2].intercept_, (35 - 18) * style.over_column_limit_penalty + 4);
  EXPECT_EQ(sut[2].gradient_, 2 * style.over_column_limit_penalty);

  EXPECT_EQ(sut[3].column_, 100 - 9);
  EXPECT_EQ(sut[3].span_, 35);
  EXPECT_EQ(sut[3].intercept_, 4 +
                               (35 - 9) * style.over_column_limit_penalty +
                               (18 - 9) * style.over_column_limit_penalty);
  EXPECT_EQ(sut[3].gradient_, 3 * style.over_column_limit_penalty);
}

class LineBlockFixture
  : public ::testing::Test,
    public UnwrappedLineMemoryHandler {
 public:
  LineBlockFixture()
      : sample_(
            "left_text "
            "middle_text "
            "right_text"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    pre_format_tokens_[0].before.spaces_required = 1;
    pre_format_tokens_[1].before.spaces_required = 1;
    pre_format_tokens_[2].before.spaces_required = 1;
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class LineBlockFixtureTest : public LineBlockFixture {};

TEST_F(LineBlockFixtureTest,
       LineBlockBelowColumnLimit) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  BasicFormatStyle style;  // default, column_limit == 100
  TreeReshaper::BlockTree line_block{
      TreeReshaper::LayoutType::kLine};

  UnwrappedLine text_1(0, begin);
  text_1.SpanUpToToken(begin + 1);
  TreeReshaper::BlockTree block_1{
      TreeReshaper::LayoutType::kText};
  block_1.Value().uwline_ = text_1;
  {
    auto* tsut_ptr = TreeReshaper::ComputeSolution(
        block_1, TreeReshaper::KnotSet{}, style);
    auto& tsut = *ABSL_DIE_IF_NULL(tsut_ptr);

    EXPECT_EQ(tsut.size(), 2);

    EXPECT_EQ(tsut[0].column_, 0);
    EXPECT_EQ(tsut[0].span_, 9);
    EXPECT_EQ(tsut[0].intercept_, 0);
    EXPECT_EQ(tsut[0].gradient_, 0);

    EXPECT_EQ(tsut[1].column_, style.column_limit - 9);
    EXPECT_EQ(tsut[1].span_, 9);
    EXPECT_EQ(tsut[1].intercept_, 0);
    EXPECT_EQ(tsut[1].gradient_, style.over_column_limit_penalty);
  }
  line_block.AdoptSubtree(block_1);

  UnwrappedLine text_2(0, begin + 1);
  text_2.SpanUpToToken(begin + 2);
  TreeReshaper::BlockTree block_2{
      TreeReshaper::LayoutType::kText};
  block_2.Value().uwline_ = text_2;
  {
    auto* tsut_ptr = TreeReshaper::ComputeSolution(
        block_2, TreeReshaper::KnotSet{}, style);
    auto& tsut = *ABSL_DIE_IF_NULL(tsut_ptr);

    EXPECT_EQ(tsut.size(), 2);

    EXPECT_EQ(tsut[0].column_, 0);
    EXPECT_EQ(tsut[0].span_, 11);
    EXPECT_EQ(tsut[0].intercept_, 0);
    EXPECT_EQ(tsut[0].gradient_, 0);

    EXPECT_EQ(tsut[1].column_, style.column_limit - 11);
    EXPECT_EQ(tsut[1].span_, 11);
    EXPECT_EQ(tsut[1].intercept_, 0);
    EXPECT_EQ(tsut[1].gradient_, style.over_column_limit_penalty);
  }
  line_block.AdoptSubtree(block_2);

  UnwrappedLine text_3(0, begin + 2);
  text_3.SpanUpToToken(begin + 3);
  TreeReshaper::BlockTree block_3{
      TreeReshaper::LayoutType::kText};
  block_3.Value().uwline_ = text_3;
  {
    auto* tsut_ptr = TreeReshaper::ComputeSolution(
        block_3, TreeReshaper::KnotSet{}, style);
    auto& tsut = *ABSL_DIE_IF_NULL(tsut_ptr);

    EXPECT_EQ(tsut.size(), 2);

    EXPECT_EQ(tsut[0].column_, 0);
    EXPECT_EQ(tsut[0].span_, 10);
    EXPECT_EQ(tsut[0].intercept_, 0);
    EXPECT_EQ(tsut[0].gradient_, 0);

    EXPECT_EQ(tsut[1].column_, style.column_limit - 10);
    EXPECT_EQ(tsut[1].span_, 10);
    EXPECT_EQ(tsut[1].intercept_, 0);
    EXPECT_EQ(tsut[1].gradient_, style.over_column_limit_penalty);
  }
  line_block.AdoptSubtree(block_3);

  auto* sut_ptr = TreeReshaper::ComputeSolution(
      line_block, TreeReshaper::KnotSet{}, style);
  auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

  EXPECT_EQ(sut.size(), 4);

  // Every pre_format_tokens_ has before.spaces_required == 1
  const auto extra_span = 1;

  EXPECT_EQ(sut[0].column_, 0);
  EXPECT_EQ(sut[0].span_, 30 + extra_span * 2);
  EXPECT_EQ(sut[0].intercept_, 0);
  EXPECT_EQ(sut[0].gradient_, 0);
  //LOG(INFO) << std::endl << TokenPartitionTreePrinter(
  //    *ABSL_DIE_IF_NULL(BuildTokenPartitionTree(
  //        *ABSL_DIE_IF_NULL(sut[0].layout_)))) << std::endl;

  // FIXME(ldk): Test DynamicSolutionTree instead of TokenPartitionTree
  //    Move this to separate test
  {
    using tree_type = TokenPartitionTree;

    const tree_type* tree = ABSL_DIE_IF_NULL(
        TreeReshaper::BuildTokenPartitionTree(*ABSL_DIE_IF_NULL(sut[0].layout_)));

    UnwrappedLine all(0, begin);
    all.SpanUpToToken(begin + 3);

    UnwrappedLine left(0, begin);
    left.SpanUpToToken(begin + 1);
    UnwrappedLine middle(0, begin + 1);
    middle.SpanUpToToken(begin + 2);
    UnwrappedLine right(0, begin + 2);
    right.SpanUpToToken(begin + 3);

    const tree_type tree_expected{
      all,
      //left,
      //middle,
      //right,
    };

    // FIXME(ldk): Borrowed from token_partition_tree_test.
    auto token_range_equal = std::function<bool(const UnwrappedLine&,
                                                const UnwrappedLine&)>(
        [](const UnwrappedLine& left, const UnwrappedLine& right) {
          return left.TokensRange() == right.TokensRange();
        });

    // FIXME(ldk): Check PartitionPolicyEnum (should be kFitOnLineElseExpand)
    const auto diff = DeepEqual(*tree, tree_expected, token_range_equal);
    EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                  << tree_expected << "\nGot:\n" << *tree << "\n";
  }

  EXPECT_EQ(sut[1].column_, style.column_limit - 30 - 2 * extra_span);
  EXPECT_EQ(sut[1].span_, 30 + 2 * extra_span);
  EXPECT_EQ(sut[1].intercept_, 0);
  EXPECT_EQ(sut[1].gradient_, style.over_column_limit_penalty);

  EXPECT_EQ(sut[2].column_, style.column_limit - 20 - extra_span);
  EXPECT_EQ(sut[2].span_, 30 + 2 * extra_span);
  EXPECT_EQ(sut[2].intercept_, 10 * style.over_column_limit_penalty);
  EXPECT_EQ(sut[2].gradient_, style.over_column_limit_penalty);

  EXPECT_EQ(sut[3].column_, style.column_limit - 9);
  EXPECT_EQ(sut[3].span_, 30 + 2 * extra_span);
  EXPECT_EQ(sut[3].intercept_, 21 * style.over_column_limit_penalty);
  EXPECT_EQ(sut[3].gradient_, style.over_column_limit_penalty);
}

class ChoiceBlockFixture
    : public ::testing::Test,
      public UnwrappedLineMemoryHandler {
 public:
  ChoiceBlockFixture()
      // combining what would normally be a type and a variable name
      // into a single string for testing convenience
      : sample_(
            "function_ffffffffffffff( "  // 24
            "type_a_aaaaaaaa, "  // 16
            "type_b_bbbbbbbbbbbbbbbb);"),  // 25
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    pre_format_tokens_[0].before.spaces_required = 1;
    pre_format_tokens_[1].before.spaces_required = 1;
    pre_format_tokens_[2].before.spaces_required = 1;
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class ChoiceBlockFixtureTest : public ChoiceBlockFixture {};

static bool DynamicSolutionTreeEqual(const TreeReshaper::Layout& left,
                                     const TreeReshaper::Layout& right) {
  if (left.type_ != right.type_) {
    return false;
  }

  if (left.type_ == TreeReshaper::LayoutType::kText) {
    return left.uwline_.TokensRange() == right.uwline_.TokensRange();
  }

  return true;
};

TEST_F(ChoiceBlockFixtureTest,
       ChoiceBlockFunctionWithTwoArguments) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  //using tree_type = DynamicSolutionTree;

  // 'function_ffffffffffffff('
  UnwrappedLine function_header(0, begin);
  function_header.SpanUpToToken(begin + 1);

  // function arguments
  UnwrappedLine unindented_arg1(0, begin + 1);
  unindented_arg1.SpanUpToToken(begin + 2);
  UnwrappedLine unindented_arg2(0, begin + 2);
  unindented_arg2.SpanUpToToken(begin + 3);

  // indented function arguments
  UnwrappedLine indented_arg1(4, begin + 1);
  indented_arg1.SpanUpToToken(begin + 2);
  UnwrappedLine indented_arg2(4, begin + 2);
  indented_arg2.SpanUpToToken(begin + 3);

  // Prepare
  TreeReshaper::BlockTree text_header{
      TreeReshaper::LayoutType::kText};
  text_header.Value().uwline_ = function_header;

  TreeReshaper::BlockTree text_unindented_arg1{
      TreeReshaper::LayoutType::kText};
  text_unindented_arg1.Value().uwline_ = unindented_arg1;
  TreeReshaper::BlockTree text_unindented_arg2{
      TreeReshaper::LayoutType::kText};
  text_unindented_arg2.Value().uwline_ = unindented_arg2;

  TreeReshaper::BlockTree text_indented_arg1{
      TreeReshaper::LayoutType::kText};
  text_indented_arg1.Value().uwline_ = indented_arg1;
  TreeReshaper::BlockTree text_indented_arg2{
      TreeReshaper::LayoutType::kText};
  text_indented_arg2.Value().uwline_ = indented_arg2;

  // Normally for situation like here we would use WrapBlock which
  // would 'generate' such layouts for us. But this time we want to do
  // it manually to test how layouts work with each other and to test
  // algorithm itself

  // FIXME(ldk): Memory leak?
  // Put everything in one line
  TreeReshaper::BlockTree all_in_one_line{
      TreeReshaper::LayoutType::kLine};
  all_in_one_line.AdoptSubtree(text_header);
  all_in_one_line.AdoptSubtree(text_unindented_arg1);
  all_in_one_line.AdoptSubtree(text_unindented_arg2);

  // Wrap all arguments
  TreeReshaper::BlockTree wrapped_arguments{
      TreeReshaper::LayoutType::kStack};
  wrapped_arguments.AdoptSubtree(text_header);
  wrapped_arguments.AdoptSubtree(text_indented_arg1);
  wrapped_arguments.AdoptSubtree(text_indented_arg2);

  // FIXME(ldk): Add rest of layouts

  // Append first argument, wrap second
  TreeReshaper::BlockTree header_and_first_argument{
      TreeReshaper::LayoutType::kLine};
  header_and_first_argument.AdoptSubtree(text_header);
  header_and_first_argument.AdoptSubtree(text_unindented_arg1);

  TreeReshaper::BlockTree appended_first_argument{
      TreeReshaper::LayoutType::kStack};
  appended_first_argument.AdoptSubtree(header_and_first_argument);
  appended_first_argument.AdoptSubtree(text_indented_arg2);

  // Choose between prepared layouts
  TreeReshaper::BlockTree choice_block{
      TreeReshaper::LayoutType::kChoice};
  choice_block.AdoptSubtree(all_in_one_line);
  choice_block.AdoptSubtree(wrapped_arguments);
  choice_block.AdoptSubtree(appended_first_argument);

  BasicFormatStyle style;

  {
    style.column_limit = 39;  // line does not fit, choice wrapped solution
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        choice_block, TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
    EXPECT_GT(sut.size(), 1);
    EXPECT_EQ(sut[0].column_, 0);
    EXPECT_EQ(sut[0].span_, 29);  // size of the last line in layout (wrapped last argument)
    EXPECT_EQ(sut[0].intercept_, style.line_break_penalty * 2);  // 3 lines => 2 line breaks
    EXPECT_EQ(sut[0].gradient_, 0);
    {
      using tree_type = TreeReshaper::LayoutTree;

      const tree_type* tree = ABSL_DIE_IF_NULL(sut[0].layout_);

      const tree_type tree_expected{
        TreeReshaper::LayoutType::kStack,
        tree_type{function_header},
        tree_type{indented_arg1},
        tree_type{indented_arg2},
      };

      const auto diff = DeepEqual(*tree, tree_expected, DynamicSolutionTreeEqual);
      EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                    << tree_expected << "\nGot:\n" << *tree << "\n";
    }
  }

  {
    style.column_limit = 100;  // line fits, choice all in one line solution
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        choice_block, TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
    EXPECT_GT(sut.size(), 1);
    EXPECT_EQ(sut[0].column_, 0);
    // FIXME(ldk): Shouldn't be here 66? (65 + extra_space)
    EXPECT_EQ(sut[0].span_, 67);  // size of last line (in this case whole expression)
    EXPECT_EQ(sut[0].intercept_, 0);
    EXPECT_EQ(sut[0].gradient_, 0);
    {
      using tree_type = TreeReshaper::LayoutTree;

      const tree_type* tree = ABSL_DIE_IF_NULL(sut[0].layout_);

      const tree_type tree_expected{
        TreeReshaper::LayoutType::kLine,
        tree_type{function_header},
        tree_type{
          TreeReshaper::LayoutType::kLine,
          tree_type{unindented_arg1},
          tree_type{unindented_arg2},
        },
      };

      const auto diff = DeepEqual(*tree, tree_expected, DynamicSolutionTreeEqual);
      EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                    << tree_expected << "\nGot:\n" << *tree << "\n";
    }
  }

  {
    style.column_limit = 60;  // fits with appended first argument
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        choice_block, TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
    EXPECT_GT(sut.size(), 1);
    EXPECT_EQ(sut[0].column_, 0);
    EXPECT_EQ(sut[0].span_, 29);
    EXPECT_EQ(sut[0].intercept_, style.line_break_penalty * 1);  // 2 lines => 1 line break
    EXPECT_EQ(sut[0].gradient_, 0);
    {
      using tree_type = TreeReshaper::LayoutTree;

      const tree_type* tree = ABSL_DIE_IF_NULL(sut[0].layout_);

      const tree_type tree_expected{
        TreeReshaper::LayoutType::kStack,
        tree_type{
          TreeReshaper::LayoutType::kLine,
          tree_type{function_header},
          tree_type{unindented_arg1},
        },
        tree_type{indented_arg2},
      };

      const auto diff = DeepEqual(*tree, tree_expected, DynamicSolutionTreeEqual);
      EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                    << tree_expected << "\nGot:\n" << *tree << "\n";
    }
  }

  // FIXME(ldk): Check rest of solutions
}

class WrapBlockFixture
    : public ::testing::Test,
      public UnwrappedLineMemoryHandler {
 public:
  WrapBlockFixture()
      // combining what would normally be a type and a variable name
      // into a single string for testing convenience
      : sample_(
            "type_a_aaaaaaaa, "  // 16
            "type_b_bbbbbbbbbbbb, "  // 20
            "type_c_ccccccccccccccc, "  // 23
            "type_d_dddddddddddddddddd, "  // 26
            "type_e_eeeeee);"),  // 15
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    pre_format_tokens_[0].before.spaces_required = 1;
    pre_format_tokens_[1].before.spaces_required = 1;
    pre_format_tokens_[2].before.spaces_required = 1;
    pre_format_tokens_[3].before.spaces_required = 1;
    pre_format_tokens_[4].before.spaces_required = 1;
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class WrapBlockFixtureTest : public WrapBlockFixture {};

TEST_F(WrapBlockFixtureTest,
       WrapBlockFiveArguments) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  // (function) arguments
  UnwrappedLine arg1(0, begin);
  arg1.SpanUpToToken(begin + 1);
  UnwrappedLine arg2(0, begin + 1);
  arg2.SpanUpToToken(begin + 2);
  UnwrappedLine arg3(0, begin + 2);
  arg3.SpanUpToToken(begin + 3);
  UnwrappedLine arg4(0, begin + 3);
  arg4.SpanUpToToken(begin + 4);
  UnwrappedLine arg5(0, begin + 4);
  arg5.SpanUpToToken(begin + 5);

  BasicFormatStyle style;

  EXPECT_EQ(UnwrappedLineLength(arg1, style), 16);
  EXPECT_EQ(UnwrappedLineLength(arg2, style), 20);
  EXPECT_EQ(UnwrappedLineLength(arg3, style), 23);
  EXPECT_EQ(UnwrappedLineLength(arg4, style), 26);
  EXPECT_EQ(UnwrappedLineLength(arg5, style), 15);

  // Prepare for WrapBlock
  TreeReshaper::BlockTree text_arg1{
      TreeReshaper::LayoutType::kText};
  text_arg1.Value().uwline_ = arg1;
  TreeReshaper::BlockTree text_arg2{
      TreeReshaper::LayoutType::kText};
  text_arg2.Value().uwline_ = arg2;
  TreeReshaper::BlockTree text_arg3{
      TreeReshaper::LayoutType::kText};
  text_arg3.Value().uwline_ = arg3;
  TreeReshaper::BlockTree text_arg4{
      TreeReshaper::LayoutType::kText};
  text_arg4.Value().uwline_ = arg4;
  TreeReshaper::BlockTree text_arg5{
      TreeReshaper::LayoutType::kText};
  text_arg5.Value().uwline_ = arg5;

  TreeReshaper::BlockTree wrap_block{
      TreeReshaper::LayoutType::kWrap};
  wrap_block.AdoptSubtree(text_arg1);
  wrap_block.AdoptSubtree(text_arg2);
  wrap_block.AdoptSubtree(text_arg3);
  wrap_block.AdoptSubtree(text_arg4);
  wrap_block.AdoptSubtree(text_arg5);

  const auto extra_span = 1;

  {
    style.column_limit = 104;
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        wrap_block, TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
    EXPECT_GT(sut.size(), 0);

    EXPECT_EQ(sut[0].column_, 0);
    EXPECT_EQ(sut[0].span_, 100 + 4 * extra_span);  // 16 + 20 + 23 + 26 + 15
    EXPECT_EQ(sut[0].intercept_, 0);  // fits (perfectly) in one line
    EXPECT_EQ(sut[0].gradient_,
        style.over_column_limit_penalty);  // next character would be over margin
    //EXPECT_TRUE(false) << *ABSL_DIE_IF_NULL(sut[0].layout_);
  }

  {
    style.column_limit = 62;
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        wrap_block, TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

    EXPECT_GT(sut.size(), 0);
    EXPECT_EQ(sut[0].column_, 0);
    EXPECT_EQ(sut[0].span_, 26 + 15 + extra_span);  // 4th and 5th argument in last line
    EXPECT_EQ(sut[0].intercept_, (2 - 1) * style.line_break_penalty);  // two lines
    EXPECT_EQ(sut[0].gradient_, 0);

    // FIXME(ldk): Test layout in same as token_partition_tree_test (DeepEqual) does
    //LOG(INFO) << std::endl << *ABSL_DIE_IF_NULL(sut[0].layout_) << std::endl;

    //LOG(INFO) << std::endl << TokenPartitionTreePrinter(
    //    *ABSL_DIE_IF_NULL(BuildTokenPartitionTree(
    //      *ABSL_DIE_IF_NULL(sut[0].layout_)))) << std::endl;
    {  // FIXME(ldk): Move to its own test
      using tree_type = TokenPartitionTree;

      const tree_type* tree = ABSL_DIE_IF_NULL(
          TreeReshaper::BuildTokenPartitionTree(
              *ABSL_DIE_IF_NULL(sut[0].layout_)));

      UnwrappedLine all(0, begin);
      all.SpanUpToToken(arg5.TokensRange().end());

      UnwrappedLine line_1(0, arg1.TokensRange().begin());
      line_1.SpanUpToToken(arg3.TokensRange().end());
      UnwrappedLine line_2(0, arg4.TokensRange().begin());
      line_2.SpanUpToToken(arg5.TokensRange().end());

      const tree_type tree_expected{
        all,
        tree_type{
          line_1,
          //arg1,
          //arg2,
          //arg3,
        },
        tree_type{
          line_2,
          //arg4,
          //arg5,
        },
      };

      // FIXME(ldk): Stolen, I mean borrowed, from token_partition_tree_test.
      //    Make token_partition_tree_test to share it.
      auto token_range_equal = std::function<bool(const UnwrappedLine&,
                                                  const UnwrappedLine&)>(
          [](const UnwrappedLine& left, const UnwrappedLine& right) {
            return left.TokensRange() == right.TokensRange();
          });

      const auto diff = DeepEqual(*tree, tree_expected, token_range_equal);
      EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                    << tree_expected << "\nGot:\n" << *tree << "\n";
    }
  }

  {
    style.column_limit = 40;
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        wrap_block, TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);

    EXPECT_GT(sut.size(), 0);
    EXPECT_EQ(sut[0].column_, 0);
    EXPECT_EQ(sut[0].span_, 15);  // last argument
    EXPECT_EQ(sut[0].intercept_, (4 - 1) * style.line_break_penalty);
    EXPECT_EQ(sut[0].gradient_, 0);
    //EXPECT_TRUE(false) << *ABSL_DIE_IF_NULL(sut[0].layout_);
    //EXPECT_TRUE(false) << TokenPartitionTreePrinter(
    //    *ABSL_DIE_IF_NULL(ABSL_DIE_IF_NULL(sut[0].layout_)->GetTokenPartitionTree()));
    //EXPECT_TRUE(false) <<
    //    *ABSL_DIE_IF_NULL(ABSL_DIE_IF_NULL(sut[0].layout_)->GetTokenPartitionTree());
  }

  // FIXME(ldk): Check all solution (including intercepts, gradients and
  //    (yet unimplemented) layouts);
}

// Compare with ReshapeFittingSubpartitions function
class ReshapeFittingSubpartitionsTestFixture
    : public ::testing::Test,
      public UnwrappedLineMemoryHandler {
 public:
  ReshapeFittingSubpartitionsTestFixture()
      // combining what would normally be a type and a variable name
      // into a single string for testing convenience
      : sample_(
            "function_fffffffffff( "
            "type_a_aaaa, type_b_bbbbb, "
            "type_c_cccccc, type_d_dddddddd, "
            "type_e_eeeeeeee, type_f_ffff);"),
        tokens_(absl::StrSplit(sample_, ' ')) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    CreateTokenInfos(ftokens_);
    for (auto& itr : pre_format_tokens_) {
      itr.before.spaces_required = 1;
    }
    pre_format_tokens_[1].before.spaces_required = 0;
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class ReshapeFittingSubpartitionsFunctionTest
    : public ReshapeFittingSubpartitionsTestFixture {};

TEST_F(ReshapeFittingSubpartitionsFunctionTest,
       FunctionWithSixArgumentsAndExpectedLayouts) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  all.SetPartitionPolicy(PartitionPolicyEnum::kApplyOptimalLayout);

  // 'function_fffffffffff('
  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);
  header.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

  // function arguments
  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  arg1.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  arg2.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  arg3.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  arg4.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);
  arg5.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg6(0, arg5.TokensRange().end());
  arg6.SpanUpToToken(all.TokensRange().end());
  arg6.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg6.TokensRange().end());
  args.SetPartitionPolicy(PartitionPolicyEnum::kWrapSubPartitions);

  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{header},
      tree_type{
          args,
          tree_type{arg1},
          tree_type{arg2},
          tree_type{arg3},
          tree_type{arg4},
          tree_type{arg5},
          tree_type{arg6},
      },
  };

  {
    TreeReshaper::LayoutTree* layout_tree =
        TreeReshaper::BuildLayoutTreeFromTokenPartitionTree(tree);

    BasicFormatStyle style;
    style.column_limit = 50;
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        *ABSL_DIE_IF_NULL(layout_tree), TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
    LOG(INFO) << "Sut:\n" << *ABSL_DIE_IF_NULL(sut[0].layout_);
    auto* formatted_tree = ABSL_DIE_IF_NULL(
        TreeReshaper::BuildTokenPartitionTree(*ABSL_DIE_IF_NULL(sut[0].layout_)));
    LOG(INFO) << "Tree:\n" << *formatted_tree;
  }

  {
    TreeReshaper::LayoutTree* layout_tree =
        TreeReshaper::BuildLayoutTreeFromTokenPartitionTree(tree);
    //EXPECT_FALSE(block_tree == nullptr);
    // FIXME(ldk): Test built block tree

    using tree_type = TokenPartitionTree;

    BasicFormatStyle style;
    style.column_limit = 51;
    auto* sut_ptr = TreeReshaper::ComputeSolution(
        *ABSL_DIE_IF_NULL(layout_tree), TreeReshaper::KnotSet{}, style);
    auto& sut = *ABSL_DIE_IF_NULL(sut_ptr);
    EXPECT_GT(sut.size(), 0);

    const tree_type* tree = ABSL_DIE_IF_NULL(
        TreeReshaper::BuildTokenPartitionTree(*ABSL_DIE_IF_NULL(sut[0].layout_)));

    UnwrappedLine group_1(0, begin);
    group_1.SpanUpToToken(arg1.TokensRange().end());
    UnwrappedLine group_2(0, arg2.TokensRange().begin());
    group_2.SpanUpToToken(arg3.TokensRange().end());
    UnwrappedLine group_3(0, arg5.TokensRange().begin());
    group_3.SpanUpToToken(arg6.TokensRange().end());

    // function_fffffffffff(type_a_aaaa,
    //                      type_b_bbbbb, type_c_cccccc,
    //                      type_d_dddddddd,
    //                      type_e_eeeeeeee, type_f_ffff);
    const tree_type tree_expected{all,
                                  tree_type{
                                      group_1,
                                      //tree_type{header},
                                      //tree_type{arg1},
                                  },
                                  tree_type{
                                      group_2,
                                      //tree_type{arg2},
                                      //tree_type{arg3},
                                  },
                                  tree_type{arg4},
                                  tree_type{
                                      group_3,
                                      //tree_type{arg5},
                                      //tree_type{arg6},
                                  }};

    // FIXME(ldk): Borrowed
    auto token_range_equal = std::function<bool(const UnwrappedLine&,
                                                const UnwrappedLine&)>(
        [](const UnwrappedLine& left, const UnwrappedLine& right) {
          return left.TokensRange() == right.TokensRange();
        });

    const auto diff = DeepEqual(*tree, tree_expected, token_range_equal);
    EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                  << tree_expected << "\nGot:\n" << *tree << "\n";

    // FIXME(ldk): Find a smarter way to test indentations
    EXPECT_EQ(tree->Children()[0].Value().IndentationSpaces(), 0);
    EXPECT_EQ(tree->Children()[1].Value().IndentationSpaces(),
        header.TokensRange()[0].Length());
    EXPECT_EQ(tree->Children()[2].Value().IndentationSpaces(),
        header.TokensRange()[0].Length());
    EXPECT_EQ(tree->Children()[3].Value().IndentationSpaces(),
        header.TokensRange()[0].Length());
  }
}

TEST_F(ReshapeFittingSubpartitionsFunctionTest,
       CompleteDynamicTest) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  all.SetPartitionPolicy(PartitionPolicyEnum::kApplyOptimalLayout);

  // 'function_fffffffffff('
  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);
  header.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

  // function arguments
  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  arg1.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  arg2.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  arg3.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  arg4.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);
  arg5.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);
  UnwrappedLine arg6(0, arg5.TokensRange().end());
  arg6.SpanUpToToken(all.TokensRange().end());
  arg6.SetPartitionPolicy(PartitionPolicyEnum::kFitOnLineElseExpand);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg6.TokensRange().end());
  args.SetPartitionPolicy(PartitionPolicyEnum::kWrapSubPartitions);

  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{header},
      tree_type{
          args,
          tree_type{arg1},
          tree_type{arg2},
          tree_type{arg3},
          tree_type{arg4},
          tree_type{arg5},
          tree_type{arg6},
      },
  };

  BasicFormatStyle style;
  style.column_limit = 51;
  TreeReshaper::ReshapeTokenPartitionTree(&tree, style);

  UnwrappedLine group_1(0, begin);
  group_1.SpanUpToToken(arg1.TokensRange().end());
  UnwrappedLine group_2(0, arg2.TokensRange().begin());
  group_2.SpanUpToToken(arg3.TokensRange().end());
  UnwrappedLine group_3(0, arg5.TokensRange().begin());
  group_3.SpanUpToToken(arg6.TokensRange().end());

  // function_fffffffffff(type_a_aaaa,
  //                      type_b_bbbbb, type_c_cccccc,
  //                      type_d_dddddddd,
  //                      type_e_eeeeeeee, type_f_ffff);
  const tree_type tree_expected{all,
                                tree_type{
                                    group_1,
                                    //tree_type{header},
                                    //tree_type{arg1},
                                },
                                tree_type{
                                    group_2,
                                    //tree_type{arg2},
                                    //tree_type{arg3},
                                },
                                tree_type{arg4},
                                tree_type{
                                    group_3,
                                    //tree_type{arg5},
                                    //tree_type{arg6},
                                }};

  // FIXME(ldk): Borrowed
  auto token_range_equal = std::function<bool(const UnwrappedLine&,
                                              const UnwrappedLine&)>(
      [](const UnwrappedLine& left, const UnwrappedLine& right) {
        return left.TokensRange() == right.TokensRange();
      });

  const auto diff = DeepEqual(tree, tree_expected, token_range_equal);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:\n" << tree << "\n";

  // FIXME(ldk): Look for a smarter way to test indentations
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
  EXPECT_EQ(tree.Children()[1].Value().IndentationSpaces(),
      header.TokensRange()[0].Length());
  EXPECT_EQ(tree.Children()[2].Value().IndentationSpaces(),
      header.TokensRange()[0].Length());
  EXPECT_EQ(tree.Children()[3].Value().IndentationSpaces(),
      header.TokensRange()[0].Length());
}

}
}  // namespace verible
