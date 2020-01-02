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

#ifndef VERIBLE_COMMON_UTIL_STATUS_H_
#define VERIBLE_COMMON_UTIL_STATUS_H_

#include <iosfwd>
#include <string>

#include "absl/strings/string_view.h"

namespace verible {
namespace util {

// Canonical error codes. These should come from absl some time.
enum class StatusCode : int {
  kOk = 0,
  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
  kInternal = 13,
  kUnavailable = 14,
  kDataLoss = 15,
  kUnauthenticated = 16,
  kDoNotUseReservedForFutureExpansionUseDefaultInSwitchInstead_ = 20
};

// Returns the name for the status code, or "" if it is an unknown value.
std::string StatusCodeToString(StatusCode code);

// Streams StatusCodeToString(code) to `os`.
std::ostream& operator<<(std::ostream& os, StatusCode code);

class Status {
 public:
  // Creates a "successful" status.
  Status();

  // Create a status in the canonical error space with the specified
  // code, and error message.  If "code == 0", error_message is
  // ignored and a Status object identical to Status::OK is
  // constructed.
  Status(StatusCode error_code, absl::string_view error_message);
  ~Status() {}

  // Accessor
  bool ok() const { return error_code_ == StatusCode::kOk; }
  StatusCode code() const { return error_code_; }
  absl::string_view error_message() const { return error_message_; }
  absl::string_view message() const { return error_message_; }

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const { return !operator==(x); }

  // Return a combination of the error code name and message.
  std::string ToString() const;

 private:
  StatusCode error_code_;
  std::string error_message_;
};

// Prints a human-readable representation of 'x' to 'os'.
std::ostream& operator<<(std::ostream& os, const Status& x);

inline Status OkStatus() { return Status(); }
inline Status InternalError(absl::string_view msg) {
  return Status(StatusCode::kInternal, msg);
}
inline Status InvalidArgumentError(absl::string_view msg) {
  return Status(StatusCode::kInvalidArgument, msg);
}
inline Status NotFoundError(absl::string_view msg) {
  return Status(StatusCode::kNotFound, msg);
}
inline Status ResourceExhaustedError(absl::string_view msg) {
  return Status(StatusCode::kResourceExhausted, msg);
}

}  // namespace util
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_STATUS_H_
