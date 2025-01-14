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

#include "verible/common/strings/rebase.h"

#include <string_view>

#include "verible/common/util/logging.h"

namespace verible {

void RebaseStringView(std::string_view *src, std::string_view dest) {
  CHECK_EQ(*src, dest) << "RebaseStringView() is only valid when the "
                          "new text referenced matches the old text.";
  *src = dest;
}

void RebaseStringView(std::string_view *src, const char *dest) {
  RebaseStringView(src, std::string_view(dest, src->length()));
}

}  // namespace verible
