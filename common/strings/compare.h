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

#ifndef VERIBLE_COMMON_STRINGS_COMPARE_H_
#define VERIBLE_COMMON_STRINGS_COMPARE_H_

#include <string_view>

namespace verible {

// This comparator enables heteregeneous lookup on a string-keyed associative
// array (using string_view).
//
// See https://abseil.io/tips/144 for further explanation.
//
// Example: std::map<std::string, OtherType, StringViewCompare>
struct StringViewCompare {
  using is_transparent = void;

  // Works on anything that is implicitly convertible to string_view.
  bool operator()(std::string_view a, std::string_view b) const {
    return a < b;
  }
};

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_COMPARE_H_
