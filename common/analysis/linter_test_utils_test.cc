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

#include "common/analysis/linter_test_utils.h"

#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/util/range.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(LintTestCaseExactMatchFindingsTest, AllEmpty) {
  const LintTestCase test{};
  const std::set<LintViolation> found_violations;
  const absl::string_view text;
  std::ostringstream diffstream;
  EXPECT_TRUE(test.ExactMatchFindings(found_violations, text, &diffstream));
}

TEST(LintTestCaseExactMatchFindingsTest, OneMatchingViolation) {
  constexpr int kToken = 42;
  const LintTestCase test{
      "abc",
      {kToken, "def"},
      "ghi",
  };
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  const std::set<LintViolation> found_violations{
      {{kToken, bad_text}, "some reason"},
  };
  std::ostringstream diffstream;
  EXPECT_TRUE(
      test.ExactMatchFindings(found_violations, text_view, &diffstream));
  EXPECT_TRUE(diffstream.str().empty());
}

TEST(LintTestCaseExactMatchFindingsTest, MultipleMatchingViolations) {
  constexpr int kToken = 42;
  const LintTestCase test{
      "abc",
      {kToken, "def"},
      "ghi",
      {kToken, "jkl"},
  };
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text1 = text_view.substr(3, 3);
  const absl::string_view bad_text2 = text_view.substr(9, 3);
  const std::set<LintViolation> found_violations{
      // must be sorted on location
      {{kToken, bad_text1}, "some reason"},
      {{kToken, bad_text2}, "different reason"},
  };
  std::ostringstream diffstream;
  EXPECT_TRUE(
      test.ExactMatchFindings(found_violations, text_view, &diffstream));
  EXPECT_TRUE(diffstream.str().empty());
}

constexpr absl::string_view kFoundNotExpectedMessage(
    "found violations did not match the expected");
constexpr absl::string_view kExpectedNotFoundMessage(
    "expected violations did not match the ones found");

TEST(LintTestCaseExactMatchFindingsTest, OneFoundNotExpected) {
  constexpr int kToken = 42;
  const LintTestCase test{"abcdefghi"};  // no expected violations
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  const std::set<LintViolation> found_violations{
      {{kToken, bad_text}, "some reason"},
  };
  std::ostringstream diffstream;
  EXPECT_FALSE(
      test.ExactMatchFindings(found_violations, text_view, &diffstream));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kFoundNotExpectedMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), bad_text));
  EXPECT_FALSE(absl::StrContains(diffstream.str(), kExpectedNotFoundMessage));
}

TEST(LintTestCaseExactMatchFindingsTest, OneExpectedNotFound) {
  constexpr int kToken = 42;
  const LintTestCase test{"abc", {kToken, "def"}, "ghi"};
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(3, 3);
  const std::set<LintViolation> found_violations;  // none expected
  std::ostringstream diffstream;
  EXPECT_FALSE(
      test.ExactMatchFindings(found_violations, text_view, &diffstream));
  EXPECT_FALSE(absl::StrContains(diffstream.str(), kFoundNotExpectedMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), bad_text));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kExpectedNotFoundMessage));
}

TEST(LintTestCaseExactMatchFindingsTest, OneMismatchEach) {
  constexpr int kToken = 42;
  const LintTestCase test{"abc", {kToken, "def"}, "ghi"};
  const std::string text_copy(test.code);
  const absl::string_view text_view(text_copy);

  // string buffers are in different memory
  EXPECT_FALSE(BoundsEqual(absl::string_view(test.code), text_view));

  const absl::string_view bad_text = text_view.substr(4, 3);  // "efg"
  const std::set<LintViolation> found_violations{
      {{kToken, bad_text}, "some reason"},
  };
  std::ostringstream diffstream;
  EXPECT_FALSE(
      test.ExactMatchFindings(found_violations, text_view, &diffstream));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kFoundNotExpectedMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), bad_text));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), kExpectedNotFoundMessage));
  EXPECT_TRUE(absl::StrContains(diffstream.str(), text_view.substr(3, 3)));
}

}  // namespace
}  // namespace verible
