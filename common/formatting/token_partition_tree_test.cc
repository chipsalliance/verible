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
TEST_F(TokenPartitionTreePrinterTest, VectorTreeShallow) {
  // Construct three partitions, sizes: 1, 3, 2
  // Change some attributes that will appear in the verbose rendition.
  pre_format_tokens_[2].before.spaces_required = 1;
  pre_format_tokens_[3].before.break_decision = SpacingOptions::MustAppend;
  pre_format_tokens_[4].before.break_penalty = 22;
  pre_format_tokens_[5].before.break_decision = SpacingOptions::MustWrap;
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

  {
    std::ostringstream stream;
    stream << TokenPartitionTreePrinter(tree);
    EXPECT_EQ(stream.str(), R"({ ([<auto>], policy: always-expand) @{}
  { (>>[one], policy: always-expand) }
  { (>>[two three four], policy: always-expand) }
  { (>>[five six], policy: always-expand) }
})");
  }
  {
    std::ostringstream stream;
    stream << TokenPartitionTreePrinter(tree, true);  // verbose
    EXPECT_EQ(stream.str(), R"({ ([<auto>], policy: always-expand) @{}
  { (>>[<_0,0>one], policy: always-expand) }
  { (>>[<_0,0>two <_1,0>three <+_0>four], policy: always-expand) }
  { (>>[<_0,22>five <\n>six], policy: always-expand) }
})");
  }
}

TEST_F(TokenPartitionTreePrinterTest, VectorTreeDeep) {
  // Construct partitions, sizes: 1, (2, 1), (1, 1)
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine partition0(2, begin);
  partition0.SpanUpToToken(begin + 1);
  UnwrappedLine partition1(2, partition0.TokensRange().end());
  partition1.SpanUpToToken(begin + 4);
  UnwrappedLine partition1a(4, partition1.TokensRange().begin());
  partition1a.SpanUpToToken(begin + 3);
  UnwrappedLine partition1b(4, partition1a.TokensRange().end());
  partition1b.SpanUpToToken(begin + 4);
  UnwrappedLine partition2(2, partition1.TokensRange().end());
  partition2.SpanUpToToken(preformat_tokens.end());
  UnwrappedLine partition2a(4, partition2.TokensRange().begin());
  partition2a.SpanUpToToken(begin + 5);
  UnwrappedLine partition2b(4, partition2a.TokensRange().end());
  partition2b.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{partition0},
      tree_type{
          partition1,
          tree_type{partition1a},
          tree_type{partition1b},
      },
      tree_type{
          partition2,
          tree_type{partition2a},
          tree_type{partition2b},
      },
  };

  std::ostringstream stream;
  stream << TokenPartitionTreePrinter(tree);
  EXPECT_EQ(stream.str(), R"({ ([<auto>], policy: always-expand) @{}
  { (>>[one], policy: always-expand) }
  { (>>[<auto>], policy: always-expand) @{1}
    { (>>>>[two three], policy: always-expand) }
    { (>>>>[four], policy: always-expand) }
  }
  { (>>[<auto>], policy: always-expand) @{2}
    { (>>>>[five], policy: always-expand) }
    { (>>>>[six], policy: always-expand) }
  }
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

class MergeConsecutiveSiblingsTest : public TokenPartitionTreeTestFixture {};

TEST_F(MergeConsecutiveSiblingsTest, OneChild) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();
  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());

  // Construct an artificial tree using the above partitions.
  using tree_type = TokenPartitionTree;
  tree_type tree{
      all, tree_type{all},  // subtree spans same range
  };

  // Need at least 2 sliblings to join.
  EXPECT_DEATH(MergeConsecutiveSiblings(&tree, 0), "");
}

TEST_F(MergeConsecutiveSiblingsTest, TwoChild) {
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
      all, tree_type{all},  // concatenated unwrapped line ranges
  };

  MergeConsecutiveSiblings(&tree, 0);
  const auto diff = DeepEqual(tree, expected_tree, TokenRangeEqual);
  EXPECT_TRUE(diff.left == nullptr) << "First differing node at:\n"
                                    << *diff.left << "\nand:\n"
                                    << *diff.right << '\n';
}

TEST_F(MergeConsecutiveSiblingsTest, TwoGenerations) {
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
  UnwrappedLine part2a(0, begin);
  part2a.SpanUpToToken(begin + 1);
  UnwrappedLine part2b(0, part1a.TokensRange().end());
  part2b.SpanUpToToken(part2.TokensRange().end());

  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{
          part1,
          tree_type{part1a},
          tree_type{part1b},
      },
      tree_type{
          part2,
          tree_type{part2a},
          tree_type{part2b},
      },
  };

  const tree_type expected_tree{
      all,
      tree_type{
          all,  // part1 + part2
          // children of part1 and part2 are concatenated
          tree_type{part1a},
          tree_type{part1b},
          tree_type{part2a},
          tree_type{part2b},
      },
  };

  MergeConsecutiveSiblings(&tree, 0);
  const auto diff = DeepEqual(tree, expected_tree, TokenRangeEqual);
  EXPECT_TRUE(diff.left == nullptr) << "First differing node at:\n"
                                    << *diff.left << "\nand:\n"
                                    << *diff.right << '\n';
}

