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

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "absl/base/attributes.h"

// Helper for `ABSL_DIE_IF_NULL`.
namespace verible {
template <typename T>
ABSL_MUST_USE_RESULT T DieIfNull(const char* file, int line,
                                 const char* exprtext, T&& t) {
  CHECK(t != nullptr) << exprtext;
  return std::forward<T>(t);
}
}  // namespace verible

#ifndef ABSL_DIE_IF_NULL
#define ABSL_DIE_IF_NULL(val) \
  ::verible::DieIfNull(__FILE__, __LINE__, #val, (val))
#endif

#endif  // VERIBLE_COMMON_UTIL_LOGGING_H_
