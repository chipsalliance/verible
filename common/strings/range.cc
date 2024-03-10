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

#include "common/strings/range.h"

#include <iterator>
#include <string_view>
#include <utility>

#include "common/util/logging.h"
#include "common/util/range.h"

namespace verible {

std::string_view make_string_view_range(const char *begin, const char *end) {
  const int length = std::distance(begin, end);
  CHECK_GE(length, 0) << "Malformed string bounds.";
  return std::string_view(begin, length);
}

std::pair<int, int> SubstringOffsets(std::string_view substring,
                                     std::string_view superstring) {
  return SubRangeIndices(substring, superstring);
}

}  // namespace verible
