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

#include "common/formatting/align.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "common/formatting/format_token.h"
#include "common/formatting/token_partition_tree.h"
#include "common/formatting/unwrapped_line_test_utils.h"
#include "common/text/tree_builder_test_util.h"
#include "common/util/range.h"
#include "common/util/spacer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using ::testing::ElementsAre;

TEST(AlignmentPolicyTest, StringRepresentation) {
  std::ostringstream stream;
  stream << AlignmentPolicy::kAlign;
  EXPECT_EQ(stream.str(), "align");
  EXPECT_EQ(AbslUnparseFlag(AlignmentPolicy::kAlign), "align");
}

TEST(AlignmentPolicyTest, InvalidEnum) {
  AlignmentPolicy policy;
  std::string error;
  EXPECT_FALSE(AbslParseFlag("invalid", &policy, &error));
}

// Helper class that initializes an array of tokens to be partitioned
// into TokenPartitionTree.
class AlignmentTestFixture : public ::testing::Test,
                             public UnwrappedLineMemoryHandler {
 public:
  explicit AlignmentTestFixture(absl::string_view text)
      : sample_(text),
        tokens_(absl::StrSplit(sample_, absl::ByAnyChar(" \n"),
                               absl::SkipEmpty())) {
    for (const auto token : tokens_) {
      ftokens_.emplace_back(TokenInfo{1, token});
    }
    // sample_ is the memory-owning string buffer
    CreateTokenInfosExternalStringBuffer(ftokens_);
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

static const AlignmentColumnProperties FlushLeft(true);
static const AlignmentColumnProperties FlushRight(false);

class TokenColumnizer : public ColumnSchemaScanner {
 public:
  TokenColumnizer() = default;

  void Visit(const SyntaxTreeNode& node) override {
    ColumnSchemaScanner::Visit(node);
  }
  void Visit(const SyntaxTreeLeaf& leaf) override {
    // Let each token occupy its own column.
    ReserveNewColumn(leaf, FlushLeft);
  }
};

class TokenColumnizerRightFlushed : public ColumnSchemaScanner {
 public:
  TokenColumnizerRightFlushed() = default;

  void Visit(const SyntaxTreeNode& node) override {
    ColumnSchemaScanner::Visit(node);
  }
  void Visit(const SyntaxTreeLeaf& leaf) override {
    // Let each token occupy its own column.
    ReserveNewColumn(leaf, FlushRight);
  }
};

class TabularAlignTokenTest : public AlignmentTestFixture {
 public:
  TabularAlignTokenTest()
      : AlignmentTestFixture("one two three four five six") {}
};

static bool IgnoreNone(const TokenPartitionTree&) { return false; }

static std::vector<verible::TaggedTokenPartitionRange>
PartitionBetweenBlankLines(const TokenPartitionRange& range) {
  // Don't care about the subtype tag.
  return GetSubpartitionsBetweenBlankLinesSingleTag(range, 0);
}

static const ExtractAlignmentGroupsFunction kDefaultAlignmentHandler =
    ExtractAlignmentGroupsAdapter(
        &PartitionBetweenBlankLines, &IgnoreNone,
        AlignmentCellScannerGenerator<TokenColumnizer>(),
        AlignmentPolicy::kAlign);

static const ExtractAlignmentGroupsFunction kFlushLeftAlignmentHandler =
    ExtractAlignmentGroupsAdapter(
        &PartitionBetweenBlankLines, &IgnoreNone,
        AlignmentCellScannerGenerator<TokenColumnizer>(),
        AlignmentPolicy::kFlushLeft);

static const ExtractAlignmentGroupsFunction kPreserveAlignmentHandler =
    ExtractAlignmentGroupsAdapter(
        &PartitionBetweenBlankLines, &IgnoreNone,
        AlignmentCellScannerGenerator<TokenColumnizer>(),
        AlignmentPolicy::kPreserve);

static const ExtractAlignmentGroupsFunction kInferAlignmentHandler =
    ExtractAlignmentGroupsAdapter(
        &PartitionBetweenBlankLines, &IgnoreNone,
        AlignmentCellScannerGenerator<TokenColumnizer>(),
        AlignmentPolicy::kInferUserIntent);

TEST_F(TabularAlignTokenTest, EmptyPartitionRange) {
  const auto begin = pre_format_tokens_.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(pre_format_tokens_.end());
  using tree_type = TokenPartitionTree;
  tree_type partition{all};  // no children subpartitions
  TabularAlignTokens(&partition, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);
  // Not crashing is success.
  // Ideally, we would like to verify that partition was *not* modified
  // by making a deep copy and then checking DeepEqual, however,
  // a deep copy of UnwrappedLine does NOT copy the PreFormatTokens,
  // so they end up pointing to the same ranges anyway.
}

class MatrixTreeAlignmentTestFixture : public AlignmentTestFixture {
 public:
  MatrixTreeAlignmentTestFixture(absl::string_view text)
      : AlignmentTestFixture(text),
        syntax_tree_(nullptr),  // for subclasses to initialize
        partition_(/* temporary */ UnwrappedLine()) {}

  std::string Render() const {
    std::ostringstream stream;
    for (const auto& child : partition_.Children()) {
      stream << FormattedExcerpt(child.Value()) << std::endl;
    }
    return stream.str();
  }

 protected:
  // Syntax tree from which token partition originates.
  SymbolPtr syntax_tree_;

  // Format token partitioning (what would be the result of TreeUnwrapper).
  TokenPartitionTree partition_;
};

class Sparse3x3MatrixAlignmentTest : public MatrixTreeAlignmentTestFixture {
 public:
  Sparse3x3MatrixAlignmentTest(
      absl::string_view text = "one two three four five six")
      : MatrixTreeAlignmentTestFixture(text) {
    // From the sample_ text, each pair of tokens will span a subpartition.
    // Construct a 2-level partition that looks like this:
    //
    //   |       | one | two  |
    //   | three |     | four |
    //   | five  | six |      |
    //
    // where blank cells represent positional nullptrs in the syntax tree.
    syntax_tree_ = TNode(1,
                         TNode(2,                    //
                               nullptr,              //
                               Leaf(1, tokens_[0]),  //
                               Leaf(1, tokens_[1])),
                         TNode(2,                    //
                               Leaf(1, tokens_[2]),  //
                               nullptr,              //
                               Leaf(1, tokens_[3])),
                         TNode(2,                    //
                               Leaf(1, tokens_[4]),  //
                               Leaf(1, tokens_[5]),  //
                               nullptr));
    // Establish format token ranges per partition.
    const auto begin = pre_format_tokens_.begin();
    UnwrappedLine all(0, begin);
    all.SpanUpToToken(pre_format_tokens_.end());
    all.SetOrigin(&*syntax_tree_);
    UnwrappedLine child1(0, begin);
    child1.SpanUpToToken(begin + 2);
    child1.SetOrigin(DescendPath(*syntax_tree_, {0}));
    UnwrappedLine child2(0, begin + 2);
    child2.SpanUpToToken(begin + 4);
    child2.SetOrigin(DescendPath(*syntax_tree_, {1}));
    UnwrappedLine child3(0, begin + 4);
    child3.SpanUpToToken(begin + 6);
    child3.SetOrigin(DescendPath(*syntax_tree_, {2}));

    // Construct 2-level token partition.
    using tree_type = TokenPartitionTree;
    partition_ = tree_type{
        all,
        tree_type{child1},
        tree_type{child2},
        tree_type{child3},
    };
  }
};

TEST_F(Sparse3x3MatrixAlignmentTest, ZeroInterTokenPadding) {
  TabularAlignTokens(&partition_, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  // Sanity check: "three" (length 5) is the long-pole of the first column:
  EXPECT_EQ(pre_format_tokens_[0].before.spaces_required, tokens_[2].length());

  // Verify string rendering of result.
  // Here, spaces_required before every token is 0, so expect no padding
  // between columns.
  EXPECT_EQ(Render(),  //
            "     onetwo\n"
            "three   four\n"
            "five six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, AlignmentPolicyFlushLeft) {
  TabularAlignTokens(&partition_, kFlushLeftAlignmentHandler,
                     &pre_format_tokens_, sample_, ByteOffsetSet(), 40);

  EXPECT_EQ(Render(),  // minimum spaces in this example is 0
            "onetwo\n"
            "threefour\n"
            "fivesix\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, AlignmentPolicyPreserve) {
  // Set previous-token string pointers to preserve spaces.
  ConnectPreFormatTokensPreservedSpaceStarts(sample_.data(),
                                             &pre_format_tokens_);

  TabularAlignTokens(&partition_, kPreserveAlignmentHandler,
                     &pre_format_tokens_, sample_, ByteOffsetSet(), 40);

  EXPECT_EQ(Render(),  // original spacing was 1
            "one two\n"
            "three four\n"
            "five six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, OneInterTokenPadding) {
  // Require 1 space between tokens.
  // Will have no effect on the first token in each partition.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }

  TabularAlignTokens(&partition_, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "      one two\n"
            "three     four\n"
            "five  six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, OneInterTokenPaddingExceptFront) {
  // Require 1 space between tokens, except ones at the beginning of partitions.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }
  pre_format_tokens_[0].before.spaces_required = 0;
  pre_format_tokens_[2].before.spaces_required = 0;
  pre_format_tokens_[4].before.spaces_required = 0;

  TabularAlignTokens(&partition_, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "      one two\n"
            "three     four\n"
            "five  six\n");
}

static const ExtractAlignmentGroupsFunction kFlushRightAlignmentHandler =
    ExtractAlignmentGroupsAdapter(
        &PartitionBetweenBlankLines, &IgnoreNone,
        AlignmentCellScannerGenerator<TokenColumnizerRightFlushed>(),
        AlignmentPolicy::kAlign);

TEST_F(Sparse3x3MatrixAlignmentTest, RightFlushed) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }

  TabularAlignTokens(&partition_, kFlushRightAlignmentHandler,
                     &pre_format_tokens_, sample_, ByteOffsetSet(), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "      one  two\n"
            "three     four\n"
            " five six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, OneInterTokenPaddingWithIndent) {
  // Require 1 space between tokens, except ones at the beginning of partitions.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }
  // Indent each partition.
  for (auto& child : partition_.Children()) {
    child.Value().SetIndentationSpaces(4);
  }

  TabularAlignTokens(&partition_, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "          one two\n"
            "    three     four\n"
            "    five  six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, IgnoreCommentLine) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }
  // Leave the 'commented' line indented.
  pre_format_tokens_[2].before.break_decision = SpacingOptions::MustWrap;
  partition_.Children()[1].Value().SetIndentationSpaces(1);

  // Pretend lines that begin with "three" are to be ignored, like comments.
  auto ignore_threes = [](const TokenPartitionTree& partition) {
    return partition.Value().TokensRange().front().Text() == "three";
  };

  const ExtractAlignmentGroupsFunction handler = ExtractAlignmentGroupsAdapter(
      &PartitionBetweenBlankLines, ignore_threes,
      AlignmentCellScannerGenerator<TokenColumnizer>(),
      AlignmentPolicy::kAlign);
  TabularAlignTokens(&partition_, handler, &pre_format_tokens_, sample_,
                     ByteOffsetSet(), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),         //
            "     one two\n"  // is aligned
            " three four\n"   // this line does not participate in alignment
            "five six\n"      // is aligned
  );
}

TEST_F(Sparse3x3MatrixAlignmentTest, CompletelyDisabledNoAlignment) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }

  TabularAlignTokens(
      &partition_, kDefaultAlignmentHandler, &pre_format_tokens_, sample_,
      // Alignment disabled over entire range.
      ByteOffsetSet({{0, static_cast<int>(sample_.length())}}), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "one two\n"
            "three four\n"
            "five six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, CompletelyDisabledNoAlignmentWithIndent) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }
  for (auto& child : partition_.Children()) {
    child.Value().SetIndentationSpaces(3);
  }
  pre_format_tokens_[0].before.break_decision = SpacingOptions::MustWrap;
  pre_format_tokens_[2].before.break_decision = SpacingOptions::MustWrap;
  pre_format_tokens_[4].before.break_decision = SpacingOptions::MustWrap;

  TabularAlignTokens(
      &partition_, kDefaultAlignmentHandler, &pre_format_tokens_, sample_,
      // Alignment disabled over entire range.
      ByteOffsetSet({{0, static_cast<int>(sample_.length())}}), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "   one two\n"
            "   three four\n"
            "   five six\n");
}

class Sparse3x3MatrixAlignmentMoreSpacesTest
    : public Sparse3x3MatrixAlignmentTest {
 public:
  Sparse3x3MatrixAlignmentMoreSpacesTest()
      : Sparse3x3MatrixAlignmentTest("one   two\nthree   four\nfive   six") {
    // This is needed for preservation of original spacing.
    ConnectPreFormatTokensPreservedSpaceStarts(sample_.data(),
                                               &pre_format_tokens_);
  }
};

TEST_F(Sparse3x3MatrixAlignmentMoreSpacesTest,
       PartiallyDisabledIndentButPreserveOtherSpaces) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }
  for (auto& child : partition_.Children()) {
    child.Value().SetIndentationSpaces(1);
  }
  pre_format_tokens_[0].before.break_decision = SpacingOptions::MustWrap;
  pre_format_tokens_[2].before.break_decision = SpacingOptions::MustWrap;
  pre_format_tokens_[4].before.break_decision = SpacingOptions::MustWrap;

  TabularAlignTokens(
      &partition_, kDefaultAlignmentHandler, &pre_format_tokens_, sample_,
      // Alignment disabled over line 2
      ByteOffsetSet({{static_cast<int>(sample_.find_first_of('\n') + 1),
                      static_cast<int>(sample_.find("four") + 4)}}),
      40);

  EXPECT_EQ(pre_format_tokens_[1].before.break_decision,
            SpacingOptions::Preserve);
  EXPECT_EQ(pre_format_tokens_[3].before.break_decision,
            SpacingOptions::Preserve);
  EXPECT_EQ(pre_format_tokens_[5].before.break_decision,
            SpacingOptions::Preserve);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),
            // all lines indented properly but internal spacing preserved
            " one   two\n"
            " three   four\n"
            " five   six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, PartiallyDisabledNoAlignment) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }

  int midpoint = sample_.length() / 2;
  TabularAlignTokens(&partition_, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_,
                     // Alignment disabled over partial range.
                     ByteOffsetSet({{midpoint, midpoint + 1}}), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "one two\n"
            "three four\n"
            "five six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, DisabledByColumnLimit) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }

  TabularAlignTokens(&partition_, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(),
                     // Column limit chosen to be smaller than sum of columns'
                     // widths. 5 (no left padding) +4 +5 = 14, so we choose 13
                     13);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "one two\n"
            "three four\n"
            "five six\n");
}

TEST_F(Sparse3x3MatrixAlignmentTest, DisabledByColumnLimitIndented) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }
  for (auto& child : partition_.Children()) {
    child.Value().SetIndentationSpaces(3);
  }

  TabularAlignTokens(
      &partition_, kDefaultAlignmentHandler, &pre_format_tokens_, sample_,
      ByteOffsetSet(),
      // Column limit chosen to be smaller than sum of columns' widths.
      // 3 (indent) +5 (no left padding) +4 +5 = 17, so we choose 16
      16);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "one two\n"
            "three four\n"
            "five six\n");
}

