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

#include "verible/common/strings/position.h"

#include <string_view>

namespace verible {

int AdvancingTextNewColumnPosition(int old_column_position,
                                   std::string_view advancing_text) {
  const auto last_newline = advancing_text.find_last_of('\n');
  if (last_newline == std::string_view::npos) {
    // No newlines, so treat every character as one column position,
    // even tabs.
    return old_column_position + advancing_text.length();
  }
  // Count characters after the last newline.
  return advancing_text.length() - last_newline - 1;
}

}  // namespace verible