class ReshapeFittingSubpartitionsTest : public TokenPartitionTreeTestFixture {};

TEST_F(ReshapeFittingSubpartitionsTest, NoArguments) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  using tree_type = TokenPartitionTree;
  tree_type tree{
      header,
      tree_type{header},
  };

  const tree_type tree_expected{
      header,
      tree_type{header},
  };

  verible::BasicFormatStyle style;  // default
  ReshapeFittingSubpartitions(&tree, style);

  // Check tree structure
  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentation
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
}

TEST_F(ReshapeFittingSubpartitionsTest, OneArgument) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);
  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);

  UnwrappedLine all(0, begin);
  all.SpanUpToToken(arg1.TokensRange().end());

  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{header},
      tree_type{
          arg1,
          tree_type{arg1},
      },
  };

  const tree_type tree_expected{
      all,
      tree_type{
          all,
          tree_type{header},
          tree_type{arg1},
      },
  };

  verible::BasicFormatStyle style;  // default
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentation
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
}

TEST_F(ReshapeFittingSubpartitionsTest, TwoArguments) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg2.TokensRange().end());

  UnwrappedLine all(0, header.TokensRange().begin());
  all.SpanUpToToken(arg2.TokensRange().end());

  using tree_type = TokenPartitionTree;
  tree_type tree{
      all,
      tree_type{header},
      tree_type{
          args,
          tree_type{arg1},
          tree_type{arg2},
      },
  };

  const tree_type tree_expected{
      all,
      tree_type{
          all,
          tree_type{header},
          tree_type{arg1},
          tree_type{arg2},
      },
  };

  verible::BasicFormatStyle style;  // default
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentation
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
}

// All fits in one partition
TEST_F(ReshapeFittingSubpartitionsTest, OnePartition) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg5.TokensRange().end());

  UnwrappedLine all(0, header.TokensRange().begin());
  all.SpanUpToToken(arg5.TokensRange().end());

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
      },
  };

  const tree_type tree_expected{
      all,
      tree_type{
          all,
          tree_type{header},
          tree_type{arg1},
          tree_type{arg2},
          tree_type{arg3},
          tree_type{arg4},
          tree_type{arg5},
      },
  };

  verible::BasicFormatStyle style;  // default
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentation
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
}

// Wrap into two partitions
TEST_F(ReshapeFittingSubpartitionsTest, TwoPartitions) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg5.TokensRange().end());

  UnwrappedLine all(0, header.TokensRange().begin());
  all.SpanUpToToken(arg5.TokensRange().end());

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
      },
  };

  UnwrappedLine group1(0, header.TokensRange().begin());
  group1.SpanUpToToken(arg2.TokensRange().end());
  UnwrappedLine group2(0, arg3.TokensRange().begin());
  group2.SpanUpToToken(arg5.TokensRange().end());

  const tree_type tree_expected{
      all,
      tree_type{
          group1,
          tree_type{header},
          tree_type{arg1},
          tree_type{arg2},
      },
      tree_type{
          group2,
          tree_type{arg3},
          tree_type{arg4},
          tree_type{arg5},
      },
  };

  verible::BasicFormatStyle style;  // default
  style.column_limit = 14;
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentation
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
  EXPECT_EQ(
      tree.Children()[1].Value().IndentationSpaces(),
      header.TokensRange()[0].Length());  // indenation equal first token length
}

// None fits
TEST_F(ReshapeFittingSubpartitionsTest, NoneOneFits) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg5.TokensRange().end());

  UnwrappedLine all(0, header.TokensRange().begin());
  all.SpanUpToToken(arg5.TokensRange().end());

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
      },
  };

  const tree_type tree_expected{
      all,
      tree_type{header, tree_type{header}},
      tree_type{arg1, tree_type{arg1}},
      tree_type{arg2, tree_type{arg2}},
      tree_type{arg3, tree_type{arg3}},
      tree_type{arg4, tree_type{arg4}},
      tree_type{arg5, tree_type{arg5}},
  };

  verible::BasicFormatStyle style;  // default
  style.column_limit = 2;
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
  EXPECT_EQ(tree.Children()[1].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[2].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[3].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[4].Value().IndentationSpaces(), style.wrap_spaces);
}

