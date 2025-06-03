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

#include "verible/common/text/line-terminator.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string_view>

namespace verible {
LineTerminatorStyle GuessLineTerminator(std::string_view text,
                                        int32_t count_at_most) {
  int32_t line_count = 0;
  int32_t crlf_count = 0;

  size_t pos = 0;
  while ((pos = text.find_first_of('\n', pos)) != std::string_view::npos) {
    ++line_count;
    if (pos > 0 && text[pos - 1] == '\r') {
      ++crlf_count;
    }
    ++pos;
    if (line_count >= count_at_most) {
      break;
    }
  }
  return (crlf_count <= line_count / 2) ? LineTerminatorStyle::kLF
                                        : LineTerminatorStyle::kCRLF;
}

void EmitLineTerminator(LineTerminatorStyle style, std::ostream &stream) {
  switch (style) {
    case LineTerminatorStyle::kLF:
      stream << "\n";
      break;
    case LineTerminatorStyle::kCRLF:
      stream << "\r\n";
      break;
  }
}

}  // namespace verible
