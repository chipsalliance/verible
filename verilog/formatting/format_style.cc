// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/formatting/format_style.h"

#include <initializer_list>
#include <map>
#include <sstream>
#include <string>

#include "absl/strings/string_view.h"
#include "common/util/enum_flags.h"

namespace verilog {
namespace formatter {

// This mapping defines how this enum is displayed and parsed.
// This is given extern linkage for the sake of testing, but not exposed
// in the public header.
extern const std::initializer_list<
    std::pair<const absl::string_view, PreserveSpaces>>
    kPreserveSpacesStringMap = {
        {"none", PreserveSpaces::None},
        {"unhandled", PreserveSpaces::UnhandledCasesOnly},
        {"all", PreserveSpaces::All},
};

std::ostream& operator<<(std::ostream& stream, PreserveSpaces p) {
  static const auto* flag_map =
      verible::MakeEnumToStringMap(kPreserveSpacesStringMap);
  return stream << flag_map->find(p)->second;
}

bool AbslParseFlag(absl::string_view text, PreserveSpaces* mode,
                   std::string* error) {
  static const auto* flag_map =
      verible::MakeStringToEnumMap(kPreserveSpacesStringMap);
  return EnumMapParseFlag(*flag_map, text, mode, error);
}

std::string AbslUnparseFlag(const PreserveSpaces& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

}  // namespace formatter
}  // namespace verilog
