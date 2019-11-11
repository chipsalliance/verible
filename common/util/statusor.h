// Copyright 2017-2019 The Verible Authors.
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

#ifndef VERIBLE_COMMON_UTIL_STATUSOR_H_
#define VERIBLE_COMMON_UTIL_STATUSOR_H_
// StatusOr<T> is the union of a Status object and a T
// object. StatusOr models the concept of an object that is either a
// usable value, or an error Status explaining why such a value is
// not present. To this end, StatusOr<T> does not allow its Status
// value to be Status::OK. Further, StatusOr<T*> does not allow the
// contained pointer to be nullptr.
//
// The primary use-case for StatusOr<T> is as the return value of a
// function which may fail.
//
// Example client usage for a StatusOr<T>, where T is not a pointer:
//
//  StatusOr<float> result = DoBigCalculationThatCouldFail();
//  if (result.ok()) {
//    float answer = result.ValueOrDie();
//    printf("Big calculation yielded: %f", answer);
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<T*>:
//
//  StatusOr<Foo*> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo(result.ValueOrDie());
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example client usage for a StatusOr<std::unique_ptr<T>>:
//
//  StatusOr<std::unique_ptr<Foo>> result = FooFactory::MakeNewFoo(arg);
//  if (result.ok()) {
//    std::unique_ptr<Foo> foo = result.ConsumeValueOrDie();
//    foo->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example factory implementation returning StatusOr<T*>:
//
//  StatusOr<Foo*> FooFactory::MakeNewFoo(int arg) {
//    if (arg <= 0) {
//      return ::util::Status(::util::error::INVALID_ARGUMENT,
//                            "Arg must be positive");
//    } else {
//      return new Foo(arg);
//    }
//  }
//
#include <iostream>
#include <utility>

#include "common/util/status.h"

namespace verible {
namespace util {

template <typename T>
class StatusOr {
  template <typename U>
  friend class StatusOr;

 public:
  // Construct a new StatusOr with Status::UNKNOWN status
  StatusOr();

  // Construct a new StatusOr with the given non-ok status. After calling
  // this constructor, calls to ValueOrDie() will CHECK-fail.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return
  // value, so it is convenient and sensible to be able to do 'return
  // Status()' when the return type is StatusOr<T>.
  //
  // REQUIRES: status != Status::OK. This requirement is DCHECKed.
  // In optimized builds, passing Status::OK here will have the effect
  // of passing PosixErrorSpace::EINVAL as a fallback.
  StatusOr(const Status& status);  // NOLINT

  // Construct a new StatusOr with the given value. If T is a plain pointer,
  // value must not be nullptr. After calling this constructor, calls to
  // ValueOrDie() will succeed, and calls to status() will return OK.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return type
  // so it is convenient and sensible to be able to do 'return T()'
  // when when the return type is StatusOr<T>.
  //
  // REQUIRES: if T is a plain pointer, value != nullptr. This requirement is
  // DCHECKed. In optimized builds, passing a null pointer here will have
  // the effect of passing PosixErrorSpace::EINVAL as a fallback.
  StatusOr(const T& value);  // NOLINT

  // Copy constructor.
  StatusOr(const StatusOr& other);

  // Conversion copy constructor, T must be copy constructible from U
  template <typename U>
  StatusOr(const StatusOr<U>& other);

  // Assignment operator.
  StatusOr& operator=(const StatusOr& other);

  // Conversion assignment operator, T must be assignable from U
  template <typename U>
  StatusOr& operator=(const StatusOr<U>& other);

  // Returns a reference to our status. If this contains a T, then
  // returns Status::OK.
  const Status& status() const;

  // Returns this->status().ok()
  bool ok() const;

  // Returns a reference to our current value, or CHECK-fails if !this->ok().
  // If you need to initialize a T object from the stored value,
  // ConsumeValueOrDie() may be more efficient.
  const T& ValueOrDie() const;

 private:
  Status status_;
  union {
    T value_;
  };
};

////////////////////////////////////////////////////////////////////////////////
// Implementation details for StatusOr<T>

namespace internal {

class StatusOrHelper {
 public:
  // Move type-agnostic error handling to the .cc.
  static void Crash(const util::Status& status);

  // Customized behavior for StatusOr<T> vs. StatusOr<T*>
  template <typename T>
  struct Specialize;
};

template <typename T>
struct StatusOrHelper::Specialize {
  // For non-pointer T, a reference can never be nullptr.
  static inline bool IsValueNull(const T& t) { return false; }
};

template <typename T>
struct StatusOrHelper::Specialize<T*> {
  static inline bool IsValueNull(const T* t) { return t == nullptr; }
};

inline void StatusOrHelper::Crash(const util::Status& status) {
  std::cerr << status.error_message();
  abort();
}
}  // namespace internal

template <typename T>
inline StatusOr<T>::StatusOr() : status_(StatusCode::kUnknown, "") {}

template <typename T>
inline StatusOr<T>::StatusOr(const Status& status) {
  if (status.ok()) {
    status_ =
        Status(StatusCode::kInternal, "Status::OK is not a valid argument.");
  } else {
    status_ = status;
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const T& value) {
  if (internal::StatusOrHelper::Specialize<T>::IsValueNull(value)) {
    status_ = Status(StatusCode::kInternal, "nullptr is not a vaild argument.");
  } else {
    status_ = Status();
    value_ = value;
  }
}

template <typename T>
inline StatusOr<T>::StatusOr(const StatusOr<T>& other)
    : status_(other.status_), value_(other.value_) {}

template <typename T>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<T>& other) {
  status_ = other.status_;
  value_ = other.value_;
  return *this;
}

template <typename T>
template <typename U>
inline StatusOr<T>::StatusOr(const StatusOr<U>& other)
    : status_(other.status_), value_(other.status_.ok() ? other.value_ : T()) {}

template <typename T>
template <typename U>
inline StatusOr<T>& StatusOr<T>::operator=(const StatusOr<U>& other) {
  status_ = other.status_;
  if (status_.ok()) value_ = other.value_;
  return *this;
}

template <typename T>
inline const Status& StatusOr<T>::status() const {
  return status_;
}

template <typename T>
inline bool StatusOr<T>::ok() const {
  return status().ok();
}

template <typename T>
inline const T& StatusOr<T>::ValueOrDie() const {
  if (!status_.ok()) {
    internal::StatusOrHelper::Crash(status_);
  }
  return value_;
}
}  // namespace util
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_STATUSOR_H_
