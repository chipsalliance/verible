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

#include "common/util/auto_iterator.h"

#include <list>
#include <map>
#include <set>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(AutoIteratorSelectorTest, NonConst) {
  static_assert(std::is_same<auto_iterator_selector<std::list<int>>::type,
                             std::list<int>::iterator>::value,
                "");
  static_assert(std::is_same<auto_iterator_selector<std::map<int, char>>::type,
                             std::map<int, char>::iterator>::value,
                "");
  static_assert(std::is_same<auto_iterator_selector<std::set<int>>::type,
                             std::set<int>::iterator>::value,
                "");
  static_assert(std::is_same<auto_iterator_selector<std::vector<int>>::type,
                             std::vector<int>::iterator>::value,
                "");
}

TEST(AutoIteratorSelectorTest, Const) {
  static_assert(std::is_same<auto_iterator_selector<const std::list<int>>::type,
                             std::list<int>::const_iterator>::value,
                "");
  static_assert(
      std::is_same<auto_iterator_selector<const std::map<int, char>>::type,
                   std::map<int, char>::const_iterator>::value,
      "");
  static_assert(std::is_same<auto_iterator_selector<const std::set<int>>::type,
                             std::set<int>::const_iterator>::value,
                "");
  static_assert(
      std::is_same<auto_iterator_selector<const std::vector<int>>::type,
                   std::vector<int>::const_iterator>::value,
      "");
}

}  // namespace
}  // namespace verible
