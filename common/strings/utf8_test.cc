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
#include "common/strings/utf8.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(UTF8Util, Utf8LenTest) {
  EXPECT_EQ(utf8_len(""), 0);
  EXPECT_EQ(utf8_len("regular ASCII"), 13);
  EXPECT_EQ(utf8_len("\n\r\t \v"), 5);
  EXPECT_EQ(utf8_len("¯"), 1);  // two byte encoding
  EXPECT_EQ(utf8_len("‱"), 1);  // three byte encoding
  EXPECT_EQ(utf8_len("𝅘𝅥𝅯"), 1);  // four byte encoding
  EXPECT_EQ(utf8_len("Heizölrückstoßabdämpfung"), 24);
  EXPECT_EQ(utf8_len(R"(¯\_(ツ)_/¯)"), 9);
}

}  // namespace
}  // namespace verible
