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

#include "verible/common/util/with-reason.h"

#include "gtest/gtest.h"

namespace verible {
namespace {

// Example of a priority-ordered function.
static WithReason<const char *> FizzBuzzer(int i) {
  if (i % 3 == 0) {
    if (i % 5 == 0) {
      return {"fizzbuzz", "value is divisible by 3 and 5."};
    }
    return {"fizz", "value is only divisible by 3."};
  }

  if (i % 5 == 0) return {"buzz", "value is only divisible by 5."};

  return {".", "value is neither divisible by 3 nor 5."};
}

TEST(WithReason, Fizz) {
  const auto result = FizzBuzzer(6);
  EXPECT_STREQ(result.value, "fizz") << result.value;
  EXPECT_STREQ(result.reason, "value is only divisible by 3.") << result.reason;
}

TEST(WithReason, Buzz) {
  const auto result = FizzBuzzer(10);
  EXPECT_STREQ(result.value, "buzz");
  EXPECT_STREQ(result.reason, "value is only divisible by 5.");
}

TEST(WithReason, Neither) {
  const auto result = FizzBuzzer(16);
  EXPECT_STREQ(result.value, ".");
  EXPECT_STREQ(result.reason, "value is neither divisible by 3 nor 5.");
}

TEST(WithReason, Both) {
  const auto result = FizzBuzzer(30);
  EXPECT_STREQ(result.value, "fizzbuzz");
  EXPECT_STREQ(result.reason, "value is divisible by 3 and 5.");
}

}  // namespace
}  // namespace verible
