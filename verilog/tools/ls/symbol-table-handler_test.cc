// Copyright 2017-2023 The Verible Authors.
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

#include "verilog/tools/ls/symbol-table-handler.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "common/util/file_util.h"
#include "gtest/gtest.h"
#include "verilog/analysis/verilog_project.h"

namespace verilog {
namespace {

static constexpr absl::string_view  //
    kSampleModuleA(
        "module a;\n"
        "  assign var1 = 1'b0;\n"
        "  assign var2 = var1 | 1'b1;\n"
        "endmodule\n");

static constexpr absl::string_view  //
    kSampleModuleB(
        "module b;\n"
        "  assign var1 = 1'b0;\n"
        "  assign var2 = var1 | 1'b1;\n"
        "  a vara;\n"
        "  assign vara.var1 = 1'b1;\n"
        "endmodule\n");

// Tests the behavior of SymbolTableHandler for not existing directory.
TEST(SymbolTableHandlerTest, InitializationNoRoot) {
  std::shared_ptr<VerilogProject> project = std::make_shared<VerilogProject>(
      "non-existing-root", std::vector<std::string>());

  SymbolTableHandler symbol_table_handler;

  symbol_table_handler.SetProject(project);
}

// Tests the behavior of SymbolTableHandler for an empty directory.
TEST(SymbolTableHandlerTest, EmptyDirectoryProject) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  std::shared_ptr<VerilogProject> project =
      std::make_shared<VerilogProject>(sources_dir, std::vector<std::string>());

  SymbolTableHandler symbol_table_handler;

  symbol_table_handler.SetProject(project);

  symbol_table_handler.BuildProjectSymbolTable();
}

TEST(SymbolTableHandlerTest, NullVerilogProject) {
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(nullptr);
}

TEST(SymbolTableHandlerTest, InvalidFileListSyntax) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  const verible::file::testing::ScopedTestFile filelist(sources_dir, "@@",
                                                        "verible.filelist");

  std::shared_ptr<VerilogProject> project =
      std::make_shared<VerilogProject>(sources_dir, std::vector<std::string>());

  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  symbol_table_handler.BuildProjectSymbolTable();
}

TEST(SymbolTableHandlerTest, LoadFileList) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  absl::string_view filelist_content =
      "a.sv\n"
      "b.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      sources_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(sources_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(sources_dir,
                                                        kSampleModuleB, "b.sv");

  std::shared_ptr<VerilogProject> project =
      std::make_shared<VerilogProject>(sources_dir, std::vector<std::string>{});

  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  std::vector<absl::Status> diagnostics =
      symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_EQ(diagnostics.size(), 0);
}

TEST(SymbolTableHandlerTest, FileListWithNonExistingFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  absl::string_view filelist_content =
      "a.sv\n"
      "b.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      sources_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(sources_dir,
                                                        kSampleModuleA, "a.sv");

  std::shared_ptr<VerilogProject> project =
      std::make_shared<VerilogProject>(sources_dir, std::vector<std::string>{});

  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  std::vector<absl::Status> diagnostics =
      symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_EQ(diagnostics.size(), 1);
}

TEST(SymbolTableHandlerTest, MissingFileList) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  const verible::file::testing::ScopedTestFile module_a(sources_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(sources_dir,
                                                        kSampleModuleB, "b.sv");

  std::shared_ptr<VerilogProject> project =
      std::make_shared<VerilogProject>(sources_dir, std::vector<std::string>{});

  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  std::vector<absl::Status> diagnostics =
      symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_EQ(diagnostics.size(), 0);
}

TEST(SymbolTableHandlerTest, DefinitionNotTrackedFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  absl::string_view filelist_content =
      "a.sv\n"
      "b.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      sources_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(sources_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(sources_dir,
                                                        kSampleModuleB, "b.sv");

  std::shared_ptr<VerilogProject> project =
      std::make_shared<VerilogProject>(sources_dir, std::vector<std::string>{});

  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  verible::lsp::DefinitionParams gotorequest;
  gotorequest.textDocument.uri = "file://b.sv";
  gotorequest.position.line = 2;
  gotorequest.position.character = 17;

  std::vector<absl::Status> diagnostics =
      symbol_table_handler.BuildProjectSymbolTable();

  verilog::BufferTrackerContainer parsed_buffers;
  parsed_buffers.FindBufferTrackerOrNull(gotorequest.textDocument.uri);
  EXPECT_EQ(
      parsed_buffers.FindBufferTrackerOrNull(gotorequest.textDocument.uri),
      nullptr);

  std::vector<verible::lsp::Location> location =
      symbol_table_handler.FindDefinition(gotorequest, parsed_buffers);
  EXPECT_EQ(location.size(), 0);
}

TEST(SymbolTableHandlerTest, MissingVerilogProject) {
  SymbolTableHandler symbol_table_handler;
  std::vector<absl::Status> diagnostics =
      symbol_table_handler.BuildProjectSymbolTable();

  ASSERT_EQ(diagnostics.size(), 1);
  ASSERT_FALSE(diagnostics[0].ok());
}

TEST(SymbolTableHandlerTest, GoToDefinitionTest) {}

}  // namespace
}  // namespace verilog
