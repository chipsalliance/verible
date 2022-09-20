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
// b/246413374
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "absl/log/check.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include "absl/log/die_if_null.h"
#include "absl/log/log.h"

// There is no vlog yet, so very simple work-around here until it shows up
#ifndef VLOG
namespace verible {
// Used in the VLOG macro to check logging condition. This value is set in
// InitCommandLine() from VERIBLE_VLOG_DETAIL environment variable.
extern int global_vlog_level_;
}  // namespace verible

#define VLOG_IS_ON(x) (::verible::global_vlog_level_ >= (x))
#define VLOG(x) LOG_IF(INFO, VLOG_IS_ON(x))

#ifdef NDEBUG
#define DVLOG(x) LOG_IF(INFO, false)
#else
#define DVLOG(x) VLOG(x)
#endif  // NDEBUG

#endif  // VLOG

#define CHECK_NOTNULL(p) (void)ABSL_DIE_IF_NULL(p)

#endif  // VERIBLE_COMMON_UTIL_LOGGING_H_
