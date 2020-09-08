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

#ifndef VERIBLE_COMMON_UTIL_ENUM_FLAGS_H_
#define VERIBLE_COMMON_UTIL_ENUM_FLAGS_H_

#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "common/strings/compare.h"
#include "common/util/logging.h"

namespace verible {

/**
This library provides some boilerplate functions for handling enums as flags.
The main benefit of this is having to only specify the relationship between
values and their string names once in an initializer_list.
This operates smoothly with the absl flags library.

Usage:
(in your .h file)
namespace your_namespace {

enum class MyEnumType {
  kValue1,
  kValue2,
  kValue3,
  ...
};

namespace internal {
// data that is private to the implementation and tests
extern const std::initializer_list<
    std::pair<const absl::string_view, MyEnumType>>
    kMyEnumTypeStringMap;
}  // namespace internal

std::ostream& operator<<(std::ostream&, MyEnumType);

// For use with the absl flags library, declare the following:

bool AbslParseFlag(absl::string_view, MyEnumType*, std::string*);

std::string AbslUnparseFlag(const MyEnumType&);

}  // namespace your_namespace


(in your .cc file, which includes your .h file)

namespace your_namespace {
namespace internal {
// This mapping defines how this enum is displayed and parsed.
// By providing this mapping, the other necessary flag functions can be
// generated for you.
const std::initializer_list<
    std::pair<const absl::string_view, MyEnumType>>
    kMyEnumTypeStringMap = {
        {"value1", MyEnumType::kValue1},
        {"value2", MyEnumType::kValue2},
        {"value3", MyEnumType::kValue3},
        // etc.
};
}  // namespace internal

// Conventional stream printer (declared in header providing enum).
std::ostream& operator<<(std::ostream& stream, MyEnumType p) {
  static const auto* flag_map =
      MakeEnumToStringMap(internal::kMyEnumTypeStringMap);
  return stream << flag_map->find(p)->second;
}

bool AbslParseFlag(absl::string_view text, MyEnumType* mode,
                   std::string* error) {
  static const auto* flag_map =
      MakeStringToEnumMap(internal::kMyEnumTypeStringMap);
  return EnumMapParseFlag(*flag_map, text, mode, error);
}

std::string AbslUnparseFlag(const MyEnumType& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

}  // namespace your_namespace
**/

// Functor that extracts the first element of a pair and appends it to a string.
// Suitable for use with absl::StrJoin()'s formatter arguments.
struct FirstElementFormatter {
  template <class P>
  void operator()(std::string* out, const P& p) const {
    out->append(std::string(p.first));
  }
};

// Looks up an enum value given its string name, and sets it by reference.
// Suitable for use with AbslParseFlag().
template <typename M>
bool EnumMapParseFlag(const M& flag_map, absl::string_view text,
                      typename M::mapped_type* flag, std::string* error) {
  const auto p = flag_map.find(text);
  if (p != flag_map.end()) {
    *flag = p->second;
    return true;
  } else {
    *error = absl::StrCat(
        "unknown value for enumeration '", text, "'.  Valid options are: ",
        absl::StrJoin(flag_map, ",", FirstElementFormatter()));
    return false;
  }
}

// For enums to be comparable for uniqueness/mapping sake,
// force interpret them as integers.
struct EnumCompare {
  template <class E>
  bool operator()(E left, E right) const {
    return static_cast<int>(left) < static_cast<int>(right);
  }
};

// Constructs a map<S, V> from a sequence of pair<S, V>'s.
// V is an enum type, S is only string_view
template <class V>
const std::map<absl::string_view, V, verible::StringViewCompare>*
MakeStringToEnumMap(
    std::initializer_list<std::pair<const absl::string_view, V>> elements) {
  return new std::map<absl::string_view, V, verible::StringViewCompare>(
      elements);
}

// Constructs a (reverse) map<V, S> from a sequence of pair<S, V>'s.
// V is an enum type, S is only string_view
// Fatal error if any keys are duplicate.
template <class V>
const std::map<V, absl::string_view, EnumCompare>* MakeEnumToStringMap(
    std::initializer_list<std::pair<const absl::string_view, V>> elements) {
  auto* result = new std::map<V, absl::string_view, EnumCompare>;
  for (const auto& p : elements) {
    const auto it = result->emplace(p.second, p.first);
    CHECK(it.second) << "Duplicate element forbidden at key: " << p.second;
  }
  return result;
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_ENUM_FLAGS_H_
