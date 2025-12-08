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

#include <utility>

#include "verible/common/util/logging.h"

namespace verible {

// Return true if sub is a substring inside super.
// SubRange and SuperRange types just need to support begin() and end().
// The SuperRange type could be a container or range.
// The iterator categories need to be RandomAccessIterator for less-comparison.
// This can be used to check string_view ranges and their invariants.
template <class SubRange, class SuperRange>
bool IsSubRange(const SubRange &sub, const SuperRange &super) {
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
bool BoundsEqual(const LRange &l, const RRange &r) {
  return l.begin() == r.begin() && l.end() == r.end();
}

// TODO(fangism): bool RangesOverlap(l, r);

// Returns offsets [x,y] where the sub-slice of 'superrange' from
// x to y == 'subrange'.
// Both Range types just needs to support begin(), end(), and std::distance
// between those iterators. SuperRange could be a container or range.
// Precondition: 'subrange' must be a sub-range of 'superrange'.
//
// Tip: This is highly useful in tests that compare ranges because
// the iterators of ranges themselves are often un-printable, however,
// pairs of integer indices or distances are printable and meaningful.
//
// Example:
//   instead of:
//     EXPECT_TRUE(BoundsEqual(range1, range2));
//
//   write:
//     EXPECT_EQ(SubRangeIndices(range1, common_base),
//               SubRangeIndices(range2, common_base));
//
//   If you don't have a common_base in the current context, you could use
//   range1 if both range types are the same.  Any differences reported will
//   be relative to range1's bounds.
//
//   To avoid passing common_base repeatedly, you could also provide:
//     auto indices = [&](const auto& range) {
//       return SubRangeIndices(range, common_base);
//     };
//     EXPECT_EQ(indices(range1), indices(range2));
//
template <class SubRange, class SuperRange>
std::pair<int, int> SubRangeIndices(const SubRange &subrange,
                                    const SuperRange &superrange) {
  const int max = std::distance(superrange.begin(), superrange.end());
  const int begin = std::distance(superrange.begin(), subrange.begin());
  const int end = std::distance(superrange.begin(), subrange.end());
  CHECK(IsSubRange(subrange, superrange))
      << "got: (" << begin << ',' << end << "), max: " << max;
  return {begin, end};
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_RANGE_H_
