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

#include "common/strings/split.h"

#include <vector>

#include "absl/strings/str_split.h"

namespace verible {

std::vector<absl::string_view> SplitLines(absl::string_view text) {
  if (text.empty()) return std::vector<absl::string_view>();
  std::vector<absl::string_view> lines(
      absl::StrSplit(text, absl::ByChar('\n')));
  // If text ends cleanly with a \n, omit the last blank split,
  // otherwise treat it as if the trailing text ends with a \n.
  if (text.back() == '\n') {
    lines.pop_back();
  }
  return lines;
}

}  // namespace verible
