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
#include <utility>

namespace verible {

// An integer-valued interval, representing [min, max).
// Currently intended for direct use in IntervalSet<>.
template <typename T>
struct Interval {
  typedef T value_type;

  // Allow direct access.  Use responsibly.  Check valid()-ity.
  T min;
  T max;

  Interval(const T& f, const T& s) : min(f), max(s) {}

  // Want this implicit constructor so that one can pass an initializer list
  // directly to make an Interval, e.g. Interval<> ii({x, y});
  template <typename S1, typename S2>
  Interval(  // NOLINT(google-explicit-constructor)
      const std::pair<S1, S2>& p)
      : min(p.first), max(p.second) {}

  bool empty() const { return min == max; }

  bool valid() const { return min <= max; }

  T length() const { return max - min; }

  // Returns true if integer value is in [min, max).
  bool contains(const T& value) const { return value >= min && value < max; }

  // Returns true if other range is in [min, max).
  bool contains(const Interval<T>& other) const {
    return min <= other.min && max >= other.max;
  }

  // Returns true if range [lower, upper) is in [min, max).
  bool contains(const T& lower, const T& upper) const {
    return contains(Interval<T>(lower, upper));
  }

  bool operator==(const Interval<T>& other) const {
    return min == other.min && max == other.max;
  }

  bool operator!=(const Interval<T>& other) const { return !(*this == other); }
};

// Forwarding function that avoids constructing a temporary Interval.
template <typename T>
const Interval<T>& AsInterval(const Interval<T>& i) {
  return i;
}

// Overloads for std::pair<> that returns a temporary Interval.
// This is useful for conveniently accessing const-methods of Interval.
// Relies on compiler optimization to elide temporary construction.
template <typename T>
Interval<T> AsInterval(const std::pair<const T, T>& p) {
  return Interval<T>(p);
}
template <typename T>
Interval<T> AsInterval(const std::pair<T, T>& p) {
  return Interval<T>(p);
}

// Default formatting of Interval<>.
template <typename T>
std::ostream& operator<<(std::ostream& stream, const Interval<T>& interval) {
  return stream << '[' << interval.min << ", " << interval.max << ')';
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_INTERVAL_H_
