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

#include "verible/common/strings/split.h"

#include <cstddef>
#include <string_view>
#include <vector>

#include "absl/strings/str_split.h"

namespace verible {

std::vector<std::string_view> SplitLines(std::string_view text) {
  if (text.empty()) return {};
  std::vector<std::string_view> lines(absl::StrSplit(text, absl::ByChar('\n')));
  // If text ends cleanly with a \n, omit the last blank split,
  // otherwise treat it as if the trailing text ends with a \n.
  if (text.back() == '\n') {
    lines.pop_back();
  }
  return lines;
}

// An zero-length absl::StrSplit delimiter that will match just after a
// specified character.
class AfterCharDelimiter {
 public:
  explicit AfterCharDelimiter(char delimiter) : delimiter_(delimiter) {}

  std::string_view Find(std::string_view text, size_t pos) const {
    const size_t found_pos = text.find(delimiter_, pos);
    if (found_pos == std::string_view::npos) {
      return {text.data() + text.size(), 0};
    }
    return text.substr(found_pos + 1, 0);
  }

 private:
  const char delimiter_;
};

std::vector<std::string_view> SplitLinesKeepLineTerminator(
    std::string_view text) {
  if (text.empty()) return {};
  return absl::StrSplit(text, AfterCharDelimiter('\n'));
}

}  // namespace verible
