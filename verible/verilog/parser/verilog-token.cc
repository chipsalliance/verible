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

#include "verible/verilog/parser/verilog-token.h"

#include <cstddef>
#include <string_view>

#include "verible/verilog/parser/verilog-parser.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

// Strings returned by this function are used as token tags
// in verible-verilog-syntax' JSON output. Changing them might
// break third-party code.

std::string_view TokenTypeToString(size_t tokentype) {
  switch (tokentype) {
// Returns stringified symbol name
#define CASE_STRINGIFY(val)    \
  case verilog_tokentype::val: \
    return #val;

    // Tokens with verbose or unusual aliases
    CASE_STRINGIFY(TK_COMMENT_BLOCK)
    CASE_STRINGIFY(TK_EOL_COMMENT)
    CASE_STRINGIFY(TK_SPACE)
    CASE_STRINGIFY(TK_NEWLINE)
    CASE_STRINGIFY(TK_LINE_CONT)
    CASE_STRINGIFY(TK_ATTRIBUTE)
    CASE_STRINGIFY(TK_FILEPATH)

    CASE_STRINGIFY(PP_define_body)
    CASE_STRINGIFY(PP_default_text)

#undef CASE_STRINGIFY

    // The string returned by verilog_symbol_name() for single quote character
    // ("'\\''") contains backslash. This is the only such case, so generic
    // unescaping code in `default` section below would be superfluous.
    case '\'':
      return "'";

    // Returns token type name or its alias (if available) as used in verilog.y
    default: {
      std::string_view symbol_name(verilog_symbol_name(tokentype));
      if (symbol_name.size() >= 2 &&
          (symbol_name[0] == '"' || symbol_name[0] == '\'')) {
        // Strip quotes
        return symbol_name.substr(1, symbol_name.size() - 2);
      }
      return symbol_name;
    }
  }
}

}  // namespace verilog
