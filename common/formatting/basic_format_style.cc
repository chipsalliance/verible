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

#include "common/formatting/basic_format_style.h"

#include <initializer_list>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "common/util/enum_flags.h"

namespace verible {

namespace internal {
// This mapping defines how this enum is displayed and parsed.
constexpr std::initializer_list<
    std::pair<const absl::string_view, IndentationStyle>>
    kIndentationStyleStringMap = {
        {"indent", IndentationStyle::kIndent},
        {"wrap", IndentationStyle::kWrap},
};
}  // namespace internal

std::ostream& operator<<(std::ostream& stream, IndentationStyle p) {
  static const auto* flag_map =
      verible::MakeEnumToStringMap(internal::kIndentationStyleStringMap);
  return stream << flag_map->find(p)->second;
}

bool AbslParseFlag(absl::string_view text, IndentationStyle* mode,
                   std::string* error) {
  static const auto* flag_map =
      verible::MakeStringToEnumMap(internal::kIndentationStyleStringMap);
  return EnumMapParseFlag(*flag_map, text, mode, error);
}

std::string AbslUnparseFlag(const IndentationStyle& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

}  // namespace verible
