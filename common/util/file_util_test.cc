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

#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

namespace verible {
namespace util {
namespace {
TEST(FileUtil, Basename) {
  EXPECT_EQ(file::Basename("/foo/bar/baz"), "baz");
  EXPECT_EQ(file::Basename("foo/bar/baz"), "baz");
  EXPECT_EQ(file::Basename("/foo/bar/"), "");
  EXPECT_EQ(file::Basename("/"), "");
  EXPECT_EQ(file::Basename(""), "");
}

TEST(FileUtil, Stem) {
  EXPECT_EQ(file::Stem(""), "");
  EXPECT_EQ(file::Stem("/foo/bar.baz"), "/foo/bar");
  EXPECT_EQ(file::Stem("foo/bar.baz"), "foo/bar");
  EXPECT_EQ(file::Stem("/foo/bar."), "/foo/bar");
  EXPECT_EQ(file::Stem("/foo/bar"), "/foo/bar");
}

TEST(FileUtil, CreateDir) {
  const std::string test_dir = file::JoinPath(testing::TempDir(), "test_dir");
  const std::string test_file = file::JoinPath(test_dir, "foo");
  const absl::string_view test_content = "directory create test";

  EXPECT_TRUE(file::CreateDir(test_dir));

  EXPECT_TRUE(file::SetContents(test_file, test_content));
  std::string read_back_content;
  EXPECT_TRUE(file::GetContents(test_file, &read_back_content));
  EXPECT_EQ(test_content, read_back_content);
}

TEST(FileUtil, ScopedTestFile) {
  const absl::string_view test_content = "Hello World!";
  file::testing::ScopedTestFile test_file(testing::TempDir(), test_content);
  std::string read_back_content;
  EXPECT_TRUE(file::GetContents(test_file.filename(), &read_back_content));
  EXPECT_EQ(test_content, read_back_content);
}

TEST(FileUtil, ScopedTestFileStdin) {
  // When running interactively, skip this test to avoid waiting for stdin.
  if (isatty(STDIN_FILENO)) return;
  // Testing is non-interactive, so reading from stdin will be immediately
  // closed, resulting in an empty string.
  const absl::string_view test_content = "";
  std::string read_back_content;
  EXPECT_TRUE(file::GetContents("-", &read_back_content));
  EXPECT_EQ(test_content, read_back_content);
}

}  // namespace
}  // namespace util
}  // namespace verible
