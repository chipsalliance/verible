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

#include "verible/common/text/token-info-json.h"

#include <ostream>
#include <string_view>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "verible/common/text/constants.h"
#include "verible/common/text/token-info.h"

namespace verible {
namespace {

TEST(TokenInfoToJsonTest, ToJsonEOF) {
  constexpr std::string_view base;  // empty
  const TokenInfo::Context context(base);
  const TokenInfo token_info(TK_EOF, base);

  EXPECT_EQ(ToJson(token_info, context), nlohmann::json::parse(R"({
    "start": 0,
    "end": 0,
    "tag": "0"
  })"));

  EXPECT_EQ(ToJson(token_info, context, true), nlohmann::json::parse(R"({
    "start": 0,
    "end": 0,
    "tag": "0",
    "text": ""
  })"));
}

TEST(TokenInfoToJsonTest, ToJsonWithBase) {
  constexpr std::string_view base("basement cat");
  const TokenInfo::Context context(base);
  const TokenInfo token_info(7, base.substr(9, 3));

  EXPECT_EQ(ToJson(token_info, context), nlohmann::json::parse(R"({
    "start": 9,
    "end": 12,
    "tag": "7"
  })"));

  EXPECT_EQ(ToJson(token_info, context, true), nlohmann::json::parse(R"({
    "start": 9,
    "end": 12,
    "tag": "7",
    "text": "cat"
  })"));
}

TEST(TokenInfoToJsonTest, ToJsonWithTokenEnumTranslator) {
  constexpr std::string_view text("string of length 19");
  const TokenInfo token_info(143, text);

  const verible::TokenInfo::Context context(
      text, [](std::ostream &stream, int e) { stream << "token enum " << e; });

  EXPECT_EQ(ToJson(token_info, context), nlohmann::json::parse(R"({
    "start": 0,
    "end": 19,
    "tag": "token enum 143"
  })"));

  EXPECT_EQ(ToJson(token_info, context, true), nlohmann::json::parse(R"({
    "start": 0,
    "end": 19,
    "tag": "token enum 143",
    "text": "string of length 19"
  })"));
}

}  // namespace
}  // namespace verible
