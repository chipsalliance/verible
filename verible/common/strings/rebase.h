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

#ifndef VERIBLE_COMMON_STRINGS_REBASE_H_
#define VERIBLE_COMMON_STRINGS_REBASE_H_

#include <string_view>

namespace verible {

// 'Moves' src string_view to point to another buffer, dest, where the
// contents still matches.  This is useful for analyzing different copies
// of text, and transplanting to buffers that belong to different
// memory owners.
// This is a potentially dangerous operation, which can be validated
// using a combination of object lifetime management and range-checking.
// It is the caller's responsibility that it points to valid memory.
void RebaseStringView(std::string_view *src, std::string_view dest);

// This overload assumes that the string of interest from other has the
// same length as the current string_view.
// string_view::iterator happens to be const char*, but do not rely on that
// fact as it can be implementation-dependent.
void RebaseStringView(std::string_view *src, const char *dest);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_REBASE_H_
