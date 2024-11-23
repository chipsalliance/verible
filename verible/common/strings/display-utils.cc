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

#include "verible/common/strings/display-utils.h"

#include <iomanip>
#include <iostream>

#include "absl/strings/string_view.h"

namespace verible {

static constexpr absl::string_view kEllipses = "...";

std::ostream &operator<<(std::ostream &stream, const AutoTruncate &trunc) {
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

std::ostream &operator<<(std::ostream &stream, const EscapeString &vis) {
  for (const unsigned char c : vis.text) {
    switch (c) {
      case '\a':
        stream << "\\a";
        break;
      case '\b':
        stream << "\\b";
        break;
      case '\f':
        stream << "\\f";
        break;
      case '\n':
        stream << "\\n";
        break;
      case '\r':
        stream << "\\r";
        break;
      case '\t':
        stream << "\\t";
        break;
      case '\v':
        stream << "\\v";
        break;

      case '\\':
      case '\'':
      case '\"':
        stream << "\\" << c;
        break;

      default:
        if (c < 0x20 || c > 0x7E) {
          stream << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                 << static_cast<unsigned>(c);
        } else {
          stream << c;
        }
    }
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const VisualizeWhitespace &vis) {
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
