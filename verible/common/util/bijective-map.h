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

#ifndef VERIBLE_COMMON_UTIL_BIJECTIVE_MAP_H_
#define VERIBLE_COMMON_UTIL_BIJECTIVE_MAP_H_

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <map>
#include <utility>

#include "verible/common/util/logging.h"

namespace verible {

// 1-to-1 key-value map that maintains bijectiveness as an invariant.
// Both keys and values are stored once, and internal cross-reference
// maps point to each other's keys.
// Using std::map internally because it provides stable iterators
// across insertion/deletion operations.
template <class K, class V, class KComp = std::less<K>,
          class VComp = std::less<V>>
class BijectiveMap {
  // Map of keys to values (by pointer).
  // Would really rather have value_type be a reverse_map_type::const_iterator
  // but cannot due to mutually recursive template type.
  using forward_map_type = std::map<K, const V *, KComp>;
  // Map of values to keys (by pointer).
  // Would really rather have value_type be a forward_map_type::const_iterator
  // but cannot due to mutually recursive template type.
  using reverse_map_type = std::map<V, const K *, VComp>;

 public:
  BijectiveMap() = default;

  // Initializes from a pair of iterators (to key-value pairs).
  // Duplicate key or value entries will result in a fatal error.
  template <class Iter>
  BijectiveMap(Iter begin, Iter end) {
    for (; begin != end; ++begin) {
      CHECK(insert(*begin)) << "duplicate key or value: (" << begin->first
                            << ", " << begin->second << ')';
    }
  }

  // Initializes from a list of pairs.
  // Duplicate key or value entries will result in a fatal error.
  BijectiveMap(std::initializer_list<std::pair<K, V>> pairs)
      : BijectiveMap(pairs.begin(), pairs.end()) {}

  // Returns number of keys, which is same as number of values.
  size_t size() const { return forward_map_.size(); }

  bool empty() const { return forward_map_.empty(); }

  // read-only direct access to internal maps
  const forward_map_type &forward_view() const { return forward_map_; }
  const reverse_map_type &reverse_view() const { return reverse_map_; }

  // Lookup value given key.  Returns nullptr if not found.
  template <typename ConvertibleToKey>
  const V *find_forward(const ConvertibleToKey &k) const {
    const auto found = forward_map_.find(k);
    return found == forward_map_.end() ? nullptr : found->second;
  }

  // Lookup key given value.  Returns nullptr if not found.
  template <typename ConvertibleToValue>
  const K *find_reverse(const ConvertibleToValue &v) const {
    const auto found = reverse_map_.find(v);
    return found == reverse_map_.end() ? nullptr : found->second;
  }

  // Returns true if successfully inserted new pair.
  bool insert(const std::pair<K, V> &p) { return insert(p.first, p.second); }

  // Establishes 1-1 association between key and value.
  // Returns true if successfully inserted new pair.
  bool insert(const K &k, const V &v) {
    const auto fwd_p = forward_map_.insert(std::make_pair(k, nullptr));
    const auto rev_p = reverse_map_.insert(std::make_pair(v, nullptr));
    if (fwd_p.second && rev_p.second) {
      // cross-link
      // pointers are guaranteed to be stable across non-destructive map
      // mutations.
      fwd_p.first->second = &rev_p.first->first;
      rev_p.first->second = &fwd_p.first->first;
      return true;
    }
    // either key or value already existed in respective set
    // undo any insertions
    if (fwd_p.second) {
      forward_map_.erase(fwd_p.first);
    }
    if (rev_p.second) {
      reverse_map_.erase(rev_p.first);
    }
    return false;
  }

  // function f is a (lazy) value generator that is invoked only when the key
  // insertion succeeds.  Retries value insertion (calling generator) until
  // success.  If the range of values generated is small, this could result
  // in more frequent retries.  Use cases for this include randomizing generated
  // values, or using secondary hashes to avoid collisions.
  const V *insert_using_value_generator(const K &k, std::function<V()> f) {
    const auto fwd_p = forward_map_.insert(std::make_pair(k, nullptr));
    if (!fwd_p.second) return fwd_p.first->second;  // key entry aleady exists
    do {
      const auto rev_p = reverse_map_.insert(std::make_pair(f(), nullptr));
      if (rev_p.second) {  // successful value insertion
        // cross-link
        fwd_p.first->second = &rev_p.first->first;
        rev_p.first->second = &fwd_p.first->first;
        return fwd_p.first->second;
      }
    } while (true);
    // never reached
    return nullptr;
  }

  // TODO(fangism): erase()

 private:
  // Internal storage of keys, and pointers to values
  forward_map_type forward_map_;
  // Internal storage of values, and pointers to keys
  reverse_map_type reverse_map_;
};

// TODO(fangism): forward and reverse view adaptors

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_BIJECTIVE_MAP_H_
