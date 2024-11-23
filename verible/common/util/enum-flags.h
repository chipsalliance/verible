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
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "verible/common/strings/compare.h"
#include "verible/common/util/bijective-map.h"

namespace verible {

namespace internal {
// Functor that extracts the first element of a pair and appends it to a string.
// Suitable for use with absl::StrJoin()'s formatter arguments.
struct FirstElementFormatter {
  template <class P>
  void operator()(std::string *out, const P &p) const {
    out->append(p.first.begin(), p.first.end());
  }
};

// For enums to be comparable for uniqueness/mapping sake,
// force interpret them as integers.
struct EnumCompare {
  template <class E>
  bool operator()(E left, E right) const {
    return static_cast<int>(left) < static_cast<int>(right);
  }
};
}  // namespace internal

/**
EnumNameMap provides a consistent way to parse and unparse enumerations with
string/named representations.
This makes enumeration types easy to use with absl flags.

Usage:

////////////////// .h header file ///////////////////
// Define an enum in public header.
enum class MyEnumType { ... };

// Conventional stream printer (declared in header providing enum).
std::ostream& operator<<(std::ostream& stream, MyEnumType p);

// If making this usable as an absl flag, provide the following overloads:
bool AbslParseFlag(absl::string_view text, MyEnumType* mode,
                   std::string* error);
std::string AbslUnparseFlag(const MyEnumType& mode);


/////////////// .cc implementation file ////////////////
const EnumNameMap<MyEnumType> MyEnumTypeNames{{
  {"enum1", MyEnumType::kEnum1},
  {"enum2", MyEnumType::kEnum2},
  ...
}};

std::ostream& operator<<(std::ostream& stream, MyEnumType p) {
  return MyEnumTypeNames.Unparse(p, stream);
}

bool AbslParseFlag(absl::string_view text, MyEnumType* mode,
                   std::string* error) {
  return kLanguageModeStringMap.Parse(text, mode, error, "MyEnumType");
}

std::string AbslUnparseFlag(const MyEnumType& mode) {
  std::ostringstream stream;
  stream << mode;
  return stream.str();
}

**/
template <typename EnumType>
class EnumNameMap {
  // Using a string_view is safe when the string-memory owned elsewhere is
  // guaranteed to outlive objects of this class.
  // String-literals are acceptable sources of string_views for this purpose.
  using key_type = absl::string_view;

  // Storage type for mapping information.
  // StringViewCompare gives the benefit of copy-free heterogeneous lookup.
  using map_type = BijectiveMap<key_type, EnumType, StringViewCompare,
                                internal::EnumCompare>;

 public:
  // Pairs must contain unique keys or values, or this will result in a fatal
  // error.
  EnumNameMap(std::initializer_list<std::pair<key_type, EnumType>> pairs)
      : enum_name_map_(pairs) {}
  ~EnumNameMap() = default;

  EnumNameMap(const EnumNameMap &) = delete;
  EnumNameMap(EnumNameMap &&) = delete;
  EnumNameMap &operator=(const EnumNameMap &) = delete;
  EnumNameMap &operator=(EnumNameMap &&) = delete;

  // Print a list of string representations of the enums.
  std::ostream &ListNames(std::ostream &stream, absl::string_view sep) const {
    return stream << absl::StrJoin(enum_name_map_.forward_view(), sep,
                                   internal::FirstElementFormatter());
  }

  // Converts the name of an enum to its corresponding value.
  // 'type_name' is a text name for the enum type used in diagnostics.
  // This variant write diagnostics to the 'errstream' stream.
  // Returns true if successful.
  bool Parse(key_type text, EnumType *enum_value, std::ostream &errstream,
             absl::string_view type_name) const {
    const EnumType *found_value = enum_name_map_.find_forward(text);
    if (found_value != nullptr) {
      *enum_value = *found_value;
      return true;
    }
    errstream << "Invalid " << type_name << ": '" << text
              << "'\nValid options are: ";
    ListNames(errstream, ",");
    return false;
  }

  // Converts the name of an enum to its corresponding value.
  // This variant write diagnostics to the 'error' string.
  bool Parse(key_type text, EnumType *enum_value, std::string *error,
             absl::string_view type_name) const {
    std::ostringstream stream;
    const bool success = Parse(text, enum_value, stream, type_name);
    *error += stream.str();
    return success;
  }

  // Returns the string representation of an enum.
  absl::string_view EnumName(EnumType value) const {
    const auto *key = enum_name_map_.find_reverse(value);
    if (key == nullptr) return "???";
    return *key;
  }

  // Prints the string representation of an enum to stream.
  std::ostream &Unparse(EnumType value, std::ostream &stream) const {
    return stream << EnumName(value);
  }

 protected:  // for testing
  // Stores the enum/string mapping.
  map_type enum_name_map_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_ENUM_FLAGS_H_
