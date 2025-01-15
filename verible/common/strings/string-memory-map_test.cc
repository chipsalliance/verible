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

#include "verible/common/strings/string-memory-map.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "verible/common/strings/range.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

static void ForAllSubstringRanges(
    std::string_view sv, const std::function<void(std::string_view)> &func) {
  for (auto start_iter = sv.begin(); start_iter != sv.end(); ++start_iter) {
    for (auto end_iter = start_iter; end_iter != sv.end(); ++end_iter) {
      if (start_iter == end_iter) continue;  // skip empty ranges
      func(make_string_view_range(start_iter, end_iter));
    }
  }
}

TEST(StringViewSuperRangeMapTest, Empty) {
  StringViewSuperRangeMap svmap;
  EXPECT_TRUE(svmap.empty());
}

TEST(StringViewSuperRangeMapTest, OneString) {
  StringViewSuperRangeMap svmap;
  constexpr std::string_view text("text");
  const auto new_iter = svmap.must_emplace(text);
  EXPECT_NE(new_iter, svmap.end());
  EXPECT_FALSE(svmap.empty());
  EXPECT_TRUE(BoundsEqual(
      make_string_view_range(new_iter->first, new_iter->second), text));
  EXPECT_TRUE(BoundsEqual(svmap.must_find(text), text));

  ForAllSubstringRanges(text, [&svmap, text](std::string_view subrange) {
    EXPECT_TRUE(BoundsEqual(svmap.must_find(subrange), text));
  });
}

TEST(StringViewSuperRangeMapTest, Overlap) {
  StringViewSuperRangeMap svmap;
  constexpr std::string_view text("text");
  svmap.must_emplace(text);
  EXPECT_DEATH(svmap.must_emplace(text), "Failed to emplace");
}

TEST(StringViewSuperRangeMapTest, OverlapSubstring) {
  StringViewSuperRangeMap svmap;
  constexpr std::string_view text("text");
  svmap.must_emplace(text);
  EXPECT_DEATH(svmap.must_emplace(text.substr(1)), "Failed to emplace");
}

TEST(StringViewSuperRangeMapTest, SuperRangeNotInSet) {
  StringViewSuperRangeMap svmap;
  constexpr std::string_view text("text");
  svmap.must_emplace(text);
  EXPECT_DEATH(svmap.must_find(std::string_view("never-there")), "");
}

TEST(StringViewSuperRangeMapTest, TwoStrings) {
  StringViewSuperRangeMap svmap;
  constexpr std::string_view text1("hello"), text2("world");
  {
    const auto new_iter = svmap.must_emplace(text1);
    EXPECT_NE(new_iter, svmap.end());
    EXPECT_FALSE(svmap.empty());
    EXPECT_TRUE(BoundsEqual(
        make_string_view_range(new_iter->first, new_iter->second), text1));
    EXPECT_TRUE(BoundsEqual(svmap.must_find(text1), text1));
  }
  {
    const auto new_iter = svmap.must_emplace(text2);
    EXPECT_NE(new_iter, svmap.end());
    EXPECT_FALSE(svmap.empty());
    EXPECT_TRUE(BoundsEqual(
        make_string_view_range(new_iter->first, new_iter->second), text2));
    EXPECT_TRUE(BoundsEqual(svmap.must_find(text2), text2));
  }

  ForAllSubstringRanges(text1, [&svmap, text1](std::string_view subrange) {
    EXPECT_TRUE(BoundsEqual(svmap.must_find(subrange), text1));
  });
  ForAllSubstringRanges(text2, [&svmap, text2](std::string_view subrange) {
    EXPECT_TRUE(BoundsEqual(svmap.must_find(subrange), text2));
  });
}

TEST(StringViewSuperRangeMapTest, EraseString) {
  constexpr std::string_view text1("onestring");
  constexpr std::string_view text2("another");
  StringViewSuperRangeMap svmap;
  svmap.must_emplace(text1);
  svmap.must_emplace(text2);

  auto found = svmap.find(text1);
  EXPECT_NE(found, svmap.end());
  svmap.erase(found);

  // should be gone now
  found = svmap.find(text1);
  EXPECT_EQ(found, svmap.end());

  found = svmap.find(text2);
  EXPECT_NE(found, svmap.end());
  svmap.erase(found);

  EXPECT_TRUE(svmap.empty());
}

// Function to get the owned address range of the underlying string.
static std::string_view StringViewKey(
    const std::unique_ptr<const std::string> &owned) {
  return std::string_view(*ABSL_DIE_IF_NULL(owned));
}

using StringSet =
    StringMemoryMap<std::unique_ptr<const std::string>, StringViewKey>;

TEST(StringMemoryMapTest, EmptyOwnsNothing) {
  const StringSet sset;
  EXPECT_EQ(sset.find("not-owned-anywhere"), nullptr);
}

static std::string_view InsertStringCopy(StringSet *sset, const char *text) {
  const auto new_iter =
      sset->insert(std::make_unique<std::string>(text));  // copy
  return make_string_view_range(new_iter->first.first, new_iter->first.second);
}

TEST(StringMemoryMapTest, OneElement) {
  StringSet sset;
  const std::string_view sv(InsertStringCopy(&sset, "OWNED"));

  // Check all valid substring ranges
  ForAllSubstringRanges(sv, [&sset, sv](std::string_view subrange) {
    const auto *f = sset.find(subrange);
    ASSERT_NE(f, nullptr) << "subrange returned nullptr: " << subrange;
    const std::string_view fv(**f);
    EXPECT_TRUE(BoundsEqual(fv, sv)) << "got: " << fv << " vs. " << sv;
    EXPECT_EQ(fv, "OWNED");
  });
}

TEST(StringMemoryMapTest, MultipleElements) {
  StringSet sset;
  // There's no telling where these allocated strings will reside in memory.
  const std::string_view sv1(InsertStringCopy(&sset, "AAA"));
  const std::string_view sv2(InsertStringCopy(&sset, "BBBB"));
  const std::string_view sv3(InsertStringCopy(&sset, "CCCCC"));

  // Check all valid substring ranges
  ForAllSubstringRanges(sv1, [&sset, sv1](std::string_view subrange) {
    const auto *f = sset.find(subrange);
    ASSERT_NE(f, nullptr) << "subrange returned nullptr: " << subrange;
    const std::string_view fv(*ABSL_DIE_IF_NULL(*f));
    EXPECT_TRUE(BoundsEqual(fv, sv1)) << "got: " << fv << " vs. " << sv1;
    EXPECT_EQ(fv, "AAA");
  });
  ForAllSubstringRanges(sv2, [&sset, sv2](std::string_view subrange) {
    const auto *f = sset.find(subrange);
    ASSERT_NE(f, nullptr) << "subrange returned nullptr: " << subrange;
    const std::string_view fv(*ABSL_DIE_IF_NULL(*f));
    EXPECT_TRUE(BoundsEqual(fv, sv2)) << "got: " << fv << " vs. " << sv2;
    EXPECT_EQ(fv, "BBBB");
  });
  ForAllSubstringRanges(sv3, [&sset, sv3](std::string_view subrange) {
    const auto *f = sset.find(subrange);
    ASSERT_NE(f, nullptr) << "subrange returned nullptr: " << subrange;
    const std::string_view fv(*ABSL_DIE_IF_NULL(*f));
    EXPECT_TRUE(BoundsEqual(fv, sv3)) << "got: " << fv << " vs. " << sv3;
    EXPECT_EQ(fv, "CCCCC");
  });
}

}  // namespace
}  // namespace verible
