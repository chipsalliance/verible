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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::ElementsAre;

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

}  // namespace verilog
