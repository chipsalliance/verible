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

#include "verible/common/formatting/verification.h"

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "gtest/gtest.h"
#include "verible/common/strings/position.h"

namespace verible {
namespace {

TEST(ReformatMustMatch, ReformatDifferent) {
  const LineNumberSet lines;
  const auto status =
      ReformatMustMatch("foo  bar ;\n", lines, "foo bar;\n", "foo  bar;\n");
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kDataLoss);
  EXPECT_TRUE(absl::StrContains(status.message(),
                                "============= Re-formatted: ============\n"
                                "foo  bar;\n"));
}

TEST(ReformatMustMatch, ReformatSame) {
  const LineNumberSet lines;
  const auto status =
      ReformatMustMatch("foo  bar ;\n", lines, "foo bar;\n", "foo bar;\n");
  EXPECT_TRUE(status.ok());
}

}  // namespace
}  // namespace verible
