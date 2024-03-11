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

#ifndef VERIBLE_COMMON_STRINGS_UTF8_H_
#define VERIBLE_COMMON_STRINGS_UTF8_H_

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace verible {
// Determine length in characters of an UTF8-encoded string.
inline int utf8_len(std::string_view str) {
  return std::count_if(str.begin(), str.end(),
                       [](char c) { return (c & 0xc0) != 0x80; });
}

// Returns the substring starting from the given character
// of the UTF8-encoded string.
inline std::string_view utf8_substr(std::string_view str,
                                    size_t character_pos) {
  // Strategy: whenever we see a start of a utf8 codepoint bump the expected
  // remaining by number of expected bytes
  size_t remaining = character_pos;
  std::string_view::const_iterator it;
  for (it = str.begin(); remaining && it != str.end(); ++it, --remaining) {
    if ((*it & 0xE0) == 0xC0) {
      remaining += 1;
    } else if ((*it & 0xF0) == 0xE0) {
      remaining += 2;
    } else if ((*it & 0xF8) == 0xF0) {
      remaining += 3;
    }
  }
  return str.substr(it - str.begin());
}

inline std::string_view utf8_substr(std::string_view str, size_t character_pos,
                                    size_t character_len) {
  const std::string_view prefix = utf8_substr(str, character_pos);
  const std::string_view chop_end = utf8_substr(prefix, character_len);
  return {prefix.data(), prefix.length() - chop_end.length()};
}
}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_UTF8_H_
