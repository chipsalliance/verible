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

#include "common/formatting/token_partition_tree.h"

#include <iosfwd>
#include <iterator>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/formatting/format_token.h"
#include "common/formatting/unwrapped_line.h"
#include "common/formatting/unwrapped_line_test_utils.h"
#include "common/util/container_iterator_range.h"
#include "common/util/vector_tree.h"

namespace verible {

// Helper class that initializes an array of tokens to be partitioned
// into TokenPartitionTree.
class TokenPartitionTreeTestFixture : public ::testing::Test,
                                      public UnwrappedLineMemoryHandler {
 public:
  TokenPartitionTreeTestFixture()
      : sample_("one two three four five six"),
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

class VerifyFullTreeFormatTokenRangesTest
    : public TokenPartitionTreeTestFixture {};

TEST_F(VerifyFullTreeFormatTokenRangesTest, ParentChildRangeBeginMismatch) {
  // Construct three partitions, sizes: 1, 3, 2
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  // one short of partition0's begin, and will fail invariant.
  UnwrappedLine all(0, begin + 1);
  all.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine partition0(0, begin);
  partition0.SpanUpToToken(begin + 1);
  UnwrappedLine partition1(0, partition0.TokensRange().end());
  partition1.SpanUpToToken(begin + 4);
  UnwrappedLine partition2(0, partition1.TokensRange().end());
  partition2.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{partition0},
      tree_type{partition1},
      tree_type{partition2},
  };

  EXPECT_DEATH(VerifyFullTreeFormatTokenRanges(tree, begin),
               "Check failed: parent_begin == children_begin");
}

TEST_F(VerifyFullTreeFormatTokenRangesTest, ParentChildRangeEndMismatch) {
  // Construct three partitions, sizes: 1, 3, 2
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  // one short of partition2's end, and will fail invariant.
  all.SpanUpToToken(preformat_tokens.end() - 1);
  UnwrappedLine partition0(0, begin);
  partition0.SpanUpToToken(begin + 1);
  UnwrappedLine partition1(0, partition0.TokensRange().end());
  partition1.SpanUpToToken(begin + 4);
  UnwrappedLine partition2(0, partition1.TokensRange().end());
  partition2.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{partition0},
      tree_type{partition1},
      tree_type{partition2},
  };

  EXPECT_DEATH(VerifyFullTreeFormatTokenRanges(tree, begin),
               "Check failed: parent_end == children_end");
}

TEST_F(VerifyFullTreeFormatTokenRangesTest, ChildrenRangeDiscontinuity) {
  // Construct three partitions, sizes: 1, 3, 2
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine partition0(0, begin);
  partition0.SpanUpToToken(begin + 1);
  UnwrappedLine partition1(0, partition0.TokensRange().end());
  partition1.SpanUpToToken(begin + 3);
  // There is a gap between partition1 and partition2 that fails an invariant.
  UnwrappedLine partition2(0, partition1.TokensRange().end() + 1);
  partition2.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{partition0},
      tree_type{partition1},
      tree_type{partition2},
  };

  EXPECT_DEATH(VerifyFullTreeFormatTokenRanges(tree, begin),
               "Check failed: current_begin == previous_end");
}

class FindLargestPartitionsTest : public TokenPartitionTreeTestFixture {};

// Verify that this finds the K largest leaf partitions.
TEST_F(FindLargestPartitionsTest, VectorTree) {
  // Construct three partitions, sizes: 1, 3, 2
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine partition0(0, begin);
  partition0.SpanUpToToken(begin + 1);
  UnwrappedLine partition1(0, partition0.TokensRange().end());
  partition1.SpanUpToToken(begin + 4);
  UnwrappedLine partition2(0, partition1.TokensRange().end());
  partition2.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{partition0},
      tree_type{partition1},
      tree_type{partition2},
  };

  // Verify sizes of two largest leaf partitions.
  const auto top_two = FindLargestPartitions(tree, 2);
  EXPECT_EQ(top_two[0]->Size(), 3);
  EXPECT_EQ(top_two[0]->TokensRange().front().Text(), "two");
  EXPECT_EQ(top_two[1]->Size(), 2);
  EXPECT_EQ(top_two[1]->TokensRange().front().Text(), "five");
}

class TokenPartitionTreePrinterTest : public TokenPartitionTreeTestFixture {};

