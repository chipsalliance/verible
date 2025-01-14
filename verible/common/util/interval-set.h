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

#ifndef VERIBLE_COMMON_UTIL_INTERVAL_SET_H_
#define VERIBLE_COMMON_UTIL_INTERVAL_SET_H_

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "verible/common/util/auto-iterator.h"
#include "verible/common/util/interval.h"
#include "verible/common/util/iterator-range.h"
#include "verible/common/util/logging.h"

namespace verible {

// Prints a sequence of intervals to stream.
// Iter can point to any Interval<> or any type constructible to Interval<>
// (like std::pair).
template <class Iter>
std::ostream &FormatIntervals(std::ostream &stream, Iter begin, Iter end) {
  using value_type = typename std::iterator_traits<Iter>::value_type;
  return stream << absl::StrJoin(
             begin, end, ", ",
             [](std::string *out, const value_type &interval) {
               std::ostringstream temp_stream;
               temp_stream << AsInterval(interval);
               out->append(temp_stream.str());
             });
}

namespace internal {
// Non-template private implementation class of IntervalSet.
class IntervalSetImpl {
 protected:
  // Returns the first (interval) iterator that spans or follows 'value'.
  // Implementing this way avoids duplication between the const_iterator and
  // iterator variants.
  template <typename M>                            // M is a map-type
  static typename auto_iterator_selector<M>::type  // const_iterator or iterator
  FindLowerBound(M &intervals, const typename M::mapped_type &value) {
    const auto lower_bound = intervals.lower_bound(value);
    if (lower_bound == intervals.begin()) {
      return lower_bound;
    }

    // Check whether the previous interval's .max spans 'value'.
    const auto prev = std::prev(lower_bound);
    if (AsInterval(*prev).contains(value)) {
      return prev;
    }
    return lower_bound;
  }

  // Returns an iterator in the 'intervals' map whose range spans that of
  // 'interval', if one exists, else end().
  template <typename M>                            // M is a map-type
  static typename auto_iterator_selector<M>::type  // const_iterator or iterator
  FindSpanningInterval(M &intervals,
                       const Interval<typename M::mapped_type> &interval) {
    CHECK(interval.valid());
    // Nothing 'contains' an empty interval.
    if (!interval.empty()) {
      // Find an interval that contains the lower bound.
      const auto found = FindSpanningInterval(intervals, interval.min);
      if (found != intervals.end()) {
        // Check if the same interval covers the upper bound.
        if (interval.max <= found->second) return found;
      }
    }
    return intervals.end();
  }

  // Variant that finds an interval that covers a single value.
  // This is more efficient than checking for [value, value +1).
  template <typename M>                            // M is a map-type
  static typename auto_iterator_selector<M>::type  // const_iterator or iterator
  FindSpanningInterval(M &intervals, const typename M::mapped_type &value) {
    const auto upper_bound = intervals.upper_bound(value);
    // lower_bound misses equality condition
    if (upper_bound != intervals.begin()) {
      const auto prev = std::prev(upper_bound);  // look at interval before
      if (AsInterval(*prev).contains(value)) {
        return prev;
      }
      // else value is greater than upper bound of this interval
    }
    // else value is less than lower bound of first interval
    return intervals.end();
  }

  // Returns (iterator, true) if a valid insertion point is found
  // that doesn't overlap with an existing range, else (undefined, false).
  // Abutment (where a start == end) does not count as overlap.
  // Precondition: 'intervals' map contains non-overlapping key ranges.
  template <typename M>  // M is a map-type
  static std::pair<typename M::iterator, bool>
  FindNonoverlappingEmplacePosition(M &intervals,
                                    const typename M::mapped_type &min,
                                    const typename M::mapped_type &max) {
    if (intervals.empty()) return {intervals.end(), true};
    const auto iter = intervals.upper_bound(min);
    if (iter == intervals.begin()) {
      // Can't use CHECK_LT because iterators are not printable (logging.h).
      CHECK(min <= iter->first);
      return {iter, max <= iter->first};
    }
    const auto prev = std::prev(iter);
    CHECK(prev->first <= min);
    return {iter, prev->second <= min &&
                      (iter == intervals.end() || max <= iter->first)};
  }
};

}  // namespace internal

// IntervalSet represents a set of integral values.
// Set membership is efficiently represented as a collection of
// non-overlapping [min, max) intervals.
// Mutating operations will automatically merge abutting intervals.
// Type T must be std::less-comparable for binary-search-ability.
template <typename T>
class IntervalSet : private internal::IntervalSetImpl {
 private:
  using impl_type = std::map<T, T>;

