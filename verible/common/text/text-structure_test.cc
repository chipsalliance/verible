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

#include "verible/common/text/text-structure.h"

#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/strings/line-column-map.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/constants.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure-test-utils.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/token-stream-view.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/text/tree-compare.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"
#include "verible/common/util/value-saver.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verible {

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::SizeIs;

// Test constructor and initial state.
TEST(TextStructureViewCtorTest, InitializeContents) {
  const char *inputs[] = {"", "<ANY>", "hello world", "foo\nbar\n"};
  for (const auto *input : inputs) {
    TextStructureView test_view(input);
    EXPECT_EQ(test_view.Contents(), input);
    EXPECT_THAT(test_view.TokenStream(), IsEmpty());
    EXPECT_THAT(test_view.GetTokenStreamView(), IsEmpty());
    EXPECT_THAT(test_view.SyntaxTree(), IsNull());
    EXPECT_OK(test_view.InternalConsistencyCheck());
  }
}

// Test that filtering nothing works.
TEST(FilterTokensTest, EmptyTokens) {
  TextStructureView test_view("blah");
  EXPECT_THAT(test_view.GetTokenStreamView(), IsEmpty());
  test_view.FilterTokens([](const TokenInfo &token) { return true; });
  EXPECT_THAT(test_view.GetTokenStreamView(), IsEmpty());
}

// Create a one-token token stream and syntax tree.
static void OneTokenTextStructureView(TextStructureView *view) {
  TokenInfo token(1, view->Contents());
  view->MutableTokenStream().push_back(token);
  view->MutableTokenStreamView().push_back(view->TokenStream().begin());
  view->MutableSyntaxTree() = Leaf(token);
}

// Create a two-token token stream, no syntax tree.
static void MultiTokenTextStructureViewNoTree(TextStructureView *view) {
  const auto contents = view->Contents();
  CHECK_GE(contents.length(), 5);
  auto &stream = view->MutableTokenStream();
  for (int i = 0; i < 5; ++i) {  // Populate with 5 single-char tokens.
    stream.emplace_back(i + 1, contents.substr(i, 1));
  }
  auto &stream_view = view->MutableTokenStreamView();
  // Populate view with 2 tokens.
  stream_view.emplace_back(stream.begin() + 1);
  stream_view.emplace_back(stream.begin() + 3);
}

// Test that filtering can keep tokens.
TEST(FilterTokensTest, OneTokenKept) {
  const std::string_view text = "blah";
  TextStructureView test_view(text);
  // Pretend to lex and parse text.
  OneTokenTextStructureView(&test_view);
  EXPECT_THAT(test_view.GetTokenStreamView(), SizeIs(1));
  test_view.FilterTokens([](const TokenInfo &token) { return true; });
  EXPECT_THAT(test_view.GetTokenStreamView(), SizeIs(1));
}

// Test that filtering can remove tokens.
TEST(FilterTokensTest, OneTokenRemoved) {
  const std::string_view text = "blah";
  TextStructureView test_view(text);
  // Pretend to lex and parse text.
  OneTokenTextStructureView(&test_view);
  EXPECT_THAT(test_view.GetTokenStreamView(), SizeIs(1));
  test_view.FilterTokens([](const TokenInfo &token) { return false; });
  EXPECT_THAT(test_view.GetTokenStreamView(), IsEmpty());
}

// Test that mutating nothing works.
TEST(MutateTokensTest, EmptyTokensNoOp) {
  TextStructureView test_view("");
  test_view.MutateTokens([](TokenInfo *) {});
  EXPECT_THAT(test_view.TokenStream(), IsEmpty());
  EXPECT_THAT(test_view.GetTokenStreamView(), IsEmpty());
  EXPECT_THAT(test_view.SyntaxTree(), IsNull());
}

// Test that a copy of writeable iterators to tokens matches const iterators.
TEST(TokenStreamReferenceViewTest, ShiftRight) {
  TextStructureView test_view("hello");
  MultiTokenTextStructureViewNoTree(&test_view);
  auto iterators = test_view.MakeTokenStreamReferenceView();
  const auto &stream_view = test_view.GetTokenStreamView();
  auto view_iter = stream_view.begin();
  for (auto iter : iterators) {
    EXPECT_EQ(iter, *view_iter);  // write-iterators same as read-iterators
    ++view_iter;
  }
}

