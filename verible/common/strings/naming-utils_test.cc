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

#include "verible/common/strings/naming-utils.h"

#include "gtest/gtest.h"

namespace verible {
namespace {

// Tests that all strings containing more than just upper case letters,
// underscores, and digits are rejected.
TEST(IsNameAllCapsUnderscoresDigitsTest, RejectTests) {
  static const char *test_cases[] = {
      "foo",  "HAS A SPACE", "NOT_ALL_UPPERCASe", "MiXeD",
      "FO?O", "lower",       "lower_underscore",
  };
  for (const auto data : test_cases) {
    EXPECT_FALSE(IsNameAllCapsUnderscoresDigits(data));
  }
}

// Tests that all strings containing only upper case letters, underscores, and
// digits are accepted.
TEST(IsNameAllCapsUnderscoresDigitsTest, AcceptTests) {
  static const char *test_cases[] = {
      "",    "FOO",       "HAS_UNDERSCORE", "_UNDERSCORE_AT_BEGINNING",
      "___", "__UPPER__", "_123",           "FOO_1",
  };
  for (const auto data : test_cases) {
    EXPECT_TRUE(IsNameAllCapsUnderscoresDigits(data));
  }
}

// Tests that all strings containing underscores not next to digits are
// rejected.
TEST(AllUnderscoresFollowedByDigitsTest, RejectTests) {
  static const char *test_cases[] = {
      "Hello_World", "_H1", "Hello_",   "1_",           "Foo1_Bar",
      "Fo_1_o",      "_",   "Hello__1", "Hello__World", "__1",
  };
  for (const auto data : test_cases) {
    EXPECT_FALSE(AllUnderscoresFollowedByDigits(data));
  }
}

// Tests that all strings containing underscores next to a digit are accepted.
TEST(AllUnderscoresFollowedByDigitsTest, AcceptTests) {
  static const char *test_cases[] = {
      "",
      "Foo_1",
      "Foo_1_1",
      "_1",
  };
  for (const auto data : test_cases) {
    EXPECT_TRUE(AllUnderscoresFollowedByDigits(data));
  }
}

// Tests that all strings not following UpperCamelCase naming convention are
// rejected.
TEST(IsUpperCamelCaseWithDigitsTest, RejectTests) {
  static const char *test_cases[] = {
      "Hello_World", "hello_world", "helloworld", "helloWorld", "Hello_1_World",
      "Hello__1",    "Foo_",        "_Foo",       "_1",         "__Foo",
  };
  for (const auto data : test_cases) {
    EXPECT_FALSE(IsUpperCamelCaseWithDigits(data));
  }
}

// Tests that all strings following UpperCamelCase naming convention are
// accepted.
TEST(IsUpperCamelCaseWithDigitsTest, AcceptTests) {
  static const char *test_cases[] = {
      "",
      "HelloWorld",
      "Foo_1",
      "HelloWorld_1",
      "HelloWorld_1_1",
      "HelloWorld1",
      "ABCWorld",
  };
  for (const auto data : test_cases) {
    EXPECT_TRUE(IsUpperCamelCaseWithDigits(data));
  }
}

// Tests that all strings following lower_snake_case naming convention are
// accepted.
TEST(IsLowerSnakeCaseWithDigitsTest, AcceptTests) {
  static const char *test_cases[] = {
      "",       "hello_world", "hello_1_world", "hello_1", "hello_world_1_1",
      "hello1", "hello_",
  };
  for (const auto data : test_cases) {
    EXPECT_TRUE(IsLowerSnakeCaseWithDigits(data));
  }
}

// Tests that all strings not following lower_snake_case naming convention are
// rejected.
TEST(IsLowerSnakeCaseWithDigitsTest, RejectTests) {
  static const char *test_cases[] = {
      "_1",       "__hello",    "Hello_world", "hello_1_World",
      "hello_1W", "HelloWorld", "helLo1_",
  };
  for (const auto data : test_cases) {
    EXPECT_FALSE(IsLowerSnakeCaseWithDigits(data));
  }
}

}  // namespace
}  // namespace verible
