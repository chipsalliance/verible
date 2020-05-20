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

// This library contains generic range-based utilities for which I could
// not find equivalents in the STL or absl or gtl.

#ifndef VERIBLE_COMMON_UTIL_RANGE_H_
#define VERIBLE_COMMON_UTIL_RANGE_H_

#include <algorithm>
#include <utility>

#include "common/util/logging.h"

namespace verible {

// Return true if sub is a substring inside super.
// SubRange and SuperRange types just need to support begin() and end().
// The SuperRange type could be a container or range.
// The iterator categories need to be RandomAccessIterator for less-comparison.
// This can be used to check string_view ranges and their invariants.
template <class SubRange, class SuperRange>
bool IsSubRange(const SubRange& sub, const SuperRange& super) {
  return sub.begin() >= super.begin() && sub.end() <= super.end();
}

// Returns true if the end points of the two ranges are equal, i.e. they point
// to the same slice.
// Suitable for string/string_view like objects.
// Left and right types need not be identical as long as their begin/end
// iterators are compatible.
// This allows mixed comparison of (element-owning) containers and ranges.
// Did not want to name this EqualRange to avoid confusion with
// std::equal_range (and other STL uses of that name).
// Could have also been named IntervalEqual.
template <class LRange, class RRange>
bool BoundsEqual(const LRange& l, const RRange& r) {
  return l.begin() == r.begin() && l.end() == r.end();
}

// TODO(fangism): bool RangesOverlap(l, r);

// Returns offsets [x,y] where the sub-slice of superstring from
// x to y == substring.
// Both Range types just needs to support begin(), end(), and std::distance
// between those iterators. SuperRange could be a container or range.
// Precondition: substring must be a sub-range of superstring.
template <class SubRange, class SuperRange>
std::pair<int, int> SubRangeIndices(const SubRange& substring,
                                    const SuperRange& superstring) {
  CHECK(IsSubRange(substring, superstring));
  const int begin = std::distance(superstring.begin(), substring.begin());
  const int end = std::distance(superstring.begin(), substring.end());
  return {begin, end};
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_RANGE_H_
