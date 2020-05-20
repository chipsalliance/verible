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

// Implementation for LineColumnMap.
#include "common/strings/line_column_map.h"

#include <algorithm>  // for binary search
#include <cstddef>
#include <iostream>
#include <iterator>
#include <vector>

#include "absl/strings/string_view.h"

namespace verible {

// Print to the user as 1-based index because that is how lines
// and columns are indexed in every file diagnostic tool.
std::ostream& operator<<(std::ostream& output_stream,
                         const LineColumn& line_column) {
  return output_stream << line_column.line + 1 << ':' << line_column.column + 1;
}

// Records locations of line breaks, which can then be used to translate
// offsets into line:column numbers.
// Offsets are guaranteed to be monotonically increasing (sorted), and
// thus, are binary-searchable.
LineColumnMap::LineColumnMap(absl::string_view text) {
  // The column number after every line break is 0.
  // The first line always starts at offset 0.
  beginning_of_line_offsets_.push_back(0);
  auto offset = text.find('\n');
  while (offset != absl::string_view::npos) {
    beginning_of_line_offsets_.push_back(offset + 1);
    offset = text.find('\n', offset + 1);
  }
  // If the text does not end with a \n (POSIX), don't implicitly behave as if
  // there were one.
}

// Constructor that calculates line break offsets given an already-split
// set of lines for a body of text.
LineColumnMap::LineColumnMap(const std::vector<absl::string_view>& lines) {
  size_t offset = 0;
  for (const auto& line : lines) {
    beginning_of_line_offsets_.push_back(offset);
    offset += line.length() + 1;
  }
}

// Translate byte-offset into line-column.
// Byte offsets beyond the end-of-file will return an unspecified result.
LineColumn LineColumnMap::operator()(int offset) const {
  const auto begin = beginning_of_line_offsets_.begin();
  const auto end = beginning_of_line_offsets_.end();
  const auto base = std::upper_bound(begin, end, offset) - 1;
  // std::upper_bound is a binary search.
  const int line_number = std::distance(begin, base);
  const int column = offset - *base;
  return LineColumn{line_number, column};
}

}  // namespace verible
