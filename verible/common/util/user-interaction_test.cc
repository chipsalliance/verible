// Copyright 2017-2021 The Verible Authors.
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

#include "verible/common/util/user-interaction.h"

#include <sstream>

#include "absl/strings/match.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(ReadCharFromUserTest, NonTerminalInput) {
  std::istringstream input("ab\nc");
  std::ostringstream output;
  char characters[6];

  for (char &c : characters) {
    c = ReadCharFromUser(input, output, false, "A prompt.");
  }

  EXPECT_EQ(characters[0], 'a');
  EXPECT_EQ(characters[1], 'b');
  EXPECT_EQ(characters[2], '\n');
  EXPECT_EQ(characters[3], 'c');
  EXPECT_EQ(characters[4], '\0');
  EXPECT_EQ(characters[5], '\0');
  EXPECT_TRUE(output.str().empty());
}

TEST(ReadCharFromUserTest, TerminalInput) {
  std::istringstream input("yes\nno\nabort");
  std::ostringstream output;
  char characters[4];

  for (char &c : characters) {
    output.str("");
    output.clear();
    c = ReadCharFromUser(input, output, true, "A prompt.");
    EXPECT_TRUE(absl::StrContains(output.str(), "A prompt."))
        << "Actual value: " << output.str();
  }

  EXPECT_EQ(characters[0], 'y');
  EXPECT_EQ(characters[1], 'n');
  EXPECT_EQ(characters[2], '\0');
  EXPECT_EQ(characters[3], '\0');
}

}  // namespace
}  // namespace verible
