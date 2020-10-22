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

#include <iostream>
#include <sstream>
#include <string>

#include "absl/strings/string_view.h"
#include "common/util/enum_flags.h"

namespace verible {

// This mapping defines how this enum is displayed and parsed.
static const verible::EnumNameMap<IndentationStyle> kIndentationStyleStringMap =
    {
        {"indent", IndentationStyle::kIndent},
        {"wrap", IndentationStyle::kWrap},
};

std::ostream& operator<<(std::ostream& stream, IndentationStyle p) {
  return kIndentationStyleStringMap.Unparse(p, stream);
}

bool AbslParseFlag(absl::string_view text, IndentationStyle* mode,
                   std::string* error) {
  return kIndentationStyleStringMap.Parse(text, mode, error,
                                          "IndentationStyle");
}

std::string AbslUnparseFlag(const IndentationStyle& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

}  // namespace verible