 protected:
  using iterator = typename impl_type::iterator;
  using reverse_iterator = typename impl_type::reverse_iterator;

 public:
  using value_type = typename impl_type::value_type;
  using const_iterator = typename impl_type::const_iterator;
  using const_reverse_iterator = typename impl_type::const_reverse_iterator;
  using size_type = typename impl_type::size_type;

 public:
  IntervalSet() = default;
  IntervalSet(std::initializer_list<Interval<T>> ranges) {
    // Add-ing will properly fuse overlapping intervals and maintain intervals_'
    // invariants.
    for (const auto &range : ranges) {
      Add(range);
    }
  }

  IntervalSet(const IntervalSet<T> &) = default;
  IntervalSet(IntervalSet<T> &&) noexcept = default;
  ~IntervalSet() { CheckIntegrity(); }

  IntervalSet<T> &operator=(const IntervalSet<T> &) = default;
  IntervalSet<T> &operator=(IntervalSet<T> &&) noexcept = default;

 public:
  const_iterator begin() const { return intervals_.begin(); }

  const_iterator end() const { return intervals_.end(); }

  // Returns the number of disjoint intervals that compose this set.
  size_type size() const { return intervals_.size(); }

  // Returns sum of sizes of all intervals.
  size_type sum_of_sizes() const;

  // Returns true if the set contains no intervals/values.
  bool empty() const { return intervals_.empty(); }

  // Remove all intervals from the set.
  void clear() { intervals_.clear(); }

  void swap(IntervalSet<T> &other) noexcept {
    intervals_.swap(other.intervals_);
  }

  bool operator==(const IntervalSet<T> &other) const {
    return intervals_ == other.intervals_;
  }

  bool operator!=(const IntervalSet<T> &other) const {
    return !(*this == other);
  }

  // Returns true if value is a member of an interval in the set.
  bool Contains(const T &value) const {
    return Find(value) != intervals_.end();
  }

  // Returns true if interval is entirely contained by an interval in the set.
  // If interval is empty, return false.
  bool Contains(const Interval<T> &interval) const {
    return Find(interval) != intervals_.end();
  }

  // TODO(fangism): bool Contains(const IntervalSet<T>& interval) const;

  // Returns the first (interval) iterator that spans or follows 'value'.
  const_iterator LowerBound(const T &value) const {
    return internal::IntervalSetImpl::FindLowerBound(intervals_, value);
  }

  // Returns the first (interval) iterator that follows 'value'.
  const_iterator UpperBound(const T &value) const {
    return intervals_.upper_bound(value);
  }

  // Returns an iterator to the interval that entirely contains [min,max),
  // or the end iterator if no such interval exists, or the input is empty.
  const_iterator Find(const Interval<T> &interval) const {
    return internal::IntervalSetImpl::FindSpanningInterval(intervals_,
                                                           interval);
  }

  // Returns an iterator to the interval that contains 'value',
  // or the end iterator if no such interval exists.
  const_iterator Find(const T &value) const {
    return internal::IntervalSetImpl::FindSpanningInterval(intervals_, value);
  }

  // Adds an interval to the interval set.
  // Also fuses any intervals that may result from the addition.
  void Add(const Interval<T> &interval) {
    CHECK(interval.valid());
    if (interval.empty()) return;  // adding empty interval changes nothing
    const auto &min = interval.min;
    const auto &max = interval.max;

    T new_max = max;
    iterator erase_end;
    const auto max_lb = LowerBound(max);
    if (max_lb == intervals_.end()) {
      // erase all the way to the end
      erase_end = max_lb;
    } else if (AsInterval(*max_lb).contains(max)) {
      // erase this interval, but use its max (.second)
      erase_end = std::next(max_lb);
      new_max = max_lb->second;
    } else {
      // erase up to the interval before this one
      erase_end = max_lb;
    }

    iterator erase_begin;
    const auto p = intervals_.insert({min, new_max});  // (bool, iterator)
    if (p.second) {
      // new interval was successfully inserted at p.first
      // If this abuts or overlaps with the previous interval, update that
      // one with new_max, and discard this one.
      if (p.first == intervals_.begin()) {
        erase_begin = std::next(p.first);
      } else {
        const auto prev = std::prev(p.first);
        if (prev->second >= min) {
          prev->second = new_max;
          erase_begin = p.first;
        } else {
          erase_begin = std::next(p.first);
        }
      }
    } else {
      // new interval was not inserted because there already exist key=min.
      // Re-use that interval, but update its max.
      p.first->second = new_max;
      // Erase intervals starting after that one.
      erase_begin = std::next(p.first);
    }

    // Finally erase range of obsolete intervals to maintain invariants.
    intervals_.erase(erase_begin, erase_end);

    CheckIntegrity();
  }

