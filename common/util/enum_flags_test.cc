// Copyright 2017-2019 The Verible Authors.
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

#include "common/util/enum_flags.h"

#include <initializer_list>
#include <iostream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "common/util/enum_flags_test_util.h"

namespace verible {
namespace {

enum class MyFakeEnum {
  kValue1,
  kValue2,
  kValue3,
};

// This mapping defines how this enum is displayed and parsed.
static const std::initializer_list<
    std::pair<const absl::string_view, MyFakeEnum>>
    kMyFakeEnumStringMap = {
        {"value1", MyFakeEnum::kValue1},
        {"value2", MyFakeEnum::kValue2},
        {"value3", MyFakeEnum::kValue3},
        // etc.
};

// Conventional stream printer (declared in header providing enum).
std::ostream& operator<<(std::ostream& stream, MyFakeEnum p) {
  static const auto* flag_map = MakeEnumToStringMap(kMyFakeEnumStringMap);
  return stream << flag_map->find(p)->second;
}

bool AbslParseFlag(absl::string_view text, MyFakeEnum* mode,
                   std::string* error) {
  static const auto* flag_map = MakeStringToEnumMap(kMyFakeEnumStringMap);
  return EnumMapParseFlag(*flag_map, text, mode, error);
}

std::string AbslUnparseFlag(const MyFakeEnum& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

TEST(EnumFlagsTest, ParseFlagValidValues) {
  EnumFlagsParseValidValuesTester<MyFakeEnum>(kMyFakeEnumStringMap);
}

TEST(EnumFlagsTest, ParseFlagTestInvalidValue) {
  EnumFlagsParseInvalidValuesTester<MyFakeEnum>(kMyFakeEnumStringMap, "value4");
}

TEST(EnumFlagsTest, UnparseFlags) {
  EnumFlagsUnparseFlagsTester<MyFakeEnum>(kMyFakeEnumStringMap);
}

}  // namespace
}  // namespace verible
