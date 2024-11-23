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

#include "verible/common/formatting/token-partition-tree-test-utils.h"

#include <vector>

#include "gtest/gtest.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/token-partition-tree.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/tree-operations.h"

namespace verible {

namespace {

bool PartitionsEqual(const UnwrappedLine &left, const UnwrappedLine &right) {
  return (left.TokensRange() == right.TokensRange()) &&
         (left.IndentationSpaces() == right.IndentationSpaces()) &&
         (left.PartitionPolicy() == right.PartitionPolicy()) &&
         (left.Origin() == right.Origin());
}

}  // namespace

TokenPartitionTree TokenPartitionTreeBuilder::build(
    const std::vector<verible::PreFormatToken> &tokens) const {
  TokenPartitionTree node;

  node.Children().reserve(children_.size());
  for (const auto &child : children_) {
    node.Children().push_back(child.build(tokens));
  }

  FormatTokenRange node_tokens;
  if (token_indexes_range_.first < 0) {
    const auto &child_nodes = node.Children();
    CHECK(!child_nodes.empty());
    CHECK_LT(token_indexes_range_.second, 0);
    node_tokens.set_begin(child_nodes.front().Value().TokensRange().begin());
    node_tokens.set_end(child_nodes.back().Value().TokensRange().end());
  } else {
    CHECK_GE(token_indexes_range_.second, token_indexes_range_.first);
    node_tokens.set_begin(tokens.begin() + token_indexes_range_.first);
    node_tokens.set_end(tokens.begin() + token_indexes_range_.second);
  }

  node.Value() = UnwrappedLine(indent_, node_tokens.begin(), policy_);
  node.Value().SpanUpToToken(node_tokens.end());
  return node;
}

::testing::AssertionResult TokenPartitionTreesEqualPredFormat(
    const char *actual_expr, const char *expected_expr,
    const TokenPartitionTree &actual, const TokenPartitionTree &expected) {
  const auto diff = DeepEqual(actual, expected, PartitionsEqual);
  if (diff.left != nullptr) {
    return ::testing::AssertionFailure()
           << "Expected equality of these trees:\n"
              "Actual:\n"
           << actual
           << "\n"
              "Expected:\n"
           << expected << "\n";
  }
  return ::testing::AssertionSuccess();
}

}  // namespace verible
