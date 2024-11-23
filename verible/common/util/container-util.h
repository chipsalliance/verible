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

// -*- c++ -*-
#ifndef VERIBLE_COMMON_UTIL_CONTAINER_UTIL_H_
#define VERIBLE_COMMON_UTIL_CONTAINER_UTIL_H_

#include <utility>  // for std::pair

#include "verible/common/util/logging.h"

namespace verible {
namespace container {

namespace internal {

// Helper class for adapting key types to associative container value types.
// Defined by specialization only.
template <class KeyType, class ValueType>
struct ValueTypeFromKeyType;

// Constructs a default-valued pair from a key-type, for map-like types.
template <class KeyType, class MappedType>
struct ValueTypeFromKeyType<KeyType, std::pair<const KeyType, MappedType>> {
  using ValueType = std::pair<const KeyType, MappedType>;
  ValueType operator()(const KeyType &k) const {
    return {k, MappedType{}};  // default-value mapped-type
  }
};

// Partial specialization for when the KeyType == ValueType, for set-like types.
template <class KeyType>
struct ValueTypeFromKeyType<KeyType, KeyType> {
  const KeyType &operator()(const KeyType &k) const {
    return k;  // just forward the key
  }
};
}  // namespace internal

template <typename M>
bool InsertOrUpdate(M *map, const typename M::key_type &k,
                    const typename M::mapped_type &v) {
  auto ret = map->insert({k, v});
  if (ret.second) return true;
  ret.first->second = v;
  return false;
}

template <typename M>
typename M::mapped_type &InsertKeyOrDie(M *m, const typename M::key_type &k) {
  using value_inserter = internal::ValueTypeFromKeyType<typename M::key_type,
                                                        typename M::value_type>;
  auto res = m->insert(value_inserter()(k));
  CHECK(res.second) << "duplicate key: " << k;
  return res.first->second;
}

template <class M>
const typename M::mapped_type &FindWithDefault(
    M &map, const typename M::key_type &key, const typename M::mapped_type &d) {
  auto found = map.find(key);
  return (found == map.end()) ? d : found->second;
}

template <class M>
const typename M::mapped_type *FindOrNull(M &map,
                                          const typename M::key_type &k) {
  auto found = map.find(k);
  return (found == map.end()) ? nullptr : &found->second;
}

template <class M>
const typename M::mapped_type &FindOrDie(M &map,
                                         const typename M::key_type &k) {
  auto found = map.find(k);
  CHECK(found != map.end());
  return found->second;
}

}  // namespace container
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_CONTAINER_UTIL_H_
