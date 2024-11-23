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

#include "verible/common/strings/random.h"

#include <cctype>

#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(RandomAlphaCharTest, Char) {
  for (int i = 0; i < 20; ++i) {
    const char c = RandomAlphaChar();
    EXPECT_TRUE(std::isalpha(c)) << "got: " << c;
  }
}

TEST(RandomAlphaNumCharTest, Char) {
  for (int i = 0; i < 20; ++i) {
    const char c = RandomAlphaNumChar();
    EXPECT_TRUE(std::isalnum(c)) << "got: " << c;
  }
}

TEST(RandomEqualLengthIdentifierTest, EmptyInput) {
  EXPECT_DEATH(RandomEqualLengthIdentifier(""), "");
}

TEST(RandomEqualLengthIdentifierTest, OneChar) {
  for (int i = 0; i < 20; ++i) {
    const auto s = RandomEqualLengthIdentifier(".");
    EXPECT_EQ(s.length(), 1);
    EXPECT_TRUE(std::isalpha(s.front())) << "got: " << s.front();
  }
}

TEST(RandomEqualLengthIdentifierTest, TwoChar) {
  for (int i = 0; i < 80; ++i) {
    const auto s = RandomEqualLengthIdentifier("..");
    EXPECT_EQ(s.length(), 2);
    EXPECT_TRUE(std::isalpha(s.front())) << "got: " << s.front();
    EXPECT_TRUE(std::isalnum(s.back())) << "got: " << s.back();
  }
}

}  // namespace
}  // namespace verible
