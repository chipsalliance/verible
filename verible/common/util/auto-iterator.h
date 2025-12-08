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

#ifndef VERIBLE_COMMON_UTIL_AUTO_ITERATOR_H_
#define VERIBLE_COMMON_UTIL_AUTO_ITERATOR_H_

#include <type_traits>  // for std::conditional, std::is_const

namespace verible {

// Type-selector to choose the appropriate iterator type, based on the
// const-ness of type T (an iterable container).
// (template metaprogramming)
//
// This is useful as a pre-C++-17 workaround without support for 'auto'
// return types.
//
// Equivalent to using partial specialization (pre-c++11):
//   template <class T>
//   struct auto_iterator_selector {
//     typedef typename T::iterator type;
//   };
//   template <class T>
//   struct auto_iterator_selector<const T> {
//     typedef typename T::const_iterator type;
//   };
//
// Usage: auto_iterator_selector<ContainerType>::type
template <class T>
using auto_iterator_selector =
    std::conditional<std::is_const_v<T>, typename T::const_iterator,
                     typename T::iterator>;

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_AUTO_ITERATOR_H_
