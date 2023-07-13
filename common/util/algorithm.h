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

#ifndef VERIBLE_COMMON_UTIL_ALGORITHM_H_
#define VERIBLE_COMMON_UTIL_ALGORITHM_H_

#include <algorithm>  // for std::copy
#include <utility>    // for std::pair

namespace verible {

// Same as std::set_symmetric_difference(), except that instead of outputting
// elements from both input sequences into the same output iterator, the
// differences go into separate output iterators.  This allows the algorithm
// to work on heterogeneous types.
// CompareSigned, instead of returning a bool like operator<, must return a
// negative value if left<right, 0 if left==right, positive value if
// left>right.  This change to Compare from the original algorithm is
// necessary because this comparison cannot be used commutatively to
// determine equality due to its heterogeneity.
// Respective input sequences must be ordered (in relation to their own
// element types).
// We don't provide a variant of set_symmetric_difference_split with a default
// comparator, because a default heterogeneous signed-compare is not intuitive.
template <class InputIt1, class InputIt2, class OutputIt1, class OutputIt2,
          class CompareSigned>
std::pair<OutputIt1, OutputIt2> set_symmetric_difference_split(
    InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2,
    OutputIt1 diff1, OutputIt2 diff2, CompareSigned comp) {
  while (first1 != last1) {
    if (first2 == last2) {
      return std::make_pair(std::copy(first1, last1, diff1), diff2);
    }
    const auto sign = comp(*first1, *first2);
    if (sign < 0) {
      *diff1++ = *first1++;
    } else if (sign > 0) {
      *diff2++ = *first2++;
    } else {  // sign == 0
      ++first1;
      ++first2;
    }
  }
  return std::make_pair(diff1, std::copy(first2, last2, diff2));
}

// Assigns to the output all iterators at which the predicate evaluates true,
// like repeated calls to std::find_if().
// Run time: O(N) because every element in [iter, end) is visited exactly once.
// Predicate may also be stateful.
// This can be very useful for partitioning ranges into sub-ranges,
// by finding boundaries at which to sub-divide.
template <class InputIter, class OutputIter, class Predicate>
void find_all(InputIter iter, InputIter end, OutputIter output,
              Predicate pred) {
  for (; iter != end; ++iter) {
    if (pred(*iter)) *output = iter;
  }
}

// Adapts std::lexicographical_compare to work on two sequences (including
// containers, ranges, spans, views).
struct LexicographicalLess {
  // Enable heterogenous lookup.
  using is_transparent = void;

  // Compare two sequences lexicographically, element-by-element.
  template <class T1, class T2>
  bool operator()(const T1 &left, const T2 &right) const {
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(),
                                        right.end());
  }
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_ALGORITHM_H_
