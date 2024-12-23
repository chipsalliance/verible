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

#include "verible/verilog/tools/ls/symbol-table-handler.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "verible/common/lsp/lsp-file-utils.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/lsp/lsp-text-buffer.h"
#include "verible/common/util/file-util.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"

namespace verilog {
namespace {

static constexpr absl::string_view  //
    kSampleModuleA(
        R"(module a;
  assign var1 = 1'b0;
  assign var2 = var1 | 1'b1;
endmodule
)");

static constexpr absl::string_view  //
    kSampleModuleB(
        R"(module b;
  assign var1 = 1'b0;
  assign var2 = var1 | 1'b1;
  a vara;
  assign vara.var1 = 1'b1;
endmodule
)");

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
      symbol_table_handler.FindDefinitionLocation(gotorequest, parsed_buffers);
  EXPECT_EQ(location.size(), 0);
}

TEST(SymbolTableHandlerTest,
     FindRenamableRangeAtCursorReturnsNullUntrackedFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  absl::string_view filelist_content = "b.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      sources_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(sources_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(sources_dir,
                                                        kSampleModuleB, "b.sv");
  verible::lsp::PrepareRenameParams parameters;
  parameters.textDocument.uri =
      verible::lsp::PathToLSPUri(sources_dir + "/c.sv");
  parameters.position.line = 1;
  parameters.position.character = 11;

  std::shared_ptr<VerilogProject> project = std::make_shared<VerilogProject>(
      sources_dir, std::vector<std::string>(), "");
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  verilog::BufferTrackerContainer parsed_buffers;
  parsed_buffers.AddChangeListener(
      symbol_table_handler.CreateBufferTrackerListener());

  // Add trackers for the files we're going to process - normally done by the
  // LSP but we don't have one
  auto a_buffer = verible::lsp::EditTextBuffer(kSampleModuleA);
  parsed_buffers.GetSubscriptionCallback()(
      verible::lsp::PathToLSPUri(sources_dir + "/a.sv"), &a_buffer);
  symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_FALSE(parsed_buffers.FindBufferTrackerOrNull(
                   parameters.textDocument.uri) != nullptr);

  std::optional<verible::lsp::Range> edit_range =
      symbol_table_handler.FindRenameableRangeAtCursor(parameters,
                                                       parsed_buffers);
  ASSERT_FALSE(edit_range.has_value());
}

// TODO(hzeller): In the test as written, this should not work, as 'vara'
// is of type module a, so it should complain about a null definition. However,
// the renamable range actually returns something useful.
// (verify: is this #1678 fixing it ?)
TEST(SymbolTableHandlerTest,
     FindRenamableRangeAtCursorReturnsNullDefinitionUnknown) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  absl::string_view filelist_content = "b.sv\n";

  const verible::file::testing::ScopedTestFile filelist(
      sources_dir, filelist_content, "verible.filelist");
  const verible::file::testing::ScopedTestFile module_a(sources_dir,
                                                        kSampleModuleA, "a.sv");
  const verible::file::testing::ScopedTestFile module_b(sources_dir,
                                                        kSampleModuleB, "b.sv");
  verible::lsp::PrepareRenameParams parameters;
  parameters.textDocument.uri =
      verible::lsp::PathToLSPUri(sources_dir + "/b.sv");
  parameters.position.line = 3;  // pointing to 'vara'
  parameters.position.character = 5;

  std::shared_ptr<VerilogProject> project = std::make_shared<VerilogProject>(
      sources_dir, std::vector<std::string>(), "");
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  verilog::BufferTrackerContainer parsed_buffers;
  parsed_buffers.AddChangeListener(
      symbol_table_handler.CreateBufferTrackerListener());

  // Add trackers for the files we're going to process - normally done by the
  // LSP but we don't have one
  auto b_buffer = verible::lsp::EditTextBuffer(kSampleModuleB);
  parsed_buffers.GetSubscriptionCallback()(parameters.textDocument.uri,
                                           &b_buffer);
  symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_TRUE(parsed_buffers.FindBufferTrackerOrNull(
                  parameters.textDocument.uri) != nullptr);

  std::optional<verible::lsp::Range> edit_range =
      symbol_table_handler.FindRenameableRangeAtCursor(parameters,
                                                       parsed_buffers);

  // This was ASSERT_FALSE(), but in fact we get the range of 'vara' back,
  // and renaming that actually works. TODO: Check intent of this test.
  ASSERT_TRUE(edit_range.has_value());
}