  // Adds a single value to the interval set.
  void Add(const T &value) { Add({value, value + 1}); }

  // Removes an interval from the set.
  // Run-time: O(lg N), where N is the number of existing intervals.
  void Difference(const Interval<T> &interval) {
    CHECK(interval.valid());
    if (interval.empty()) return;  // removing an empty interval changes nothing
    const auto &min = interval.min;
    const auto &max = interval.max;

    iterator erase_end;
    bool replace_upper = false;
    Interval<T> replaced_upper_interval;
    const auto max_ub = UpperBound(max);
    if (max_ub == intervals_.begin()) {
      return;  // interval is out of range of this set
    }
    {
      const auto prev = std::prev(max_ub);
      if (AsInterval(*prev).contains(max)) {
        if (prev->first == max) {
          erase_end = prev;  // erase up to this interval
        } else {
          // max falls in the middle of this prev interval
          erase_end = max_ub;  // erase including this interval
          // Add new interval with higher min, which will be ordered after prev.
          replaced_upper_interval = {max, prev->second};
          replace_upper = replaced_upper_interval.valid() &&
                          !replaced_upper_interval.empty();
        }
      } else {
        erase_end = max_ub;
      }
    }

    iterator erase_begin;
    bool replace_lower = false;
    Interval<T> replaced_lower_interval;
    const auto min_lb = LowerBound(min);
    if (min_lb == intervals_.end()) {
      return;  // interval is out of range of this set
    }
    if (AsInterval(*min_lb).contains(min)) {
      if (min_lb->first == min) {
        erase_begin = min_lb;  // erase starting at this interval
        replaced_lower_interval = {max, min_lb->second};
        replace_lower =
            replaced_lower_interval.valid() && !replaced_lower_interval.empty();
      } else {
        // FIXME
        min_lb->second = min;  // reduce the upper bound of matching interval
        erase_begin = std::next(min_lb);  // erase starting past this interval
      }
    } else {  // min is to the left of the first interval we might erase
      erase_begin = min_lb;
    }

    // Erase range of obsolete intervals.
    intervals_.erase(erase_begin, erase_end);

    // Add new interval as necessary.
    if (replace_lower) {
      AddUnsafe(replaced_lower_interval);
    } else if (replace_upper) {
      AddUnsafe(replaced_upper_interval);
    }

    CheckIntegrity();
  }

  // Removes a single value from the interval set.
  void Difference(const T &value) { Difference({value, value + 1}); }

  // Subtracts all intervals in the other set from this one.
  void Difference(const IntervalSet<T> &iset) {
    // TODO(fangism): optimize by implementing with two advancing iterators,
    // like linear-time sorted-sequence set operations.
    for (const auto &interval : iset) {
      Difference(AsInterval(interval));
    }
  }

  // Adds all intervals in the other set from this one.
  void Union(const IntervalSet<T> &iset) {
    // Could be optimized with a hand-written linear-merge.
    for (const auto &interval : iset) {
      Add(AsInterval(interval));
    }
  }

  // Inverts the set of integers with respect to the given interval bound.
  void Complement(const Interval<T> &interval) {
    // This could be more efficient with a direct insertion of elements.
    IntervalSet<T> temp{{interval}};
    temp.Difference(*this);
    swap(temp);
  }

  // Point-to-point transforms one interval set into another using
  // a strictly monotonic function (which may be inverting).
  // Interpretation of inverted interval bounds is up to the user.
  template <typename S>
  IntervalSet<S> MonotonicTransform(std::function<S(T)> func) const {
    IntervalSet<S> result;
    for (const auto &interval : intervals_) {
      S left = func(interval.first);
      S right = func(interval.second);
      // ignore empty intervals that may result from range compression
      if (left == right) continue;
      if (left > right) std::swap(left, right);  // inverting
      result.AddUnsafe({left, right});
    }
    result.CheckIntegrity();
    return result;
  }

