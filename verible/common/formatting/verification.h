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

#ifndef VERIBLE_COMMON_FORMATTING_VERIFICATION_H_
#define VERIBLE_COMMON_FORMATTING_VERIFICATION_H_

#include <string_view>

#include "absl/status/status.h"
#include "verible/common/strings/position.h"

namespace verible {

// Verifies that 'formatted_text' == 'reformatted_text', and return a status
// indicating the success of that comparison.
// The following parameters are only used for diagnostics:
// 'original_text' is the text before any formatting was done.
// 'lines' is the set of lines requested if incrementally formatting.
absl::Status ReformatMustMatch(std::string_view original_text,
                               const LineNumberSet &lines,
                               std::string_view formatted_text,
                               std::string_view reformatted_text);

}  // namespace verible

#endif  // VERIBLE_COMMON_FORMATTING_VERIFICATION_H_
