// Copyright 2022 The Verible Authors.
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

#include "verilog/analysis/verilog_filelist.h"

#include "common/util/file_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;
using verible::file::testing::ScopedTestFile;

namespace verilog {

TEST(FileListTest, Append) {
  FileList a;
  a.file_list_path = "path A";
  a.file_paths = {"file1.sv", "file2.sv"};
  a.preprocessing.include_dirs = {"foo", "bar"};
  a.preprocessing.defines = {{"DEBUG", "1"}, {"A", "B"}};

  FileList b;
  b.file_list_path = "path B";
  b.file_paths = {"file3.sv"};
  b.preprocessing.include_dirs = {"baz"};
  b.preprocessing.defines = {{"C", "D"}};

  a.Append(b);

  EXPECT_EQ(a.file_list_path, "path A");  // not modified
  EXPECT_THAT(a.file_paths, ElementsAre("file1.sv", "file2.sv", "file3.sv"));
  EXPECT_THAT(a.preprocessing.include_dirs, ElementsAre("foo", "bar", "baz"));
  EXPECT_EQ(a.preprocessing.defines.size(), 3);
}

TEST(FileListTest, ParseSourceFileList) {
  const auto tempdir = ::testing::TempDir();
  const std::string file_list_content = R"(
    # A comment to ignore.
    +incdir+/an/include_dir1
    // Another comment
    // on two lines
    +incdir+/an/include_dir2

    /a/source/file/1.sv
    /a/source/file/2.sv
  )";
  const ScopedTestFile file_list_file(tempdir, file_list_content);
  auto parsed_file_list =
      ParseSourceFileListFromFile(file_list_file.filename());
  ASSERT_TRUE(parsed_file_list.ok());

  EXPECT_EQ(parsed_file_list->file_list_path, file_list_file.filename());
  EXPECT_THAT(parsed_file_list->file_paths,
              ElementsAre("/a/source/file/1.sv", "/a/source/file/2.sv"));
  EXPECT_THAT(parsed_file_list->preprocessing.include_dirs,
              ElementsAre(".", "/an/include_dir1", "/an/include_dir2"));
}

TEST(FileListTest, ParseInvalidSourceFileListFromCommandline) {
  std::vector<std::vector<absl::string_view>> test_cases = {
      {"+define+macro1="}, {"+define+"}, {"+not_valid_define+"}};
  for (const auto& cmdline : test_cases) {
    auto parsed_file_list = ParseSourceFileListFromCommandline(cmdline);
    EXPECT_FALSE(parsed_file_list.ok());
  }
}

TEST(FileListTest, ParseSourceFileListFromCommandline) {
  std::vector<absl::string_view> cmdline = {
      "+define+macro1=text1+macro2+macro3=text3",
      "file1",
      "+define+macro4",
      "file2",
      "+incdir+~/path/to/file1+path/to/file2",
      "+incdir+./path/to/file3",
      "+define+macro5",
      "file3",
      "+define+macro6=a=b",
      "+incdir+../path/to/file4+./path/to/file5"};
  auto parsed_file_list = ParseSourceFileListFromCommandline(cmdline);
  ASSERT_TRUE(parsed_file_list.ok());

  EXPECT_THAT(parsed_file_list->file_paths,
              ElementsAre("file1", "file2", "file3"));
  EXPECT_THAT(parsed_file_list->preprocessing.include_dirs,
              ElementsAre("~/path/to/file1", "path/to/file2", "./path/to/file3",
                          "../path/to/file4", "./path/to/file5"));
  std::vector<TextMacroDefinition> macros = {
      {"macro1", "text1"}, {"macro2", ""}, {"macro3", "text3"},
      {"macro4", ""},      {"macro5", ""}, {"macro6", "a=b"}};
  EXPECT_THAT(parsed_file_list->preprocessing.defines,
              ElementsAre(macros[0], macros[1], macros[2], macros[3], macros[4],
                          macros[5]));
}

}  // namespace verilog
