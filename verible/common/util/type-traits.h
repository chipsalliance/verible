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

#include <type_traits>  // IWYU pragma: export

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
using match_const = std::conditional_t<std::is_const_v<Other>,
                                       std::add_const<T>, std::remove_const<T>>;

template <class T, class Other>
using match_const_t = typename match_const<T, Other>::type;

// Detection idiom from
// C++ Extensions for Library Fundamentals, Version 2 (ISO/IEC TS 19568:2017)
// https://en.cppreference.com/w/cpp/experimental/is_detected

// Only utilities that have been needed so far are implemented.

namespace type_traits_internal {
template <class Default, class AlwaysVoid, template <class...> class Op,
          class... Args>
struct detected_impl {
  using type = Default;
};
template <class Default, template <class...> class Op, class... Args>
struct detected_impl<Default, std::void_t<Op<Args...>>, Op, Args...> {
  using type = Op<Args...>;
};

}  // namespace type_traits_internal

// Alias to `Op<Args...>` if that type is valid; otherwise alias to `Default`.
template <class Default, template <class...> class Op, class... Args>
using detected_or_t =
    typename type_traits_internal::detected_impl<Default, void, Op,
                                                 Args...>::type;

// Alias to type T with removed reference, const, and volatile qualifiers.
// Backport from C++20.
template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// Helper types for creating trait classes with a boolean member `available`
// indicating whether the traits are available for given type `T`. Similar idea
// as C++'s std::true_type and std::false_type, but with easier usage and
// without `type` and `value` member declarations (which can be used for other
// purposes).
//
// Usage example:
//
//   template <typename T /*, some SFINAE feature test */>
//   struct SomeFeatureTraits: FeatureTraits<T> { /* ... */ };
//
//   template <typename T>
//   using SomeFeature = detected_or_t<UnavailableFeatureTraits,
//                                     SomeFeatureTraits, T>;
//
// Then use `SomeFeature<T>::available` boolean for checking feature
// availability in any context.

// Type used as a placeholder for unavailable feature in type traits.
struct UnavailableFeatureTraits {
  static constexpr bool available = false;
};

// Type used as a base for available feature in type traits.
struct FeatureTraits {
  static constexpr bool available = true;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_TYPE_TRAITS_H_
