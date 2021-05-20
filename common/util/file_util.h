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
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace verible {
namespace file {

// A representation of a filesystem directory.
struct Directory {
  // Path to the directory.
  std::string path;
  // Files inside this directory (prefixed by the path). Sorted.
  std::vector<std::string> files;
  // Directories inside this directory (prefixed by the path). Sorted.
  std::vector<std::string> directories;
};

// Returns the part of the path after the final "/".  If there is no
// "/" in the path, the result is the same as the input.
// Note that this function's behavior differs from the Unix basename
// command if path ends with "/". For such paths, this function returns the
// empty string.
absl::string_view Basename(absl::string_view filename);

// Returns the the directory that contains the file.  If there is no
// "/" in the path, the result is the same as the input.
// If no "/" is found the result is the same as the input.
absl::string_view Dirname(absl::string_view filename);

// Returns the part of the basename of path prior to the final ".".  If
// there is no "." in the basename, this is equivalent to file::Basename(path).
absl::string_view Stem(absl::string_view filename);

// Search for 'filename' file starting at 'start' (can be file or directory)
// and going upwards the directory tree. Returns true if anything is found and
// puts the found file in 'result'.
absl::Status UpwardFileSearch(absl::string_view start,
                              absl::string_view filename, std::string* result);

// Determines whether the given filename exists and is a regular file or pipe.
absl::Status FileExists(const std::string& filename);

// Read file "filename" and store its content in "content"
absl::Status GetContents(absl::string_view filename, std::string* content);

// Create file "filename" and store given content in it.
absl::Status SetContents(absl::string_view filename, absl::string_view content);

// Join directory + filename
std::string JoinPath(absl::string_view base, absl::string_view name);

// Create directory with given name, return success.
absl::Status CreateDir(absl::string_view dir);

// Returns the content of the directory. POSIX only. Ignores symlinks and
// unknown nodes which it fails to resolve to a file or a directory. Returns an
// error status on any read error (doesn't allow partial results) except
// resolving symlinks and unknown nodes.
// TODO (after bump to c++17) rewrite this function to use std::filesystem
absl::StatusOr<Directory> ListDir(absl::string_view dir);

namespace testing {

// Generate a random file name (no directory).  This does not create any file.
std::string RandomFileBasename(absl::string_view prefix);

// Useful for testing: a temporary file with a randomly generated name
// that is pre-populated with a particular content.
// File is deleted when instance goes out of scope.
class ScopedTestFile {
 public:
  // Write a new file in directory 'base_dir' with given 'content'.
  // 'base_dir' needs to already exist, and will not be automatically created.
  // If 'use_this_filename' is provided as a base name, that will be used,
  // otherwise, a file name will be randomly generated.
  ScopedTestFile(absl::string_view base_dir, absl::string_view content,
                 absl::string_view use_this_filename = "");
  ~ScopedTestFile();

  // not copy-able
  ScopedTestFile(const ScopedTestFile&) = delete;
  ScopedTestFile& operator=(const ScopedTestFile&) = delete;

  // move-able (to support vector::emplace_back())
  ScopedTestFile(ScopedTestFile&&) = default;
  ScopedTestFile& operator=(ScopedTestFile&&) = default;

  // Filename created by this instance.
  absl::string_view filename() const { return filename_; }

 private:
  std::string filename_;
};

}  // namespace testing
}  // namespace file
}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_FILE_UTIL_H_
