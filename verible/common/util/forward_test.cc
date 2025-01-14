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

#include "verible/common/util/forward.h"

#include <string>
#include <string_view>
#include <type_traits>

#include "gtest/gtest.h"

namespace verible {
namespace {

class TestClassA {};

class TestClassB {
 public:
  TestClassB() = default;
  explicit TestClassB(const TestClassA &) {}
};

TEST(ForwardReferenceElseConstructTest, ForwardReference) {
  TestClassA a;
  const auto &ref = ForwardReferenceElseConstruct<TestClassA>()(a);
  EXPECT_EQ(&ref, &a);  // same object forwarded
}

TEST(ForwardReferenceElseConstructTest, ForwardReferenceConst) {
  const TestClassA a;
  const auto &ref = ForwardReferenceElseConstruct<TestClassA>()(a);
  EXPECT_EQ(&ref, &a);  // same object forwarded
}

TEST(ForwardReferenceElseConstructTest, Construct) {
  const TestClassA a;
  const auto &ref = ForwardReferenceElseConstruct<TestClassB>()(a);
  static_assert(!std::is_same_v<decltype(ref), TestClassA>,
                "!std::is_same<decltype(ref), TestClassA>::value");
}

TEST(ForwardReferenceElseConstructTest, ForwardStringView) {
  const std::string_view a("hello");
  const auto &ref = ForwardReferenceElseConstruct<std::string_view>()(a);
  EXPECT_EQ(&ref, &a);  // same object forwarded
}

TEST(ForwardReferenceElseConstructTest, ConstructString) {
  const std::string_view a("hello");
  const auto &ref = ForwardReferenceElseConstruct<std::string>()(a);
  static_assert(!std::is_same_v<decltype(ref), std::string_view>,
                "!std::is_same<decltype(ref), std::string_view>::value");
}

TEST(ForwardReferenceElseConstructTest, ForwardString) {
  const std::string a("hello");
  const auto &ref = ForwardReferenceElseConstruct<std::string>()(a);
  EXPECT_EQ(&ref, &a);  // same object forwarded
}

TEST(ForwardReferenceElseConstructTest, ConstructStringView) {
  const std::string a("hello");
  const auto &ref = ForwardReferenceElseConstruct<std::string_view>()(a);
  static_assert(!std::is_same_v<decltype(ref), std::string>,
                "!std::is_same<decltype(ref), std::string>::value");
}

}  // namespace
}  // namespace verible