  // Returns a generator that returns a random element of the interval set,
  // uniformly distributed.  The distribution is taken as a snapshot of the
  // current interval set; subsequent modifications will not affect the returned
  // generator; this object is safe to destroy with the generator still being
  // valid.
  std::function<T()> UniformRandomGenerator() const {
    struct cumulative_weighted_interval {
      // cumulative distribution from lowest interval to the highest interval
      size_t cumulative_weight;
      Interval<T> interval;
    };
    // comparator for binary search
    auto less = [](size_t l, const cumulative_weighted_interval &r) {
      return l < r.cumulative_weight;
    };

    // build cumulative distribution array for weighted random sampling
    std::vector<cumulative_weighted_interval> interval_map;
    CHECK(!empty()) << "Non-empty interval set required for random generator";
    size_t cumulative_size = 0;
    for (const auto &range : intervals_) {
      const auto interval = AsInterval(range);
      interval_map.push_back({cumulative_size, interval});
      cumulative_size += interval.length();
    }
    // here, cumulative_size == sum_of_sizes().

    return [=]() {
      static absl::BitGen gen;
      const size_t rand = absl::Uniform<size_t>(gen, 0, cumulative_size);
      // Convert effectively from uniform to weighted random, by interval size.
      // binary_search (upper_bound) is O(lg N) where N is the number
      // of disjoint intervals.
      const auto interval_iter = std::prev(std::upper_bound(
          interval_map.begin(), interval_map.end(), rand, less));
      // rand - interval_iter->cumulative_weight can be used as the offset
      // into the chosen interval, which is already uniformly distributed.
      return interval_iter->interval.min + rand -
             interval_iter->cumulative_weight;
    };
  }

  std::ostream &FormatInclusive(std::ostream &stream, bool compact,
                                char delim = '-') const {
    return stream << absl::StrJoin(
               intervals_, ",",
               [=](std::string *out, const value_type &interval) {
                 std::ostringstream temp_stream;
                 AsInterval(interval).FormatInclusive(temp_stream, compact,
                                                      delim);
                 out->append(temp_stream.str());
               });
  }

 protected:
  // This operation is only intended for constructing test expect values.
  // It does not guarantee any invariants among intervals_.
  void AddUnsafe(const Interval<T> &interval) {
    CHECK(interval.valid());
    CHECK(!interval.empty());
    intervals_[interval.min] = interval.max;
  }

  // Checks invariant properties described in class description.
  void CheckIntegrity() const {
    using interval_type = Interval<T>;
    if (intervals_.empty()) return;

    // Check front outside of loop.
    const_iterator iter(intervals_.begin());
    const const_iterator end(intervals_.end());
    {
      const interval_type ii(*iter);
      CHECK(ii.valid());
      CHECK(!ii.empty());
    }
    // Track previous max, and check the rest in loop.
    T prev_max = iter->second;
    for (++iter; iter != end; ++iter) {
      {
        const interval_type ii(*iter);
        CHECK(ii.valid());
        CHECK(!ii.empty());
      }
      CHECK_LT(prev_max, iter->first);
      prev_max = iter->second;
    }
  }

  // Mutable variants of Find(), LowerBound() are protected to preserve
  // invariants.
  iterator Find(const Interval<T> &interval) {
    return internal::IntervalSetImpl::FindSpanningInterval(intervals_,
                                                           interval);
  }
  iterator Find(const T &value) {
    return internal::IntervalSetImpl::FindSpanningInterval(intervals_, value);
  }
  iterator LowerBound(const T &value) {
    return internal::IntervalSetImpl::FindLowerBound(intervals_, value);
  }
  iterator UpperBound(const T &value) { return intervals_.upper_bound(value); }

