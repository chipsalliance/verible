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

#include "verible/common/formatting/basic-format-style.h"

#include <string>

#include "gtest/gtest.h"

namespace verible {
namespace {

TEST(IndentationStyleParseFlagTest, Parse) {
  std::string error;
  IndentationStyle istyle;
  // Test valid values.
  EXPECT_TRUE(AbslParseFlag("indent", &istyle, &error));
  EXPECT_EQ(istyle, IndentationStyle::kIndent);
  EXPECT_TRUE(AbslParseFlag("wrap", &istyle, &error));
  EXPECT_EQ(istyle, IndentationStyle::kWrap);
  // Test for invalid string.
  EXPECT_FALSE(AbslParseFlag("invalid", &istyle, &error));
}

TEST(IndentationStyleUnparseFlagTest, Unparse) {
  EXPECT_EQ(AbslUnparseFlag(IndentationStyle::kIndent), "indent");
  EXPECT_EQ(AbslUnparseFlag(IndentationStyle::kWrap), "wrap");
}

}  // namespace
}  // namespace verible
