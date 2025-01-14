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

#include "verible/verilog/analysis/verilog-filelist.h"

#include <string>
#include <string_view>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/file-util.h"

namespace verilog {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::verible::file::testing::ScopedTestFile;

TEST(FileListTest, AppendFileListFromFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string file_list_content = R"(
    # A comment to ignore.
    +incdir+/an/include_dir1
    // Another comment
    // on two lines
    +incdir+/an/include_dir2
    +define+macro1=a
    +define+invalid_macro

    /a/source/file/1.sv
    /a/source/file/2.sv
  )";
  const ScopedTestFile file_list_file(tempdir, file_list_content);
  FileList result;
  auto status = AppendFileListFromFile(file_list_file.filename(), &result);
  ASSERT_TRUE(status.ok()) << status;

  EXPECT_THAT(result.file_paths,
              ElementsAre("/a/source/file/1.sv", "/a/source/file/2.sv"));
  EXPECT_THAT(result.preprocessing.include_dirs,
              ElementsAre(".", "/an/include_dir1", "/an/include_dir2"));
  EXPECT_THAT(result.preprocessing.defines,
              ElementsAre(TextMacroDefinition("macro1", "a")));
}

TEST(FileListTest, AppendFileListFromInvalidCommandline) {
  std::vector<std::vector<std::string_view>> test_cases = {
      {"+define+macro1="},
      {"+define+"},
      {"+not_valid_define+"},
      {"+foobar+baz"}};
  for (const auto &cmdline : test_cases) {
    FileList result;
    auto status = AppendFileListFromCommandline(cmdline, &result);
    EXPECT_FALSE(status.ok());
  }
}

TEST(FileListTest, AppendFileListFromCommandline) {
  std::vector<std::string_view> cmdline = {
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
  FileList result;
  auto status = AppendFileListFromCommandline(cmdline, &result);
  ASSERT_TRUE(status.ok()) << status;

  EXPECT_THAT(result.file_paths, ElementsAre("file1", "file2", "file3"));
  EXPECT_THAT(result.preprocessing.include_dirs,
              ElementsAre("~/path/to/file1", "path/to/file2", "./path/to/file3",
                          "../path/to/file4", "./path/to/file5"));
  std::vector<TextMacroDefinition> macros = {
      {"macro1", "text1"}, {"macro2", ""}, {"macro3", "text3"},
      {"macro4", ""},      {"macro5", ""}, {"macro6", "a=b"}};
  EXPECT_THAT(result.preprocessing.defines,
              ElementsAre(macros[0], macros[1], macros[2], macros[3], macros[4],
                          macros[5]));
}

TEST(FileListTest, ToString) {
  const auto tempdir = ::testing::TempDir();
  const std::string file_list_content = R"(
    # A comment to ignore.
    +incdir+/an/include_dir1
    // Another comment
    // on two lines
    +incdir+/an/include_dir2
    +define+macro1=a
    file0
    +define+macro2=a=b

    /a/source/file/1.sv
    /a/source/file/2.sv
  )";
  const ScopedTestFile file_list_file(tempdir, file_list_content);
  FileList result;
  auto status = AppendFileListFromFile(file_list_file.filename(), &result);
  ASSERT_TRUE(status.ok()) << status;

  EXPECT_THAT(result.ToString(), Eq(R"(+define+macro1=a
+define+macro2=a=b
+incdir+.
+incdir+/an/include_dir1
+incdir+/an/include_dir2
file0
/a/source/file/1.sv
/a/source/file/2.sv
)"));
}

}  // namespace
}  // namespace verilog
