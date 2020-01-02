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

#include "common/strings/range.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/util/range.h"

namespace verible {
namespace {

TEST(MakeStringViewRangeTest, Empty) {
  absl::string_view text("");
  auto copy_view = make_string_view_range(text.begin(), text.end());
  EXPECT_TRUE(BoundsEqual(copy_view, text));
}

TEST(MakeStringViewRangeTest, NonEmpty) {
  absl::string_view text("I'm not empty!!!!");
  auto copy_view = make_string_view_range(text.begin(), text.end());
  EXPECT_TRUE(BoundsEqual(copy_view, text));
}

TEST(MakeStringViewRangeTest, BadRange) {
  absl::string_view text("backwards");
  EXPECT_DEATH(make_string_view_range(text.end(), text.begin()), "Malformed");
}

}  // namespace
}  // namespace  verible
