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

#include "verible/common/text/token-stream-view.h"

#include <cstddef>
#include <iterator>
#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/text/text-structure-test-utils.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info.h"
#include "verible/common/util/iterator-range.h"

namespace verible {

class TokenStreamViewTest : public testing::Test {
 protected:
  TokenStreamViewTest() {
    // Generate with bogus token enums, with 0 at the end.
    for (int i = 0; i < 10; ++i) {
      tokens_.push_back(TokenInfo(i + 2, "moo"));
    }
    tokens_.push_back(TokenInfo::EOFToken());
  }

  static bool KeepEvenTokens(const TokenInfo &t) {
    return !(t.token_enum() & 0x1);
  }

 protected:
  TokenSequence tokens_;
};

TEST_F(TokenStreamViewTest, Init) {
  TokenStreamView view;
  InitTokenStreamView(tokens_, &view);
  EXPECT_EQ(tokens_.size(), view.size());
}

TEST_F(TokenStreamViewTest, Filter) {
  TokenStreamView view1, view2;
  InitTokenStreamView(tokens_, &view1);
  FilterTokenStreamView(KeepEvenTokens, view1, &view2);
  EXPECT_EQ(6, view2.size());
  EXPECT_EQ(2, view2.front()->token_enum());
  EXPECT_EQ(0, view2.back()->token_enum());
}

TEST_F(TokenStreamViewTest, FilterInPlace) {
  TokenStreamView view;
  InitTokenStreamView(tokens_, &view);
  FilterTokenStreamViewInPlace(KeepEvenTokens, &view);
  EXPECT_EQ(6, view.size());
  EXPECT_EQ(2, view.front()->token_enum());
  EXPECT_EQ(0, view.back()->token_enum());
}

// Helper class for testing Token range methods.
class TokenViewRangeTest : public ::testing::Test,
                           public TextStructureTokenized {
 public:
  static constexpr int kSpace = 733;
  static constexpr int kNewline = 734;
  TokenViewRangeTest()
      : TextStructureTokenized(
            {{TokenInfo(3, "hello"), TokenInfo(1, ","), TokenInfo(kSpace, " "),
              TokenInfo(3, "world"), TokenInfo(kNewline, "\n")},
             {TokenInfo(kNewline, "\n")},
             {TokenInfo(3, "hello"), TokenInfo(1, ","), TokenInfo(kSpace, " "),
              TokenInfo(3, "world"), TokenInfo(kNewline, "\n")}}) {
    InitTokenStreamView(data_.TokenStream(), &view_);
  }

  TokenStreamView view_;
};

struct TokenViewRangeTestCase {
  size_t left_offset, right_offset;
  size_t left_index, right_index;
};

// Checks that token ranges span the given offsets.
TEST_F(TokenViewRangeTest, TokenViewRangeSpanningOffsetsNonEmpty) {
  const TokenViewRangeTestCase test_cases[] = {
      {0, 1, 0, 1},   {0, 5, 0, 1},    {0, 6, 0, 2},     {0, 14, 0, 6},
      {0, 15, 0, 7},  {0, 27, 0, 11},  {1, 27, 1, 11},   {5, 27, 1, 11},
      {6, 27, 2, 11}, {21, 27, 9, 11}, {22, 27, 10, 11}, {26, 27, 10, 11},
      {9, 12, 4, 4},  // empty, does not span a whole token
      {9, 9, 4, 4},   {9, 19, 4, 7},
  };
  for (const auto &test_case : test_cases) {
    const int length = test_case.right_offset - test_case.left_offset;
    const std::string_view text_range(
        data_.Contents().substr(test_case.left_offset, length));
    const auto token_view_range =
        TokenViewRangeSpanningOffsets(view_, text_range);
    EXPECT_EQ(std::distance(view_.cbegin(), token_view_range.begin()),
              test_case.left_index);
    EXPECT_EQ(std::distance(view_.cbegin(), token_view_range.end()),
              test_case.right_index);
  }
}

}  // namespace verible
