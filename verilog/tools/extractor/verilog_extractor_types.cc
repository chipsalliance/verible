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

#include "verilog_extractor_types.h"
#include "absl/strings/str_cat.h"

#include <ostream>
#include <string>

std::string TypeEnumToString(Type type) {
    switch (type) {
#define CONSIDER(val) \
  case Type::val: \
    return #val;

#include "verilog/tools/extractor/verilog_extractor_types_foreach.inc"  // IWYU pragma: keep

#undef CONSIDER
        default:
            return absl::StrCat("No Associated String: ",
                                static_cast<int>(type));
    }
}

std::ostream &operator<<(std::ostream &stream, const Type &e) {
    return stream << TypeEnumToString(e);
}