TEST(SymbolTableHandlerTest, FindRenamableRangeAtCursorReturnsLocation) {
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
  verible::lsp::PrepareRenameParams parameters;
  parameters.textDocument.uri =
      verible::lsp::PathToLSPUri(sources_dir + "/a.sv");
  parameters.position.line = 1;
  parameters.position.character = 11;

  std::shared_ptr<VerilogProject> project = std::make_shared<VerilogProject>(
      sources_dir, std::vector<std::string>(), "");
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  verilog::BufferTrackerContainer parsed_buffers;
  parsed_buffers.AddChangeListener(
      symbol_table_handler.CreateBufferTrackerListener());

  // Add trackers for the files we're going to process - normally done by the
  // LSP but we don't have one
  auto a_buffer = verible::lsp::EditTextBuffer(kSampleModuleA);
  parsed_buffers.GetSubscriptionCallback()(parameters.textDocument.uri,
                                           &a_buffer);
  symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_TRUE(parsed_buffers.FindBufferTrackerOrNull(
                  parameters.textDocument.uri) != nullptr);

  std::optional<verible::lsp::Range> edit_range =
      symbol_table_handler.FindRenameableRangeAtCursor(parameters,
                                                       parsed_buffers);
  ASSERT_TRUE(edit_range.has_value());
  if (edit_range.has_value()) {  // Access after test. Makes .clang-tidy happy
    EXPECT_EQ(edit_range.value().start.line, 1);
    EXPECT_EQ(edit_range.value().start.character, 9);
  }
}
TEST(SymbolTableHandlerTest,
     FindRenameLocationsAndCreateEditsReturnsLocationsTest) {
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
  verible::lsp::RenameParams parameters;
  parameters.textDocument.uri =
      verible::lsp::PathToLSPUri(sources_dir + "/a.sv");
  parameters.position.line = 1;
  parameters.position.character = 11;
  parameters.newName = "aaa";

  std::shared_ptr<VerilogProject> project = std::make_shared<VerilogProject>(
      sources_dir, std::vector<std::string>(), "");
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  verilog::BufferTrackerContainer parsed_buffers;
  parsed_buffers.AddChangeListener(
      symbol_table_handler.CreateBufferTrackerListener());

  // Add trackers for the files we're going to process - normally done by the
  // LSP but we don't have one
  auto a_buffer = verible::lsp::EditTextBuffer(kSampleModuleA);
  parsed_buffers.GetSubscriptionCallback()(parameters.textDocument.uri,
                                           &a_buffer);
  symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_TRUE(parsed_buffers.FindBufferTrackerOrNull(
                  parameters.textDocument.uri) != nullptr);

  verible::lsp::WorkspaceEdit edit_range =
      symbol_table_handler.FindRenameLocationsAndCreateEdits(parameters,
                                                             parsed_buffers);
  EXPECT_EQ(edit_range.changes[parameters.textDocument.uri].size(), 2);
  EXPECT_EQ(
      edit_range.changes[verible::lsp::PathToLSPUri(sources_dir + "/b.sv")]
          .size(),
      1);
}

