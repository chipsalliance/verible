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

#include "verible/common/util/interval.h"

#include <sstream>

#include "gtest/gtest.h"

namespace verible {
namespace {

using interval_type = Interval<int>;

TEST(IntervalTest, IsEmpty) {
  const interval_type ii{1, 1};
  EXPECT_TRUE(ii.valid());
  EXPECT_TRUE(ii.empty());
  EXPECT_EQ(ii.length(), 0);

  for (int i = -1; i < 3; ++i) {
    EXPECT_FALSE(ii.contains(i));
    EXPECT_FALSE(ii.contains({i, i + 1}));
    EXPECT_FALSE(ii.contains(i, i + 1));
  }

  EXPECT_FALSE(ii.contains(0, 0));
  EXPECT_TRUE(ii.contains(1, 1));  // even if interval is empty
  EXPECT_FALSE(ii.contains(2, 2));

  EXPECT_FALSE(ii.contains({0, 0}));
  EXPECT_TRUE(ii.contains({1, 1}));  // even if interval is empty
  EXPECT_FALSE(ii.contains({2, 2}));

  for (int i = -1; i < 3; ++i) {
    for (int j = i; j < 3; ++j) {
      EXPECT_FALSE(ii.contains({i, j}) && i != 1 && j != 1);
      EXPECT_FALSE(ii.contains(i, j) && i != 1 && j != 1);
    }
  }
}

TEST(IntervalTest, NotEmptyLengthOne) {
  const interval_type ii{1, 2};
  EXPECT_TRUE(ii.valid());
  EXPECT_FALSE(ii.empty());
  EXPECT_EQ(ii.length(), 1);

  EXPECT_FALSE(ii.contains(0));
  EXPECT_TRUE(ii.contains(1));
  EXPECT_FALSE(ii.contains(2));

  EXPECT_FALSE(ii.contains({0, 1}));
  EXPECT_TRUE(ii.contains({1, 2}));
  EXPECT_FALSE(ii.contains({2, 3}));

  EXPECT_FALSE(ii.contains({0, 2}));
  EXPECT_FALSE(ii.contains({1, 3}));
  EXPECT_FALSE(ii.contains({0, 3}));
}

TEST(IntervalTest, NotEmptyLengthGreaterThanOne) {
  const interval_type ii{5, 8};
  EXPECT_TRUE(ii.valid());
  EXPECT_FALSE(ii.empty());
  EXPECT_EQ(ii.length(), 3);

  for (int i = 3; i < 9; ++i) {
    for (int j = i; j < 9; ++j) {
      EXPECT_EQ(ii.contains({i, j}), i >= 5 && j <= 8);
      EXPECT_EQ(ii.contains(i, j), i >= 5 && j <= 8);
    }
  }
}

TEST(IntervalTest, Invalid) {
  const interval_type ii{2, 1};
  EXPECT_FALSE(ii.valid());
  EXPECT_FALSE(ii.empty());
  // other methods considered undefined for invalid intervals
}

TEST(IntervalTest, Equality) {
  const interval_type i1{2, 2}, i2({2, 2});  // different constructors
  EXPECT_EQ(i1, i2);
  EXPECT_EQ(i2, i1);

  const interval_type i3{1, 2}, i4{2, 3}, i5{1, 3};
  EXPECT_NE(i1, i3);
  EXPECT_NE(i1, i4);
  EXPECT_NE(i1, i5);
  EXPECT_NE(i3, i1);
  EXPECT_NE(i4, i1);
  EXPECT_NE(i5, i1);
}

TEST(IntervalTest, Stream) {
  const interval_type ii{1, 2};
  std::ostringstream stream;
  stream << ii;
  EXPECT_EQ(stream.str(), "[1, 2)");
}

TEST(ParseInclusiveRangeTest, EmptyStrings) {
  interval_type interval;
  {
    std::ostringstream errstream;
    EXPECT_FALSE(ParseInclusiveRange(&interval, "", "1", &errstream));
    EXPECT_FALSE(errstream.str().empty());
  }
  {
    std::ostringstream errstream;
    EXPECT_FALSE(ParseInclusiveRange(&interval, "1", "", &errstream));
    EXPECT_FALSE(errstream.str().empty());
  }
}

TEST(FormatInclusiveRangeTest, Singleton) {
  const interval_type ii{5, 6};
  {
    std::ostringstream stream;
    ii.FormatInclusive(stream, false);
    EXPECT_EQ(stream.str(), "5-5");
  }
  {
    std::ostringstream stream;
    ii.FormatInclusive(stream, true);
    EXPECT_EQ(stream.str(), "5");
  }
}

TEST(FormatInclusiveRangeTest, LargerThanOne) {
  const interval_type ii{7, 9};
  {
    std::ostringstream stream;
    ii.FormatInclusive(stream, false);
    EXPECT_EQ(stream.str(), "7-8");
  }
  {
    std::ostringstream stream;
    ii.FormatInclusive(stream, true);
    EXPECT_EQ(stream.str(), "7-8");
  }
}

TEST(ParseInclusiveRangeTest, ValidRangeSingleValue) {
  interval_type interval;
  std::ostringstream errstream;
  EXPECT_TRUE(ParseInclusiveRange(&interval, "3", "3", &errstream));
  EXPECT_TRUE(errstream.str().empty());
  EXPECT_EQ(interval.min, 3);
  EXPECT_EQ(interval.max, 4);  // half-open
}

TEST(ParseInclusiveRangeTest, ValidRangeBigInterval) {
  interval_type interval;
  std::ostringstream errstream;
  EXPECT_TRUE(ParseInclusiveRange(&interval, "3", "30", &errstream));
  EXPECT_TRUE(errstream.str().empty());
  EXPECT_EQ(interval.min, 3);
  EXPECT_EQ(interval.max, 31);  // half-open
}

TEST(ParseInclusiveRangeTest, ValidRangeReversed) {
  interval_type interval;
  std::ostringstream errstream;
  EXPECT_TRUE(ParseInclusiveRange(&interval, "5", "3", &errstream));
  EXPECT_TRUE(errstream.str().empty());
  EXPECT_EQ(interval.min, 3);
  EXPECT_EQ(interval.max, 6);  // half-open
}

}  // namespace
}  // namespace verible
