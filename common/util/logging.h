// Copyright 2017-2022 The Verible Authors.
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

#ifndef VERIBLE_COMMON_UTIL_LOGGING_H_
#define VERIBLE_COMMON_UTIL_LOGGING_H_

#ifdef __GNUC__
// glog/logging does some integer signed/unsigned compares
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

// Quiet fatal logging does not exist in glog; work around here
#define COMPACT_GOOGLE_LOG_QFATAL COMPACT_GOOGLE_LOG_FATAL
#include "glog/logging.h"
#include "glog/vlog_is_on.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "absl/base/attributes.h"

// Helper for `ABSL_DIE_IF_NULL`.
namespace verible {
template <typename T>
ABSL_MUST_USE_RESULT T DieIfNull(const char* file, int line,
                                 const char* exprtext, T&& t) {
  CHECK((t != nullptr)) << file << ":" << line << ": " << exprtext;
  return std::forward<T>(t);
}
}  // namespace verible

#ifndef ABSL_DIE_IF_NULL
#define ABSL_DIE_IF_NULL(val) \
  ::verible::DieIfNull(__FILE__, __LINE__, #val, (val))
#endif

#include <string_view>

namespace verible {

// Returns std::string_view with full (qualifiers included) and human
// readable name of type `T`. Use only for debugging purposes. Works only on
// GCC, Clang, MSVC, and can stop working reliably in future versions of even
// these compilers.
template <typename T>
constexpr auto TypeNameAsString() {
  std::string_view name;
  int prefix_len = 0;
  int suffix_len = 0;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix_len = sizeof("auto verible::TypeNameAsString() [T = ") - 1;
  suffix_len = sizeof("]") - 1;
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix_len =
      sizeof("constexpr auto verible::TypeNameAsString() [with T = ") - 1;
  suffix_len = sizeof("]") - 1;
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix_len = sizeof("auto __cdecl verible::TypeNameAsString<") - 1;
  suffix_len = sizeof(">(void)") - 1;
#else
  name = "/*UNKNOWN*/";
#endif
  name.remove_prefix(prefix_len);
  name.remove_suffix(suffix_len);
  return name;
}

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_LOGGING_H_
