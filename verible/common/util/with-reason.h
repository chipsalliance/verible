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

#ifndef VERIBLE_COMMON_UTIL_WITH_REASON_H_
#define VERIBLE_COMMON_UTIL_WITH_REASON_H_

namespace verible {

// In functions with priority-ordered return statements, this helps with
// identifying the statement that took effect.
//
// Instead of writing:
// int f() {
//   ...
//   return value1;   // explanation1
//   ...
//   return value2;   // explanation2
// }
//
// Write:
// WithReason<int> f() {
//   ...
//   return {value1; "explanation1..."};
//   ...
//   return {value2; "explanation2..."};
// }
//
// The caller should unpack the struct, and can print the reason string
// when desired.
template <typename T>
struct WithReason {
  T value;
  const char *reason;  // A simple string literal shall suffice.
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_WITH_REASON_H_
