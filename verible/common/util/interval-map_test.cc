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

#include "verible/common/util/interval-map.h"

#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/util/range.h"

namespace verible {
namespace {

using IntIntervalMap = DisjointIntervalMap<int, std::unique_ptr<int>>;
using StringIntervalMap =
    DisjointIntervalMap<int, std::unique_ptr<std::string>>;

TEST(DisjointIntervalMapTest, DefaultCtor) {
  const IntIntervalMap imap;
  EXPECT_TRUE(imap.empty());
}

TEST(DisjointIntervalMapTest, FindEmpty) {
  const IntIntervalMap imap;
  EXPECT_EQ(imap.find(3), imap.end());
}

TEST(DisjointIntervalMapTest, EmplaceOne) {
  IntIntervalMap imap;
  const auto p = imap.emplace({3, 4}, std::make_unique<int>(5));
  EXPECT_TRUE(p.second);
  EXPECT_EQ(p.first->first, std::make_pair(3, 4));
  EXPECT_EQ(*p.first->second, 5);
  EXPECT_FALSE(imap.empty());
}

TEST(DisjointIntervalMapTest, EmplaceOneEnsureMove) {
  StringIntervalMap imap;
  auto s = std::make_unique<std::string>("Gruetzi!");
  const absl::string_view sv(*s);
  const auto p = imap.emplace({3, 7}, std::move(s));
  EXPECT_TRUE(p.second);
  EXPECT_EQ(p.first->first, std::make_pair(3, 7));
  // ownership transferred, string_view range is still valid
  const absl::string_view new_sv(*p.first->second);
  EXPECT_TRUE(BoundsEqual(new_sv, sv)) << "got: " << new_sv << " vs. " << sv;
}

TEST(DisjointIntervalMapTest, EmplaceNonoverlappingAbutting) {
  IntIntervalMap imap;
  {
    const auto p = imap.emplace({3, 4}, std::make_unique<int>(5));
    EXPECT_TRUE(p.second);
  }
  {
    // insert new leftmost range
    const auto p = imap.emplace({1, 3}, std::make_unique<int>(9));
    EXPECT_TRUE(p.second);
  }
  {
    // insert new rightmost range
    const auto p = imap.emplace({4, 7}, std::make_unique<int>(2));
    EXPECT_TRUE(p.second);
  }

  EXPECT_EQ(imap.find(0), imap.end());
  for (int i = 1; i < 3; ++i) {
    const auto f = imap.find(i);
    ASSERT_NE(f, imap.end());
    EXPECT_EQ(*f->second, 9);
  }
  for (int i = 3; i < 4; ++i) {
    const auto f = imap.find(i);
    ASSERT_NE(f, imap.end());
    EXPECT_EQ(*f->second, 5);
  }
  for (int i = 4; i < 7; ++i) {
    const auto f = imap.find(i);
    ASSERT_NE(f, imap.end());
    EXPECT_EQ(*f->second, 2);
  }
  EXPECT_EQ(imap.find(7), imap.end());
}

TEST(DisjointIntervalMapTest, EmplaceNonoverlappingWithGaps) {
  IntIntervalMap imap;
  {
    const auto p = imap.emplace({20, 25}, std::make_unique<int>(4));
    EXPECT_TRUE(p.second);
  }
  {
    // insert new rightmost range
    const auto p = imap.emplace({30, 40}, std::make_unique<int>(2));
    EXPECT_TRUE(p.second);
  }
  {
    // insert new leftmost range
    const auto p = imap.emplace({10, 15}, std::make_unique<int>(8));
    EXPECT_TRUE(p.second);
  }

  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(imap.find(i), imap.end());
  }
  for (int i = 10; i < 15; ++i) {
    const auto f = imap.find(i);
    ASSERT_NE(f, imap.end());
    EXPECT_EQ(*f->second, 8);
  }
  for (int i = 15; i < 20; ++i) {
    EXPECT_EQ(imap.find(i), imap.end());
  }
  for (int i = 20; i < 25; ++i) {
    const auto f = imap.find(i);
    ASSERT_NE(f, imap.end());
    EXPECT_EQ(*f->second, 4);
  }
  for (int i = 25; i < 30; ++i) {
    EXPECT_EQ(imap.find(i), imap.end());
  }
  for (int i = 30; i < 40; ++i) {
    const auto f = imap.find(i);
    ASSERT_NE(f, imap.end());
    EXPECT_EQ(*f->second, 2);
  }
  EXPECT_EQ(imap.find(40), imap.end());

