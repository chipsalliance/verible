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

#include "verible/common/util/file-util.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

using testing::HasSubstr;

#undef EXPECT_OK
#define EXPECT_OK(value)      \
  {                           \
    const auto &s = (value);  \
    EXPECT_TRUE(s.ok()) << s; \
  }
#undef ASSERT_OK
#define ASSERT_OK(value)      \
  {                           \
    const auto &s = (value);  \
    ASSERT_TRUE(s.ok()) << s; \
  }

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
static std::string PlatformPath(const std::string &path) {
  return std::filesystem::path(path).lexically_normal().string();
}

TEST(FileUtil, GetContentAsMemBlock) {
  auto result = file::GetContentAsMemBlock("non-existing-file");
  EXPECT_FALSE(result.status().ok());

  const std::string test_file = file::JoinPath(testing::TempDir(), "blockfile");
  constexpr std::string_view kTestContent = "Some file content\nbaz\r\n";
  EXPECT_OK(file::SetContents(test_file, kTestContent));

  result = file::GetContentAsMemBlock(test_file);
  EXPECT_OK(result.status());
  auto &block = *result;
  EXPECT_EQ(block->AsStringView(), kTestContent);
}

TEST(FileUtil, JoinPath) {
  EXPECT_EQ(file::JoinPath("foo", ""), PlatformPath("foo/"));
  EXPECT_EQ(file::JoinPath("", "bar"), "bar");
  EXPECT_EQ(file::JoinPath("foo", "bar"), PlatformPath("foo/bar"));

  // Assemble an absolute path
  EXPECT_EQ(file::JoinPath("", "/bar"), PlatformPath("/bar"));
  EXPECT_EQ(file::JoinPath("/", "bar"), PlatformPath("/bar"));
  EXPECT_EQ(file::JoinPath("/", "/bar"), PlatformPath("/bar"));

  // Absolute path stays absolute, base not prepended.
  EXPECT_EQ(file::JoinPath("foo/", "/bar"), PlatformPath("/bar"));
  EXPECT_EQ(file::JoinPath("foo/", "///bar"), PlatformPath("/bar"));

  // Lightly canonicalize multiple consecutive slashes
  EXPECT_EQ(file::JoinPath("foo/", "bar"), PlatformPath("foo/bar"));
  EXPECT_EQ(file::JoinPath("foo///", "bar"), PlatformPath("foo/bar"));

  // Lightly canonicalize ./ and ../
  EXPECT_EQ(file::JoinPath("", "./bar"), PlatformPath("bar"));
  EXPECT_EQ(file::JoinPath("", "./bar/../bar"), PlatformPath("bar"));
  EXPECT_EQ(file::JoinPath(".", "./bar"), PlatformPath("bar"));
  EXPECT_EQ(file::JoinPath("foo/./", "./bar"), PlatformPath("foo/bar"));
  EXPECT_EQ(file::JoinPath("/foo/./", "./bar"), PlatformPath("/foo/bar"));
  EXPECT_EQ(file::JoinPath("/foo/baz/../", "./bar"), PlatformPath("/foo/bar"));

  // Document behavior of concatenating current directory.
  EXPECT_EQ(file::JoinPath("", "."), PlatformPath("."));
  EXPECT_EQ(file::JoinPath("", "./"), PlatformPath("."));
  EXPECT_EQ(file::JoinPath("./", ""), PlatformPath("."));
  EXPECT_EQ(file::JoinPath(".", ""), PlatformPath("."));

#ifdef _WIN32
  EXPECT_EQ(file::JoinPath("C:\\foo", "bar"), "C:\\foo\\bar");
#endif
}

TEST(FileUtil, CreateDir) {
  const std::string test_dir = file::JoinPath(testing::TempDir(), "test_dir");
  const std::string test_file = file::JoinPath(test_dir, "foo");
  const std::string_view test_content = "directory create test";

  EXPECT_OK(file::CreateDir(test_dir));
  EXPECT_OK(file::CreateDir(test_dir));  // Creating twice should succeed

  EXPECT_OK(file::SetContents(test_file, test_content));
  absl::StatusOr<std::string> read_back_content_or =
      file::GetContentAsString(test_file);
  ASSERT_OK(read_back_content_or.status());
  EXPECT_EQ(test_content, *read_back_content_or);
}

