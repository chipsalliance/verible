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

#ifndef VERIBLE_VERILOG_TOKEN_VERILOG_TOKEN_H_
#define VERIBLE_VERILOG_TOKEN_VERILOG_TOKEN_H_

#include <cstddef>
#include <string_view>

namespace verilog {

// Returns token identifier suitable for use in string-based APIs (such as JSON
// export in verible-verilog-syntax). The identifiers are easy to type in
// programming languages and are mostly self-explanatory. They use:
// - Token text for string and character literal tokens. Examples:
//   "module", "==", ";", "'"
// - Token name used in verilog/parser/verilog.y. This uses the original token
//   names, not their (optional) display names. Examples:
//   "SymbolIdentifier", "TK_DecNumber", "TK_EOL_COMMENT", "TK_NEWLINE"
//
// See also: verilog_symbol_name() in verilog_parser.h
std::string_view TokenTypeToString(size_t tokentype);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOKEN_VERILOG_TOKEN_H_
