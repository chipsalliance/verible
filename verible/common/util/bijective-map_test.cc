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

#include "verible/common/util/bijective-map.h"

#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/strings/compare.h"
#include "verible/common/util/logging.h"

namespace verible {
namespace {

using TestMapType = BijectiveMap<std::string, int>;

TEST(BijectiveMapTest, DefaultCtor) {
  const TestMapType m;
  EXPECT_EQ(m.size(), 0);
  EXPECT_TRUE(m.empty());
  EXPECT_FALSE(m.find_forward("a"));
  EXPECT_FALSE(m.find_reverse(1));
  EXPECT_TRUE(m.forward_view().empty());
  EXPECT_TRUE(m.reverse_view().empty());
}

TEST(BijectiveMapTest, InitializerListCtor) {
  const TestMapType m{{{"www", 2}, {"qqq", 3}}};
  EXPECT_EQ(m.size(), 2);
  EXPECT_FALSE(m.empty());
  EXPECT_FALSE(m.find_forward("a"));
  EXPECT_FALSE(m.find_reverse(1));
  EXPECT_EQ(*m.find_forward("qqq"), 3);
  EXPECT_EQ(*m.find_reverse(2), "www");
  EXPECT_FALSE(m.forward_view().empty());
  EXPECT_FALSE(m.reverse_view().empty());
}

TEST(BijectiveMapTest, InitializerListCtorDupeKey) {
  EXPECT_DEATH(const TestMapType m({{"www", 2}, {"www", 3}}), "duplicate");
}

TEST(BijectiveMapTest, InitializerListCtorDupeValue) {
  EXPECT_DEATH(const TestMapType m({{"www", 2}, {"qqq", 2}}), "duplicate");
}

TEST(BijectiveMapTest, IteratorPairCtor) {
  const std::initializer_list<std::pair<std::string, int>> values = {
      {"qqq", 3}, {"www", 2}};
  const TestMapType m(values.begin(), values.end());
  EXPECT_EQ(m.size(), 2);
  EXPECT_FALSE(m.empty());
  EXPECT_FALSE(m.find_forward("a"));
  EXPECT_FALSE(m.find_reverse(1));
  EXPECT_EQ(*m.find_forward("qqq"), 3);
  EXPECT_EQ(*m.find_reverse(2), "www");
  EXPECT_FALSE(m.forward_view().empty());
  EXPECT_FALSE(m.reverse_view().empty());
}

TEST(BijectiveMapTest, IteratorPairCtorDupeKey) {
  const std::initializer_list<std::pair<std::string, int>> values = {
      {"qqq", 4}, {"qqq", 2}};
  EXPECT_DEATH(const TestMapType m(values.begin(), values.end()), "duplicate");
}

TEST(BijectiveMapTest, IteratorPairCtorDupeValue) {
  const std::initializer_list<std::pair<std::string, int>> values = {
      {"zzz", 3}, {"qqq", 3}};
  EXPECT_DEATH(const TestMapType m(values.begin(), values.end()), "duplicate");
}

TEST(BijectiveMapTest, OneElement) {
  TestMapType m;
  EXPECT_TRUE(m.insert("a", 1));
  EXPECT_EQ(m.size(), 1);
  EXPECT_FALSE(m.empty());
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_forward("a")), 1);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_reverse(1)), "a");
  EXPECT_EQ(m.find_forward("b"), nullptr);
  EXPECT_EQ(m.find_reverse(2), nullptr);
}

TEST(BijectiveMapTest, InsertPair) {
  TestMapType m;
  EXPECT_TRUE(m.insert(std::make_pair("b", 2)));
  EXPECT_EQ(m.size(), 1);
  EXPECT_FALSE(m.empty());
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_forward("b")), 2);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_reverse(2)), "b");
  EXPECT_EQ(m.find_forward("a"), nullptr);
  EXPECT_EQ(m.find_reverse(1), nullptr);
}

TEST(BijectiveMapTest, InsertKeyAlreadyExists) {
  TestMapType m;
  EXPECT_TRUE(m.insert("a", 1));
  EXPECT_FALSE(m.insert("a", 3));
  EXPECT_EQ(m.size(), 1);
}

TEST(BijectiveMapTest, InsertValueAlreadyExists) {
  TestMapType m;
  EXPECT_TRUE(m.insert("a", 1));
  EXPECT_FALSE(m.insert("z", 1));
  EXPECT_EQ(m.size(), 1);
}

TEST(BijectiveMapTest, InsertNewPairs) {
  TestMapType m;
  EXPECT_TRUE(m.insert("a", 1));
  EXPECT_TRUE(m.insert("z", 4));
  EXPECT_EQ(m.size(), 2);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_forward("a")), 1);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_reverse(1)), "a");
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_forward("z")), 4);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_reverse(4)), "z");
  EXPECT_EQ(m.find_forward("b"), nullptr);
  EXPECT_EQ(m.find_reverse(3), nullptr);
}

static std::function<int()> LoopGeneratorGenerator(
    const std::vector<int> &values) {
  return [=]() {  // fake random generator
    static const std::vector<int> v(values);
    static std::vector<int>::const_iterator iter = v.begin();
    if (iter == v.end()) {
      iter = v.begin();
    }  // loop
    return *iter++;
  };
}

TEST(BijectiveMapTest, InsertRandom) {
  TestMapType m;
  auto gen = LoopGeneratorGenerator({1, 1, 2, 2, 3, 3});
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.insert_using_value_generator("b", gen)), 1);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.insert_using_value_generator("b", gen)), 1);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.insert_using_value_generator("g", gen)), 2);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.insert_using_value_generator("g", gen)), 2);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.insert_using_value_generator("f", gen)), 3);
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.insert_using_value_generator("f", gen)), 3);
}

// Testing heterogenous lookup: internally stored std::string, supporting
// copy-less lookup with absl::string_view.
using StringMapType = BijectiveMap<std::string, std::string, StringViewCompare,
                                   StringViewCompare>;

TEST(BijectiveMapTest, HeterogeneousStringLookup) {
  StringMapType m;
  EXPECT_TRUE(m.insert("a", "G"));
  EXPECT_TRUE(m.insert("z", "Q"));
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_forward(absl::string_view("a"))), "G");
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_reverse(absl::string_view("G"))), "a");
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_forward(absl::string_view("z"))), "Q");
  EXPECT_EQ(*ABSL_DIE_IF_NULL(m.find_reverse(absl::string_view("Q"))), "z");
  EXPECT_EQ(m.find_forward(absl::string_view("b")), nullptr);
  EXPECT_EQ(m.find_reverse(absl::string_view("3")), nullptr);
}

}  // namespace
}  // namespace verible
