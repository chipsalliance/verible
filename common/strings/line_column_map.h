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

// LineColumnMap translates byte-offset into line-column.
//
// usage:
// absl::string_view text = ...;
// LineColumnMap lcmap(text);
// token_error_offset = ...;  // some file diagnosis
// LineColumn error_location = lcmap(token_error_offset);
// std::cout << "Error at line " << error_location << std::endl;

#ifndef VERIBLE_COMMON_STRINGS_LINE_COLUMN_MAP_H_
#define VERIBLE_COMMON_STRINGS_LINE_COLUMN_MAP_H_

#include <algorithm>
#include <cstddef>
#include <iosfwd>
#include <vector>

#include "absl/strings/string_view.h"

namespace verible {

// Pair: line number and column number.
struct LineColumn {
  int line;    // 0-based index
  int column;  // 0-based index

  bool operator==(const LineColumn& r) const {
    return line == r.line && column == r.column;
  }
};

std::ostream& operator<<(std::ostream&, const LineColumn&);

class LineColumnMap {
 public:
  explicit LineColumnMap(absl::string_view);

  explicit LineColumnMap(const std::vector<absl::string_view>& lines);

  void Clear() { beginning_of_line_offsets_.clear(); }

  bool Empty() const { return beginning_of_line_offsets_.empty(); }

  // Returns byte offset corresponding to the 0-based line number.
  // If lineno exceeds number of lines, return the final byte offset.
  int OffsetAtLine(size_t lineno) const {
    const size_t index =
        std::min(lineno, beginning_of_line_offsets_.size() - 1);
    return beginning_of_line_offsets_[index];
  }

  // Translate byte-offset into line and column.
  LineColumn operator()(int bytes_offset) const;

  const std::vector<int>& GetBeginningOfLineOffsets() const {
    return beginning_of_line_offsets_;
  }

  int EndOffset() const {
    if (Empty()) return 0;
    return beginning_of_line_offsets_.back();
  }

 private:
  // Index: line number, Value: byte offset that starts the line.
  // The first value will always be 0 because the beginning of the first line
  // has offset 0.  The last value will be the offset following the last
  // newline, which is the length of the original text.
  std::vector<int> beginning_of_line_offsets_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_LINE_COLUMN_MAP_H_
