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

#include "common/strings/display_utils.h"

#include <iostream>

namespace verible {

static constexpr absl::string_view kEllipses = "...";

std::ostream& operator<<(std::ostream& stream, const AutoTruncate& trunc) {
  const auto text = trunc.text;
  const int length = text.length();
  if (length <= trunc.max_chars) return stream << text;
  const auto context_length = trunc.max_chars - kEllipses.length();
  const auto tail_length = context_length / 2;
  const auto head_length = context_length - tail_length;
  const auto tail_start = length - tail_length;
  return stream << text.substr(0, head_length) << kEllipses
                << text.substr(tail_start, tail_length);
}

std::ostream& operator<<(std::ostream& stream, const VisualizeWhitespace& vis) {
  for (const char c : vis.text) {
    switch (c) {
      case ' ':
        stream << vis.space_alt;
        break;
      case '\t':
        stream << vis.tab_alt;
        break;
      case '\n':
        stream << vis.newline_alt;
        break;
      default:
        stream << c;
    }
  }
  return stream;
}

}  // namespace verible