// Test that EOFToken is properly constructed to the correct range.
TEST(EOFTokenTest, TokenRange) {
  const std::string_view kTestCases[] = {
      "",
      "\n",
      "foobar",
      "foobar\n",
  };
  for (auto test : kTestCases) {
    TextStructureView test_view(test);
    TokenInfo token(test_view.EOFToken());
    EXPECT_EQ(token.token_enum(), verible::TK_EOF);
    EXPECT_TRUE(token.text().empty());
    EXPECT_EQ(token.text().begin(), test.end());
    EXPECT_EQ(token.left(test_view.Contents()), test.length());
  }
}

// Test that string_views can point to memory owned in new location,
// where new location is a superstring of the original.
TEST(RebaseTokensToSuperstringTest, NewOwner) {
  const std::string_view superstring = "abcdefgh";
  const std::string_view substring = "cdef";
  EXPECT_FALSE(IsSubRange(substring, superstring));
  TextStructureView test_view(substring);
  OneTokenTextStructureView(&test_view);
  const TokenInfo expect_pre(1, substring);
  EXPECT_EQ(test_view.TokenStream().front(), expect_pre);
  EXPECT_TRUE(EqualTrees(test_view.SyntaxTree().get(), Leaf(expect_pre).get()));
  test_view.RebaseTokensToSuperstring(superstring, substring, 2);
  const TokenInfo expect_post(1, superstring.substr(2, 4));
  EXPECT_EQ(test_view.TokenStream().front(), expect_post);
  EXPECT_TRUE(
      EqualTrees(test_view.SyntaxTree().get(), Leaf(expect_post).get()));
  EXPECT_TRUE(IsSubRange(test_view.TokenStream().front().text(), superstring));
}

// Helper class for testing Token range methods.
class TokenRangeTest : public ::testing::Test, public TextStructureTokenized {
 public:
  static constexpr int kSpace = 2;
  static constexpr int kNewline = 4;
  TokenRangeTest()
      : TextStructureTokenized(
            {{TokenInfo(3, "hello"), TokenInfo(1, ","), TokenInfo(kSpace, " "),
              TokenInfo(3, "world"), TokenInfo(kNewline, "\n")},
             {TokenInfo(kNewline, "\n")},
             {TokenInfo(3, "hello"), TokenInfo(1, ","), TokenInfo(kSpace, " "),
              TokenInfo(3, "world"), TokenInfo(kNewline, "\n")}}) {}
};

// Checks for consistency between beginning-of-line offset map and the
// beginning-of-line token iterator map.
TEST_F(TokenRangeTest, CalculateFirstTokensPerLineTest) {
  const auto &line_token_map = data_.GetLineTokenMap();
  const auto &line_column_map = data_.GetLineColumnMap();
  // There is always one more entry in the line_token_map that points to end().
  EXPECT_EQ(line_column_map.GetBeginningOfLineOffsets().size() + 1,
            line_token_map.size());
  const auto &tokens = data_.TokenStream();
  EXPECT_EQ(line_token_map.front(), tokens.begin());
  EXPECT_EQ(line_token_map.back(), tokens.end());
  EXPECT_EQ(line_token_map[1], tokens.begin() + 5);
  EXPECT_EQ(line_token_map[2], tokens.begin() + 6);
  EXPECT_EQ(line_token_map[3], tokens.begin() + 11);
}

TEST_F(TokenRangeTest, GetRangeOfTokenVerifyAllRangesExclusive) {
  // Bulk testing: let's see that we constantly progress in emitted ranges.
  LineColumnRange previous{{0, 0}, {0, 0}};
  for (const TokenInfo &token : data_.TokenStream()) {
    LineColumnRange token_range = data_.GetRangeForToken(token);
    EXPECT_EQ(token_range.start, previous.end);
    EXPECT_LT(previous.end, token_range.end);
    EXPECT_GE(token_range.end, token_range.start);
    previous = token_range;
  }
}

