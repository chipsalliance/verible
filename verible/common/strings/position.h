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

#ifndef VERIBLE_COMMON_STRINGS_POSITION_H_
#define VERIBLE_COMMON_STRINGS_POSITION_H_

#include <initializer_list>
#include <string_view>

#include "verible/common/util/interval-set.h"
#include "verible/common/util/interval.h"

namespace verible {

// Returns the updated column position of text, given a starting column
// position and advancing_text.  Each newline in the advancing_text effectively
// resets the column position back to zero.  All non-newline characters count as
// one space.
int AdvancingTextNewColumnPosition(int old_column_position,
                                   std::string_view advancing_text);

// Collection of ranges of byte offsets.
// Intentionally defining a class instead of merely typedef-ing to
// IntervalSet<int> to avoid potential confusion with other IntervalSet<int>.
// Type safety will enforce intent of meaning.
class ByteOffsetSet : public verible::IntervalSet<int> {
  using impl_type = verible::IntervalSet<int>;

 public:
  ByteOffsetSet() = default;

  explicit ByteOffsetSet(const impl_type &iset) : impl_type(iset) {}

  // This constructor can initialize from a sequence of pairs, e.g.
  //   ByteOffsetSet s{{0,1}, {4,7}, {8,10}};
  ByteOffsetSet(std::initializer_list<verible::Interval<int>> ranges)
      : impl_type(ranges) {}
};

// Collection of ranges of line numbers.
// Intentionally defining this as its own class instead of a typedef to
// avoid potential confusion with other IntervalSet<int>.
// Mismatches will be caught as type errors.
class LineNumberSet : public verible::IntervalSet<int> {
  using impl_type = verible::IntervalSet<int>;

 public:
  LineNumberSet() = default;

  explicit LineNumberSet(const impl_type &iset) : impl_type(iset) {}

  // This constructor can initialize from a sequence of pairs, e.g.
  //   LineNumberSet s{{0,1}, {4,7}, {8,10}};
  LineNumberSet(std::initializer_list<verible::Interval<int>> ranges)
      : impl_type(ranges) {}
};

}  // namespace verible

#endif  // VERIBLE_COMMON_STRINGS_POSITION_H_
