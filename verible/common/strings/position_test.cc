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

#include "verible/common/strings/position.h"

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(AdvancingTextNewColumnPositionTest, EmptyString) {
  const absl::string_view text;
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 0);
  EXPECT_EQ(AdvancingTextNewColumnPosition(1, text), 1);
  EXPECT_EQ(AdvancingTextNewColumnPosition(8, text), 8);
}

TEST(AdvancingTextNewColumnPositionTest, OneChar) {
  const absl::string_view text("x");
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 1);
  EXPECT_EQ(AdvancingTextNewColumnPosition(1, text), 2);
  EXPECT_EQ(AdvancingTextNewColumnPosition(8, text), 9);
}

TEST(AdvancingTextNewColumnPositionTest, MultiChar) {
  const absl::string_view text("12345");
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 5);
  EXPECT_EQ(AdvancingTextNewColumnPosition(4, text), 9);
}

TEST(AdvancingTextNewColumnPositionTest, NewlineOnly) {
  const absl::string_view text("\n");
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 0);
  EXPECT_EQ(AdvancingTextNewColumnPosition(4, text), 0);
}

TEST(AdvancingTextNewColumnPositionTest, EndsWithNewline) {
  const absl::string_view text("asdfasdf\n");
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 0);
  EXPECT_EQ(AdvancingTextNewColumnPosition(77, text), 0);
}

TEST(AdvancingTextNewColumnPositionTest, StartsWithNewline) {
  const absl::string_view text("\nasdf");
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 4);
  EXPECT_EQ(AdvancingTextNewColumnPosition(7, text), 4);
}

TEST(AdvancingTextNewColumnPositionTest, MultipleNewlines) {
  const absl::string_view text("as\ndfasdf\n");
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 0);
  EXPECT_EQ(AdvancingTextNewColumnPosition(77, text), 0);
}

TEST(AdvancingTextNewColumnPositionTest, NonNewlinesAfterMultipleNewlines) {
  const absl::string_view text("as\ndfasdf\nqwerty");
  EXPECT_EQ(AdvancingTextNewColumnPosition(0, text), 6);
  EXPECT_EQ(AdvancingTextNewColumnPosition(11, text), 6);
}

}  // namespace
}  // namespace verible
