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

#include <sstream>
#include <string>

#include "nlohmann/json.hpp"
#include "verible/common/text/token-info.h"

namespace verible {

nlohmann::json ToJson(const TokenInfo &token_info,
                      const TokenInfo::Context &context, bool include_text) {
  nlohmann::json json(nlohmann::json::object());
  std::ostringstream stream;
  context.token_enum_translator(stream, token_info.token_enum());
  json["start"] = token_info.left(context.base);
  json["end"] = token_info.right(context.base);
  json["tag"] = stream.str();

  if (include_text) {
    json["text"] = std::string(token_info.text());
  }

  return json;
}

}  // namespace verible
