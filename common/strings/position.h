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

#ifndef VERIBLE_COMMON_STRINGS_POSITION_H_
#define VERIBLE_COMMON_STRINGS_POSITION_H_

#include "absl/strings/string_view.h"

namespace verible {

// Returns the updated column position of text, given a starting column
// position and advancing_text.  Each newline in the advancing_text effectively
// resets the column position back to zero.  All non-newline characters count as
// one space.
int AdvancingTextNewColumnPosition(int old_column_position,
                                   absl::string_view advancing_text);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_POSITION_H_
