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

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "verible/common/util/enum-flags.h"

namespace verible {

// This mapping defines how this enum is displayed and parsed.
static const verible::EnumNameMap<IndentationStyle> &IndentationStyleStrings() {
  static const verible::EnumNameMap<IndentationStyle>
      kIndentationStyleStringMap({
          {"indent", IndentationStyle::kIndent},
          {"wrap", IndentationStyle::kWrap},
      });
  return kIndentationStyleStringMap;
}

std::ostream &operator<<(std::ostream &stream, IndentationStyle p) {
  return IndentationStyleStrings().Unparse(p, stream);
}

bool AbslParseFlag(std::string_view text, IndentationStyle *mode,
                   std::string *error) {
  return IndentationStyleStrings().Parse(text, mode, error, "IndentationStyle");
}

std::string AbslUnparseFlag(const IndentationStyle &mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

}  // namespace verible
