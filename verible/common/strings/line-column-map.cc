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
#include "verible/common/strings/line-column-map.h"

#include <algorithm>  // for binary search
#include <cstddef>
#include <iostream>
#include <iterator>
#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/strings/utf8.h"

namespace verible {

// Print to the user as 1-based index because that is how lines
// and columns are indexed in every file diagnostic tool.
std::ostream &operator<<(std::ostream &out, const LineColumn &line_column) {
  return out << line_column.line + 1 << ':' << line_column.column + 1;
}

std::ostream &operator<<(std::ostream &out, const LineColumnRange &r) {
  // Unlike 'technical' representation where we point the end pos one past
  // the relevant range, for human consumption we want to point to the last
  // character.
  LineColumn right = r.end;
  right.column--;
  out << r.start;
  // Only if we cover more than a single character, print range of columns.
  if (r.start.line == right.line) {
    if (right.column > r.start.column) out << '-' << right.column + 1;
  } else {
    // TODO(hzeller): what is a good format to mark a range of multiple lines
    // that is understood by common editors ?
    out << ':' << right;
  }
  out << ':';
  return out;
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
LineColumnMap::LineColumnMap(const std::vector<absl::string_view> &lines) {
  size_t offset = 0;
  for (const auto &line : lines) {
    beginning_of_line_offsets_.push_back(offset);
    offset += line.length() + 1;
  }
}

LineColumn LineColumnMap::GetLineColAtOffset(absl::string_view base,
                                             int bytes_offset) const {
  const auto begin = beginning_of_line_offsets_.begin();
  const auto end = beginning_of_line_offsets_.end();
  // std::upper_bound is a binary search.
  const auto line_at_offset = std::upper_bound(begin, end, bytes_offset) - 1;
  const int line_number = std::distance(begin, line_at_offset);
  const int len_within_line = bytes_offset - *line_at_offset;
  absl::string_view line = base.substr(*line_at_offset, len_within_line);
  return LineColumn{line_number, utf8_len(line)};
}

int LineColumnMap::LineAtOffset(int bytes_offset) const {
  const auto begin = beginning_of_line_offsets_.begin();
  const auto end = beginning_of_line_offsets_.end();
  // std::upper_bound is a binary search.
  const auto line_at_offset = std::upper_bound(begin, end, bytes_offset) - 1;
  return std::distance(begin, line_at_offset);
}
}  // namespace verible