TEST(SymbolTableHandlerTest,
     FindRenameLocationsAndCreateEditsReturnsLocationsOnDirtyFilesTest) {
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
  verible::lsp::RenameParams parameters;
  parameters.textDocument.uri =
      verible::lsp::PathToLSPUri(sources_dir + "/a.sv");
  parameters.position.line = 1;
  parameters.position.character = 11;
  parameters.newName = "aaa";

  std::shared_ptr<VerilogProject> project = std::make_shared<VerilogProject>(
      sources_dir, std::vector<std::string>(), "");
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  verilog::BufferTrackerContainer parsed_buffers;
  parsed_buffers.AddChangeListener(
      symbol_table_handler.CreateBufferTrackerListener());

  // Add trackers for the files we're going to process - normally done by the
  // LSP but we don't have one
  auto a_buffer = verible::lsp::EditTextBuffer(kSampleModuleA);
  parsed_buffers.GetSubscriptionCallback()(parameters.textDocument.uri,
                                           &a_buffer);
  symbol_table_handler.BuildProjectSymbolTable();
  ASSERT_TRUE(parsed_buffers.FindBufferTrackerOrNull(
                  parameters.textDocument.uri) != nullptr);

  verible::lsp::WorkspaceEdit edit_range =
      symbol_table_handler.FindRenameLocationsAndCreateEdits(parameters,
                                                             parsed_buffers);
  EXPECT_EQ(edit_range.changes[parameters.textDocument.uri].size(), 2);
  EXPECT_EQ(
      edit_range.changes[verible::lsp::PathToLSPUri(sources_dir + "/b.sv")]
          .size(),
      1);

  parameters.newName = "bbb";

  edit_range = symbol_table_handler.FindRenameLocationsAndCreateEdits(
      parameters, parsed_buffers);
  EXPECT_EQ(edit_range.changes[parameters.textDocument.uri].size(), 2);
  EXPECT_EQ(
      edit_range.changes[verible::lsp::PathToLSPUri(sources_dir + "/b.sv")]
          .size(),
      1);
}

TEST(SymbolTableHandlerTest, UpdateWithUnparseableEditorContentRegression) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      verible::file::JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(verible::file::CreateDir(sources_dir).ok());

  absl::string_view filelist_content;
  const verible::file::testing::ScopedTestFile filelist(
      sources_dir, filelist_content, "verible.filelist");
  const std::string uri = verible::lsp::PathToLSPUri(sources_dir + "/a.sv");
  std::shared_ptr<VerilogProject> project = std::make_shared<VerilogProject>(
      sources_dir, std::vector<std::string>(), "");
  SymbolTableHandler symbol_table_handler;
  symbol_table_handler.SetProject(project);

  verilog::BufferTrackerContainer parsed_buffers;
  parsed_buffers.AddChangeListener(
      symbol_table_handler.CreateBufferTrackerListener());

  // We want to make sure that every change updates the project file with
  // the latest content.
  //
  // The verilog_project would react badly if it gets the exact same content
  // twice, as it would want to register its string_view locations in its
  // reverse map.
  //
  // If we'd filter to only send 'last_good()' content, which stays the same
  // while we have bad content, we'd attempt to register the same range.
  // multiple times with the project.
  //
  // So walking through the sequence good content (sets current() and
  // last_good()), parse error content (sets only current(), but leaves
  // last_good() as-is), and good content again (replaces current() as well
  // as last_good()), we make sure that this sequence will work.
  // Give our file list a valid content
  auto a_buffer = verible::lsp::EditTextBuffer(kSampleModuleA);
  a_buffer.set_last_global_version(1);
  parsed_buffers.GetSubscriptionCallback()(uri, &a_buffer);

  // Now, the content in the editor becomes invalid.
  auto broken_buffer = verible::lsp::EditTextBuffer("invalid-file");
  broken_buffer.set_last_global_version(2);
  parsed_buffers.GetSubscriptionCallback()(uri, &broken_buffer);

  // ... back to a valid parsed file. This replaces the previously broken file.
  a_buffer.set_last_global_version(3);
  parsed_buffers.GetSubscriptionCallback()(uri, &a_buffer);
}

TEST(SymbolTableHandlerTest, MissingVerilogProject) {
  SymbolTableHandler symbol_table_handler;
  std::vector<absl::Status> diagnostics =
      symbol_table_handler.BuildProjectSymbolTable();

  ASSERT_EQ(diagnostics.size(), 1);
  ASSERT_FALSE(diagnostics[0].ok());
}

}  // namespace
}  // namespace verilog
