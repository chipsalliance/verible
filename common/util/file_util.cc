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

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

#include "absl/strings/str_cat.h"
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

bool GetContents(absl::string_view filename, std::string *content) {
  std::ifstream fs(std::string(filename).c_str());
  if (!fs.good()) return false;
  content->assign((std::istreambuf_iterator<char>(fs)),
                  std::istreambuf_iterator<char>());
  return true;
}

bool SetContents(absl::string_view filename, absl::string_view content) {
  std::ofstream f(std::string(filename).c_str());
  if (!f.good()) return false;
  f << content;
  return true;
}

std::string JoinPath(absl::string_view base, absl::string_view name) {
  return absl::StrCat(base, "/", name);
}

bool CreateDir(absl::string_view dir) {
  const std::string path(dir);
  int ret = mkdir(path.c_str(), 0755);
  return ret == 0 || errno == EEXIST;
}

namespace testing {
ScopedTestFile::ScopedTestFile(absl::string_view base_dir,
                               absl::string_view content)
    // There is no secrecy needed for test files, just need to be unique enough.
    : filename_(JoinPath(
          base_dir, absl::StrCat("scoped-file-", getpid(), "-", random()))) {
  CHECK(SetContents(filename_, content));
}

ScopedTestFile::~ScopedTestFile() { unlink(filename_.c_str()); }
}  // namespace testing
}  // namespace file
}  // namespace verible
