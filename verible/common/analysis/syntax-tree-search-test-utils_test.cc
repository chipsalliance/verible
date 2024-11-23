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

#include "verible/common/analysis/syntax-tree-search-test-utils.h"

#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/tree-builder-test-util.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest, AllEmpty) {
  const SyntaxTreeSearchTestCase test{};
  const std::vector<TreeSearchMatch> actual_findings;
  const absl::string_view text;
  std::ostringstream diffstream;
  EXPECT_TRUE(test.ExactMatchFindings(actual_findings, text, &diffstream));
}

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest, OneMatchingViolation) {
  constexpr int kToken = 42;
  const SyntaxTreeSearchTestCase test{
      "abc",
      {kToken, "def"},
      "ghi",
  };
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  constexpr int kTag = -1;
  auto leaf = Leaf(kTag, bad_text);
  const std::vector<TreeSearchMatch> actual_findings{
      {leaf.get(), {/* context ignored */}},
  };
  std::ostringstream diffstream;
  EXPECT_TRUE(test.ExactMatchFindings(actual_findings, text_view, &diffstream));
  EXPECT_TRUE(diffstream.str().empty());
}

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest, IgnoreEmptyStringSpan) {
  constexpr int kToken = 42;
  const SyntaxTreeSearchTestCase test{
      "abc",
      {kToken, "def"},
      "ghi",
  };
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  constexpr int kTag = -1;
  auto leaf = Leaf(kTag, bad_text);
  auto ignored_leaf = Leaf(kTag, bad_text.substr(0, 0));
  const std::vector<TreeSearchMatch> actual_findings{
      {ignored_leaf.get(), {/* context ignored */}},
      {leaf.get(), {/* context ignored */}},
  };
  std::ostringstream diffstream;
  EXPECT_TRUE(test.ExactMatchFindings(actual_findings, text_view, &diffstream));
  EXPECT_TRUE(diffstream.str().empty());
}

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest, IgnoreNullptrSymbol) {
  constexpr int kToken = 42;
  const SyntaxTreeSearchTestCase test{
      "abc",
      {kToken, "def"},
      "ghi",
  };
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  constexpr int kTag = -1;
  auto leaf = Leaf(kTag, bad_text);
  const std::vector<TreeSearchMatch> actual_findings{
      {leaf.get(), {/* context ignored */}},
      {nullptr, {/* context ignored */}},
  };
  std::ostringstream diffstream;
  EXPECT_TRUE(test.ExactMatchFindings(actual_findings, text_view, &diffstream));
  EXPECT_TRUE(diffstream.str().empty());
}

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest,
     MultipleMatchingViolations) {
  constexpr int kToken = 42;  // enum ignored
  const SyntaxTreeSearchTestCase test{
      "abc",
      {kToken, "def"},
      "ghi",
      {kToken, "jkl"},
  };
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const auto bad_text1 = Leaf(kToken, text_view.substr(3, 3));
  const auto bad_text2 = Leaf(kToken, text_view.substr(9, 3));
  const std::vector<TreeSearchMatch> actual_findings{
      // must be sorted on location
      {bad_text1.get(), {/* context ignored */}},
      {bad_text2.get(), {/* context ignored */}},
  };
  std::ostringstream diffstream;
  EXPECT_TRUE(test.ExactMatchFindings(actual_findings, text_view, &diffstream));
  EXPECT_TRUE(diffstream.str().empty());
}

constexpr absl::string_view kFoundNotExpectedMessage(
    "actual findings did not match the expected");
constexpr absl::string_view kExpectedNotFoundMessage(
    "expected findings did not match the ones found");

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest, OneFoundNotExpected) {
  constexpr int kToken = 42;
  const SyntaxTreeSearchTestCase test{"abcdefghi"};  // no expected violations
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  const auto leaf = Leaf(kToken, bad_text);
  const std::vector<TreeSearchMatch> actual_findings{
      {leaf.get(), {/* context ignored */}},
  };
  std::ostringstream diffstream;
  EXPECT_FALSE(
      test.ExactMatchFindings(actual_findings, text_view, &diffstream));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kFoundNotExpectedMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), bad_text));
  EXPECT_FALSE(absl::StrContains(diffstream.str(), kExpectedNotFoundMessage));
}

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest, OneExpectedNotFound) {
  constexpr int kToken = 42;
  const SyntaxTreeSearchTestCase test{"abc", {kToken, "def"}, "ghi"};
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  const std::vector<TreeSearchMatch> actual_findings;  // none expected
  std::ostringstream diffstream;
  EXPECT_FALSE(
      test.ExactMatchFindings(actual_findings, text_view, &diffstream));
  EXPECT_FALSE(absl::StrContains(diffstream.str(), kFoundNotExpectedMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), bad_text));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kExpectedNotFoundMessage));
}

TEST(SyntaxTreeSearchTestCaseExactMatchFindingsTest, OneMismatchEach) {
  constexpr int kToken = 42;
  const SyntaxTreeSearchTestCase test{"abc", {kToken, "def"}, "ghi"};
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(4, 3);  // "efg"
  const auto leaf = Leaf(kToken, bad_text);
  const std::vector<TreeSearchMatch> actual_findings{
      {leaf.get(), {/* context ignored */}},
  };
  std::ostringstream diffstream;
  EXPECT_FALSE(
      test.ExactMatchFindings(actual_findings, text_view, &diffstream));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kFoundNotExpectedMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), bad_text));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kExpectedNotFoundMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), text_view.substr(3, 3)));
}

}  // namespace
}  // namespace verible