class MultiAlignmentGroupTest : public AlignmentTestFixture {
 public:
  MultiAlignmentGroupTest()
      : AlignmentTestFixture("one two three four\n\nfive seven six eight"),
        // From the sample_ text, each pair of tokens will span a subpartition.
        // Construct a 2-level partition that looks like this (grouped):
        //
        //   |       | one | two  |
        //   | three |     | four |
        //
        //   | five  | seven |       |
        //   |       | six   | eight |
        //
        // where blank cells represent positional nullptrs in the syntax tree.
        syntax_tree_(TNode(1,
                           TNode(2,                    //
                                 nullptr,              //
                                 Leaf(1, tokens_[0]),  //
                                 Leaf(1, tokens_[1])),
                           TNode(2,                    //
                                 Leaf(1, tokens_[2]),  //
                                 nullptr,              //
                                 Leaf(1, tokens_[3])),
                           TNode(2,                    //
                                 Leaf(1, tokens_[4]),  //
                                 Leaf(1, tokens_[5]),  //
                                 nullptr),
                           TNode(2,                    //
                                 nullptr,              //
                                 Leaf(1, tokens_[6]),  //
                                 Leaf(1, tokens_[7])))),
        partition_(/* temporary */ UnwrappedLine()) {
    // Establish format token ranges per partition.
    const auto begin = pre_format_tokens_.begin();
    UnwrappedLine all(0, begin);
    all.SpanUpToToken(pre_format_tokens_.end());
    all.SetOrigin(&*syntax_tree_);
    UnwrappedLine child1(0, begin);
    child1.SpanUpToToken(begin + 2);
    child1.SetOrigin(DescendPath(*syntax_tree_, {0}));
    UnwrappedLine child2(0, begin + 2);
    child2.SpanUpToToken(begin + 4);
    child2.SetOrigin(DescendPath(*syntax_tree_, {1}));
    UnwrappedLine child3(0, begin + 4);
    child3.SpanUpToToken(begin + 6);
    child3.SetOrigin(DescendPath(*syntax_tree_, {2}));
    UnwrappedLine child4(0, begin + 6);
    child4.SpanUpToToken(begin + 8);
    child4.SetOrigin(DescendPath(*syntax_tree_, {3}));

    // Construct 2-level token partition.
    using tree_type = TokenPartitionTree;
    partition_ = tree_type{
        all,
        tree_type{child1},
        tree_type{child2},
        tree_type{child3},
        tree_type{child4},
    };
  }