TEST_F(TokenRangeTest, GetRangeOfTokenEofTokenAcceptedUniversally) {
  // For the EOF token, the returned range should automatically be relative
  // to the TextView no matter where it comes from.
  EXPECT_EQ(data_.GetRangeForToken(data_.EOFToken()),
            data_.GetRangeForToken(TokenInfo::EOFToken()));
}

TEST_F(TokenRangeTest, GetRangeForTokenOrText) {
  const TokenInfo &token = data_.FindTokenAt({0, 7});
  EXPECT_EQ(token.text(), "world");
  {  // Extract from token
    const LineColumnRange range = data_.GetRangeForToken(token);
    EXPECT_EQ(range.start.line, 0);
    EXPECT_EQ(range.start.column, 7);
  }
  {  // Extract from token text
    const LineColumnRange range = data_.GetRangeForText(token.text());
    EXPECT_EQ(range.start.line, 0);
    EXPECT_EQ(range.start.column, 7);
  }

  {  // Entire text range
    const LineColumnRange range = data_.GetRangeForText(data_.Contents());
    EXPECT_EQ(range.start.line, 0);
    EXPECT_EQ(range.start.column, 0);
    EXPECT_EQ(range.end.line, data_.Lines().size() - 1);
    EXPECT_EQ(range.end.column, data_.Lines().back().length());
  }
}

TEST_F(TokenRangeTest, CheckContainsText) {
  const TokenInfo &token = data_.FindTokenAt({0, 7});
  const std::string_view other_string = "other_string";
  EXPECT_TRUE(data_.ContainsText(token.text()));
  EXPECT_FALSE(data_.ContainsText(other_string));
}

TEST_F(TokenRangeTest, FindTokenAtPosition) {
  EXPECT_EQ(data_.FindTokenAt({0, 0}).text(), "hello");
  EXPECT_EQ(data_.FindTokenAt({0, 4}).text(), "hello");
  EXPECT_EQ(data_.FindTokenAt({0, 5}).text(), ",");
  EXPECT_EQ(data_.FindTokenAt({0, 6}).text(), " ");
  EXPECT_EQ(data_.FindTokenAt({0, 7}).text(), "world");

  // Out of range column: return last token in line.
  EXPECT_EQ(data_.FindTokenAt({0, 200}).text(), "\n");

  // Graceful handling of values out of range: EOF
  EXPECT_TRUE(data_.FindTokenAt({-1, -1}).isEOF());
  EXPECT_TRUE(data_.FindTokenAt({42, 7}).isEOF());
}

// Checks that when lower == upper, returned range is empty.
TEST_F(TokenRangeTest, TokenRangeSpanningOffsetsEmpty) {
  const size_t test_offsets[] = {0, 1, 4, 12, 18, 22, 26};
  for (const auto offset : test_offsets) {
    const auto token_range = data_.TokenRangeSpanningOffsets(offset, offset);
    EXPECT_EQ(token_range.begin(), token_range.end());
  }
}

struct TokenRangeTestCase {
  size_t left_offset, right_offset;
  size_t left_index, right_index;
};

// Checks that token ranges span the given offsets.
TEST_F(TokenRangeTest, TokenRangeSpanningOffsetsNonEmpty) {
  const TokenRangeTestCase test_cases[] = {
      {0, 1, 0, 1},      // noformat
      {0, 5, 0, 1},      // noformat
      {0, 6, 0, 2},      // noformat
      {0, 14, 0, 6},     // noformat
      {0, 15, 0, 7},     // noformat
      {0, 27, 0, 11},    // noformat
      {1, 27, 1, 11},    // noformat
      {5, 27, 1, 11},    // noformat
      {6, 27, 2, 11},    // noformat
      {21, 27, 9, 11},   // noformat
      {22, 27, 10, 11},  // noformat
      {26, 27, 10, 11},  // noformat
      {9, 12, 4, 4},     // empty, does not span a whole token
      {9, 19, 4, 7},
  };
  for (const auto &test_case : test_cases) {
    const auto token_range = data_.TokenRangeSpanningOffsets(
        test_case.left_offset, test_case.right_offset);
    EXPECT_EQ(std::distance(data_.TokenStream().cbegin(), token_range.begin()),
              test_case.left_index);
    EXPECT_EQ(std::distance(data_.TokenStream().cbegin(), token_range.end()),
              test_case.right_index);
  }
}

