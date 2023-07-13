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

#ifndef VERIBLE_COMMON_UTIL_FORWARD_H_
#define VERIBLE_COMMON_UTIL_FORWARD_H_

namespace verible {

// Helper class that automatically constructs a temporary or forwards a direct
// (const) reference, depending on the source object type and T, the desired
// forwarded type.
// This works as a fallback in cases where heterogeneous lookup (C++14)
// is not available.  Use this when it doesn't matter whether or not
// a temporary value or reference is returned.
// This is defined in as a helper struct so that template argument deduction
// only needs to work with one parameter type, because T is explicit.
template <typename T>
struct ForwardReferenceElseConstruct {
  // Forwards a const-reference of type T.
  // Intentionally restricted to const reference to avoid the surprise
  // of modifying a temporary reference.
  // See also the two-parameter overload.
  const T &operator()(const T &t) const { return t; }

  // (overload) Constructs a temporary rvalue, when types are different.
  // This works with explicit-only constructors and implicit constructors.
  template <typename ConvertibleToT>
  T operator()(const ConvertibleToT &other) const {
    return T(other);
  }
};

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_FORWARD_H_
