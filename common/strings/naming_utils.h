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

#ifndef VERIBLE_COMMON_STRINGS_NAMING_UTILS_H_
#define VERIBLE_COMMON_STRINGS_NAMING_UTILS_H_

#include <string_view>

namespace verible {

// Returns true if the string contains only capital letters, digits, and
// underscores.
bool IsNameAllCapsUnderscoresDigits(std::string_view);

// Returns true if the all the underscores in the string are followed by digits.
bool AllUnderscoresFollowedByDigits(std::string_view);

// Returns true if the string follows UpperCamelCase naming convention, where
// underscores are allowed when separating a digit.
bool IsUpperCamelCaseWithDigits(std::string_view);

// Returns true if the string follows lower_snake_case naming convention.
bool IsLowerSnakeCaseWithDigits(std::string_view);

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_NAMING_UTILS_H_