struct TokenLineTestCase {
  size_t lineno;
  size_t left_index, right_index;
};

// Verify the ranges of tokens spanned per line, and that they end with '\n'.
TEST_F(TokenRangeTest, TokenRangeOnLine) {
  const TokenLineTestCase test_cases[] = {
      {0, 0, 5},  // The first entry always points to the first token at [0].
      {1, 5, 6},  // empty line that only contains newline
      {2, 6, 11},
      {3, 11, 11},  // There is no line[3], this represents an empty range.
  };
  for (const auto &test_case : test_cases) {
    const auto token_range = data_.TokenRangeOnLine(test_case.lineno);
    EXPECT_EQ(std::distance(data_.TokenStream().cbegin(), token_range.begin()),
              test_case.left_index);
    EXPECT_EQ(std::distance(data_.TokenStream().cbegin(), token_range.end()),
              test_case.right_index);
    // All lines end with newline in this example.
    EXPECT_EQ((token_range.end() - 1)->text(), "\n");
  }
}

// Testing select public methods of TextStructureView.
class TextStructureViewPublicTest : public ::testing::Test,
                                    public TextStructureView {
 public:
  TextStructureViewPublicTest() : TextStructureView("hello, world") {
    // Manually lex and parse into token stream and syntax tree.
    // Token enums must be nonzero to not be considered EOF.
    tokens_.push_back(TokenInfo(3, contents_.substr(0, 5)));  // "hello"
    tokens_.push_back(TokenInfo(1, contents_.substr(5, 1)));  // ","
    tokens_.push_back(TokenInfo(2, contents_.substr(6, 1)));  // " "
    tokens_.push_back(TokenInfo(3, contents_.substr(7, 5)));  // "world"
    // Stream view will omit the space token.
    tokens_view_.push_back(tokens_.begin());
    tokens_view_.push_back(tokens_.begin() + 1);
    tokens_view_.push_back(tokens_.begin() + 3);
    // Make syntax tree from stream view.
    syntax_tree_ = Node(Leaf(tokens_[0]), Leaf(tokens_[1]), Leaf(tokens_[3]));
    // Calculate map of beginning-of-line tokens.
    CalculateFirstTokensPerLine();
  }
};

// Testing select protected methods of TextStructureView.
class TextStructureViewInternalsTest : public TextStructureViewPublicTest {
 public:
  // Clean-up to prevent consistency checks from failing after this internal
  // modifications.  This is only appropriate for tests on private or protected
  // methods; public methods should always leave the structure in a consistent
  // state.
  ~TextStructureViewInternalsTest() override { Clear(); }  // not yet final
};

// Test that whole tree is returned with offset 0.
TEST_F(TextStructureViewInternalsTest, TrimSyntaxTreeWholeTree) {
  TrimSyntaxTree(0, contents_.length());
  const auto expect_tree =
      Node(Leaf(tokens_[0]), Leaf(tokens_[1]), Leaf(tokens_[3]));
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
}

// Test that partial tree is returned with nonzero offset.
TEST_F(TextStructureViewInternalsTest, TrimSyntaxTreeOneLeaf) {
  TrimSyntaxTree(1, contents_.length());
  const auto expect_tree = Leaf(tokens_[1]);
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
}

// Test that partial tree is returned with nonzero offset, last leaf.
TEST_F(TextStructureViewInternalsTest, TrimSyntaxTreeLastLeaf) {
  TrimSyntaxTree(7, contents_.length());
  const auto expect_tree = Leaf(tokens_[3]);
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
}

// Test that trimming tokens changes nothing when range spans whole contents.
TEST_F(TextStructureViewInternalsTest, TrimTokensToSubstringKeepEverything) {
  TrimTokensToSubstring(0, contents_.length());
  EXPECT_THAT(tokens_, SizeIs(5));
  EXPECT_THAT(tokens_view_, SizeIs(3));
  const TokenInfo &back(tokens_.back());
  EXPECT_TRUE(back.isEOF());
  EXPECT_TRUE(
      BoundsEqual(back.text(), make_range(contents_.end(), contents_.end())));
}

