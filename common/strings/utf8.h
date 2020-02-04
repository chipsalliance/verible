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

#include "absl/strings/string_view.h"

namespace verible {
// Determine length in characters of an UTF8-encoded string.
inline int utf8_len(absl::string_view str) {
  return std::count_if(str.begin(), str.end(),
                       [](char c) { return (c & 0xc0) != 0x80; });
}
}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_UTF8_H_
