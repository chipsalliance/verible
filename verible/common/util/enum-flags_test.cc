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

#include "verible/common/util/enum-flags.h"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "absl/strings/match.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

enum class MyFakeEnum {
  kValue1,
  kValue2,
  kValue3,
  kValue4,
};

class TestMapType : public EnumNameMap<MyFakeEnum> {
 public:
  TestMapType()
      : EnumNameMap({
            // This mapping defines how this enum is displayed and parsed.
            {"value1", MyFakeEnum::kValue1},
            {"value2", MyFakeEnum::kValue2},
            {"value3", MyFakeEnum::kValue3},
            // intentionally omitting kValue4
            // etc.
        }) {}
};

class EnumNameMapTest : public ::testing::Test, public TestMapType {
 public:
  EnumNameMapTest() = default;
};

static const TestMapType test_map;

// Conventional stream printer (declared in header providing enum).
std::ostream &operator<<(std::ostream &stream, MyFakeEnum p) {
  return test_map.Unparse(p, stream);
}

// Testing using the absl::flags API, but we're only testing this particular
// overload, and thus, don't actually need to depend on their library.

bool AbslParseFlag(std::string_view text, MyFakeEnum *mode,
                   std::string *error) {
  return test_map.Parse(text, mode, error, "MyFakeEnum");
}

std::string AbslUnparseFlag(const MyFakeEnum &mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

TEST_F(EnumNameMapTest, ParseFlagValueValues) {
  for (const auto &p : enum_name_map_.forward_view()) {
    MyFakeEnum e = MyFakeEnum::kValue1;
    std::string error;
    EXPECT_TRUE(AbslParseFlag(p.first, &e, &error)) << " parsing " << p.first;
    EXPECT_EQ(e, *p.second) << " from " << p.first;
    EXPECT_TRUE(error.empty()) << " from " << p.first;
  }
}

TEST_F(EnumNameMapTest, ParseFlagTestInvalidValue) {
  MyFakeEnum e;
  std::string error;
  constexpr std::string_view bad_value("invalidEnumName");
  EXPECT_FALSE(AbslParseFlag(bad_value, &e, &error));
  // Make sure error message names the offending value, and lists all valid
  // values (map keys).
  EXPECT_TRUE(absl::StrContains(error, bad_value));
  for (const auto &p : enum_name_map_.forward_view()) {
    EXPECT_TRUE(absl::StrContains(error, p.first));
  }
}

TEST_F(EnumNameMapTest, UnparseFlags) {
  for (const auto &p : enum_name_map_.forward_view()) {
    EXPECT_EQ(AbslUnparseFlag(*p.second), p.first);
  }
}

TEST_F(EnumNameMapTest, UnparseFlagsForgottenEnum) {
  // don't die, could print string like "???"
  AbslUnparseFlag(MyFakeEnum::kValue4);
}

enum class AnotherFakeEnum {
  kValueA,
  kValueB,
  kValueC,
};

std::ostream &operator<<(std::ostream &stream, AnotherFakeEnum p) {
  return stream << "!!!";  // don't care
}

TEST(DuplicateKeyTest, ExpectFail) {
  EXPECT_DEATH(const EnumNameMap<AnotherFakeEnum> m({
                   {"value1", AnotherFakeEnum::kValueA},
                   {"value2", AnotherFakeEnum::kValueB},
                   {"value1", AnotherFakeEnum::kValueC},  // duplicate key
                                                          // etc.
               }),
               "duplicate");
}

TEST(DuplicateValueTest, ExpectFail) {
  EXPECT_DEATH(const EnumNameMap<AnotherFakeEnum> m({
                   {"value1", AnotherFakeEnum::kValueA},
                   {"value2", AnotherFakeEnum::kValueB},
                   {"value3", AnotherFakeEnum::kValueB},  // duplicate value
                                                          // etc.
               }),
               "duplicate");
}

}  // namespace
}  // namespace verible