// Verify specialized tree printing of UnwrappedLines.
TEST_F(TokenPartitionTreePrinterTest, VectorTree) {
  // Construct three partitions, sizes: 1, 3, 2
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine partition0(2, begin);
  partition0.SpanUpToToken(begin + 1);
  UnwrappedLine partition1(2, partition0.TokensRange().end());
  partition1.SpanUpToToken(begin + 4);
  UnwrappedLine partition2(2, partition1.TokensRange().end());
  partition2.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{partition0},
      tree_type{partition1},
      tree_type{partition2},
  };

  std::ostringstream stream;
  stream << TokenPartitionTreePrinter(tree);
  EXPECT_EQ(stream.str(), R"({ ([<auto>])
  { (  [one]) }
  { (  [two three four]) }
  { (  [five six]) }
})");
}

class MoveLastLeafIntoPreviousSiblingTest
    : public TokenPartitionTreeTestFixture {};

static bool TokenRangeEqual(const UnwrappedLine& left,
                            const UnwrappedLine& right) {
  return left.TokensRange() == right.TokensRange();
}

TEST_F(MoveLastLeafIntoPreviousSiblingTest, RootOnly) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
  };

  const auto saved_tree(tree);  // deep copy
  MoveLastLeafIntoPreviousSibling(&tree);

  // Expect no change.
  const auto diff = DeepEqual(tree, saved_tree, TokenRangeEqual);
  EXPECT_TRUE(diff.left == nullptr) << "First differing node at:\n"
                                    << *diff.left << "\nand:\n"
                                    << *diff.right << '\n';
}

TEST_F(MoveLastLeafIntoPreviousSiblingTest, OneChild) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all, tree_type{all},  // subtree spans same range
  };

  const auto saved_tree(tree);  // deep copy
  MoveLastLeafIntoPreviousSibling(&tree);

  // Expect no change.
  const auto diff = DeepEqual(tree, saved_tree, TokenRangeEqual);
  EXPECT_TRUE(diff.left == nullptr) << "First differing node at:\n"
                                    << *diff.left << "\nand:\n"
                                    << *diff.right << '\n';
}

TEST_F(MoveLastLeafIntoPreviousSiblingTest, TwoChild) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  // Construct an artificial tree using the following partitions.
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine part1(0, begin);
  part1.SpanUpToToken(begin + 3);
  UnwrappedLine part2(0, part1.TokensRange().end());
  part2.SpanUpToToken(all.TokensRange().end());

  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{part1},
      tree_type{part2},
  };

  const tree_type expected_tree{
      all,
      tree_type{all},
  };

  MoveLastLeafIntoPreviousSibling(&tree);
  const auto diff = DeepEqual(tree, expected_tree, TokenRangeEqual);
  EXPECT_TRUE(diff.left == nullptr) << "First differing node at:\n"
                                    << *diff.left << "\nand:\n"
                                    << *diff.right << '\n';
}

TEST_F(MoveLastLeafIntoPreviousSiblingTest, TwoGenerations) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  // Construct an artificial tree using the following partitions.
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine part1(0, begin);
  part1.SpanUpToToken(begin + 3);
  UnwrappedLine part1a(0, begin);
  part1a.SpanUpToToken(begin + 2);
  UnwrappedLine part1b(0, part1a.TokensRange().end());
  part1b.SpanUpToToken(part1.TokensRange().end());
  UnwrappedLine part2(0, part1.TokensRange().end());
  part2.SpanUpToToken(all.TokensRange().end());

  // New expected partition should be part1b + part2
  UnwrappedLine fused_part(0, part1b.TokensRange().begin());
  fused_part.SpanUpToToken(all.TokensRange().end());

  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{
          part1, tree_type{part1a},  // unchanged
          tree_type{part1b},         // target partition
      },
      tree_type{part2},  // rightmost_leaf
  };

  const tree_type expected_tree{
      all,
      tree_type{
          all, tree_type{part1a},
          tree_type{fused_part},  // fused target partition
      },
  };

  MoveLastLeafIntoPreviousSibling(&tree);
  const auto diff = DeepEqual(tree, expected_tree, TokenRangeEqual);
  EXPECT_TRUE(diff.left == nullptr) << "First differing node at:\n"
                                    << *diff.left << "\nand:\n"
                                    << *diff.right << '\n';
}

}  // namespace verible
