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

#include "common/text/token_info_json.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "common/text/constants.h"
#include "common/text/token_info.h"
#include "gtest/gtest.h"
#include "json/value.h"

namespace verible {
namespace {

static Json::Value ParseJson(absl::string_view text) {
  Json::Value json;
  std::unique_ptr<Json::CharReader> reader(
      Json::CharReaderBuilder().newCharReader());
  reader->parse(text.begin(), text.end(), &json, nullptr);
  return json;
}

TEST(TokenInfoToJsonTest, ToJsonEOF) {
  constexpr absl::string_view base;  // empty
  const TokenInfo::Context context(base);
  const TokenInfo token_info(TK_EOF, base);

  const Json::Value json(ToJson(token_info, context));
  const Json::Value expected_json = ParseJson(R"({
    "start": 0,
    "end": 0,
    "tag": "0"
  })");
  EXPECT_EQ(json, expected_json);
}

TEST(TokenInfoToJsonTest, ToJsonWithBase) {
  constexpr absl::string_view base("basement cat");
  const TokenInfo::Context context(base);
  const TokenInfo token_info(7, base.substr(9, 3));

  const Json::Value json(ToJson(token_info, context));
  const Json::Value expected_json = ParseJson(R"({
    "start": 9,
    "end": 12,
    "tag": "7"
  })");
  EXPECT_EQ(json, expected_json);
}

TEST(TokenInfoToJsonTest, ToJsonWithTokenEnumTranslator) {
  constexpr absl::string_view text("string of length 19");
  const TokenInfo token_info(143, text);

  const verible::TokenInfo::Context context(
      text, [](std::ostream& stream, int e) { stream << "token enum " << e; });

  const Json::Value json(ToJson(token_info, context));
  const Json::Value expected_json = ParseJson(R"({
    "start": 0,
    "end": 19,
    "tag": "token enum 143"
  })");
  EXPECT_EQ(json, expected_json);
}

}  // namespace
}  // namespace verible
