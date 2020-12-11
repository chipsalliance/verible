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

#include "verilog/parser/verilog_token.h"

#include <string>

namespace verilog {

std::string TokenTypeToString(verilog_tokentype tokentype) {
  switch (tokentype) {
#define CONSIDER(val) \
  case verilog_tokentype::val: \
    return #val;
#include "verilog/parser/verilog_token_enum_foreach.inc"  // IWYU pragma: keep

#undef CONSIDER
    default:
      // Character literal
      if (tokentype > 0 && tokentype < 256) {
        return std::string(1, static_cast<char>(tokentype));
      }
      // Should not happen
      return "UNKNOWN";
  }
}

}  // namespace verilog