// Test that trimming tokens changes nothing when range is empty.
TEST_F(TextStructureViewInternalsTest, TrimTokensToSubstringKeepNothing) {
  TrimTokensToSubstring(5, 5);  // an empty range
  EXPECT_THAT(tokens_, IsEmpty());
  EXPECT_THAT(tokens_view_, IsEmpty());
}

// Test that trimming tokens can reduce to a subset.
TEST_F(TextStructureViewInternalsTest, TrimTokensToSubstringKeepSubset) {
  TrimTokensToSubstring(3, 12);
  EXPECT_THAT(tokens_, SizeIs(4));
  EXPECT_THAT(tokens_view_, SizeIs(2));
  const TokenInfo &back(tokens_.back());
  EXPECT_TRUE(back.isEOF());
  EXPECT_TRUE(BoundsEqual(
      back.text(), make_range(contents_.begin() + 12, contents_.begin() + 12)));
}

// Test that trimming tokens can reduce to one leaf.
TEST_F(TextStructureViewInternalsTest, TrimTokensToSubstringKeepLeaf) {
  TrimTokensToSubstring(0, 6);
  EXPECT_THAT(tokens_, SizeIs(3));
  EXPECT_THAT(tokens_view_, SizeIs(2));
  const TokenInfo &back(tokens_.back());
  EXPECT_TRUE(back.isEOF());
  EXPECT_TRUE(BoundsEqual(
      back.text(), make_range(contents_.begin() + 6, contents_.begin() + 6)));
}

// Test trimming the contents to narrower range of text.
TEST_F(TextStructureViewInternalsTest, TrimContents) {
  TrimContents(2, 9);
  EXPECT_EQ(contents_, "llo, worl");
  TrimContents(1, 6);
  EXPECT_EQ(contents_, "lo, wo");
}

// Test that a span of the whole contents preserves everything.
TEST_F(TextStructureViewPublicTest, FocusOnSubtreeSpanningSubstringWholeTree) {
  const auto expect_tree =
      Node(Leaf(tokens_[0]), Leaf(tokens_[1]), Leaf(tokens_[3]));
  FocusOnSubtreeSpanningSubstring(0, contents_.length());
  EXPECT_THAT(tokens_, SizeIs(5));
  EXPECT_THAT(tokens_view_, SizeIs(3));
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
  EXPECT_TRUE(tokens_.back().isEOF());
}

// Test that a substring range yields a subtree.
TEST_F(TextStructureViewPublicTest, FocusOnSubtreeSpanningSubstringFirstLeaf) {
  const auto expect_tree = Leaf(tokens_[0]);
  FocusOnSubtreeSpanningSubstring(0, tokens_[0].text().length());
  EXPECT_THAT(tokens_, SizeIs(2));
  EXPECT_THAT(tokens_view_, SizeIs(1));
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
  EXPECT_TRUE(tokens_.back().isEOF());
}

// Test that ExpandSubtrees on an empty map changes nothing.
TEST_F(TextStructureViewPublicTest, ExpandSubtreesEmpty) {
  const auto expect_tree =
      Node(Leaf(tokens_[0]), Leaf(tokens_[1]), Leaf(tokens_[3]));
  TextStructureView::NodeExpansionMap expansion_map;
  ExpandSubtrees(&expansion_map);
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
}

// Splits a single token into a syntax tree node with two leaves.
static void FakeParseToken(TextStructureView *data, int offset, int node_tag) {
  TokenSequence &tokens = data->MutableTokenStream();
  tokens.push_back(TokenInfo(11, data->Contents().substr(0, offset)));
  tokens.push_back(TokenInfo(12, data->Contents().substr(offset)));
  TokenStreamView &tokens_view = data->MutableTokenStreamView();
  tokens_view.push_back(tokens.begin());
  tokens_view.push_back(tokens.begin() + 1);
  data->MutableSyntaxTree() = TNode(node_tag, Leaf(tokens[0]), Leaf(tokens[1]));
}

