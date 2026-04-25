// Copyright 2025 The Verible Authors.
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

#ifndef VERIBLE_COMMON_TEXT_LINE_TERMINATOR_H_
#define VERIBLE_COMMON_TEXT_LINE_TERMINATOR_H_

#include <cstdint>
#include <ostream>
#include <string_view>

namespace verible {

enum class LineTerminatorStyle {
  // Line Feed `\n` (UNIX Style)
  kLF,
  // Carriage return + Line Feed `\r\n` (DOS Style)
  kCRLF,
};

// Emit the given line terminator to stream.
void EmitLineTerminator(LineTerminatorStyle style, std::ostream &stream);

// Look at "count_at_most" lines to decide if this is mostly LF or CRLF text.
LineTerminatorStyle GuessLineTerminator(std::string_view text,
                                        int32_t count_at_most);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_LINE_TERMINATOR_H_
