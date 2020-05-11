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

#include "common/strings/display_utils.h"

#include <sstream>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

struct TruncateTestCase {
  absl::string_view input;
  int max_chars;
  absl::string_view expected;
};

TEST(AutoTruncateTest, Various) {
  constexpr TruncateTestCase kTestCases[] = {
      {"abcde", 9, "abcde"},
      {"abcdef", 9, "abcdef"},
      {"abcdefg", 9, "abcdefg"},
      {"abcdefgh", 9, "abcdefgh"},
      {"abcdefghi", 9, "abcdefghi"},
      {"abcdefghij", 9, "abc...hij"},
      {"abcdefghijk", 9, "abc...ijk"},
      {"abcdefghijk", 10, "abcd...ijk"},  // more head than tail
      {"123!(@*#&)!#$!@#(*xyz", 9, "123...xyz"},
      {"123!(@*#&)!#$!@#(*xyz", 10, "123!...xyz"},
      {"123!(@*#&)!#$!@#(*xyz", 11, "123!...*xyz"},
      {"123!(@*#&)!#$!@#(*xyz", 12, "123!(...*xyz"},
  };
  for (const auto& test : kTestCases) {
    std::ostringstream stream;
    stream << AutoTruncate{test.input, test.max_chars};
    EXPECT_EQ(stream.str(), test.expected);
  }
}

}  // namespace
}  // namespace verible