// Test that ExpandSubtrees expands a single leaf into a subtree.
TEST_F(TextStructureViewPublicTest, ExpandSubtreesOneLeaf) {
  // Expand the "hello" token into ("hel", "lo").
  const int divide = 3;
  const int new_node_tag = 7;
  std::string subtext(tokens_[0].text().data(), tokens_[0].text().length());
  std::unique_ptr<TextStructure> subanalysis(new TextStructure(subtext));
  FakeParseToken(&subanalysis->MutableData(), divide, new_node_tag);
  auto &replacement_node =
      down_cast<SyntaxTreeNode *>(syntax_tree_.get())->front();
  TextStructureView::DeferredExpansion expansion{&replacement_node,
                                                 std::move(subanalysis)};
  // Expect tree must be built using substrings of contents_.
  // Build the expect tree first because it references text using
  // pre-mutation indices.
  const auto expect_tree = Node(                            // noformat
      TNode(new_node_tag,                                   // noformat
            Leaf(11, tokens_[0].text().substr(0, divide)),  // noformat
            Leaf(12, tokens_[0].text().substr(divide))      // noformat
            ),                                              // noformat
      Leaf(tokens_[1]),                                     // noformat
      Leaf(tokens_[3]));
  TextStructureView::NodeExpansionMap expansion_map;
  expansion_map[tokens_[0].left(contents_)] = std::move(expansion);
  ExpandSubtrees(&expansion_map);
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
}

// Test that ExpandSubtrees expands a single leaf into a subtree.
TEST_F(TextStructureViewPublicTest, ExpandSubtreesMultipleLeaves) {
  const int divide1 = 3;
  const int new_node_tag1 = 7;
  const int divide2 = 2;
  const int new_node_tag2 = 9;
  const int offset2 = tokens_[3].left(contents_);
  TextStructureView::NodeExpansionMap expansion_map;
  {
    // Expand the "hello" token into ("hel", "lo").
    std::string subtext(tokens_[0].text().data(), tokens_[0].text().length());
    std::unique_ptr<TextStructure> subanalysis(new TextStructure(subtext));
    FakeParseToken(&subanalysis->MutableData(), divide1, new_node_tag1);
    auto &replacement_node =
        down_cast<SyntaxTreeNode *>(syntax_tree_.get())->front();
    TextStructureView::DeferredExpansion expansion{&replacement_node,
                                                   std::move(subanalysis)};
    expansion_map[tokens_[0].left(contents_)] = std::move(expansion);
  }
  {
    // Expand the "world" token into ("wo", "rld").
    std::string subtext(tokens_[3].text().data(), tokens_[3].text().length());
    std::unique_ptr<TextStructure> subanalysis(new TextStructure(subtext));
    FakeParseToken(&subanalysis->MutableData(), divide2, new_node_tag2);
    auto &replacement_node =
        down_cast<SyntaxTreeNode *>(syntax_tree_.get())->back();
    TextStructureView::DeferredExpansion expansion{&replacement_node,
                                                   std::move(subanalysis)};
    expansion_map[offset2] = std::move(expansion);
  }
  // Expect tree must be built using substrings of contents_.
  // Build the expect tree first because it references text using
  // pre-mutation indices.
  const auto expect_tree = Node(                             // noformat
      TNode(new_node_tag1,                                   // noformat
            Leaf(11, tokens_[0].text().substr(0, divide1)),  // noformat
            Leaf(12, tokens_[0].text().substr(divide1))      // noformat
            ),                                               // noformat
      Leaf(tokens_[1]),                                      // noformat
      TNode(new_node_tag2,                                   // noformat
            Leaf(11, tokens_[3].text().substr(0, divide2)),  // noformat
            Leaf(12, tokens_[3].text().substr(divide2))      // noformat
            )                                                // noformat
  );
  ExpandSubtrees(&expansion_map);
  EXPECT_TRUE(EqualTrees(syntax_tree_.get(), expect_tree.get()));
}

// The following tests intentionally cause internal violations to
// make sure the consistency checks work as intended.
// The mutated fields are restored so that the consistency checks
// pass at destruction time.

