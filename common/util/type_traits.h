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

#ifndef VERIBLE_COMMON_UTIL_TYPE_TRAITS_H_
#define VERIBLE_COMMON_UTIL_TYPE_TRAITS_H_

#include <type_traits>  // for std::conditional, std::is_const

namespace verible {

// Type-selector to match constness of one type to another.
// (template metaprogramming)
//
// This is useful as a pre-C++-17 workaround without support for 'auto'
// return types.
// This allow eliminates duplication of functions that have analogous
// const and non-const overloads, where the return type's constness
// matches the argument's constness, and otherwise the definition of the
// function is identical.
//
// Equivalent to using partial specialization (pre-c++11):
//   template <class T, class Other>
//   struct match_const {
//     typedef T type;
//   };
//   template <class T, class Other>
//   struct match_const<T, const Other> {
//     typedef const T type;
//   };
//
// Usage: match_const<T, Other>::type
template <class T, class Other>
using match_const =
    typename std::conditional<std::is_const<Other>::value, std::add_const<T>,
                              std::remove_const<T>>::type;

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_TYPE_TRAITS_H_
