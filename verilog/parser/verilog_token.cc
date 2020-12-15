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

#include "verilog/parser/verilog_parser.h"
#include "absl/strings/string_view.h"

namespace verilog {

// Strings returned by this function are used as token tags
// in verible-verilog-syntax' JSON output. Changing them might
// break third-party code.

std::string TokenTypeToString(verilog_tokentype tokentype) {
  switch (tokentype) {
    // Returns stringified symbol name
    #define CASE_STRINGIFY(val) \
        case verilog_tokentype::val: return #val;

    // Tokens with verbose aliases
    CASE_STRINGIFY(TK_COMMENT_BLOCK)
    CASE_STRINGIFY(TK_EOL_COMMENT)
    CASE_STRINGIFY(TK_SPACE)
    CASE_STRINGIFY(TK_NEWLINE)
    CASE_STRINGIFY(TK_LINE_CONT)
    CASE_STRINGIFY(TK_ATTRIBUTE)
    CASE_STRINGIFY(TK_FILEPATH)

    #undef CASE_STRINGIFY

    // Returns token type name or its alias (if available) as used in verilog.y
    default: {
      absl::string_view symbol_name(verilog_symbol_name(tokentype));
      if (symbol_name[0] == '"' || symbol_name[0] == '\'') {
        // Strip quotes
        return std::string(symbol_name.substr(1, symbol_name.size() - 2));
      } else {
        return std::string(symbol_name);
      }
    }
  }
}

}  // namespace verilog