  {
    // fill a gap completely
    const auto p = imap.emplace({15, 20}, std::make_unique<int>(77));
    EXPECT_TRUE(p.second);
    EXPECT_EQ(*imap.find(14)->second, 8);
    for (int i = 15; i < 20; ++i) {
      const auto f = imap.find(i);
      ASSERT_NE(f, imap.end());
      EXPECT_EQ(*f->second, 77);
    }
    EXPECT_EQ(*imap.find(20)->second, 4);
  }

  {
    // fill a gap partially
    const auto p = imap.emplace({27, 29}, std::make_unique<int>(44));
    EXPECT_TRUE(p.second);
    for (int i = 25; i < 27; ++i) {
      EXPECT_EQ(imap.find(i), imap.end());
    }
    for (int i = 27; i < 29; ++i) {
      const auto f = imap.find(i);
      ASSERT_NE(f, imap.end());
      EXPECT_EQ(*f->second, 44);
    }
    for (int i = 29; i < 30; ++i) {
      EXPECT_EQ(imap.find(i), imap.end());
    }
  }
}

TEST(DisjointIntervalMapTest, EmplaceBackwardsRange) {
  IntIntervalMap imap;
  EXPECT_DEATH(imap.emplace({4, 3}, std::make_unique<int>(5)), "");
}

TEST(DisjointIntervalMapTest, MustEmplaceSuccess) {
  IntIntervalMap imap;
  constexpr std::tuple<int, int, int> kTestValues[] = {
      {3, 4, 5}, {1, 3, 9}, {4, 7, 2}, {-10, -5, 0}, {10, 15, 33},
  };
  for (const auto &t : kTestValues) {
    const auto iter = imap.must_emplace({std::get<0>(t), std::get<1>(t)},
                                        std::make_unique<int>(std::get<2>(t)));
    // Ensure that inserted value is the expected key and value.
    EXPECT_EQ(iter->first.first, std::get<0>(t));
    EXPECT_EQ(iter->first.second, std::get<1>(t));
    EXPECT_EQ(*iter->second, std::get<2>(t));
  }
}

TEST(DisjointIntervalMapTest, MustEmplaceOverlapLeft) {
  IntIntervalMap imap;
  imap.must_emplace({30, 40}, std::make_unique<int>(5));
  EXPECT_DEATH(imap.must_emplace({20, 31}, std::make_unique<int>(9)),
               "Failed to emplace");
}

TEST(DisjointIntervalMapTest, MustEmplaceOverlapRight) {
  IntIntervalMap imap;
  imap.must_emplace({30, 40}, std::make_unique<int>(5));
  EXPECT_DEATH(imap.must_emplace({39, 45}, std::make_unique<int>(22)),
               "Failed to emplace");
}

TEST(DisjointIntervalMapTest, MustEmplaceOverlapInterior) {
  IntIntervalMap imap;
  imap.must_emplace({30, 40}, std::make_unique<int>(5));
  EXPECT_DEATH(imap.must_emplace({31, 39}, std::make_unique<int>(12)),
               "Failed to emplace");
}

TEST(DisjointIntervalMapTest, MustEmplaceOverlapEnveloped) {
  IntIntervalMap imap;
  imap.must_emplace({30, 40}, std::make_unique<int>(5));
  EXPECT_DEATH(imap.must_emplace({29, 40}, std::make_unique<int>(29)),
               "Failed to emplace");
}

TEST(DisjointIntervalMapTest, MustEmplaceSpanningTwo) {
  IntIntervalMap imap;
  imap.must_emplace({30, 40}, std::make_unique<int>(5));
  imap.must_emplace({50, 60}, std::make_unique<int>(5));
  EXPECT_DEATH(imap.must_emplace({35, 55}, std::make_unique<int>(99)),
               "Failed to emplace");
}

TEST(DisjointIntervalMapTest, MustEmplaceOverlapsLower) {
  IntIntervalMap imap;
  imap.must_emplace({30, 40}, std::make_unique<int>(5));
  imap.must_emplace({50, 60}, std::make_unique<int>(5));
  EXPECT_DEATH(imap.must_emplace({35, 45}, std::make_unique<int>(55)),
               "Failed to emplace");
}

TEST(DisjointIntervalMapTest, MustEmplaceOverlapsUpper) {
  IntIntervalMap imap;
  imap.must_emplace({30, 40}, std::make_unique<int>(5));
  imap.must_emplace({50, 60}, std::make_unique<int>(5));
  EXPECT_DEATH(imap.must_emplace({45, 55}, std::make_unique<int>(66)),
               "Failed to emplace");
}

TEST(DisjointIntervalMapTest, FindInterval) {
  IntIntervalMap imap;
  imap.must_emplace({20, 25}, std::make_unique<int>(1));
  for (int i = 19; i < 26; ++i) {
    for (int j = i + 1; j < 26; ++j) {
      const auto found = imap.find({i, j});
      if (i >= 20 && j <= 25) {
        ASSERT_NE(found, imap.end());
        EXPECT_EQ(found->first.first, 20);
        EXPECT_EQ(found->first.second, 25);
        EXPECT_EQ(*found->second, 1);
      } else {
        EXPECT_EQ(found, imap.end());
      }
    }
  }
}

TEST(DisjointIntervalMapTest, BeginEndRangeConstIterators) {
  IntIntervalMap imap;
  // values chosen to be == interval size
  imap.must_emplace({50, 60}, std::make_unique<int>(10));
  imap.must_emplace({30, 35}, std::make_unique<int>(5));
  imap.must_emplace({39, 46}, std::make_unique<int>(7));
  for (const auto &pair : imap) {
    EXPECT_EQ(pair.first.second - pair.first.first, *pair.second);
  }
}

// std::vector is moveable and guaranteed to transfer ownership over its
// internal array, i.e. no small/inline-vector optimization.
using VectorIntervalMap =
    DisjointIntervalMap<std::vector<int>::const_iterator, std::vector<int>>;

static VectorIntervalMap::iterator AllocateVectorBlock(VectorIntervalMap *vmap,
                                                       int min, int max) {
  std::vector<int> v(max - min);
  int i = min;
  for (auto &e : v) {
    e = i;
    ++i;
  }
  // Caution: do not reference v and move(v) in the same set of call parameters
  // [sequence-point].
  const auto key(std::make_pair(v.cbegin(), v.cend()));
  const auto new_iter = vmap->must_emplace(key, std::move(v));
  EXPECT_EQ(new_iter->first, key);
  return new_iter;
}

static void VerifyVectorBlock(const VectorIntervalMap &vmap,
                              VectorIntervalMap::const_iterator block) {
  for (auto left = block->first.first; left != std::prev(block->first.second);
       ++left) {
    // Verify scalar find.
    EXPECT_TRUE(vmap.find(left) == block);
    // Verify range find.
    for (auto right = std::next(left); right != block->first.second; ++right) {
      EXPECT_TRUE(vmap.find({left, right}) == block);
    }
  }
}

TEST(DisjointIntervalMapTest, VectorDemo) {
  VectorIntervalMap vmap;
  const auto block1 = AllocateVectorBlock(&vmap, 10, 20);
  VerifyVectorBlock(vmap, block1);

  const auto block2 = AllocateVectorBlock(&vmap, 30, 40);
  VerifyVectorBlock(vmap, block1);
  VerifyVectorBlock(vmap, block2);

  const auto block3 = AllocateVectorBlock(&vmap, 20, 30);
  VerifyVectorBlock(vmap, block1);
  VerifyVectorBlock(vmap, block2);
  VerifyVectorBlock(vmap, block3);
}

}  // namespace
}  // namespace verible
