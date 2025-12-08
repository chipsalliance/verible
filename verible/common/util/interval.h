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

#ifndef VERIBLE_COMMON_UTIL_INTERVAL_H_
#define VERIBLE_COMMON_UTIL_INTERVAL_H_

#include <iostream>
#include <string_view>
#include <utility>

#include "absl/strings/numbers.h"
#include "verible/common/util/forward.h"

namespace verible {

// An integer-valued interval, representing [min, max).
// Currently intended for direct use in IntervalSet<>.
template <typename T>
struct Interval {
  using value_type = T;
  using forwarder = ForwardReferenceElseConstruct<Interval<T>>;

  // Allow direct access.  Use responsibly.  Check valid()-ity.
  T min = {};
  T max = {};

  Interval() = default;
  Interval(const T &f, const T &s) : min(f), max(s) {}

  // Want this implicit constructor so that one can pass an initializer list
  // directly to make an Interval, e.g. Interval<> ii({x, y});
  template <typename S1, typename S2>
  Interval(  // NOLINT(google-explicit-constructor)
      const std::pair<S1, S2> &p)
      : min(p.first), max(p.second) {}

  bool empty() const { return min == max; }

  bool valid() const { return min <= max; }

  T length() const { return max - min; }

  // Returns true if integer value is in [min, max).
  bool contains(const T &value) const { return value >= min && value < max; }

  // Returns true if other range is in [min, max).
  bool contains(const Interval<T> &other) const {
    return min <= other.min && max >= other.max;
  }

  // Returns true if range [lower, upper) is in [min, max).
  bool contains(const T &lower, const T &upper) const {
    return contains(Interval<T>(lower, upper));
  }

  bool operator==(const Interval<T> &other) const {
    return min == other.min && max == other.max;
  }

  bool operator!=(const Interval<T> &other) const { return !(*this == other); }

  // Formats the interval to show an inclusive range.
  // If 'compact' is true and (min +1 == max), only print the min.
  // The inverse operation is ParseInclusiveRange().
  std::ostream &FormatInclusive(std::ostream &stream, bool compact,
                                char delim = '-') const {
    const T upper = max - 1;
    if ((min == upper) && compact) {
      stream << min;  // N instead of N-N
    } else {
      stream << min << delim << upper;
    }
    return stream;
  }
};

// Forwarding function that avoids constructing a temporary Interval.
template <typename T>
const Interval<T> &AsInterval(const Interval<T> &i) {
  return typename Interval<T>::forwarder()(i);
}

// Overloads for std::pair<> that returns a temporary Interval.
// This is useful for conveniently accessing const-methods of Interval.
// Relies on compiler optimization to elide temporary construction.
template <typename T>
Interval<T> AsInterval(const std::pair<const T, T> &p) {
  return typename Interval<T>::forwarder()(p);
}
template <typename T>
Interval<T> AsInterval(const std::pair<T, T> &p) {
  return typename Interval<T>::forwarder()(p);
}

// Default formatting of Interval<>.
template <typename T>
std::ostream &operator<<(std::ostream &stream, const Interval<T> &interval) {
  return stream << '[' << interval.min << ", " << interval.max << ')';
}

// Parses "N", "M" into an interval [N, M+1) == [N, M] (inclusive).
// Since range is inclusive, we automatically rectify backward ranges.
// Returns true on success, false on parse error.
// This is the reverse of Interval::FormatInclusive().
template <typename T>
bool ParseInclusiveRange(Interval<T> *interval, std::string_view first_str,
                         std::string_view last_str, std::ostream *errstream) {
  T first, last;
  if (!absl::SimpleAtoi(first_str, &first)) {
    *errstream << "Expected number, but got: \"" << first_str << "\"."
               << std::endl;
    return false;
  }
  if (!absl::SimpleAtoi(last_str, &last)) {
    *errstream << "Expected number, but got: \"" << last_str << "\"."
               << std::endl;
    return false;
  }
  if (last < first) {
    std::swap(first, last);
  }
  *interval = {first, last + 1};  // convert inclusive range to half-open range
  return true;
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_INTERVAL_H_