  std::string Render() const {
    std::ostringstream stream;
    int position = 0;
    const absl::string_view text(sample_);
    for (const auto& child : partition_.Children()) {
      // emulate preserving vertical spacing
      const auto tokens_range = child.Value().TokensRange();
      const auto front_offset = tokens_range.front().token->left(text);
      const absl::string_view spaces =
          text.substr(position, front_offset - position);
      const auto newlines =
          std::max<int>(std::count(spaces.begin(), spaces.end(), '\n') - 1, 0);
      stream << Spacer(newlines, '\n');
      stream << FormattedExcerpt(child.Value()) << std::endl;
      position = tokens_range.back().token->right(text);
    }
    return stream.str();
  }

 protected:
  // Syntax tree from which token partition originates.
  SymbolPtr syntax_tree_;

  // Format token partitioning (what would be the result of TreeUnwrapper).
  TokenPartitionTree partition_;
};

TEST_F(MultiAlignmentGroupTest, BlankLineSeparatedGroups) {
  // Require 1 space between tokens.
  for (auto& ftoken : pre_format_tokens_) {
    ftoken.before.spaces_required = 1;
  }

  TabularAlignTokens(&partition_, kDefaultAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  // Verify string rendering of result.
  EXPECT_EQ(Render(),  //
            "      one two\n"
            "three     four\n"
            "\n"  // preserve blank line
            "five seven\n"
            "     six   eight\n");
}

// TODO(fangism): test case that demonstrates repeated constructs in a deeper
// syntax tree.

// TODO(fangism): test case for demonstrating flush-right

class GetPartitionAlignmentSubrangesTestFixture : public AlignmentTestFixture {
 public:
  GetPartitionAlignmentSubrangesTestFixture()
      : AlignmentTestFixture(
            "ignore match nomatch match match match nomatch nomatch match "
            "ignore match"),
        syntax_tree_(TNode(
            1,  // one token per partition for simplicity
            TNode(2, Leaf(1, tokens_[0])),     //
            TNode(2, Leaf(1, tokens_[1])),     // singleton range too short here
            TNode(2, Leaf(1, tokens_[2])),     //
            TNode(2, Leaf(1, tokens_[3])),     // expect match from here
            TNode(2, Leaf(1, tokens_[4])),     // ...
            TNode(2, Leaf(1, tokens_[5])),     // ... to here (inclusive)
            TNode(2, Leaf(1, tokens_[6])),     //
            TNode(2, Leaf(1, tokens_[7])),     //
            TNode(2, Leaf(1, tokens_[8])),     // and from here to the end().
            TNode(2, Leaf(1, tokens_[9])),     // ...
            TNode(2, Leaf(1, tokens_[10])))),  // ...
        partition_(/* temporary */ UnwrappedLine()) {
    // Establish format token ranges per partition.
    const auto begin = pre_format_tokens_.begin();
    UnwrappedLine all(0, begin);
    all.SpanUpToToken(pre_format_tokens_.end());
    all.SetOrigin(&*syntax_tree_);

    std::vector<UnwrappedLine> uwlines;
    for (size_t i = 0; i < pre_format_tokens_.size(); ++i) {
      uwlines.emplace_back(0, begin + i);
      uwlines.back().SpanUpToToken(begin + i + 1);
      uwlines.back().SetOrigin(DescendPath(*syntax_tree_, {i}));
    }

    // Construct 2-level token partition.
    using tree_type = TokenPartitionTree;
    partition_ = tree_type{
        all,
        tree_type{uwlines[0]},
        tree_type{uwlines[1]},  // one match not enough
        tree_type{uwlines[2]},
        tree_type{uwlines[3]},  // start of match
        tree_type{uwlines[4]},  // ...
        tree_type{uwlines[5]},  // ...
        tree_type{uwlines[6]},  // end of match
        tree_type{uwlines[7]},
        tree_type{uwlines[8]},   // start of match
        tree_type{uwlines[9]},   // ...
        tree_type{uwlines[10]},  // ...
    };
  }

 protected:
  static AlignmentGroupAction PartitionSelector(
      const TokenPartitionTree& partition) {
    const absl::string_view text =
        partition.Value().TokensRange().front().Text();
    if (text == "match") {
      return AlignmentGroupAction::kMatch;
    } else if (text == "nomatch") {
      return AlignmentGroupAction::kNoMatch;
    } else {
      return AlignmentGroupAction::kIgnore;
    }
  }

 protected:
  // Syntax tree from which token partition originates.
  SymbolPtr syntax_tree_;

  // Format token partitioning (what would be the result of TreeUnwrapper).
  TokenPartitionTree partition_;
};

TEST_F(GetPartitionAlignmentSubrangesTestFixture, VariousRanges) {
  const TokenPartitionRange children(partition_.Children().begin(),
                                     partition_.Children().end());

  const std::vector<TaggedTokenPartitionRange> ranges(
      GetPartitionAlignmentSubranges(children, [](const TokenPartitionTree&
                                                      partition) {
        // Don't care about the subtype tag.
        return AlignedPartitionClassification{PartitionSelector(partition), 0};
      }));

  using P = std::pair<int, int>;
  std::vector<P> range_indices;
  for (const auto& range : ranges) {
    range_indices.push_back(SubRangeIndices(range.range, children));
  }
  EXPECT_THAT(range_indices, ElementsAre(P(3, 6), P(8, 11)));
}

class GetPartitionAlignmentSubrangesSubtypedTestFixture
    : public AlignmentTestFixture {
 public:
  GetPartitionAlignmentSubrangesSubtypedTestFixture()
      : AlignmentTestFixture(
            "match:X match:X match:X match:Y match:Y match:Y nomatch match:Z "
            "match:X match:Z match:Z ignore match:Y match:Y"),
        syntax_tree_(TNode(
            1,  // one token per partition for simplicity
            TNode(2, Leaf(1, tokens_[0])),     // match:X start
            TNode(2, Leaf(1, tokens_[1])),     // ...
            TNode(2, Leaf(1, tokens_[2])),     // match:X end
            TNode(2, Leaf(1, tokens_[3])),     // match:Y start
            TNode(2, Leaf(1, tokens_[4])),     // ...
            TNode(2, Leaf(1, tokens_[5])),     // match:Y end
            TNode(2, Leaf(1, tokens_[6])),     //
            TNode(2, Leaf(1, tokens_[7])),     // match:Z start/end (too small)
            TNode(2, Leaf(1, tokens_[8])),     // match:X start/end (too small)
            TNode(2, Leaf(1, tokens_[9])),     // match:Z start
            TNode(2, Leaf(1, tokens_[10])),    // match:Z end
            TNode(2, Leaf(1, tokens_[11])),    //
            TNode(2, Leaf(1, tokens_[12])),    // match:Y start
            TNode(2, Leaf(1, tokens_[13])))),  // match:Y end
        partition_(/* temporary */ UnwrappedLine()) {
    // Establish format token ranges per partition.
    const auto begin = pre_format_tokens_.begin();
    UnwrappedLine all(0, begin);
    all.SpanUpToToken(pre_format_tokens_.end());
    all.SetOrigin(&*syntax_tree_);

    std::vector<UnwrappedLine> uwlines;
    for (size_t i = 0; i < pre_format_tokens_.size(); ++i) {
      uwlines.emplace_back(0, begin + i);
      uwlines.back().SpanUpToToken(begin + i + 1);
      uwlines.back().SetOrigin(DescendPath(*syntax_tree_, {i}));
    }

    // Construct 2-level token partition.
    using tree_type = TokenPartitionTree;
    partition_ = tree_type{
        all,
        tree_type{uwlines[0]},  // match:X start
        tree_type{uwlines[1]},  // ...
        tree_type{uwlines[2]},  // match:X end
        tree_type{uwlines[3]},  // match:Y start
        tree_type{uwlines[4]},  // ...
        tree_type{uwlines[5]},  // match:Y end
        tree_type{uwlines[6]},
        tree_type{uwlines[7]},   // match:Z start/end (no group)
        tree_type{uwlines[8]},   // match:X start/end (no group)
        tree_type{uwlines[9]},   // match:Z start
        tree_type{uwlines[10]},  // match:Z end
        tree_type{uwlines[11]},
        tree_type{uwlines[12]},  // match:Y start
        tree_type{uwlines[13]},  // match:Y end
    };
  }

 protected:
  static AlignedPartitionClassification PartitionSelector(
      const TokenPartitionTree& partition) {
    const absl::string_view text =
        partition.Value().TokensRange().front().Text();
    if (absl::StartsWith(text, "match")) {
      const auto toks = absl::StrSplit(text, absl::ByChar(':'));
      CHECK(toks.begin() != toks.end());
      absl::string_view last = *std::next(toks.begin());
      // Use the first character after the : as the subtype, so 'X', 'Y', 'Z'.
      return {AlignmentGroupAction::kMatch, static_cast<int>(last.front())};
    } else if (text == "nomatch") {
      return {AlignmentGroupAction::kNoMatch};
    } else {
      return {AlignmentGroupAction::kIgnore};
    }
  }

 protected:
  // Syntax tree from which token partition originates.
  SymbolPtr syntax_tree_;

  // Format token partitioning (what would be the result of TreeUnwrapper).
  TokenPartitionTree partition_;
};

TEST_F(GetPartitionAlignmentSubrangesSubtypedTestFixture, VariousRanges) {
  const TokenPartitionRange children(partition_.Children().begin(),
                                     partition_.Children().end());

  const std::vector<TaggedTokenPartitionRange> ranges(
      GetPartitionAlignmentSubranges(children, &PartitionSelector));

  using P = std::pair<int, int>;
  std::vector<P> range_indices;
  for (const auto& range : ranges) {
    range_indices.push_back(SubRangeIndices(range.range, children));
  }
  EXPECT_THAT(range_indices,
              ElementsAre(P(0, 3), P(3, 6), P(9, 12), P(12, 14)));
  // Check subtype tags.
  ASSERT_EQ(ranges.size(), 4);
  EXPECT_EQ(ranges[0].match_subtype, 'X');  // [0,3)
  EXPECT_EQ(ranges[1].match_subtype, 'Y');  // [3,6)
  EXPECT_EQ(ranges[2].match_subtype, 'Z');  // [9,12)
  EXPECT_EQ(ranges[3].match_subtype, 'Y');  // [12,14)
}

class Dense2x2MatrixAlignmentTest : public MatrixTreeAlignmentTestFixture {
 public:
  Dense2x2MatrixAlignmentTest(absl::string_view text = "one two three four")
      : MatrixTreeAlignmentTestFixture(text) {
    CHECK_EQ(tokens_.size(), 4);

    // Need to know original spacing to be able to infer user-intent.
    ConnectPreFormatTokensPreservedSpaceStarts(sample_.data(),
                                               &pre_format_tokens_);

    // Require 1 space between tokens.
    for (auto& ftoken : pre_format_tokens_) {
      ftoken.before.spaces_required = 1;
      // Default to append, so we can see the effect of falling-back to
      // preserve-spacing behavior.
      ftoken.before.break_decision = SpacingOptions::MustAppend;
    }

    // From the sample_ text, each pair of tokens will span a subpartition.
    // Construct a 2-level partition that looks like this:
    //
    //   | one   | two  |
    //   | three | four |
    //
    syntax_tree_ = TNode(1,
                         TNode(2,                    //
                               Leaf(1, tokens_[0]),  //
                               Leaf(1, tokens_[1])),
                         TNode(2,                    //
                               Leaf(1, tokens_[2]),  //
                               Leaf(1, tokens_[3])));

    // Establish format token ranges per partition.
    const auto begin = pre_format_tokens_.begin();
    UnwrappedLine all(0, begin);
    all.SpanUpToToken(pre_format_tokens_.end());
    all.SetOrigin(&*syntax_tree_);
    UnwrappedLine child1(0, begin);
    child1.SpanUpToToken(begin + 2);
    child1.SetOrigin(DescendPath(*syntax_tree_, {0}));
    UnwrappedLine child2(0, begin + 2);
    child2.SpanUpToToken(begin + 4);
    child2.SetOrigin(DescendPath(*syntax_tree_, {1}));

    // Construct 2-level token partition.
    using tree_type = TokenPartitionTree;
    partition_ = tree_type{
        all,
        tree_type{child1},
        tree_type{child2},
    };
  }
};

class InferSmallAlignDifferenceTest : public Dense2x2MatrixAlignmentTest {
 public:
  InferSmallAlignDifferenceTest()
      : Dense2x2MatrixAlignmentTest("one two three four") {}
};

TEST_F(InferSmallAlignDifferenceTest, DifferenceSufficientlySmall) {
  // aligned matrix:
  //   | one   | two  |
  //   | three | four |
  //
  // flushed-left looks only different by a maximum of 2 spaces ("one" vs.
  // "three", so just align it.

  TabularAlignTokens(&partition_, kInferAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  EXPECT_EQ(Render(),      //
            "one   two\n"  //
            "three four\n");
}

class InferFlushLeftTest : public Dense2x2MatrixAlignmentTest {
 public:
  InferFlushLeftTest()
      : Dense2x2MatrixAlignmentTest("one  two threeeee  four") {}
};

TEST_F(InferFlushLeftTest, DifferenceSufficientlySmall) {
  // aligned matrix:
  //   | one      | two  |
  //   | threeeee | four |
  //
  // Aligned vs. flushed contains a spacing difference of 4,
  // So this avoids triggering alignment due to small differences.
  // The original text contains no error greater than 2 spaces relative to
  // flush-left, therefore flush-left.

  TabularAlignTokens(&partition_, kInferAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  EXPECT_EQ(Render(),    //
            "one two\n"  //
            "threeeee four\n");
}

class InferForceAlignTest : public Dense2x2MatrixAlignmentTest {
 public:
  InferForceAlignTest()
      : Dense2x2MatrixAlignmentTest("one two threeeee     four") {}
};

TEST_F(InferForceAlignTest, DifferenceSufficientlySmall) {
  // aligned matrix:
  //   | one      | two  |
  //   | threeeee | four |
  //
  // The original text contains 4 excess spaces before "four" which should
  // trigger alignment.

  TabularAlignTokens(&partition_, kInferAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  EXPECT_EQ(Render(),         //
            "one      two\n"  //
            "threeeee four\n");
}

class InferAmbiguousAlignIntentTest : public Dense2x2MatrixAlignmentTest {
 public:
  InferAmbiguousAlignIntentTest()
      : Dense2x2MatrixAlignmentTest("one two threeeee    four") {}
};

TEST_F(InferAmbiguousAlignIntentTest, DifferenceSufficientlySmall) {
  // aligned matrix:
  //   | one      | two  |
  //   | threeeee | four |
  //
  // The original text contains only 3 excess spaces before "four" which should
  // not trigger alignment, but fall back to preserving original spacing.

  TabularAlignTokens(&partition_, kInferAlignmentHandler, &pre_format_tokens_,
                     sample_, ByteOffsetSet(), 40);

  EXPECT_EQ(Render(),    //
            "one two\n"  //
            "threeeee    four\n");
}

}  // namespace
}  // namespace verible
