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

#ifndef VERIBLE_COMMON_UTIL_ENUM_FLAGS_TEST_UTIL_H_
#define VERIBLE_COMMON_UTIL_ENUM_FLAGS_TEST_UTIL_H_

#include <initializer_list>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace verible {

// Tests valid enum/name pair lookups.
template <class E>
void EnumFlagsParseValidValuesTester(
    std::initializer_list<std::pair<const absl::string_view, E>> test_pairs) {
  for (const auto& p : test_pairs) {
    E e;
    std::string error;
    EXPECT_TRUE(AbslParseFlag(p.first, &e, &error)) << " parsing " << p.first;
    EXPECT_EQ(e, p.second) << " from " << p.first;
    EXPECT_TRUE(error.empty()) << " from " << p.first;
  }
}

// Tests invalid enum name lookup.
template <class E>
void EnumFlagsParseInvalidValuesTester(
    std::initializer_list<std::pair<const absl::string_view, E>> test_pairs,
    absl::string_view bad_value) {
  E e;
  std::string error;
  EXPECT_FALSE(AbslParseFlag(bad_value, &e, &error));
  // Make sure error message names the offending value, and lists all valid
  // values (map keys).
  EXPECT_TRUE(absl::StrContains(error, bad_value));
  for (const auto& p : test_pairs) {
    EXPECT_TRUE(absl::StrContains(error, p.first));
  }
}

// Tests enum to name (reverse) mapping.
template <class E>
void EnumFlagsUnparseFlagsTester(
    std::initializer_list<std::pair<const absl::string_view, E>> test_pairs) {
  for (const auto& p : test_pairs) {
    EXPECT_EQ(AbslUnparseFlag(p.second), p.first);
  }
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_ENUM_FLAGS_TEST_UTIL_H_