// All fits in one partition
TEST_F(ReshapeFittingSubpartitionsTest, IndentationOnePartition) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg5.TokensRange().end());

  UnwrappedLine all(0, header.TokensRange().begin());
  all.SpanUpToToken(arg5.TokensRange().end());

  verible::BasicFormatStyle style;  // default

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
      },
  };

  tree.Children()[1].ApplyPreOrder([&style](TokenPartitionTree& node) {
    auto& uwline = node.Value();
    uwline.SetIndentationSpaces(3 + style.indentation_spaces);
  });

  tree.Children()[0].Value().SetIndentationSpaces(3);
  tree.Value().SetIndentationSpaces(3);

  const tree_type tree_expected{
      all,
      tree_type{
          all,
          tree_type{header},
          tree_type{arg1},
          tree_type{arg2},
          tree_type{arg3},
          tree_type{arg4},
          tree_type{arg5},
      },
  };

  auto saved_tree(tree);
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // keep orig indentation
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(),
            saved_tree.Value().IndentationSpaces());
}

// Wrap into two partitions
TEST_F(ReshapeFittingSubpartitionsTest, IndentationTwoPartitions) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg5.TokensRange().end());

  UnwrappedLine all(0, header.TokensRange().begin());
  all.SpanUpToToken(arg5.TokensRange().end());

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
      },
  };

  verible::BasicFormatStyle style;  // default
  style.column_limit = 14 + 7;

  tree.Children()[1].ApplyPreOrder([&style](TokenPartitionTree& node) {
    auto& uwline = node.Value();
    uwline.SetIndentationSpaces(7 + style.indentation_spaces);
  });

  tree.Children()[0].Value().SetIndentationSpaces(7);
  tree.Value().SetIndentationSpaces(7);

  UnwrappedLine group1(0, header.TokensRange().begin());
  group1.SpanUpToToken(arg2.TokensRange().end());
  UnwrappedLine group2(0, arg3.TokensRange().begin());
  group2.SpanUpToToken(arg5.TokensRange().end());

  const tree_type tree_expected{
      all,
      tree_type{
          group1,
          tree_type{header},
          tree_type{arg1},
          tree_type{arg2},
      },
      tree_type{
          group2,
          tree_type{arg3},
          tree_type{arg4},
          tree_type{arg5},
      },
  };

  auto saved_tree(tree);
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check group nodes indentation
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 7);
  EXPECT_EQ(tree.Children()[1].Value().IndentationSpaces(),
            header.TokensRange()[0].Length() + 7);  // appended

  EXPECT_EQ(saved_tree.Children()[0].Value().IndentationSpaces(),
            tree.Children()[0].Children()[0].Value().IndentationSpaces());
  EXPECT_EQ(saved_tree.Children()[1].Children()[0].Value().IndentationSpaces(),
            tree.Children()[0].Children()[1].Value().IndentationSpaces());
  EXPECT_EQ(saved_tree.Children()[1].Children()[1].Value().IndentationSpaces(),
            tree.Children()[0].Children()[2].Value().IndentationSpaces());

  EXPECT_EQ(saved_tree.Children()[1].Children()[2].Value().IndentationSpaces(),
            tree.Children()[1].Children()[0].Value().IndentationSpaces());
  EXPECT_EQ(saved_tree.Children()[1].Children()[3].Value().IndentationSpaces(),
            tree.Children()[1].Children()[1].Value().IndentationSpaces());
  EXPECT_EQ(saved_tree.Children()[1].Children()[4].Value().IndentationSpaces(),
            tree.Children()[1].Children()[2].Value().IndentationSpaces());
}

// Tests with real-world example
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
  }

 protected:
  const std::string sample_;
  const std::vector<absl::string_view> tokens_;
  std::vector<TokenInfo> ftokens_;
};

class ReshapeFittingSubpartitionsFunctionTest
    : public ReshapeFittingSubpartitionsTestFixture {};

TEST_F(ReshapeFittingSubpartitionsFunctionTest,
       FunctionWithSixArgumentsAndColumnLimit20Characters) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());

  // 'function_fffffffffff('
  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  // function arguments
  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);
  UnwrappedLine arg6(0, arg5.TokensRange().end());
  arg6.SpanUpToToken(all.TokensRange().end());

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg6.TokensRange().end());

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

  // Expect each subpartition in its own partition...
  const tree_type tree_expected{
      all,
      tree_type{header, tree_type{header}},
      tree_type{arg1, tree_type{arg1}},
      tree_type{arg2, tree_type{arg2}},
      tree_type{arg3, tree_type{arg3}},
      tree_type{arg4, tree_type{arg4}},
      tree_type{arg5, tree_type{arg5}},
      tree_type{arg6, tree_type{arg6}},
  };

  verible::BasicFormatStyle style;
  style.column_limit = 20;
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentations
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
  EXPECT_EQ(tree.Children()[1].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[2].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[3].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[4].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[5].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[6].Value().IndentationSpaces(), style.wrap_spaces);
}

