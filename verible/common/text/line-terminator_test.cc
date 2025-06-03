// Copyright 2025 The Verible Authors.
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

// Test MacroDefinition and its supporting structs.

#include "verible/common/text/line-terminator.h"

#include <ostream>

#include "gtest/gtest.h"

namespace verible {
static std::ostream &operator<<(std::ostream &out, LineTerminatorStyle lt) {
  switch (lt) {
    case LineTerminatorStyle::kLF:
      out << "Linefeed";
      break;
    case LineTerminatorStyle::kCRLF:
      out << "CarriageReturn-Linefeed";
      break;
  }
  return out;
}

TEST(LineTerminatorTest, ProperLineGuessing) {
  EXPECT_EQ(LineTerminatorStyle::kLF, GuessLineTerminator("", 10));
  EXPECT_EQ(LineTerminatorStyle::kLF, GuessLineTerminator("\n", 10));
  EXPECT_EQ(LineTerminatorStyle::kCRLF, GuessLineTerminator("\r\n", 10));

  // Majority vote
  EXPECT_EQ(LineTerminatorStyle::kLF, GuessLineTerminator("\r\n\n\n", 10));
  EXPECT_EQ(LineTerminatorStyle::kCRLF, GuessLineTerminator("\r\n\r\n\n", 10));

  // Only looking at some of the lines
  EXPECT_EQ(LineTerminatorStyle::kCRLF, GuessLineTerminator("\r\n\n\n", 1));

  // On break-even, LF is chosen
  EXPECT_EQ(LineTerminatorStyle::kLF, GuessLineTerminator("\r\n\n", 10));
  EXPECT_EQ(LineTerminatorStyle::kLF, GuessLineTerminator("\n\r\n", 10));
}
}  // namespace verible
