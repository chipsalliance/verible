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
#include "verible/common/strings/utf8.h"

#include <cstring>

#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(UTF8Util, Utf8LenTest) {
  EXPECT_EQ(utf8_len(""), 0);
  EXPECT_EQ(utf8_len("regular ASCII"), 13);
  EXPECT_EQ(utf8_len("\n\r\t \v"), 5);

  EXPECT_EQ(strlen("¯"), 2);  // two byte encoding
  EXPECT_EQ(utf8_len("¯¯"), 2);

  EXPECT_EQ(strlen("ä"), 2);
  EXPECT_EQ(utf8_len("ää"), 2);

  EXPECT_EQ(strlen("‱"), 3);  // three byte encoding
  EXPECT_EQ(utf8_len("‱‱"), 2);

  EXPECT_EQ(strlen("😀"), 4);  // four byte encoding`
  EXPECT_EQ(utf8_len("😀😀"), 2);

  // Something practical
  EXPECT_EQ(utf8_len("Heizölrückstoßabdämpfung"), 24);
  EXPECT_EQ(utf8_len(R"(¯\_(ツ)_/¯)"), 9);
}

TEST(UTF8Util, Utf8SubstrPrefixTest) {
  EXPECT_EQ(utf8_substr("ä", 0), "ä");
  EXPECT_EQ(utf8_substr("ä", 1), "");

  // Can deal with regular characters
  EXPECT_EQ(utf8_substr("abc", 0), "abc");
  EXPECT_EQ(utf8_substr("abc", 1), "bc");
  EXPECT_EQ(utf8_substr("abc", 2), "c");
  EXPECT_EQ(utf8_substr("abc", 3), "");
  EXPECT_EQ(utf8_substr("abc", 42), "");  // Graceful handling of overlength

  // Two byte encoding
  EXPECT_EQ(utf8_substr("äöü", 0), "äöü");
  EXPECT_EQ(utf8_substr("äöü", 1), "öü");
  EXPECT_EQ(utf8_substr("äöü", 2), "ü");
  EXPECT_EQ(utf8_substr("äöü", 3), "");
  EXPECT_EQ(utf8_substr("äöü", 42), "");
  EXPECT_EQ(utf8_substr("¯¯¯", 1), "¯¯");

  // Three byte encoding
  EXPECT_EQ(utf8_substr("‱‱‱", 0), "‱‱‱");
  EXPECT_EQ(utf8_substr("‱‱‱", 1), "‱‱");
  EXPECT_EQ(utf8_substr("‱‱‱", 2), "‱");
  EXPECT_EQ(utf8_substr("‱‱‱", 3), "");
  EXPECT_EQ(utf8_substr("‱‱‱", 42), "");

  // Four byte encoding
  EXPECT_EQ(utf8_substr("😀🙂😐", 0), "😀🙂😐");
  EXPECT_EQ(utf8_substr("😀🙂😐", 1), "🙂😐");
  EXPECT_EQ(utf8_substr("😀🙂😐", 2), "😐");
  EXPECT_EQ(utf8_substr("😀🙂😐", 3), "");
  EXPECT_EQ(utf8_substr("😀🙂😐", 42), "");

  EXPECT_EQ(utf8_substr("Heizölrückstoßabdämpfung", 14), "abdämpfung");
}

TEST(UTF8Util, Utf8SubstrRangeTest) {
  // Can deal with regular characters
  EXPECT_EQ(utf8_substr("abc", 1, 1), "b");
  EXPECT_EQ(utf8_substr("abc", 1, 2), "bc");
  EXPECT_EQ(utf8_substr("abc", 42, 2), "");  // Graceful handling of overlength

  EXPECT_EQ(utf8_substr("äöü", 1, 1), "ö");
  EXPECT_EQ(utf8_substr("äöü", 1, 2), "öü");

  EXPECT_EQ(utf8_substr("😀‱ü", 0, 1), "😀");
  EXPECT_EQ(utf8_substr("😀‱ü", 1, 1), "‱");
  EXPECT_EQ(utf8_substr("😀‱ü", 2, 1), "ü");

  EXPECT_EQ(utf8_substr("Heizölrückstoßabdämpfung", 0, 6), "Heizöl");
  EXPECT_EQ(utf8_substr("Heizölrückstoßabdämpfung", 6, 8), "rückstoß");
  EXPECT_EQ(utf8_substr("Heizölrückstoßabdämpfung", 14, 10), "abdämpfung");
}
}  // namespace
}  // namespace verible
