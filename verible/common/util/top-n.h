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

#ifndef VERIBLE_COMMON_UTIL_TOP_N_H_
#define VERIBLE_COMMON_UTIL_TOP_N_H_

#include <cstddef>
#include <functional>  // for std::greater
#include <queue>       // for std::priority_queue
#include <vector>

namespace verible {

// Maintains a collection of best N elements, as determined by a comparator.
// This class exists as a drop-in replacement for a similar library,
// and provides a subset of the original class's methods and interfaces.
//
// This priority-queue based implementation is NOT optimized;
// its performance does not matter at this time.
template <typename T, typename Comp = std::greater<T>>
class TopN {
 public:
  using value_type = T;

  explicit TopN(size_t limit) : max_size_(limit) {}

  // Capacity.
  size_t max_size() const { return max_size_; }

  // Current number of elements, always <= max_size().
  size_t size() const { return elements_.size(); }

  bool empty() const { return size() == 0; }

  // TODO(fangism): void reserve(size_t n);

  // Inserts an element in the prescribed sorted order, and caps the size (K).
  // Has same run-time as heap-insertion, no worse than O(lg K) ~ O(1).
  // Removing the worst element is a heap-remove-min-key, which is O(1).
  void push(const value_type &v) {
    elements_.push(v);
    if (size() > max_size()) {
      elements_.pop();  // remove worst element
    }
  }

  // TODO(fangism): bottom(): the next element to fall off the end, if replaced.
  // const value_type& bottom() const;

  // Returns a copy of elements ordered best-to-worst.  (nondestructive)
  std::vector<value_type> Take() const {
    // Copying a priority_queue only to pluck out its elements
    // destructively is inefficient.
    // TODO(fangism): if desired, make a destructive variant that avoids copy
    // TODO(fangism): re-implement using direct heap operations on array
    impl_type copy(elements_);
    // vector(size) constructor requires default constructibility
    std::vector<value_type> result(size());
    auto iter = result.rbegin();  // the first value is the worst, last is best
    while (!copy.empty()) {
      *iter++ = copy.top();
      copy.pop();
    }
    return result;
  }

 private:
  // Maximum number of best elements to retain.
  size_t max_size_;

  // For simplicity, use existing priority_queue internally, which implements
  // the desired ordering.
  using impl_type = std::priority_queue<T, std::vector<T>, Comp>;

  // Internal storage of elements.
  impl_type elements_;

  // Comparator (same as that used in the elements_ container)
  Comp comp_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_TOP_N_H_
