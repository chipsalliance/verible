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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>
#include <system_error>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "common/util/logging.h"

namespace fs = std::filesystem;

namespace verible {
namespace file {
absl::string_view Basename(absl::string_view filename) {
  auto last_slash_pos = filename.find_last_of("/\\");

  return last_slash_pos == absl::string_view::npos
             ? filename
             : filename.substr(last_slash_pos + 1);
}

absl::string_view Dirname(absl::string_view filename) {
  auto last_slash_pos = filename.find_last_of("/\\");

  return last_slash_pos == absl::string_view::npos
             ? filename
             : filename.substr(0, last_slash_pos);
}

absl::string_view Stem(absl::string_view filename) {
  auto last_dot_pos = filename.find_last_of('.');

  return last_dot_pos == absl::string_view::npos
             ? filename
             : filename.substr(0, last_dot_pos);
}

// This will always return an error, even if we can't determine anything
// from errno. Returns "fallback_msg" in that case.
static absl::Status CreateErrorStatusFromSysError(const char *fallback_msg,
                                                  int sys_error) {
  using absl::StatusCode;
  const char *const system_msg =
      sys_error == 0 ? fallback_msg : strerror(sys_error);
  switch (sys_error) {
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

static absl::Status CreateErrorStatusFromErrno(const char *fallback_msg) {
  return CreateErrorStatusFromSysError(fallback_msg, errno);
}
static absl::Status CreateErrorStatusFromErr(const char *fallback_msg,
                                             const std::error_code &err) {
  // TODO: this assumes that err.value() returns errno-like values. Might not
  // always be the case.
  return CreateErrorStatusFromSysError(fallback_msg, err.value());
}

absl::Status UpwardFileSearch(absl::string_view start,
                              absl::string_view filename, std::string *result) {
  std::error_code err;
  const std::string search_file(filename);
  fs::path absolute_path = fs::absolute(std::string(start), err);
  if (err.value() != 0)
    return CreateErrorStatusFromErr("invalid config path specified.", err);

  fs::path probe_dir = absolute_path;
  for (;;) {
    *result = (probe_dir / search_file).string();
    if (FileExists(*result).ok()) return absl::OkStatus();
    fs::path one_up = probe_dir.parent_path();
    if (one_up == probe_dir) break;
    probe_dir = one_up;
  }
  return absl::NotFoundError("No matching file found.");
}

absl::Status FileExists(const std::string &filename) {
  std::error_code err;
  fs::file_status stat = fs::status(filename, err);

  if (err.value() != 0) {
    return absl::NotFoundError(absl::StrCat(filename, ": ", err.message()));
  }

  if (fs::is_regular_file(stat) || fs::is_fifo(stat)) {
    return absl::OkStatus();
  }

  if (fs::is_directory(stat)) {
    return absl::InvalidArgumentError(
        absl::StrCat(filename, ": is a directory, not a file"));
  }
  return absl::InvalidArgumentError(
      absl::StrCat(filename, ": not a regular file."));
}

absl::Status GetContents(absl::string_view filename, std::string *content) {
  std::ifstream fs;
  std::istream *stream = nullptr;
  const bool use_stdin = filename == "-";  // convention: honor "-" as stdin
  if (use_stdin) {
    stream = &std::cin;
  } else {
    const std::string filename_str = std::string(filename);
    absl::Status usable_file = FileExists(filename_str);
    if (!usable_file.ok()) return usable_file;  // Bail
    fs.open(filename_str.c_str());
    std::error_code err;
    const size_t prealloc = fs::file_size(filename_str, err);
    if (err.value() == 0) content->reserve(prealloc);
    stream = &fs;
  }
  if (!stream->good()) return CreateErrorStatusFromErrno("can't read");
  char buffer[4096];
  while (stream->good() && !stream->eof()) {
    stream->read(buffer, sizeof(buffer));
    content->append(buffer, stream->gcount());
  }

  // Allow stdin to be reopened for more input.
  if (use_stdin && std::cin.eof()) std::cin.clear();
  return absl::OkStatus();
}

absl::Status SetContents(absl::string_view filename,
                         absl::string_view content) {
  VLOG(1) << __FUNCTION__ << ": Writing file: " << filename;
  std::ofstream f(std::string(filename).c_str());
  if (!f.good()) return CreateErrorStatusFromErrno("can't write.");
  f << content;
  return absl::OkStatus();
}

std::string JoinPath(absl::string_view base, absl::string_view name) {
  // Make sure the second element is not already absolute, otherwise
  // the fs::path() uses this as toplevel path. This is only an issue with
  // Unix paths. Windows paths will have a c:\ for explicit full-pathness
  while (!name.empty() && name[0] == '/') {
    name = name.substr(1);
  }

  fs::path p = fs::path(std::string(base)) / fs::path(std::string(name));
  return p.lexically_normal().string();
}

absl::Status CreateDir(absl::string_view dir) {
  const std::string path(dir);
  std::error_code err;
  if (fs::create_directory(path, err) || err.value() == 0)
    return absl::OkStatus();

  return CreateErrorStatusFromErr("can't create directory", err);
}

absl::StatusOr<Directory> ListDir(absl::string_view dir) {
  std::error_code err;
  Directory d;

  d.path = dir.empty() ? "." : std::string(dir);

  fs::file_status stat = fs::status(d.path, err);
  if (err.value() != 0)
    return CreateErrorStatusFromErr("Opening directory", err);
  if (!fs::is_directory(stat)) {
    return absl::InvalidArgumentError(absl::StrCat(dir, ": not a directory"));
  }

  for (const fs::directory_entry &entry : fs::directory_iterator(d.path)) {
    const std::string entry_name = entry.path().string();
    if (entry.is_directory()) {
      d.directories.push_back(entry_name);
    } else {
      d.files.push_back(entry_name);
    }
  }

  std::sort(d.files.begin(), d.files.end());
  std::sort(d.directories.begin(), d.directories.end());
  return d;
}

namespace testing {

std::string RandomFileBasename(absl::string_view prefix) {
  return absl::StrCat(prefix, "-", rand());
}

ScopedTestFile::ScopedTestFile(absl::string_view base_dir,
                               absl::string_view content,
                               absl::string_view use_this_filename)
    // There is no secrecy needed for test files,
    // file name just need to be unique enough.
    : filename_(JoinPath(base_dir, use_this_filename.empty()
                                       ? RandomFileBasename("scoped-file")
                                       : use_this_filename)) {
  const absl::Status status = SetContents(filename_, content);
  CHECK(status.ok()) << status.message();  // ok for test-only code
}

ScopedTestFile::~ScopedTestFile() { unlink(filename_.c_str()); }
}  // namespace testing
}  // namespace file
}  // namespace verible