TEST_F(ReshapeFittingSubpartitionsFunctionTest,
       FunctionWithSixArgumentsAndColumnLimit40Characters) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());

  // 'function_fffffffffff('
  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  // function arguments
  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);
  UnwrappedLine arg6(0, arg5.TokensRange().end());
  arg6.SpanUpToToken(all.TokensRange().end());

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg6.TokensRange().end());

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

  UnwrappedLine group1(0, begin);
  group1.SpanUpToToken(header.TokensRange().end());
  UnwrappedLine group2(0, arg1.TokensRange().begin());
  group2.SpanUpToToken(arg2.TokensRange().end());
  UnwrappedLine group3(0, arg3.TokensRange().begin());
  group3.SpanUpToToken(arg4.TokensRange().end());
  UnwrappedLine group4(0, arg5.TokensRange().begin());
  group4.SpanUpToToken(arg6.TokensRange().end());

  const tree_type tree_expected{all,
                                tree_type{
                                    group1,
                                    tree_type{header},
                                },
                                tree_type{
                                    group2,
                                    tree_type{arg1},
                                    tree_type{arg2},
                                },
                                tree_type{
                                    group3,
                                    tree_type{arg3},
                                    tree_type{arg4},
                                },
                                tree_type{
                                    group4,
                                    tree_type{arg5},
                                    tree_type{arg6},
                                }};

  verible::BasicFormatStyle style;
  style.column_limit = 40;
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentations
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
  EXPECT_EQ(tree.Children()[1].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[2].Value().IndentationSpaces(), style.wrap_spaces);
  EXPECT_EQ(tree.Children()[3].Value().IndentationSpaces(), style.wrap_spaces);
}

TEST_F(ReshapeFittingSubpartitionsFunctionTest,
       FunctionWithSixArgumentsAndColumnLimit56Characters) {
  const auto& preformat_tokens = pre_format_tokens_;
  const auto begin = preformat_tokens.begin();

  UnwrappedLine all(0, begin);
  all.SpanUpToToken(preformat_tokens.end());

  // 'function_fffffffffff('
  UnwrappedLine header(0, begin);
  header.SpanUpToToken(begin + 1);

  // function arguments
  UnwrappedLine arg1(0, header.TokensRange().end());
  arg1.SpanUpToToken(begin + 2);
  UnwrappedLine arg2(0, arg1.TokensRange().end());
  arg2.SpanUpToToken(begin + 3);
  UnwrappedLine arg3(0, arg2.TokensRange().end());
  arg3.SpanUpToToken(begin + 4);
  UnwrappedLine arg4(0, arg3.TokensRange().end());
  arg4.SpanUpToToken(begin + 5);
  UnwrappedLine arg5(0, arg4.TokensRange().end());
  arg5.SpanUpToToken(begin + 6);
  UnwrappedLine arg6(0, arg5.TokensRange().end());
  arg6.SpanUpToToken(all.TokensRange().end());

  UnwrappedLine args(0, arg1.TokensRange().begin());
  args.SpanUpToToken(arg6.TokensRange().end());

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

  UnwrappedLine group1(0, begin);
  group1.SpanUpToToken(arg2.TokensRange().end());
  UnwrappedLine group2(0, arg3.TokensRange().begin());
  group2.SpanUpToToken(arg4.TokensRange().end());
  UnwrappedLine group3(0, arg5.TokensRange().begin());
  group3.SpanUpToToken(arg6.TokensRange().end());

  const tree_type tree_expected{all,
                                tree_type{
                                    group1,
                                    tree_type{header},
                                    tree_type{arg1},
                                    tree_type{arg2},
                                },
                                tree_type{
                                    group2,
                                    tree_type{arg3},
                                    tree_type{arg4},
                                },
                                tree_type{
                                    group3,
                                    tree_type{arg5},
                                    tree_type{arg6},
                                }};

  verible::BasicFormatStyle style;
  style.column_limit = 56;
  ReshapeFittingSubpartitions(&tree, style);

  const auto diff = DeepEqual(tree, tree_expected, TokenRangeEqual);
  EXPECT_EQ(diff.left, nullptr) << "Expected:\n"
                                << tree_expected << "\nGot:" << tree << "\n";

  // Check indentations (appending)
  EXPECT_EQ(tree.Children()[0].Value().IndentationSpaces(), 0);
  EXPECT_EQ(tree.Children()[1].Value().IndentationSpaces(),
            header.TokensRange()[0].Length());
  EXPECT_EQ(tree.Children()[2].Value().IndentationSpaces(),
            header.TokensRange()[0].Length());
}

}  // namespace verible
