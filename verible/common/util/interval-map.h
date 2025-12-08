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

#ifndef VERIBLE_COMMON_UTIL_INTERVAL_MAP_H_
#define VERIBLE_COMMON_UTIL_INTERVAL_MAP_H_

#include <map>
#include <utility>

#include "verible/common/util/auto-iterator.h"
#include "verible/common/util/logging.h"

namespace verible {

namespace internal {

// Comparator to treat the first value of a pair as the effective key.
template <class K>
struct CompareFirst {
  bool operator()(const K &left, const std::pair<K, K> &right) const {
    return left < right.first;
  }
  bool operator()(const std::pair<K, K> &left, const K &right) const {
    return left.first < right;
  }
  bool operator()(const std::pair<K, K> &left,
                  const std::pair<K, K> &right) const {
    return left.first < right.first;
  }

  // Enable heterogeneous lookup.
  using is_transparent = void;
};

// Returns an iterator to the map element whose key spans 'key',
// or else end().
template <typename M>                            // M is a map-type
static typename auto_iterator_selector<M>::type  // const_iterator or iterator
FindSpanningInterval(M &intervals,
                     const typename M::key_type::first_type &key) {
  const auto iter = intervals.upper_bound({key, key});
  // lower_bound misses equality condition
  if (iter != intervals.begin()) {
    const auto prev = std::prev(iter);  // look at interval before
    if (key < prev->first.second) return prev;
    // else key is greater than upper bound of this interval
  }
  // else key is less than lower bound of first interval
  return intervals.end();
}

// Returns an iterator to the map element whose key spans the whole
// range of 'value' if there is one, or else end().
template <typename M>                            // M is a map-type
static typename auto_iterator_selector<M>::type  // const_iterator or iterator
FindSpanningInterval(M &intervals, const typename M::key_type interval) {
  // Nothing 'contains' an empty interval.
  if (interval.first != interval.second) {  // not an empty interval
    // Find an interval that contains the lower bound.
    const auto iter = FindSpanningInterval(intervals, interval.first);
    if (iter != intervals.end()) {
      // Check if the same interval covers the upper bound.
      if (interval.second <= iter->first.second) return iter;
    }
  }
  return intervals.end();
}

// Returns (iterator, true) if a valid insertion point is found
// that doesn't overlap with an existing range, else (undefined, false).
// Abutment (where a start == end) does not count as overlap.
// Precondition: 'intervals' map contains non-overlapping key ranges.
template <typename M>  // M is a map-type
static std::pair<typename M::iterator, bool> FindNonoverlappingEmplacePosition(
    M &intervals, const typename M::key_type &interval) {
  if (intervals.empty()) return {intervals.end(), true};
  const auto iter =
      intervals.upper_bound({interval.first, interval.second /* don't care */});
  if (iter == intervals.begin()) {
    // Can't use CHECK_LT because iterators are not printable (logging.h).
    CHECK(interval.first <= iter->first.first);
    return {iter, interval.second <= iter->first.first};
  }
  const auto prev = std::prev(iter);
  CHECK(prev->first.first <= interval.first);
  return {iter, prev->first.second <= interval.first &&
                    (iter == intervals.end() ||
                     interval.second <= iter->first.first)};
}

}  // namespace internal

// DisjointIntervalMap is a non-overlapping set of intervals of K, mapped onto
//   corresponding values.  There need not be a direct relationship between
//   intervals of K and their values.
//
// K is a key type, where non-overlapping intervals are ranges of K.
//   K must be std::less-comparable, and support +/- arithmetic
//     e.g. random-access-iterators.
//     This requirement comes from binary-search-ability.
//   K can be numerical (need not be dereference-able),
//     or iterator-like, such as a pointer.
//   Ranges on K are interpreted as half-open [min, max).
//     This is consistent with the interpretations of iterator-pairs as ranges.
//   K-ranges may abut, but must be non-overlapping.
//   K only needs to be copy-able.
// V is a value associated with an interval of K.
//   V must be move-able.
//
// Lookup using one of the find() overloaded methods.
// Insert using emplace() or must_emplace();
// Insertion consumes the given value by rvalue-reference.
//
template <typename K, typename V>
class DisjointIntervalMap {
  // Interpreted as the interval [i,j)
  using interval_key_type = std::pair<K, K>;
  using comparator = internal::CompareFirst<K>;
  using map_type = std::map<interval_key_type, V, comparator>;

 public:
  using key_type = typename map_type::key_type;
  using mapped_type = typename map_type::mapped_type;
  using value_type = typename map_type::value_type;
  using iterator = typename map_type::iterator;
  using const_iterator = typename map_type::const_iterator;

 public:
  DisjointIntervalMap() = default;

  DisjointIntervalMap(const DisjointIntervalMap &) = delete;
  DisjointIntervalMap(DisjointIntervalMap &&) =
      delete;  // could be default if needed
  DisjointIntervalMap &operator=(const DisjointIntervalMap &) = delete;
  DisjointIntervalMap &operator=(DisjointIntervalMap &&) =
      delete;  // could be default if needed

  bool empty() const { return map_.empty(); }
  const_iterator begin() const { return map_.begin(); }
  const_iterator end() const { return map_.end(); }

  // Returns an iterator to the entry whose key-range contains 'key', or else
  // end().
  const_iterator find(const K &key) const {
    return internal::FindSpanningInterval(map_, key);
  }
  // Returns an iterator to the entry whose key-range wholly contains the 'key'
  // range, or else end().
  const_iterator find(const key_type &key) const {
    return internal::FindSpanningInterval(map_, key);
  }
  // Returns an iterator to the entry whose key-range contains 'key', or else
  // end().
  iterator find(const K &key) {
    return internal::FindSpanningInterval(map_, key);
  }
  // Returns an iterator to the entry whose key-range wholly contains the 'key'
  // range, or else end().
  iterator find(const key_type &key) {
    return internal::FindSpanningInterval(map_, key);
  }

  // Inserts a value associated with the 'key' interval if it does not overlap
  // with any other key-interval already in the map.
  // The 'value' must be moved in (emplace).
  std::pair<iterator, bool> emplace(key_type key, V &&value) {
    CHECK(key.first <= key.second);  // CHECK_LE requires printability
    const std::pair<iterator, bool> p(
        internal::FindNonoverlappingEmplacePosition(map_, key));
    // p.second: ok to emplace
    if (p.second) {
      return {map_.emplace_hint(p.first, std::move(key), std::move(value)),
              p.second};
    }
    return p;
  }

  // Same as emplace(), but fails fatally if emplacement fails,
  // and only returns the iterator to the new map entry (which should have
  // consumed 'value').
  // Recommend using this for key-ranges that correspond to allocated memory,
  // because allocators must return non-overlapping memory ranges.
  iterator must_emplace(const key_type &key, V &&value) {
    const auto p(emplace(key, std::move(value)));
    CHECK(p.second) << "Failed to emplace!";
    return p.first;
  }

  // TODO: implement when needed
  // bool erase(const K& key);

 private:
  // Internal storage of intervals and values.
  map_type map_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_INTERVAL_MAP_H_
