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

#include "verible/common/strings/naming-utils.h"

#include <algorithm>
#include <string_view>

#include "absl/strings/ascii.h"

namespace verible {

bool IsNameAllCapsUnderscoresDigits(std::string_view text) {
  return std::all_of(text.begin(), text.end(), [](char c) {
    return absl::ascii_isupper(c) || c == '_' || absl::ascii_isdigit(c);
  });
}

bool AllUnderscoresFollowedByDigits(std::string_view text) {
  if (text.empty()) return true;

  // Return false if the underscore is the last character.
  if (text[text.length() - 1] == '_') return false;

  for (std::string_view::size_type i = 0; i < text.length() - 1; ++i) {
    if (text[i] == '_' && !absl::ascii_isdigit(text[i + 1])) return false;
  }
  return true;
}

bool IsUpperCamelCaseWithDigits(std::string_view text) {
  if (text.empty()) return true;

  // Check that the first letter is capital. Not allowing "_foo" cases.
  if (!absl::ascii_isupper(text[0])) return false;

  // Check for underscores followed by digits
  auto pos = text.find('_');
  if (pos != std::string_view::npos) {
    return AllUnderscoresFollowedByDigits(text.substr(pos));
  }
  return true;
}

bool IsLowerSnakeCaseWithDigits(std::string_view text) {
  if (text.empty()) return true;

  // Check that the first letter is lowercase. Not allowing "_foo" cases.
  if (!absl::ascii_islower(text[0])) return false;

  // Check for anything that is not a lowercase letter, underscore, or digit.
  return std::all_of(text.begin(), text.end(), [](char c) {
    return absl::ascii_islower(c) || c == '_' || absl::ascii_isdigit(c);
  });
}

}  // namespace verible