 private:
  // Internal storage of intervals.
  // Invariants: all intervals are
  //   * non-overlapping
  //   * non-empty
  //   * ordered (by interval.min).
  impl_type intervals_;
};  // class IntervalSet

template <typename T>
void swap(IntervalSet<T> &t1, IntervalSet<T> &t2) noexcept {
  t1.swap(t2);
}

template <typename T>
std::ostream &operator<<(std::ostream &stream, const IntervalSet<T> &iset) {
  // Format each IntervalSet internal interval as an Interval<T>.
  return FormatIntervals(stream, iset.begin(), iset.end());
}

// Parses a sequence of range specifications, each which can be a single value
// or a range like N-M (similar to page-numbers for printing).
// Overlapping/adjoining ranges are automatically merged by IntervalSet.
// Iter is any iterator that points to a string (or string-like).
// Returns false on any parse eror, true on complete success.
template <typename T, typename Iter>
bool ParseInclusiveRanges(IntervalSet<T> *iset, Iter begin, Iter end,
                          std::ostream *errstream, const char sep = '-') {
  std::vector<std::string_view> bounds;  // re-use allocated memory
  for (const auto &range : verible::make_range(begin, end)) {
    bounds = absl::StrSplit(range, sep);
    if (bounds.size() == 1) {
      const auto &arg = bounds.front();
      // ignore blanks, which comes from splitting ""
      if (arg.empty()) continue;
      int line_number;
      if (!absl::SimpleAtoi(arg, &line_number)) {
        *errstream << "Expected number, but got: \"" << arg << "\"."
                   << std::endl;
        return false;
      }
      iset->Add(line_number);
    } else if (bounds.size() >= 2) {
      Interval<T> interval;
      if (!ParseInclusiveRange(&interval, bounds.front(), bounds.back(),
                               errstream)) {
        return false;
      }
      iset->Add(interval);
    }
  }
  return true;
}

//------------------------------------------------------------------------------
// DisjointIntervalSet
//------------------------------------------------------------------------------

// DisjointIntervalSet is a collection of non-overlapping intervals (abutment
// permitted).
// Type T must be std::less-comparable for binary-search-ability.
// e.g. integers, pointers, random-access-iterators.
// When T is a pointer or iterator, this does not maintain any ownership of the
// spanned ranges.
//
// See also DisjointIntervalMap in interval_map.h.
template <typename T>
class DisjointIntervalSet : private internal::IntervalSetImpl {
 private:
  // This makes the value_type an immutable std::pair<const T, const T>.
  using impl_type = std::map<T, const T>;

 public:
  using key_type = typename impl_type::key_type;
  using value_type = typename impl_type::value_type;
  using mapped_type = typename impl_type::mapped_type;
  using const_iterator = typename impl_type::const_iterator;
  using const_reverse_iterator = typename impl_type::const_reverse_iterator;
  using size_type = typename impl_type::size_type;

 public:
  DisjointIntervalSet() = default;

  DisjointIntervalSet(const DisjointIntervalSet &) = default;
  DisjointIntervalSet(DisjointIntervalSet &&) noexcept = default;
  DisjointIntervalSet &operator=(const DisjointIntervalSet &) = default;
  DisjointIntervalSet &operator=(DisjointIntervalSet &&) noexcept = default;

  bool empty() const { return intervals_.empty(); }
  const_iterator begin() const { return intervals_.begin(); }
  const_iterator end() const { return intervals_.end(); }

  // Returns an iterator to the entry whose key-range contains 'key', or else
  // end().
  const_iterator find(const T &key) const {
    return FindSpanningInterval(intervals_, key);
  }
  // Returns an iterator to the entry whose key-range wholly contains the 'key'
  // range, or else end().
  const_iterator find(const std::pair<T, T> &key) const {
    return FindSpanningInterval(intervals_, key);
  }

  // Inserts a value associated with the 'key' interval if it does not overlap
  // with any other key-interval already in the map.
  // The 'value' must be moved in (emplace).
  std::pair<const_iterator, bool> emplace(const T &min_key, const T &max_key) {
    CHECK(min_key <= max_key);  // CHECK_LE requires printability
    const std::pair<const_iterator, bool> p(
        FindNonoverlappingEmplacePosition(intervals_, min_key, max_key));
    // p.second: ok to emplace
    if (p.second) {
      return {intervals_.emplace_hint(p.first, min_key, max_key), p.second};
    }
    return p;
  }

  // Erase given interval.
  const_iterator erase(const_iterator pos) { return intervals_.erase(pos); }

  // Same as emplace(), but fails fatally if emplacement fails,
  // and only returns the iterator to the new map entry (which should have
  // consumed 'value').
  // Recommend using this for key-ranges that correspond to allocated memory,
  // because allocators must return non-overlapping memory ranges.
  const_iterator must_emplace(const T &min_key, const T &max_key) {
    const auto p(emplace(min_key, max_key));
    CHECK(p.second) << "Failed to emplace!";
    return p.first;
  }

 private:
  impl_type intervals_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_INTERVAL_SET_H_