TEST(FileUtil, StatusErrorReporting) {
  absl::StatusOr<std::string> content_or;

  // Reading contents from a non-existent file.
  content_or = file::GetContentAsString("does-not-exist");
  EXPECT_FALSE(content_or.ok());
  EXPECT_TRUE(absl::IsNotFound(content_or.status())) << content_or.status();
  EXPECT_TRUE(
      absl::StartsWith(content_or.status().message(), "does-not-exist:"))
      << "expect filename prefixed, but got " << content_or.status();

  // Write/read roundtrip
  const std::string test_file = file::JoinPath(testing::TempDir(), "test-trip");
  unlink(test_file.c_str());  // Remove file if left from previous test.
  // Add a bunch of text 'special' characcters to make sure even on Windows
  // they roundtrip correctly.
  constexpr std::string_view kTestContent = "foo\nbar\r\nbaz\rquux";
  EXPECT_OK(file::SetContents(test_file, kTestContent));

  // Writing again, should not append, but re-write.
  EXPECT_OK(file::SetContents(test_file, kTestContent));

  content_or = file::GetContentAsString(test_file);
  EXPECT_OK(content_or.status());
  EXPECT_EQ(*content_or, kTestContent);

  const std::string test_dir = file::JoinPath(testing::TempDir(), "test-dir");
  ASSERT_OK(file::CreateDir(test_dir));

  // Attempt to write to a directory as file.
  {
    auto status = file::SetContents(test_dir, "shouldn't write to directory");
    EXPECT_FALSE(status.ok()) << status;
    EXPECT_TRUE(absl::IsInvalidArgument(status) ||  // Unix reports this
                absl::IsPermissionDenied(status))   // Windows this
        << status;
    EXPECT_TRUE(absl::StartsWith(status.message(), test_dir))
        << "expect filename prefixed, but got " << status;
  }

  // Attempt to read a directory as file.
  {
    content_or = file::GetContentAsString(test_dir);
    EXPECT_FALSE(content_or.status().ok());
    EXPECT_TRUE(absl::IsInvalidArgument(content_or.status()));
    EXPECT_TRUE(absl::StartsWith(content_or.status().message(), test_dir))
        << "expect filename prefixed, but got " << content_or.status();
  }

#ifndef _WIN32
  // The following chmod() is not working on Win32. So let's not use
  // this test here.
  // TODO: Can we make permission-denied test that works on Windows ?

  // Issue #963 - if this test is run as root, the permission issue
  // will not manifest. Also if chmod() did not succeed. Skip in this case.
  if (geteuid() != 0 && chmod(test_file.c_str(), 0) == 0) {
    // Enforce a permission denied situation
    content_or = file::GetContentAsString(test_file);
    EXPECT_FALSE(content_or.ok())
        << "Expected permission denied for " << test_file;
    const absl::Status &status = content_or.status();
    EXPECT_TRUE(absl::IsPermissionDenied(status)) << status;
    EXPECT_TRUE(absl::StartsWith(status.message(), test_file))
        << "expect filename prefixed, but got " << status;
  }
#endif
}

TEST(FileUtil, ScopedTestFile) {
  const std::string_view test_content = "Hello World!";
  ScopedTestFile test_file(testing::TempDir(), test_content);
  auto read_back_content_or = file::GetContentAsString(test_file.filename());
  ASSERT_TRUE(read_back_content_or.ok());
  EXPECT_EQ(test_content, *read_back_content_or);
}

static ScopedTestFile TestFileGenerator(std::string_view content) {
  return ScopedTestFile(testing::TempDir(), content);
}

static void TestFileConsumer(ScopedTestFile &&f) {
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
      names.emplace_back(files.back().filename());
    }
    for (const auto &name : names) {
      EXPECT_TRUE(file::FileExists(name).ok());
    }
  }
  for (const auto &name : names) {
    EXPECT_FALSE(file::FileExists(name).ok());
  }
}

TEST(FileUtil, FileNotExistsTests) {
  absl::Status s = file::FileExists("not/an/existing/file");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), absl::StatusCode::kNotFound);
}

TEST(FileUtil, FileExistsDirectoryErrorMessage) {
  absl::Status s;
  s = file::FileExists(testing::TempDir());
  EXPECT_FALSE(s.ok());
  EXPECT_THAT(s.message(), HasSubstr("is a directory"));
}

static bool CreateFsStructure(std::string_view base_dir,
                              const std::vector<std::string_view> &tree) {
  for (std::string_view path : tree) {
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

  const auto &dir = *dir_or;
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

  const std::string_view test_content = "Hello World!";
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

  const auto &dir = *dir_or;
  EXPECT_EQ(dir.path, test_dir);

  EXPECT_EQ(dir.directories.size(), test_directories.size());
  EXPECT_EQ(dir.directories, test_directories);

  EXPECT_EQ(dir.files.size(), test_files.size());
  EXPECT_EQ(dir.files, test_files);
}

TEST(FileUtil, IsStdin) {
  EXPECT_EQ(file::IsStdin("-"), true);
  EXPECT_EQ(file::IsStdin("not stdin!"), false);
}

}  // namespace
}  // namespace util
}  // namespace verible
