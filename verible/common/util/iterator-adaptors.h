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

#ifndef VERIBLE_COMMON_UTIL_ITERATOR_ADAPTORS_H_
#define VERIBLE_COMMON_UTIL_ITERATOR_ADAPTORS_H_

#include <iterator>

#include "verible/common/util/auto-iterator.h"
#include "verible/common/util/iterator-range.h"

namespace verible {

// Convenience function that uses template argument deduction to construct
// a std::reverse_iterator from a non-reversed iterator.
// Provided for forward compatibility with C++14's std::make_reverse_iterator.
template <class Iter>
constexpr std::reverse_iterator<Iter> make_reverse_iterator(Iter i) {
  return std::reverse_iterator<Iter>(i);
}

// Construct a reverse-iterator range from a forward-iterating range.
// Type T can be a (memory-owning) container or a (non-owning) iterator range.
// The use of _iterator_selector allows this to automatically select the
// right iterator member type, depending on the const-ness of T,
// and save from having to write an explicit const T& overload.
// Note: With c++17, the return type can be simplified to 'auto'.
// It is written as such to accommodate c++11 builds.
template <class T>
verible::iterator_range<
    std::reverse_iterator<typename auto_iterator_selector<T>::type>>
reversed_view(T &t) {
  // equivalent to:
  // return make_range(t.rbegin(), t.rend());
  // but does not require the rbegin/rend methods.
  return make_range(verible::make_reverse_iterator(t.end()),
                    verible::make_reverse_iterator(t.begin()));
  // calling verible::-qualified helper to avoid ambiguity with the one provided
  // in std:: when compiling with C++14 or newer.
}

template <class T>
verible::iterator_range<
    std::reverse_iterator<typename auto_iterator_selector<T>::type>>
const_reversed_view(const T &t) {
  return make_range(verible::make_reverse_iterator(t.end()),
                    verible::make_reverse_iterator(t.begin()));
}

// Given a const_iterator and a mutable iterator to the original mutable
// container, return the corresponding mutable iterator (without resorting to
// const_cast).
template <class Iter, class ConstIter>
Iter ConvertToMutableIterator(ConstIter const_iter, Iter base) {
  auto cbase = ConstIter(base);
  return base + std::distance(cbase, const_iter);
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_ITERATOR_ADAPTORS_H_
