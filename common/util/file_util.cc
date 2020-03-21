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

#include "common/util/file_util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "common/util/logging.h"

namespace verible {
namespace file {
absl::string_view Basename(absl::string_view filename) {
  auto last_slash_pos = filename.find_last_of("/\\");

  return last_slash_pos == absl::string_view::npos
             ? filename
             : filename.substr(last_slash_pos + 1);
}

absl::string_view Stem(absl::string_view filename) {
  auto last_dot_pos = filename.find_last_of('.');

  return last_dot_pos == absl::string_view::npos
             ? filename
             : filename.substr(0, last_dot_pos);
}

// This will always return an error, even if we can't determine anything
// from errno. Returns "fallback_msg" in that case.
static absl::Status CreateErrorStatusFromErrno(const char *fallback_msg) {
  using absl::StatusCode;
  const char *const system_msg = errno == 0 ? fallback_msg : strerror(errno);
  switch (errno) {
    case EPERM:
    case EACCES:
      return absl::Status(StatusCode::kPermissionDenied, system_msg);
    case ENOENT:
      return absl::Status(StatusCode::kNotFound, system_msg);
    case EEXIST:
      return absl::Status(StatusCode::kAlreadyExists, system_msg);
    case EINVAL:
      return absl::Status(StatusCode::kInvalidArgument, system_msg);
    default:
      return absl::Status(StatusCode::kUnknown, system_msg);
  }
}

// TODO (after bump to c++17) rewrite this function to use std::filesystem
absl::Status UpwardFileSearch(absl::string_view start,
                              absl::string_view filename,
                              std::string* result) {
  static constexpr char dir_separator[] = "/";

  /* Convert to absolute path */
  char absolute_path[PATH_MAX];
  if (realpath(std::string(start).c_str(), absolute_path) < 0) {
    return absl::InvalidArgumentError("Invalid config path specified.\n");
  }

  /* Add trailing slash */
  const std::string full_path = absl::StrCat(absolute_path, dir_separator);

  VLOG(1) << "Upward search for " << filename << ", starting in " << start;

  size_t search_up_to = full_path.length();
  do {
    const size_t separator_pos = full_path.rfind(dir_separator, search_up_to);

    if (separator_pos == search_up_to || separator_pos == std::string::npos) {
      break;
    }

    const std::string candidate =
      absl::StrCat(full_path.substr(0, separator_pos + 1), filename);

    if (access(candidate.c_str(), R_OK) != -1) {
      *result = candidate;
      VLOG(1) << "Found: " << *result;
      return absl::OkStatus();
    }

    search_up_to = separator_pos - 1;

    if (separator_pos == 0) {
      break;
    }
  } while (true);

  return absl::NotFoundError("No matching file found.\n");
}

absl::Status GetContents(absl::string_view filename, std::string *content) {
  std::ifstream fs;
  std::istream* stream = nullptr;
  if (filename == "-") {
    // convention: honor "-" as stdin
    stream = &std::cin;
  } else {
    fs.open(std::string(filename).c_str());
    stream = &fs;
  }
  if (!stream->good()) return CreateErrorStatusFromErrno("can't read");
  content->assign((std::istreambuf_iterator<char>(*stream)),
                  std::istreambuf_iterator<char>());
  return absl::OkStatus();
}

absl::Status SetContents(absl::string_view filename,
                         absl::string_view content) {
  std::ofstream f(std::string(filename).c_str());
  if (!f.good()) return CreateErrorStatusFromErrno("can't write.");
  f << content;
  return absl::OkStatus();
}

std::string JoinPath(absl::string_view base, absl::string_view name) {
  return absl::StrCat(base, "/", name);
}

absl::Status CreateDir(absl::string_view dir) {
  const std::string path(dir);
  int ret = mkdir(path.c_str(), 0755);
  if (ret == 0 || errno == EEXIST) return absl::OkStatus();
  return CreateErrorStatusFromErrno("can't create directory");
}

namespace testing {
ScopedTestFile::ScopedTestFile(absl::string_view base_dir,
                               absl::string_view content)
    // There is no secrecy needed for test files, just need to be unique enough.
    : filename_(JoinPath(
          base_dir, absl::StrCat("scoped-file-", getpid(), "-", random()))) {
  absl::Status status = SetContents(filename_, content);
  CHECK(status.ok()) << status.message();
}

ScopedTestFile::~ScopedTestFile() { unlink(filename_.c_str()); }
}  // namespace testing
}  // namespace file
}  // namespace verible
