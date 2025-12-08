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
// std::string_view text = ...;
// LineColumnMap lcmap(text);
// token_error_offset = ...;  // some file diagnosis
// LineColumn error_location = lcmap(token_error_offset);
// std::cout << "Error at line " << error_location << std::endl;

#ifndef VERIBLE_COMMON_STRINGS_LINE_COLUMN_MAP_H_
#define VERIBLE_COMMON_STRINGS_LINE_COLUMN_MAP_H_

#include <algorithm>
#include <cstddef>
#include <iosfwd>
#include <string_view>
#include <vector>

namespace verible {

// Pair: line number and column number.
struct LineColumn {
  int line;    // 0-based index
  int column;  // 0-based index

  constexpr bool operator==(const LineColumn &r) const {
    return line == r.line && column == r.column;
  }
  constexpr bool operator<(const LineColumn &r) const {
    if (line < r.line) return true;
    if (line > r.line) return false;
    return column < r.column;
  }
  constexpr bool operator>=(const LineColumn &r) const { return !(*this < r); }
};

std::ostream &operator<<(std::ostream &, const LineColumn &);

// A complete range.
struct LineColumnRange {
  LineColumn start;  // Inclusive
  LineColumn end;    // Exclusive

  constexpr bool operator==(const LineColumnRange &r) const {
    return start == r.start && end == r.end;
  }
  constexpr bool PositionInRange(const LineColumn &pos) const {
    return pos >= start && pos < end;
  }
};

std::ostream &operator<<(std::ostream &, const LineColumnRange &);

// Fast mapping of substring position to human-useful line/column
class LineColumnMap {
 public:
  // Build line column map from pre-split contiguous blob of content.
  // The distance between consecutive string_views is expected to have
  // a gap of one character (the splitting '\n' character).
  explicit LineColumnMap(const std::vector<std::string_view> &lines);

  // This constructor is only used in LintStatusFormatter and
  // LintWaiverBuilder.
  // TODO: If these already have access to pre-split lines, then this
  // constructor is not needed.
  explicit LineColumnMap(std::string_view);

  bool empty() const { return beginning_of_line_offsets_.empty(); }

  // Returns byte offset corresponding to the 0-based line number.
  // If lineno exceeds number of lines, return the final byte offset.
  int OffsetAtLine(size_t lineno) const {
    const size_t index =
        std::min(lineno, beginning_of_line_offsets_.size() - 1);
    return beginning_of_line_offsets_[index];
  }

  // Get line number at the given byte offset.
  int LineAtOffset(int bytes_offset) const;

  // Get line and column at the given offset. The column takes multi-byte
  // encodings into account, so the column represents the true character not
  // simply the byte-offset within the line.
  //
  // TODO(hzeller): technically, we don't need the base as we already got it
  // in the constructor, but change separately after lifetime questions have
  // been considered.
  LineColumn GetLineColAtOffset(std::string_view base, int bytes_offset) const;

  const std::vector<int> &GetBeginningOfLineOffsets() const {
    return beginning_of_line_offsets_;
  }

  // Byte-offset of start of last line after last newline in file.
  int LastLineOffset() const {
    return empty() ? 0 : beginning_of_line_offsets_.back();
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
