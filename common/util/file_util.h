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

// -*- c++ -*-
// Some filesystem utilities.
#ifndef VERIBLE_COMMON_UTIL_FILE_UTIL_H_
#define VERIBLE_COMMON_UTIL_FILE_UTIL_H_

#include <string>

#include "absl/strings/string_view.h"

namespace verible {
namespace file {
// Returns the part of the path after the final "/".  If there is no
// "/" in the path, the result is the same as the input.
// Note that this function's behavior differs from the Unix basename
// command if path ends with "/". For such paths, this function returns the
// empty string.
absl::string_view Basename(absl::string_view filename);

// Returns the part of the basename of path prior to the final ".".  If
// there is no "." in the basename, this is equivalent to file::Basename(path).
absl::string_view Stem(absl::string_view filename);

// Read file "filename" and store its content in "content"
// TODO(hzeller): consider util::Status return?
bool GetContents(absl::string_view filename, std::string *content);

// Create file "filename" and store given content in it.
// TODO(hzeller): consider util::Status return ?
bool SetContents(absl::string_view filename, absl::string_view content);

// Join directory + filename
std::string JoinPath(absl::string_view base, absl::string_view name);

// Create directory with given name, return success.
bool CreateDir(absl::string_view dir);

namespace testing {
// Useful for testing: a temporary file that is pre-populated with a particular
// content. File is deleted when instance goes out of scope.
class ScopedTestFile {
 public:
  // Initialize a new file in directory "base_dir" with given "content".
  ScopedTestFile(absl::string_view base_dir, absl::string_view content);
  ~ScopedTestFile();

  // Filename created by this instance.
  absl::string_view filename() const { return filename_; }

 private:
  const std::string filename_;
};
}  // namespace testing
}  // namespace file
}  // namespace verible
#endif  // VERIBLE_COMMON_UTIL_FILE_UTIL_H_
