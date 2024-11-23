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

// Tests for value's argument forwarding constructor

#include "verible/common/text/concrete-syntax-leaf.h"

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/text/token-info.h"

namespace verible {
namespace {

TEST(ValueSymbolTest, EqualityArity1Args) {
  constexpr absl::string_view text("foo");
  SyntaxTreeLeaf value1(10, text);
  TokenInfo info1(10, text);
  auto info2 = value1.get();
  EXPECT_EQ(info1, info2);
}

TEST(ValueSymbolTest, InequalityDifferentStringLocations) {
  // Check that two tokens with the same text content but different
  // location are _not_ considered equal.
  constexpr absl::string_view longtext("foofoo");
  constexpr absl::string_view first = longtext.substr(0, 3);
  constexpr absl::string_view second = longtext.substr(3);

  SyntaxTreeLeaf value1(10, first);
  TokenInfo info1(10, second);
  auto info2 = value1.get();
  EXPECT_NE(info1, info2);
}
}  // namespace
}  // namespace verible
