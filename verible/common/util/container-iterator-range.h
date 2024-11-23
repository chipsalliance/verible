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

// This library provides some basic interface extensions to
// iterator_range.

#ifndef VERIBLE_COMMON_UTIL_CONTAINER_ITERATOR_RANGE_H_
#define VERIBLE_COMMON_UTIL_CONTAINER_ITERATOR_RANGE_H_

#include <cstddef>
#include <iterator>  // for std::iterator_traits
#include <utility>   // for std::pair

#include "verible/common/util/iterator-range.h"
#include "verible/common/util/range.h"

namespace verible {

// container_iterator_range provides container-like interfaces to
// an iterator_range (which only has begin() and end()).
// Like iterator_range, this does not own the memory referenced by the range.
// IT is an iterator type.
// Some methods are available for only certain iterator types.
// Any operation on the underlying container that causes internal reallocation
// invalidates these iterator ranges, e.g. vector::push_back().
template <typename IT>
class container_iterator_range : public iterator_range<IT> {
  using this_type = container_iterator_range<IT>;
  using range_type = iterator_range<IT>;

 public:
  using iterator = IT;
  using const_iterator = IT;
  using value_type = typename std::iterator_traits<IT>::value_type;
  using reference = typename std::iterator_traits<IT>::reference;

  // Allow default initialization to be usable inside resize-able containers.
  container_iterator_range() = default;

  explicit container_iterator_range(std::pair<IT, IT> p)
      : range_type(std::move(p.first), std::move(p.second)) {}

  // Constructs range from two iterators.
  // To initialize this range to empty, pass the same iterator twice, e.g.
  // container.begin().  This way it will already point to a valid range, and be
  // extendable.
  container_iterator_range(IT b, IT e)
      : range_type(std::move(b), std::move(e)) {}

  bool empty() const { return this->begin() == this->end(); }

  // Returns the number of elements that span this range.
  // The run-time complexity of this depends on that of the underlying iterator.
  size_t size() const { return std::distance(this->begin(), this->end()); }

  // Dereferences first element at begin().
  // Only valid if !this->empty(), just like a vector.
  // Available to bidirectional_iterators.
  reference front() const { return *this->begin(); }

  // Dereferences last element at end() -1.
  // Only valid if !this->empty(), just like a vector.
  // Available to bidirectional_iterators.
  reference back() const {
    auto it = this->end();
    return *(--it);
  }

  // Returns reference to the i'th element.
  // Only available to random-access iterators.
  reference operator[](size_t i) const { return *(this->begin() + i); }

  // Clears this range by moving the end back to the beginning.
  void clear_to_begin() { this->reset(this->begin(), this->begin()); }

  // Clears this range by moving the beginning forward to the end.
  void clear_to_end() { this->reset(this->end(), this->end()); }

  // Sets the lower-bound of this range, which could grow or shrink.
  void set_begin(iterator iter) { this->reset(iter, this->end()); }

  // Sets the upper-bound of this range, which could grow or shrink.
  void set_end(iterator iter) { this->reset(this->begin(), iter); }

  // Grows front-side bound by one element.
  // Caller is responsible for making sure new range still falls within
  // valid backed memory.
  // Available to bidirectional_iterators.
  void extend_front() {
    auto iter = this->begin();
    --iter;
    this->reset(iter, this->end());
  }

  // TODO(fangism): extend_front(N) for random-access iterators

  // Shrink the range from the front-side by one element.
  // Only valid if !this->empty().
  // Available to bidirectional_iterators.
  void pop_front() {
    auto iter = this->begin();
    ++iter;
    this->reset(iter, this->end());
  }

  // TODO(fangism): pop_front(N), like string_view::remove_prefix()

  // Grows back-side bound by one element.
  // Caller is responsible for making sure new range still falls within
  // valid backed memory.
  // Available to bidirectional_iterators.
  void extend_back() {
    auto iter = this->end();
    ++iter;
    this->reset(this->begin(), iter);
  }

  // TODO(fangism): extend_back(N) for random-access iterators

  // Shrink the range from the back-side by one element.
  // Only valid if !this->empty().
  // Available to bidirectional_iterators.
  void pop_back() {
    auto iter = this->end();
    --iter;
    this->reset(this->begin(), iter);
  }

  // TODO(fangism): pop_back(N), like string_view::remove_suffix()

  // TODO(fangism): subrange(N, M), like string_view::substr()

  // Returns true if begin/end bounds of iterator range are identical.
  // Templated to allow comparison between const-mismatched iterators.
  template <typename T2>
  bool operator==(const container_iterator_range<T2> &other) const {
    return BoundsEqual(*this, other);
  }

  template <typename T2>
  bool operator!=(const container_iterator_range<T2> &other) const {
    return !(*this == other);
  }

 protected:
  // Overwrite internal iterators.
  // Provided because they are private (not protected) in base class.
  void reset(iterator b, iterator e) {
    *this = container_iterator_range<IT>(b, e);
  }
};

// Convenience constructor of container_iterator_range,
// analogous to std::make_pair().
template <typename T>
container_iterator_range<T> make_container_range(T x, T y) {
  return container_iterator_range<T>(std::move(x), std::move(y));
}

// Converts std::pair<Iter,Iter> to container_iterator_range<Iter>. E.g.:
//   for (const auto& e : make_container_range(m.equal_range(k))) ...
template <typename Iter>
container_iterator_range<Iter> make_container_range(std::pair<Iter, Iter> p) {
  return container_iterator_range<Iter>(std::move(p));
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_CONTAINER_ITERATOR_RANGE_H_
