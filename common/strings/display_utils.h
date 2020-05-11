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

#ifndef VERIBLE_COMMON_STRINGS_DISPLAY_UTILS_H_
#define VERIBLE_COMMON_STRINGS_DISPLAY_UTILS_H_

#include <iosfwd>

#include "absl/strings/string_view.h"

namespace verible {

// Stream printable object that limits the length of text printed.
// Applications: debugging potentially long strings, where only the head and
// tail are sufficient to comprehend the text being referenced.
//
// usage: stream << AutoTruncate{text, limit};
//
// example output (limit: 9): "abc...xyz"
struct AutoTruncate {
  const absl::string_view text;
  // Maximum number of characters to show, including "..."
  const int max_chars;
};

std::ostream& operator<<(std::ostream&, const AutoTruncate& trunc);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_DISPLAY_UTILS_H_
