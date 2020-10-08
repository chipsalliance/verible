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

#include <algorithm>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

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

TEST(FileUtil, Direname) {
  EXPECT_EQ(file::Direname("/foo/bar/baz"), "/foo/bar");
  EXPECT_EQ(file::Direname("foo/bar/baz"), "foo/bar");
  EXPECT_EQ(file::Direname("/foo/bar/"), "/foo/bar");
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

  EXPECT_OK(file::CreateDir(test_dir));

  EXPECT_OK(file::SetContents(test_file, test_content));
  std::string read_back_content;
  EXPECT_OK(file::GetContents(test_file, &read_back_content));
  EXPECT_EQ(test_content, read_back_content);
}

TEST(FileUtil, StatusErrorReporting) {
  std::string content;
  absl::Status status = file::GetContents("does-not-exist", &content);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound) << status;

  const std::string test_file = file::JoinPath(testing::TempDir(), "test-err");
  unlink(test_file.c_str());  // Remove file if left from previous test.
  EXPECT_OK(file::SetContents(test_file, "foo"));

  EXPECT_OK(file::GetContents(test_file, &content));
  EXPECT_EQ(content, "foo");

  chmod(test_file.c_str(), 0);  // Enforce a permission denied situation
  status = file::GetContents(test_file, &content);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
}

TEST(FileUtil, ScopedTestFile) {
  const absl::string_view test_content = "Hello World!";
  file::testing::ScopedTestFile test_file(testing::TempDir(), test_content);
  std::string read_back_content;
  EXPECT_OK(file::GetContents(test_file.filename(), &read_back_content));
  EXPECT_EQ(test_content, read_back_content);
}

TEST(FileUtil, ScopedTestFileStdin) {
  // When running interactively, skip this test to avoid waiting for stdin.
  if (isatty(STDIN_FILENO)) return;
  // Testing is non-interactive, so reading from stdin will be immediately
  // closed, resulting in an empty string.
  const absl::string_view test_content = "";
  std::string read_back_content;
  EXPECT_OK(file::GetContents("-", &read_back_content));
  EXPECT_EQ(test_content, read_back_content);
}

TEST(FileUtil, ReadEmptyDirectory) {
  const std::string test_dir = file::JoinPath(testing::TempDir(), "empty_dir");
  ASSERT_TRUE(file::CreateDir(test_dir).ok());

  auto dir_or = file::ListDir(test_dir);
  ASSERT_TRUE(dir_or.ok());

  const auto& dir = *dir_or;
  EXPECT_EQ(dir.path, test_dir);
  EXPECT_TRUE(dir.directories.empty());
  EXPECT_TRUE(dir.files.empty());
}

TEST(FileUtil, ReadDirectory) {
  const std::string test_dir = file::JoinPath(testing::TempDir(), "mixed_dir");

  const std::string test_subdir1 = file::JoinPath(test_dir, "subdir1");
  const std::string test_subdir2 = file::JoinPath(test_dir, "subdir2");
  std::vector<std::string> test_directories{test_subdir1, test_subdir2};
  ASSERT_TRUE(file::CreateDir(test_dir).ok());
  ASSERT_TRUE(file::CreateDir(test_subdir1).ok());
  ASSERT_TRUE(file::CreateDir(test_subdir2).ok());

  const absl::string_view test_content = "Hello World!";
  file::testing::ScopedTestFile test_file1(test_dir, test_content);
  file::testing::ScopedTestFile test_file2(test_dir, test_content);
  file::testing::ScopedTestFile test_file3(test_dir, test_content);
  std::vector<std::string> test_files;
  test_files.emplace_back(test_file1.filename());
  test_files.emplace_back(test_file2.filename());
  test_files.emplace_back(test_file3.filename());
  std::sort(test_files.begin(), test_files.end());

  auto dir_or = file::ListDir(test_dir);
  ASSERT_TRUE(dir_or.ok()) << dir_or.status().message();

  const auto& dir = *dir_or;
  EXPECT_EQ(dir.path, test_dir);
  EXPECT_EQ(dir.directories, test_directories);
  EXPECT_EQ(dir.files, test_files);
}

}  // namespace
}  // namespace util
}  // namespace verible
