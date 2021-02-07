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

#include <sstream>

#include "common/text/token_info.h"
#include "json/json.h"

namespace verible {

Json::Value ToJson(const TokenInfo& token_info,
                   const TokenInfo::Context& context) {
  Json::Value json(Json::objectValue);
  std::ostringstream stream;
  context.token_enum_translator(stream, token_info.token_enum());
  json["start"] = token_info.left(context.base);
  json["end"] = token_info.right(context.base);
  json["tag"] = stream.str();
  return json;
}

}  // namespace verible
