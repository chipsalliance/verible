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

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// Temporary until we have all platforms on C++17, see comment in file_util.cc
#ifdef _WIN32
#define USE_CPP_17_FILESYSTEM 1
#endif

#ifdef USE_CPP_17_FILESYSTEM
#include <filesystem>
#endif

#undef EXPECT_OK
#define EXPECT_OK(value)      \
  {                           \
    auto s = (value);         \
    EXPECT_TRUE(s.ok()) << s; \
  }
#undef ASSERT_OK
#define ASSERT_OK(value)      \
  {                           \
    auto s = (value);         \
    ASSERT_TRUE(s.ok()) << s; \
  }

using testing::HasSubstr;

namespace verible {
namespace util {
namespace {

using file::testing::ScopedTestFile;

TEST(FileUtil, Basename) {
  EXPECT_EQ(file::Basename("/foo/bar/baz"), "baz");
  EXPECT_EQ(file::Basename("foo/bar/baz"), "baz");
  EXPECT_EQ(file::Basename("/foo/bar/"), "");
  EXPECT_EQ(file::Basename("/"), "");
  EXPECT_EQ(file::Basename(""), "");
}

TEST(FileUtil, Dirname) {
  EXPECT_EQ(file::Dirname("/foo/bar/baz"), "/foo/bar");
  EXPECT_EQ(file::Dirname("/foo/bar/baz.txt"), "/foo/bar");
  EXPECT_EQ(file::Dirname("foo/bar/baz"), "foo/bar");
  EXPECT_EQ(file::Dirname("/foo/bar/"), "/foo/bar");
  EXPECT_EQ(file::Dirname("/"), "");
  EXPECT_EQ(file::Dirname(""), "");
  EXPECT_EQ(file::Dirname("."), ".");
  EXPECT_EQ(file::Dirname("./"), ".");
}

TEST(FileUtil, Stem) {
  EXPECT_EQ(file::Stem(""), "");
  EXPECT_EQ(file::Stem("/foo/bar.baz"), "/foo/bar");
  EXPECT_EQ(file::Stem("foo/bar.baz"), "foo/bar");
  EXPECT_EQ(file::Stem("/foo/bar."), "/foo/bar");
  EXPECT_EQ(file::Stem("/foo/bar"), "/foo/bar");
}

// Returns the forward-slashed path in platform-specific notation.
static std::string PlatformPath(const std::string& path) {
#ifdef USE_CPP_17_FILESYSTEM
  return std::filesystem::path(path).lexically_normal().string();
#else
  return path;
#endif
}

TEST(FileUtil, JoinPath) {
  EXPECT_EQ(file::JoinPath("foo", ""), PlatformPath("foo/"));
  EXPECT_EQ(file::JoinPath("", "bar"), "bar");
  EXPECT_EQ(file::JoinPath("foo", "bar"), PlatformPath("foo/bar"));

  // Assemble an absolute path
  EXPECT_EQ(file::JoinPath("", "bar"), "bar");
  EXPECT_EQ(file::JoinPath("/", "bar"), PlatformPath("/bar"));

  // Lightly canonicalize multiple consecutive slashes
  EXPECT_EQ(file::JoinPath("foo/", "bar"), PlatformPath("foo/bar"));
  EXPECT_EQ(file::JoinPath("foo///", "bar"), PlatformPath("foo/bar"));
  EXPECT_EQ(file::JoinPath("foo/", "/bar"), PlatformPath("foo/bar"));
  EXPECT_EQ(file::JoinPath("foo/", "///bar"), PlatformPath("foo/bar"));

#ifdef _WIN32
  EXPECT_EQ(file::JoinPath("C:\\foo", "bar"), "C:\\foo\\bar");
#endif
}

TEST(FileUtil, CreateDir) {
  const std::string test_dir = file::JoinPath(testing::TempDir(), "test_dir");
  const std::string test_file = file::JoinPath(test_dir, "foo");
  const absl::string_view test_content = "directory create test";

  EXPECT_OK(file::CreateDir(test_dir));
  EXPECT_OK(file::CreateDir(test_dir));  // Creating twice should succeed

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

#ifndef _WIN32
  // The following chmod() is not working on Win32. So let's not use
  // this test here.
  // TODO: Can we make permission-denied test that works on Windows ?
  chmod(test_file.c_str(), 0);  // Enforce a permission denied situation
  status = file::GetContents(test_file, &content);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
#endif
}

TEST(FileUtil, ScopedTestFile) {
  const absl::string_view test_content = "Hello World!";
  ScopedTestFile test_file(testing::TempDir(), test_content);
  std::string read_back_content;
  EXPECT_OK(file::GetContents(test_file.filename(), &read_back_content));
  EXPECT_EQ(test_content, read_back_content);
}

static ScopedTestFile TestFileGenerator(absl::string_view content) {
  return ScopedTestFile(testing::TempDir(), content);
}

static void TestFileConsumer(ScopedTestFile&& f) {
  ScopedTestFile temp(std::move(f));
}

TEST(FileUtil, ScopedTestFileMove) {
  ScopedTestFile f(TestFileGenerator("barfoo"));
  std::string tmpname(f.filename());
  EXPECT_TRUE(file::FileExists(tmpname).ok());
  TestFileConsumer(std::move(f));
  EXPECT_FALSE(file::FileExists(tmpname).ok());
}

TEST(FileUtil, ScopedTestFileEmplace) {
  std::vector<std::string> names;
  {
    std::vector<ScopedTestFile> files;
    for (size_t i = 0; i < 10; ++i) {
      files.emplace_back(::testing::TempDir(), "zzz");
      names.push_back(std::string(files.back().filename()));
    }
    for (const auto& name : names) {
      EXPECT_TRUE(file::FileExists(name).ok());
    }
  }
  for (const auto& name : names) {
    EXPECT_FALSE(file::FileExists(name).ok());
  }
}

TEST(FileUtil, FileExistsDirectoryErrorMessage) {
  absl::Status s;
  s = file::FileExists(testing::TempDir());
  EXPECT_FALSE(s.ok());
  EXPECT_THAT(s.message(), HasSubstr("is a directory"));
}

static bool CreateFsStructure(absl::string_view base_dir,
                              const std::vector<absl::string_view>& tree) {
  for (absl::string_view path : tree) {
    const std::string full_path = file::JoinPath(base_dir, path);
    if (absl::EndsWith(path, "/")) {
      if (!file::CreateDir(full_path).ok()) return false;
    } else {
      if (!file::SetContents(full_path, "(content)").ok()) return false;
    }
  }
  return true;
}

TEST(FileUtil, UpwardFileSearchTest) {
  const std::string root_dir = file::JoinPath(testing::TempDir(), "up-search");
  ASSERT_OK(file::CreateDir(root_dir));
  ASSERT_TRUE(CreateFsStructure(root_dir, {
                                              "toplevel-file",
                                              "foo/",
                                              "foo/foo-file",
                                              "foo/bar/",
                                              "foo/bar/baz/",
                                              "foo/bar/baz/baz-file",
                                          }));
  std::string result;
  // Same directory
  EXPECT_OK(file::UpwardFileSearch(file::JoinPath(root_dir, "foo"), "foo-file",
                                   &result));
  // We don't compare the full path, as the UpwardFileSearch() makes it an
  // realpath which might not entirely match our root_dir prefix anymore; but
  // we do know that the suffix should be the same.
  EXPECT_TRUE(absl::EndsWith(result, PlatformPath("up-search/foo/foo-file")));

  // Somewhere below
  EXPECT_OK(file::UpwardFileSearch(file::JoinPath(root_dir, "foo/bar/baz"),
                                   "foo-file", &result));
  EXPECT_TRUE(absl::EndsWith(result, PlatformPath("up-search/foo/foo-file")));

  // Find toplevel file
  EXPECT_OK(file::UpwardFileSearch(file::JoinPath(root_dir, "foo/bar/baz"),
                                   "toplevel-file", &result));
  EXPECT_TRUE(absl::EndsWith(result, PlatformPath("up-search/toplevel-file")));

  // Negative test.
  auto status = file::UpwardFileSearch(file::JoinPath(root_dir, "foo/bar/baz"),
                                       "unknownfile", &result);
  EXPECT_FALSE(status.ok());
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

TEST(FileUtil, ReadNonexistentDirectory) {
  const std::string test_dir =
      file::JoinPath(testing::TempDir(), "dir_not_there");

  auto dir_or = file::ListDir(test_dir);
  EXPECT_FALSE(dir_or.ok());
  EXPECT_EQ(dir_or.status().code(), absl::StatusCode::kNotFound);
}

TEST(FileUtil, ListNotADirectory) {
  ScopedTestFile tempfile(testing::TempDir(), "O HAI, WRLD");

  auto dir_or = file::ListDir(tempfile.filename());
  EXPECT_FALSE(dir_or.ok());
  EXPECT_EQ(dir_or.status().code(), absl::StatusCode::kInvalidArgument);
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

  EXPECT_EQ(dir.directories.size(), test_directories.size());
  EXPECT_EQ(dir.directories, test_directories);

  EXPECT_EQ(dir.files.size(), test_files.size());
  EXPECT_EQ(dir.files, test_files);
}

}  // namespace
}  // namespace util
}  // namespace verible
