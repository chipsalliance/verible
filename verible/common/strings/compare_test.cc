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

#include "verible/common/strings/compare.h"

#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

using testing::ElementsAre;
using testing::Pair;

TEST(StringViewCompareTest, ConstCharPointers) {
  const char a[] = "aaa";
  const char b[] = "bbb";
  StringViewCompare comp;
  EXPECT_TRUE(comp(a, b));
  EXPECT_FALSE(comp(b, a));
}

TEST(StringViewCompareTest, StringViews) {
  const absl::string_view a("aaa");
  const absl::string_view b("bbb");
  StringViewCompare comp;
  EXPECT_TRUE(comp(a, b));
  EXPECT_FALSE(comp(b, a));
}

TEST(StringViewCompareTest, StdStrings) {
  const std::string a("aaa");
  const std::string b("bbb");
  StringViewCompare comp;
  EXPECT_TRUE(comp(a, b));
  EXPECT_FALSE(comp(b, a));
}

TEST(StringViewCompareTest, HeterogeneousStrings) {
  const char a[] = "aaa";
  const std::string b("bbb");
  const absl::string_view c("ccc");
  StringViewCompare comp;
  EXPECT_TRUE(comp(a, b));
  EXPECT_FALSE(comp(b, a));
  EXPECT_TRUE(comp(b, c));
  EXPECT_FALSE(comp(c, b));
  EXPECT_TRUE(comp(a, c));
  EXPECT_FALSE(comp(c, a));
}

TEST(StringViewCompareTest, MapStdStringKey) {
  const std::map<std::string, int, StringViewCompare> numbers{
      {"one", 1},
      {"two", 2},
      {"three", 3},
      {"four", 4},
  };
  EXPECT_THAT(numbers,  // alphabetically ordered by key name
              ElementsAre(Pair("four", 4), Pair("one", 1), Pair("three", 3),
                          Pair("two", 2)));
}

TEST(StringViewCompareTest, MapStringViewKey) {
  const std::map<absl::string_view, int, StringViewCompare> numbers{
      {"one", 1},
      {"two", 2},
      {"three", 3},
      {"four", 4},
  };
  EXPECT_THAT(numbers,  // alphabetically ordered by key name
              ElementsAre(Pair("four", 4), Pair("one", 1), Pair("three", 3),
                          Pair("two", 2)));
}

}  // namespace
}  // namespace verible
