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

#ifndef VERIBLE_COMMON_UTIL_STATUS_MACROS_H_
#define VERIBLE_COMMON_UTIL_STATUS_MACROS_H_

#include "absl/base/optimization.h"

namespace verible {
namespace util {

// Run a command that returns a absl::Status.  If the called code returns an
// error status, return that status up out of this method too.
//
// Example:
//   RETURN_IF_ERROR(DoThings(4));
#define RETURN_IF_ERROR(expr)                                                \
  do {                                                                       \
    /* Using _status below to avoid capture problems if expr is "status". */ \
    absl::Status _status = (expr);                                           \
    if (ABSL_PREDICT_FALSE(!_status.ok())) return _status;                   \
  } while (0)

}  // namespace util
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_STATUS_MACROS_H_
