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

#ifndef VERIBLE_COMMON_UTIL_ITERATOR_RANGE_H_
#define VERIBLE_COMMON_UTIL_ITERATOR_RANGE_H_

#include <iterator>  // for std::iterator_traits
#include <utility>   // for std::move, std::pair

namespace verible {

// A range adaptor suitable for range-based for-loops.
// Modeled after c++17 library, provided for compatibility.
//
// e.g. iterating over a container without the first and last elements:
//   for (const auto& item : make_range(c.begin() +1, c.end() -1) { ... }
//
template <typename Iter>
class iterator_range {
 public:
  using iterator = Iter;
  using const_iterator = Iter;
  using value_type = typename std::iterator_traits<iterator>::value_type;

  iterator_range() = default;

  iterator_range(iterator b, iterator e)
      : begin_(std::move(b)), end_(std::move(e)) {}

  explicit iterator_range(std::pair<Iter, Iter> p)
      : begin_(std::move(p.first)), end_(std::move(p.second)) {}

  iterator_range(const iterator_range &) = default;
  iterator_range(iterator_range &&) noexcept = default;
  iterator_range &operator=(const iterator_range &) = default;
  iterator_range &operator=(iterator_range &&) noexcept = default;

  const iterator &begin() const { return begin_; }
  const iterator &end() const { return end_; }

 private:
  iterator begin_;
  iterator end_;
};

template <typename Iter>
iterator_range(Iter, Iter) -> iterator_range<Iter>;

template <typename Iter>
iterator_range(std::pair<Iter, Iter>) -> iterator_range<Iter>;

// Helper function returning an iterator_range using template argument
// deduction.
// OBSOLETE: Use iterator_range constructor directly.
template <typename Iter>
iterator_range<Iter> make_range(Iter begin, Iter end) {
  return iterator_range<Iter>(std::move(begin), std::move(end));
}

// Overload to operator on a std::pair of iterators.
// OBSOLETE: Use iterator_range constructor directly.
template <typename Iter>
iterator_range<Iter> make_range(std::pair<Iter, Iter> p) {
  return iterator_range<Iter>(std::move(p.first), std::move(p.second));
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_ITERATOR_RANGE_H_
