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

#include "common/util/file-util.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
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

#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

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

// Create an error message derived from "sys_error" (which contains an errno
// number). The message includes the filename as prefix.
// If "sys_error" can not be resolved, creates an UNKNOWN message.
// The "filename" and "fallback_msg" will be copied, no need for them to
// stay alive after the call.
static absl::Status CreateErrorStatusFromSysError(absl::string_view filename,
                                                  int sys_error,
                                                  const char *fallback_msg) {
  const char *const system_msg =
      sys_error == 0 ? fallback_msg : strerror(sys_error);
  const std::string msg = filename.empty()
                              ? std::string{system_msg}
                              : absl::StrCat(filename, ": ", system_msg);
  switch (sys_error) {
    case EPERM:
    case EACCES:
      return {absl::StatusCode::kPermissionDenied, msg};
    case ENOENT:
      return {absl::StatusCode::kNotFound, msg};
    case EEXIST:
      return {absl::StatusCode::kAlreadyExists, msg};
    case EINVAL:
    case EISDIR:
      return {absl::StatusCode::kInvalidArgument, msg};
    default:
      return {absl::StatusCode::kUnknown, msg};
  }
}

static absl::Status CreateErrorStatusFromErrno(absl::string_view filename,
                                               const char *fallback_msg) {
  return CreateErrorStatusFromSysError(filename, errno, fallback_msg);
}
static absl::Status CreateErrorStatusFromErr(absl::string_view filename,
                                             const std::error_code &err,
                                             const char *fallback_msg) {
  // TODO: this assumes that err.value() returns errno-like values. Might not
  // always be the case.
  return CreateErrorStatusFromSysError(filename, err.value(), fallback_msg);
}

absl::Status UpwardFileSearch(absl::string_view start,
                              absl::string_view filename, std::string *result) {
  std::error_code err;
  const std::string search_file(filename);
  fs::path absolute_path = fs::absolute(std::string(start), err);
  if (err.value() != 0) {
    return CreateErrorStatusFromErr(filename, err,
                                    "invalid config path specified.");
  }
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

absl::StatusOr<std::string> GetContentAsString(absl::string_view filename) {
  std::string content;
  std::ifstream fs;
  std::istream *stream = nullptr;
  const bool use_stdin = IsStdin(filename);
  if (use_stdin) {
    stream = &std::cin;
  } else {
    const std::string filename_str = std::string{filename};
    if (absl::Status status = FileExists(filename_str); !status.ok()) {
      return status;  // Bail
    }
    fs.open(filename_str.c_str());
    std::error_code err;
    const size_t prealloc = fs::file_size(filename_str, err);
    if (err.value() == 0) content.reserve(prealloc);
    stream = &fs;
  }
  if (!stream->good()) {
    return CreateErrorStatusFromErrno(filename, "can't read");
  }
  char buffer[4096];
  while (stream->good() && !stream->eof()) {
    stream->read(buffer, sizeof(buffer));
    content.append(buffer, stream->gcount());
  }

  // Allow stdin to be reopened for more input.
  if (use_stdin && std::cin.eof()) std::cin.clear();
  return std::move(content);
}

static absl::StatusOr<std::unique_ptr<MemBlock>> AttemptMemMapFile(
    absl::string_view filename) {
#ifndef _WIN32
  class MemMapBlock final : public MemBlock {
   public:
    MemMapBlock(char *buffer, size_t size) : buffer_(buffer), size_(size) {}
    ~MemMapBlock() final { munmap(buffer_, size_); }
    absl::string_view AsStringView() const final { return {buffer_, size_}; }

   private:
    char *const buffer_;
    const size_t size_;
  };

  const std::string nul_terminated_filename(filename);
  const int fd = open(nul_terminated_filename.c_str(), O_RDONLY);
  if (fd < 0) {
    return CreateErrorStatusFromErrno(filename, "Can't open file");
  }

  struct stat s;
  if (fstat(fd, &s) < 0) {
    close(fd);
    return CreateErrorStatusFromErrno(filename, "can't stat");
  }

  const size_t file_size = s.st_size;
  void *const buffer = mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
  close(fd);
  if (buffer == MAP_FAILED) {  // NOLINT(performance-no-int-to-ptr)
    return CreateErrorStatusFromErrno(filename, "Can't mmap file");
  }
#ifdef POSIX_MADV_WILLNEED
  // Trigger read-ahead if possible.
  posix_madvise(buffer, file_size, POSIX_MADV_WILLNEED);
#endif
  return std::make_unique<MemMapBlock>(reinterpret_cast<char *>(buffer),
                                       file_size);
#else
  // TODO: implement some memory mapping on Windows.
  return absl::UnimplementedError("No windows mmap implementation yet.");
#endif
}

absl::StatusOr<std::unique_ptr<MemBlock>> GetContentAsMemBlock(
    absl::string_view filename) {
  auto mmap_result = AttemptMemMapFile(filename);
  if (mmap_result.status().ok()) {
    return mmap_result;
  }

  // Still here ? Well, let's try the traditional way
  auto content_or = GetContentAsString(filename);
  if (!content_or.ok()) return content_or.status();
  return std::make_unique<StringMemBlock>(std::move(*content_or));
}

absl::Status SetContents(absl::string_view filename,
                         absl::string_view content) {
  VLOG(1) << __FUNCTION__ << ": Writing file: " << filename;
  std::ofstream f(std::string(filename).c_str());
  if (!f.good()) return CreateErrorStatusFromErrno(filename, "can't write.");
  f << content;
  f.close();
  if (!f.good()) return CreateErrorStatusFromErrno(filename, "closing.");
  return absl::OkStatus();
}

std::string JoinPath(absl::string_view base, absl::string_view name) {
  fs::path p = fs::path(std::string(base)) / fs::path(std::string(name));
  return p.lexically_normal().string();
}

absl::Status CreateDir(absl::string_view dir) {
  const std::string path(dir);
  std::error_code err;
  if (fs::create_directory(path, err) || err.value() == 0) {
    return absl::OkStatus();
  }
  return CreateErrorStatusFromErr(dir, err, "can't create directory");
}

absl::StatusOr<Directory> ListDir(absl::string_view dir) {
  std::error_code err;
  Directory d;

  d.path = dir.empty() ? "." : std::string(dir);

  fs::file_status stat = fs::status(d.path, err);
  if (err.value() != 0) {
    return CreateErrorStatusFromErr(d.path, err, "Opening directory");
  }
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

bool IsStdin(absl::string_view filename) {
  static constexpr absl::string_view kStdinFilename = "-";
  return filename == kStdinFilename;
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
