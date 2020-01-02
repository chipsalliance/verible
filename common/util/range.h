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

namespace verible {

// Return true if sub is a substring inside super.
// This is mostly used to check string_view ranges and their invariants.
template <class Range>
bool IsSubRange(const Range& sub, const Range& super) {
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

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_RANGE_H_
