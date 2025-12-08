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

#ifndef VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_TEST_UTILS_H_
#define VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_TEST_UTILS_H_

#include <initializer_list>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/formatting/unwrapped-line.h"

namespace verible {

// Helper class for creating TokenPartitionTree hierarchy using compact and easy
// to read/write/modify syntax. Its main advantage is token range deduction from
// child nodes and specification of token range using indexes instead of
// iterators.
//
// Example use:
//
//   using TPT = TokenPartitionTreeBuilder;
//   const auto tree =
//       TPT(PartitionPolicyEnum::kAlwaysExpand,
//       {
//           TPT({0, 1}, PartitionPolicyEnum::kAlreadyFormatted),
//           TPT(4, {1, 3}, PartitionPolicyEnum::kAlreadyFormatted),
//           TPT(4, {3, 5}, PartitionPolicyEnum::kAlreadyFormatted),
//           TPT(4, {5, 7}, PartitionPolicyEnum::kAlreadyFormatted),
//       })
//       .build(pre_format_tokens_);
//
class TokenPartitionTreeBuilder {
 public:
  TokenPartitionTreeBuilder(
      int indent, std::pair<int, int> token_indexes_range,
      PartitionPolicyEnum policy,
      std::initializer_list<TokenPartitionTreeBuilder> children = {})
      : indent_(indent),
        token_indexes_range_(token_indexes_range),
        policy_(policy),
        children_(children) {}

  TokenPartitionTreeBuilder(
      int indent, PartitionPolicyEnum policy,
      std::initializer_list<TokenPartitionTreeBuilder> children = {})
      : indent_(indent), policy_(policy), children_(children) {}

  TokenPartitionTreeBuilder(
      int indent, std::pair<int, int> token_indexes_range,
      std::initializer_list<TokenPartitionTreeBuilder> children = {})
      : indent_(indent),
        token_indexes_range_(token_indexes_range),
        children_(children) {}

  TokenPartitionTreeBuilder(
      std::pair<int, int> token_indexes_range, PartitionPolicyEnum policy,
      std::initializer_list<TokenPartitionTreeBuilder> children = {})
      : token_indexes_range_(token_indexes_range),
        policy_(policy),
        children_(children) {}

  explicit TokenPartitionTreeBuilder(
      std::pair<int, int> token_indexes_range,
      std::initializer_list<TokenPartitionTreeBuilder> children = {})
      : token_indexes_range_(token_indexes_range), children_(children) {}

  TokenPartitionTreeBuilder(
      PartitionPolicyEnum policy,
      std::initializer_list<TokenPartitionTreeBuilder> children)
      : policy_(policy), children_(children) {}

  TokenPartitionTreeBuilder(
      std::initializer_list<TokenPartitionTreeBuilder> children)
      : children_(children) {}

  // Builds TokenPartitionTree. Token indexes used during construction are
  // replaced with iterators from 'tokens' vector.
  TokenPartitionTree build(
      const std::vector<verible::PreFormatToken> &tokens) const;

 private:
  int indent_ = 0;
  std::pair<int, int> token_indexes_range_ = {-1, -1};
  PartitionPolicyEnum policy_ = PartitionPolicyEnum::kUninitialized;

  const std::vector<TokenPartitionTreeBuilder> children_;
};

// Tests whetherTokenPartitionTrees 'actual' and 'expected' are equal. The
// function compares the trees structure and values of all corresponding nodes.
// Intended for use with EXPECT_PRED_FORMAT2(), e.g.:
//
//   EXPECT_PRED_FORMAT2(TokenPartitionTreesEqualPredFormat, actual_tree,
//                       expected_tree);
//
::testing::AssertionResult TokenPartitionTreesEqualPredFormat(
    const char *actual_expr, const char *expected_expr,
    const TokenPartitionTree &actual, const TokenPartitionTree &expected);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_TOKEN_PARTITION_TREE_TEST_UTILS_H_
