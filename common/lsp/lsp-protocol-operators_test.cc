// Copyright 2021 The Verible Authors.
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

#include "common/lsp/lsp-protocol-operators.h"

#include "gtest/gtest.h"

namespace verible {
namespace lsp {
TEST(LspPositionTest, BasicOperatorsLessThanGreaterEqual) {
  constexpr Position lowerLine = {.line = 32, .character = 0};
  constexpr Position higherLine = {.line = 42, .character = 0};
  EXPECT_LT(lowerLine, higherLine);
  EXPECT_GE(higherLine, lowerLine);

  constexpr Position lowerChar = {.line = 32, .character = 7};
  constexpr Position higherChar = {.line = 32, .character = 8};
  EXPECT_LT(lowerChar, higherChar);
  EXPECT_GE(higherChar, lowerChar);
}

TEST(LspPositionTest, InsideRangeNested) {
  constexpr Range large_range = {
      .start = {.line = 10, .character = 1},
      .end = {.line = 20, .character = 1},
  };
  constexpr Range inside_large = {
      .start = {.line = 12, .character = 1},
      .end = {.line = 18, .character = 1},
  };

  // One range solidly within the other one
  EXPECT_TRUE(rangeOverlap(large_range, inside_large));
  EXPECT_TRUE(rangeOverlap(inside_large, large_range));

  // Also self-overlapping.
  EXPECT_TRUE(rangeOverlap(inside_large, inside_large));
  EXPECT_TRUE(rangeOverlap(large_range, large_range));
}

TEST(LspPositionTest, InsideRangeOverlapAtEnd) {
  constexpr Range large_range = {
      .start = {.line = 10, .character = 1},
      .end = {.line = 20, .character = 1},
  };

  // Overlaps the large range at the end range
  constexpr Range overlap_at_end = {
      .start = {.line = 15, .character = 1},
      .end = {.line = 25, .character = 1},
  };
  EXPECT_TRUE(rangeOverlap(large_range, overlap_at_end));
  EXPECT_TRUE(rangeOverlap(overlap_at_end, large_range));
}

TEST(LspPositionTest, InsideRangeOverlapUpperEndEdge) {
  // Overlap right at the upper end.
  constexpr Range large_range = {
      .start = {.line = 10, .character = 1},
      .end = {.line = 20, .character = 1},
  };

  constexpr Range overlap_at_edge = {
      .start = {.line = 20, .character = 0},
      .end = {.line = 25, .character = 1},
  };
  EXPECT_TRUE(rangeOverlap(overlap_at_edge, large_range));
  EXPECT_TRUE(rangeOverlap(large_range, overlap_at_edge));
}

TEST(LspPositionTest, OutsideRangeNoOverlapAtUpperEnd) {
  constexpr Range large_range = {
      .start = {.line = 10, .character = 1},
      .end = {.line = 20, .character = 1},  // This marks the char beyond end
  };

  // The end range is one character beyond the actual range. So if we
  // start at that character (chara 1 at line 20), we're outside.
  constexpr Range just_outside_at_edge = {
      .start = {.line = 20, .character = 1},  // This starts at the beyond other
      .end = {.line = 25, .character = 1},
  };
  EXPECT_FALSE(rangeOverlap(just_outside_at_edge, large_range));
  EXPECT_FALSE(rangeOverlap(large_range, just_outside_at_edge));
}

TEST(LspPositionTest, CompletelyOutsideRange) {
  constexpr Range large_range = {
      .start = {.line = 10, .character = 1},
      .end = {.line = 20, .character = 1},  // This marks the char beyond end
  };

  // Solidly outside range.
  constexpr Range outside_range = {
      .start = {.line = 30, .character = 1},
      .end = {.line = 35, .character = 1},
  };
  EXPECT_FALSE(rangeOverlap(outside_range, large_range));
  EXPECT_FALSE(rangeOverlap(large_range, outside_range));
}
}  // namespace lsp
}  // namespace verible
