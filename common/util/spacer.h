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

#ifndef VERIBLE_COMMON_UTIL_SPACER_H_
#define VERIBLE_COMMON_UTIL_SPACER_H_

#include <cstddef>
#include <iosfwd>

namespace verible {

// Streamable print adapter that prints a number of spaces without allocating
// any temporary string.
struct Spacer {
  explicit Spacer(size_t n, char c = ' ') : repeat(n), repeated_char(c) {}
  size_t repeat;
  char repeated_char;
};

std::ostream &operator<<(std::ostream &, const Spacer &);

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_SPACER_H_