// Test that FastLineRangeConsistencyCheck catches text mismatch at first line.
TEST_F(TextStructureViewInternalsTest, LineConsistencyFailsBeginning) {
  const ValueSaver<std::string_view> save_contents(&contents_);
  contents_ = contents_.substr(1);
  EXPECT_FALSE(FastLineRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastLineRangeConsistencyCheck catches text mismatch at last line.
TEST_F(TextStructureViewInternalsTest, LineConsistencyFailsEnd) {
  const ValueSaver<std::string_view> save_contents(&contents_);
  contents_ = contents_.substr(0, contents_.length() - 1);
  EXPECT_FALSE(FastLineRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches location past end.
TEST_F(TextStructureViewInternalsTest, RangeConsistencyFailPastContentsEnd) {
  const ValueSaver<std::string_view> save_contents(&contents_);
  contents_ = contents_.substr(0, contents_.length() - 1);
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches location past begin.
TEST_F(TextStructureViewInternalsTest, RangeConsistencyFailPastContentsBegin) {
  const ValueSaver<std::string_view> save_contents(&contents_);
  contents_ = contents_.substr(1);
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// The glibcc debug is seeing right through the marginal error conditions
// we try to test that they are discovered by our checks here.
// Good, but that also means that we have to disable with GLBCCXX debug
#ifndef _GLIBCXX_DEBUG
// Test that FastTokenRangeConsistencyCheck catches first token iterator past
// begin.
TEST_F(TextStructureViewInternalsTest,
       RangeConsistencyFailViewFrontPastTokensBegin) {
  const ValueSaver<TokenSequence::const_iterator> save_iterator(
      &tokens_view_.front());
  --tokens_view_.front();
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches first token iterator past
// end.
TEST_F(TextStructureViewInternalsTest,
       RangeConsistencyFailViewFrontPastTokensEnd) {
  const ValueSaver<TokenSequence::const_iterator> save_iterator(
      &tokens_view_.front());
  tokens_view_.front() += tokens_.size();
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches last token iterator past
// end.
TEST_F(TextStructureViewInternalsTest,
       RangeConsistencyFailViewBackPastTokensEnd) {
  const ValueSaver<TokenSequence::const_iterator> save_iterator(
      &tokens_view_.back());
  ++tokens_view_.back();
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches last token iterator past
// begin.
TEST_F(TextStructureViewInternalsTest,
       RangeConsistencyFailViewBackPastTokensBegin) {
  const ValueSaver<TokenSequence::const_iterator> save_iterator(
      &tokens_view_.back());
  tokens_view_.back() -= tokens_.size();
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}
#endif  // GLIBCC_DEBUG

// Test that FastTokenRangeConsistencyCheck catches last token in tree
// located past the begin.
TEST_F(TextStructureViewInternalsTest,
       SyntaxTreeConsistencyFailViewRightmostLeafPastBegin) {
  const ValueSaver<std::string_view> save_contents(&contents_);
  contents_ = contents_.substr(1);
  EXPECT_FALSE(SyntaxTreeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches last token in tree
// located past the end.
TEST_F(TextStructureViewInternalsTest,
       SyntaxTreeConsistencyFailViewRightmostLeafPastEnd) {
  const ValueSaver<std::string_view> save_contents(&contents_);
  contents_ = contents_.substr(0, contents_.length() - 1);
  EXPECT_FALSE(SyntaxTreeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches an incorrect beginning of
// the per-line token map.
TEST_F(TextStructureViewInternalsTest, LineTokenMapWrongBegin) {
  EXPECT_FALSE(tokens_.empty());
  ASSERT_FALSE(GetLineTokenMap().empty());
  ++lazy_line_token_map_.front();
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

// Test that FastTokenRangeConsistencyCheck catches an incorrect end of
// the per-line token map.
TEST_F(TextStructureViewInternalsTest, LineTokenMapWrongEnd) {
  EXPECT_FALSE(tokens_.empty());
  ASSERT_FALSE(GetLineTokenMap().empty());
  --lazy_line_token_map_.back();
  EXPECT_FALSE(FastTokenRangeConsistencyCheck().ok());
  EXPECT_FALSE(InternalConsistencyCheck().ok());
}

}  // namespace verible
