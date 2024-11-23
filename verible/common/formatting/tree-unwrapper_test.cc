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

#include "verible/common/formatting/tree-unwrapper.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/formatting/format-token.h"
#include "verible/common/formatting/unwrapped-line.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/text-structure-test-utils.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/util/container-iterator-range.h"
#include "verible/common/util/range.h"

namespace verible {

static bool KeepNonWhitespace(const TokenInfo &token) {
  const absl::string_view text(absl::StripAsciiWhitespace(token.text()));
  return !text.empty();
}

class TreeUnwrapperData {
 public:
  explicit TreeUnwrapperData(const verible::TokenSequence &tokens) {
    verible::InitTokenStreamView(tokens, &tokens_view_);
    verible::FilterTokenStreamViewInPlace(KeepNonWhitespace, &tokens_view_);

    preformatted_tokens_.reserve(tokens_view_.size());
    std::transform(tokens_view_.begin(), tokens_view_.end(),
                   std::back_inserter(preformatted_tokens_),
                   [](const TokenSequence::const_iterator iter) {
                     return PreFormatToken(&*iter);
                   });
  }

 protected:
  verible::TokenStreamView tokens_view_;
  std::vector<verible::PreFormatToken> preformatted_tokens_;
};

class FakeTreeUnwrapper : public TreeUnwrapperData, public TreeUnwrapper {
 public:
  explicit FakeTreeUnwrapper(const TextStructureView &view)
      : TreeUnwrapperData(view.TokenStream()),
        TreeUnwrapper(view, TreeUnwrapperData::preformatted_tokens_) {}

  // There are no filtered-out tokens in this fake.
  void CollectLeadingFilteredTokens() final {}
  void CollectTrailingFilteredTokens() final {}

  // Leaf visit that adds a PreFormatToken from the leaf's TokenInfo
  // to the current_unwrapped_line_
  void Visit(const verible::SyntaxTreeLeaf &leaf) final {
    CatchUpFilteredTokens();
    AddTokenToCurrentUnwrappedLine();
  }

  // Node visit that always starts a new unwrapped line
  void Visit(const SyntaxTreeNode &node) final {
    StartNewUnwrappedLine(PartitionPolicyEnum::kAlwaysExpand, &node);
    TraverseChildren(node);
  }

  void InterChildNodeHook(const SyntaxTreeNode &node) final {}

  using TreeUnwrapper::StartNewUnwrappedLine;

 protected:
  void CatchUpFilteredTokens() {
    const auto iter = CurrentFormatTokenIterator();
    SkipUnfilteredTokens(
        [=](const verible::TokenInfo &token) { return &token != iter->token; });
  }
};

// Test that no new UnwrappedLines are created when current_unwrapped_line_
// is empty.
TEST(TreeUnwrapperTest, EmptyStartNewUnwrappedLine) {
  std::unique_ptr<TextStructureView> view = MakeTextStructureViewWithNoLeaves();
  FakeTreeUnwrapper tree_unwrapper(*view);

  // const reference forces use of const method
  const auto &const_tree_unwrapper(tree_unwrapper);

  // Do not call .Unwrap() for this test.

  const auto &current = const_tree_unwrapper.CurrentUnwrappedLine();
  tree_unwrapper.StartNewUnwrappedLine(PartitionPolicyEnum::kAlwaysExpand,
                                       &*view->SyntaxTree());
  const auto &next = const_tree_unwrapper.CurrentUnwrappedLine();
  EXPECT_EQ(&current, &next);
}

// Test that StartNewUnwrappedLine properly handles current_unwrapped_line_ by
// creating new unwrapped lines at each Node and appending the
// current_unwrapped_line to UnwrappedLines.
TEST(TreeUnwrapperTest, NonEmptyUnwrap) {
  std::unique_ptr<TextStructureView> view = MakeTextStructureViewHelloWorld();
  FakeTreeUnwrapper tree_unwrapper(*view);
  EXPECT_TRUE(
      verible::BoundsEqual(tree_unwrapper.FullText(), view->Contents()));

  tree_unwrapper.Unwrap();
  const auto unwrapped_lines = tree_unwrapper.FullyPartitionedUnwrappedLines();
  ASSERT_EQ(unwrapped_lines.size(), 2);
  const UnwrappedLine &first_unwrapped_line = unwrapped_lines[0];
  const UnwrappedLine &second_unwrapped_line = unwrapped_lines[1];

  EXPECT_EQ(first_unwrapped_line.Size(), 2);
  EXPECT_EQ(second_unwrapped_line.Size(), 1);

  const auto first_range = first_unwrapped_line.TokensRange();
  const auto second_range = second_unwrapped_line.TokensRange();
  EXPECT_EQ(first_range.front().TokenEnum(), 0);
  EXPECT_EQ(first_range.back().TokenEnum(), 1);
  EXPECT_EQ(second_range.front().TokenEnum(), 3);
  EXPECT_EQ(second_range.back().TokenEnum(), 3);
}

// Test that TreeUnwrapper does not gain any UnwrappedLines after calling
// StartNewUnwrappedLine when visiting a tree with no leaves.
TEST(TreeUnwrapperTest, UnwrapNoLeaves) {
  std::unique_ptr<TextStructureView> view = MakeTextStructureViewWithNoLeaves();
  FakeTreeUnwrapper tree_unwrapper(*view);
  tree_unwrapper.Unwrap();
  const auto unwrapped_lines = tree_unwrapper.FullyPartitionedUnwrappedLines();
  EXPECT_TRUE(unwrapped_lines.empty());  // Blank line removed.
}

TEST(TreeUnwrapperTest, StreamPrinting) {
  std::unique_ptr<TextStructureView> view = MakeTextStructureViewHelloWorld();
  FakeTreeUnwrapper tree_unwrapper(*view);
  EXPECT_TRUE(
      verible::BoundsEqual(tree_unwrapper.FullText(), view->Contents()));

  tree_unwrapper.Unwrap();
  std::ostringstream stream;
  stream << tree_unwrapper;
  EXPECT_EQ(stream.str(),  //
            "[hello ,], policy: always-expand, (origin: \"hello, world\")\n"
            "[world], policy: always-expand, (origin: \"world\")\n");
}

}  // namespace verible
