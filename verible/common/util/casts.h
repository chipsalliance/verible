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

#ifndef VERIBLE_COMMON_UTIL_CASTS_H_
#define VERIBLE_COMMON_UTIL_CASTS_H_

#include <cassert>
#include <type_traits>

namespace verible {
template <typename To, typename From>
inline To down_cast(From *f) {
  static_assert((std::is_base_of_v<From, std::remove_pointer_t<To>>),
                "target type not derived from source type");

  // We skip the assert and hence the dynamic_cast if RTTI is disabled.
#if !defined(__GNUC__) || defined(__GXX_RTTI)
  assert(f == nullptr || dynamic_cast<To>(f) != nullptr);
#endif  // !defined(__GNUC__) || defined(__GXX_RTTI)

  return static_cast<To>(f);
}

template <typename To, typename From>
inline To down_cast(From &f) {
  static_assert(std::is_lvalue_reference_v<To>, "target type not a reference");
  static_assert((std::is_base_of_v<From, std::remove_reference_t<To>>),
                "target type not derived from source type");

  // We skip the assert and hence the dynamic_cast if RTTI is disabled.
#if !defined(__GNUC__) || defined(__GXX_RTTI)
  assert(dynamic_cast<typename std::remove_reference<To>::type *>(&f) !=
         nullptr);
#endif  // !defined(__GNUC__) || defined(__GXX_RTTI)

  return static_cast<To>(f);
}
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_CASTS_H_
