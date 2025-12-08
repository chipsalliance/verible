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

#ifndef VERIBLE_VERILOG_TRANSFORM_STRIP_COMMENTS_H_
#define VERIBLE_VERILOG_TRANSFORM_STRIP_COMMENTS_H_

#include <iosfwd>
#include <string_view>

namespace verilog {

// Removes or alters comments from Verilog code.
// This also covers comments inside macro definitions and arguments.
// 'replacement' character:
//   '\0' - delete the comment text
//   ' ' - replace comment text with equal number of spaces and newlines
//       (including comment start/ends)
//       This preserves byte offsets and line numbers of all unchanged text.
//   other - replace comment text with another character
//       (excluding comment start/ends), and preserve newlines.
//       This preserves byte offsets and line numbers of all unchanged text.
//       This option is good for visibility.
// All lexical errors are ignored.
void StripVerilogComments(std::string_view content, std::ostream *output,
                          char replacement = '\0');

}  // namespace verilog

#endif  // VERIBLE_VERILOG_TRANSFORM_STRIP_COMMENTS_H_
