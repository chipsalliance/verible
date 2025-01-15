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

#include "verible/verilog/analysis/verilog-project.h"

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/range.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/analysis/verilog-analyzer.h"

namespace verilog {
namespace {

using verible::TextStructureView;
using verible::file::Basename;
using verible::file::CreateDir;
using verible::file::JoinPath;
using verible::file::testing::ScopedTestFile;

class TempDirFile : public ScopedTestFile {
 public:
  explicit TempDirFile(std::string_view content)
      : ScopedTestFile(::testing::TempDir(), content) {}
};

TEST(VerilogSourceFileTest, Initialization) {
  const VerilogSourceFile file("a.sv", "x/y/a.sv", "");
  // no attempt to open this file yet
  EXPECT_EQ(file.ReferencedPath(), "a.sv");
  EXPECT_EQ(file.ResolvedPath(), "x/y/a.sv");
  EXPECT_TRUE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), nullptr);
}

TEST(VerilogSourceFileTest, OpenExistingFile) {
  constexpr std::string_view text("localparam int p = 1;\n");
  TempDirFile tf(text);
  const std::string_view basename(Basename(tf.filename()));
  VerilogSourceFile file(basename, tf.filename(), "");
  EXPECT_TRUE(file.Open().ok());
  EXPECT_TRUE(file.Status().ok());
  EXPECT_EQ(file.ReferencedPath(), basename);
  EXPECT_EQ(file.ResolvedPath(), tf.filename());
  const std::string_view owned_string_range(file.GetContent());
  EXPECT_EQ(owned_string_range, text);

  // Re-opening doesn't change anything
  EXPECT_TRUE(file.Open().ok());
  EXPECT_TRUE(file.Status().ok());
  EXPECT_TRUE(verible::BoundsEqual(file.GetContent(), owned_string_range));
}

TEST(VerilogSourceFileTest, NonExistingFile) {
  VerilogSourceFile file("aa.sv", "/does/not/exist/aa.sv", "");
  EXPECT_FALSE(file.Open().ok());
  EXPECT_FALSE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), nullptr);
  // Still not there.
  EXPECT_FALSE(file.Open().ok());
  EXPECT_FALSE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), nullptr);
}

TEST(VerilogSourceFileTest, ParseValidFile) {
  constexpr std::string_view text("localparam int p = 1;\n");
  TempDirFile tf(text);
  const std::string_view basename(Basename(tf.filename()));
  VerilogSourceFile file(basename, tf.filename(), "");
  // Parse automatically opens.
  EXPECT_TRUE(file.Parse().ok());
  EXPECT_TRUE(file.Status().ok());
  const TextStructureView *text_structure =
      ABSL_DIE_IF_NULL(file.GetTextStructure());
  const std::string_view owned_string_range(text_structure->Contents());
  EXPECT_EQ(owned_string_range, text);
  const auto *tokens = &text_structure->TokenStream();
  EXPECT_NE(tokens, nullptr);
  const auto *tree = &text_structure->SyntaxTree();
  EXPECT_NE(tree, nullptr);

  // Re-parsing doesn't change anything
  EXPECT_TRUE(file.Parse().ok());
  EXPECT_TRUE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), text_structure);
  EXPECT_EQ(&text_structure->TokenStream(), tokens);
  EXPECT_EQ(&text_structure->SyntaxTree(), tree);
}

TEST(VerilogSourceFileTest, ParseInvalidFile) {
  constexpr std::string_view text("localparam 1 = p;\n");
  TempDirFile tf(text);
  const std::string_view basename(Basename(tf.filename()));
  VerilogSourceFile file(basename, tf.filename(), "");
  // Parse automatically opens.
  EXPECT_FALSE(file.Parse().ok());
  EXPECT_FALSE(file.Status().ok());
  const TextStructureView *text_structure =
      ABSL_DIE_IF_NULL(file.GetTextStructure());
  const std::string_view owned_string_range(text_structure->Contents());
  EXPECT_EQ(owned_string_range, text);
  const auto *tokens = &text_structure->TokenStream();
  EXPECT_NE(tokens, nullptr);
  const auto *tree = &text_structure->SyntaxTree();
  EXPECT_NE(tree, nullptr);
  // but syntax tree may be empty, depends on error-recovery

  // Re-parsing doesn't change anything
  EXPECT_FALSE(file.Parse().ok());
  EXPECT_FALSE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), text_structure);
  EXPECT_EQ(&text_structure->TokenStream(), tokens);
  EXPECT_EQ(&text_structure->SyntaxTree(), tree);
}

TEST(VerilogSourceFileTest, StreamPrint) {
  constexpr std::string_view text("localparam foo = bar;\n");
  TempDirFile tf(text);
  const std::string_view basename(Basename(tf.filename()));
  VerilogSourceFile file(basename, tf.filename(), "");

  {
    std::ostringstream stream;
    stream << file;
    const std::string str(stream.str());
    EXPECT_TRUE(
        absl::StrContains(str, absl::StrCat("referenced path: ", basename)));
    EXPECT_TRUE(
        absl::StrContains(str, absl::StrCat("resolved path: ", tf.filename())));
    EXPECT_TRUE(absl::StrContains(str, "status: ok"));
    EXPECT_TRUE(absl::StrContains(str, "have text structure? no"));
  }

  {  // After parsng, we have a text structure.
    EXPECT_TRUE(file.Parse().ok());
    std::ostringstream stream;
    stream << file;
    const std::string str(stream.str());
    EXPECT_TRUE(absl::StrContains(str, "have text structure? yes"));
  }
}

TEST(InMemoryVerilogSourceFileTest, ParseValidFile) {
  constexpr std::string_view text("localparam int p = 1;\n");
  InMemoryVerilogSourceFile file("/not/using/file/system.v", text);
  // Parse automatically opens.
  EXPECT_TRUE(file.Parse().ok());
  EXPECT_TRUE(file.Status().ok());
  const TextStructureView *text_structure =
      ABSL_DIE_IF_NULL(file.GetTextStructure());
  const std::string_view owned_string_range(text_structure->Contents());
  EXPECT_EQ(owned_string_range, text);
  const auto *tokens = &text_structure->TokenStream();
  EXPECT_NE(tokens, nullptr);
  const auto *tree = &text_structure->SyntaxTree();
  EXPECT_NE(tree, nullptr);

  // Re-parsing doesn't change anything
  EXPECT_TRUE(file.Parse().ok());
  EXPECT_TRUE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), text_structure);
  EXPECT_EQ(&text_structure->TokenStream(), tokens);
  EXPECT_EQ(&text_structure->SyntaxTree(), tree);
}

TEST(InMemoryVerilogSourceFileTest, ParseInvalidFile) {
  constexpr std::string_view text("class \"dismissed\"!\n");
  InMemoryVerilogSourceFile file("/not/using/file/system.v", text);
  // Parse automatically opens.
  EXPECT_FALSE(file.Parse().ok());
  EXPECT_FALSE(file.Status().ok());
  const TextStructureView *text_structure =
      ABSL_DIE_IF_NULL(file.GetTextStructure());
  const std::string_view owned_string_range(text_structure->Contents());
  EXPECT_EQ(owned_string_range, text);
  const auto *tokens = &text_structure->TokenStream();
  EXPECT_NE(tokens, nullptr);
  const auto *tree = &text_structure->SyntaxTree();
  EXPECT_NE(tree, nullptr);
  // but syntax tree may be empty, depends on error-recovery

  // Re-parsing doesn't change anything
  EXPECT_FALSE(file.Parse().ok());
  EXPECT_FALSE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), text_structure);
  EXPECT_EQ(&text_structure->TokenStream(), tokens);
  EXPECT_EQ(&text_structure->SyntaxTree(), tree);
}

TEST(ParsedVerilogSourceFileTest, PreparsedValidFile) {
  constexpr std::string_view text("localparam int p = 1;\n");
  std::unique_ptr<VerilogAnalyzer> analyzed_structure =
      std::make_unique<VerilogAnalyzer>(text, "internal");
  absl::Status status = analyzed_structure->Analyze();
  EXPECT_TRUE(status.ok());

  ParsedVerilogSourceFile file("internal", "resolved", *analyzed_structure);
  // Parse automatically opens.
  EXPECT_TRUE(file.Parse().ok());
  EXPECT_TRUE(file.Status().ok());
  const TextStructureView *text_structure =
      ABSL_DIE_IF_NULL(file.GetTextStructure());
  EXPECT_EQ(&analyzed_structure->Data(), text_structure);
  const std::string_view owned_string_range(text_structure->Contents());
  EXPECT_EQ(owned_string_range, text);
  const auto *tokens = &text_structure->TokenStream();
  EXPECT_NE(tokens, nullptr);
  const auto *tree = &text_structure->SyntaxTree();
  EXPECT_NE(tree, nullptr);

  // Re-parsing doesn't change anything
  EXPECT_TRUE(file.Parse().ok());
  EXPECT_TRUE(file.Status().ok());
  EXPECT_EQ(file.GetTextStructure(), text_structure);
  EXPECT_EQ(&text_structure->TokenStream(), tokens);
  EXPECT_EQ(&text_structure->SyntaxTree(), tree);
}

TEST(ParsedVerilogSourceFileTest, PreparsedInvalidValidFile) {
  constexpr std::string_view text("localp_TYPO_aram int p = 1;\n");
  std::unique_ptr<VerilogAnalyzer> analyzed_structure =
      std::make_unique<VerilogAnalyzer>(text, "internal");
  absl::Status status = analyzed_structure->Analyze();
  EXPECT_FALSE(status.ok());

  ParsedVerilogSourceFile file("internal", "resolved", *analyzed_structure);
  EXPECT_TRUE(file.Open().ok());    // Already successfully implicitly opened.
  EXPECT_FALSE(file.Parse().ok());  // We expect the same parse failure.
  EXPECT_FALSE(file.Status().ok());
  const TextStructureView *text_structure =
      ABSL_DIE_IF_NULL(file.GetTextStructure());
  EXPECT_EQ(&analyzed_structure->Data(), text_structure);
  const std::string_view owned_string_range(text_structure->Contents());
  EXPECT_EQ(owned_string_range, text);
}

TEST(VerilogProjectTest, NonexistentTranslationUnit) {
  const auto tempdir = ::testing::TempDir();
  VerilogProject project(tempdir, {tempdir});
  const auto status_or_file = project.OpenTranslationUnit("never-there.v");
  EXPECT_FALSE(status_or_file.ok());
}

TEST(VerilogProjectTest, NonexistentIncludeFile) {
  const auto tempdir = ::testing::TempDir();
  VerilogProject project(tempdir, {tempdir});
  const auto status_or_file = project.OpenIncludedFile("nope.svh");
  EXPECT_FALSE(status_or_file.ok());
}

TEST(VerilogProjectTest, NonexistentFileLookup) {
  const auto tempdir = ::testing::TempDir();
  VerilogProject project(tempdir, {tempdir});
  {  // non-const-lookup overload
    VerilogSourceFile *file = project.LookupRegisteredFile("never-there.v");
    EXPECT_EQ(file, nullptr);
  }
  {
    // const-lookup overload
    const VerilogProject &cproject(project);
    const VerilogSourceFile *file =
        cproject.LookupRegisteredFile("never-there.v");
    EXPECT_EQ(file, nullptr);
  }
}

TEST(VerilogProjectTest, UpdateFileContents) {
  // The update file content is used typically by the language server.
  // By default, we load a file from the filesystem unless we get it from the
  // editor, which then overrides it. If removed, we should fall back to file
  // contents.
  const auto tempdir = ::testing::TempDir();
  const std::string project_root_dir = JoinPath(tempdir, "srcs");
  EXPECT_TRUE(CreateDir(project_root_dir).ok());

  // root is director with sources.
  VerilogProject project(project_root_dir, {});

  // Prepare file to be auto-loaded later
  constexpr std::string_view file_content("module foo();\nendmodule\n");
  const ScopedTestFile tf(project_root_dir, file_content);
  const std::string_view reference_name = Basename(tf.filename());

  VerilogSourceFile *from_file;
  std::string_view search_substring;

  // Push a local analyzed name under the name of the file.
  constexpr std::string_view external_content("localparam int p = 1;\n");
  std::unique_ptr<VerilogAnalyzer> analyzed_structure =
      std::make_unique<VerilogAnalyzer>(external_content, "internal");
  project.UpdateFileContents(tf.filename(), analyzed_structure.get());

  // Look up the file and see that content is the external content
  from_file = *project.OpenTranslationUnit(reference_name);
  EXPECT_EQ(from_file->GetContent(), external_content);

  // ... and we find our file given the substring.
  search_substring = from_file->GetContent().substr(5);
  EXPECT_EQ(project.LookupFileOrigin(search_substring), from_file);

  // Updating with empty content, i.e. removing the memory virtual file
  // force reading from file.
  project.UpdateFileContents(tf.filename(), nullptr);

  // Should be file content.
  from_file = *project.OpenTranslationUnit(reference_name);
  EXPECT_EQ(from_file->GetContent(), file_content);

  // Looking up by the old substring should not find anything anymore.
  EXPECT_EQ(project.LookupFileOrigin(search_substring), nullptr);

  // But we find our file from our substring.
  search_substring = from_file->GetContent().substr(5);
  EXPECT_EQ(project.LookupFileOrigin(search_substring), from_file);
}

TEST(VerilogProjectTest, UpdateFileContentsEmptyFile) {
  // Users can add empty files to the filelist and subsequently
  // remove them.
  const auto tempdir = ::testing::TempDir();
  const std::string project_root_dir = JoinPath(tempdir, "srcs");
  EXPECT_TRUE(CreateDir(project_root_dir).ok());

  // root is directory with sources.
  VerilogProject project(project_root_dir, {});

  // Prepare file to be auto-loaded later
  const ScopedTestFile tf(project_root_dir, "");
  const std::string_view reference_name = Basename(tf.filename());

  // Push a local analyzed name under the name of the file.
  constexpr std::string_view external_content("localparam int p = 1;\n");
  std::unique_ptr<VerilogAnalyzer> analyzed_structure =
      std::make_unique<VerilogAnalyzer>(external_content, "internal");
  project.UpdateFileContents(tf.filename(), analyzed_structure.get());

  // Look up the file and see that content is the external content
  VerilogSourceFile *from_file;
  std::string_view search_substring;
  from_file = *project.OpenTranslationUnit(reference_name);
  EXPECT_EQ(from_file->GetContent(), external_content);

  // ... and we find our file given the substring.
  search_substring = from_file->GetContent().substr(5);
  EXPECT_EQ(project.LookupFileOrigin(search_substring), from_file);

  // Prepare an empty file
  constexpr std::string_view empty_file_content;
  const ScopedTestFile empty_file(project_root_dir, empty_file_content);
  const std::string_view empty_file_reference = Basename(empty_file.filename());

  // Push the empty file into the project
  std::unique_ptr<VerilogAnalyzer> analyzed_empty_structure =
      std::make_unique<VerilogAnalyzer>(empty_file_content, "internal");
  project.UpdateFileContents(empty_file.filename(),
                             analyzed_empty_structure.get());

  // Check the content from the two files are present
  from_file = *project.OpenTranslationUnit(reference_name);
  EXPECT_EQ(from_file->GetContent(), external_content);

  from_file = *project.OpenTranslationUnit(empty_file_reference);
  EXPECT_EQ(from_file->GetContent(), empty_file_content);

  // Remove the empty file
  project.UpdateFileContents(empty_file.filename(), nullptr);

  // Make sure the remaining file is still present
  from_file = *project.OpenTranslationUnit(reference_name);
  EXPECT_EQ(from_file->GetContent(), external_content);

  // Make sure the empty file was removed
  EXPECT_EQ(project.LookupFileOrigin(empty_file.filename()), nullptr);
}

TEST(VerilogProjectTest, LookupFileOriginTest) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, "srcs");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  VerilogProject project(sources_dir, {});
  // no files yet

  {
    constexpr std::string_view foreign_text("not from any file");
    EXPECT_EQ(project.LookupFileOrigin(foreign_text), nullptr);
  }

  // Add one file.  Don't even need to parse it.
  const ScopedTestFile tf(sources_dir, "module m;\nendmodule\n");
  const auto status_or_file =
      project.OpenTranslationUnit(Basename(tf.filename()));
  VerilogSourceFile *verilog_source_file = *status_or_file;
  const std::string_view content1(verilog_source_file->GetContent());

  {
    constexpr std::string_view foreign_text("still not from any file");
    EXPECT_EQ(project.LookupFileOrigin(foreign_text), nullptr);
  }

  // Pick a substring known to come from that file.
  EXPECT_EQ(project.LookupFileOrigin(content1.substr(2, 4)),
            verilog_source_file);

  // Add one more file.
  const ScopedTestFile tf2(sources_dir, "class c;\nendclass\n");
  const auto status_or_file2 =
      project.OpenTranslationUnit(Basename(tf2.filename()));
  VerilogSourceFile *verilog_source_file2 = *status_or_file2;
  const std::string_view content2(verilog_source_file2->GetContent());

  // Pick substrings known to come from those files.
  EXPECT_EQ(project.LookupFileOrigin(content1.substr(5, 5)),
            verilog_source_file);
  EXPECT_EQ(project.LookupFileOrigin(content2.substr(9, 4)),
            verilog_source_file2);
}

TEST(VerilogProjectTest, LookupFileOriginTestMoreFiles) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir =
      JoinPath(tempdir, "LookupFileOriginTestMoreFiles");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  VerilogProject project(sources_dir, {});
  // no files yet

  constexpr std::string_view foreign_text("not from any file");

  // WArning: Test size is quadratic in N, but linear in memory in N.
  constexpr int N = 50;
  std::vector<std::unique_ptr<ScopedTestFile>> test_files;
  std::vector<const VerilogSourceFile *> sources;
  for (int i = 0; i < N; ++i) {
    // Write files, need not be parse-able.
    test_files.emplace_back(std::make_unique<ScopedTestFile>(
        sources_dir, "sa89*(98<Na! 89 89891231!@#ajk jasoij(*&^ asaissd0afd "));
    const auto status_or_file =
        project.OpenTranslationUnit(Basename(test_files.back()->filename()));
    const VerilogSourceFile *source_file = *status_or_file;
    ASSERT_NE(source_file, nullptr);
    sources.push_back(source_file);

    for (const auto &source : sources) {
      // Pick substrings known to come from those files.
      EXPECT_EQ(project.LookupFileOrigin(source->GetContent().substr(15, 12)),
                source);
    }
    EXPECT_EQ(project.LookupFileOrigin(foreign_text), nullptr);
  }
}

TEST(VerilogProjectTest, ValidTranslationUnit) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, "srcs");
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  VerilogProject project(sources_dir, {includes_dir});

  constexpr std::string_view text("module m;\nendmodule\n");
  const ScopedTestFile tf(sources_dir, text);
  const auto status_or_file =
      project.OpenTranslationUnit(Basename(tf.filename()));
  VerilogSourceFile *verilog_source_file = *status_or_file;
  EXPECT_TRUE(verilog_source_file->Status().ok());
  EXPECT_EQ(verilog_source_file->ReferencedPath(), Basename(tf.filename()));
  EXPECT_EQ(verilog_source_file->ResolvedPath(), tf.filename());
  EXPECT_EQ(project.LookupRegisteredFile(Basename(tf.filename())),
            verilog_source_file);
  const std::string_view content(verilog_source_file->GetContent());
  {  // const-lookup overload
    const VerilogProject &cproject(project);
    EXPECT_EQ(cproject.LookupRegisteredFile(Basename(tf.filename())),
              verilog_source_file);
    EXPECT_EQ(cproject.LookupFileOrigin(content.substr(2, 4)),
              verilog_source_file);
  }

  EXPECT_TRUE(verilog_source_file->Parse().ok());
  const TextStructureView &text_structure(
      *verilog_source_file->GetTextStructure());
  const auto *tree = ABSL_DIE_IF_NULL(text_structure.SyntaxTree().get());
  EXPECT_EQ(FindAllModuleDeclarations(*tree).size(), 1);

  {
    // Re-parsing the file changes nothing.
    EXPECT_TRUE(verilog_source_file->Parse().ok());
    const auto *tree2 = ABSL_DIE_IF_NULL(text_structure.SyntaxTree().get());
    EXPECT_EQ(tree2, tree);
    EXPECT_EQ(FindAllModuleDeclarations(*tree).size(), 1);
  }
  {  // Re-opening the file changes nothing.
    const auto status_or_file2 =
        project.OpenTranslationUnit(Basename(tf.filename()));
    VerilogSourceFile *verilog_source_file2 = *status_or_file2;
    EXPECT_EQ(verilog_source_file2, verilog_source_file);
    EXPECT_TRUE(verilog_source_file2->Status().ok());
  }

  // Testing begin/end iteration.
  for (auto &file : project) {
    EXPECT_TRUE(file.second->Parse().ok());
  }
  for (const auto &file : project) {
    EXPECT_TRUE(file.second->Status().ok());
  }
}

TEST(VerilogProjectTest, ValidIncludeFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, "srcs");
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  VerilogProject project(sources_dir, {includes_dir});

  constexpr std::string_view text("`define FOO 1\n");
  const ScopedTestFile tf(includes_dir, text);
  const std::string_view basename(Basename(tf.filename()));
  const auto status_or_file = project.OpenIncludedFile(basename);
  VerilogSourceFile *verilog_source_file = *status_or_file;
  EXPECT_TRUE(verilog_source_file->Status().ok());
  EXPECT_EQ(verilog_source_file->ReferencedPath(), basename);
  EXPECT_EQ(verilog_source_file->ResolvedPath(), tf.filename());
  EXPECT_EQ(project.LookupRegisteredFile(Basename(tf.filename())),
            verilog_source_file);
  {  // const-lookup overload
    const VerilogProject &cproject(project);
    EXPECT_EQ(cproject.LookupRegisteredFile(Basename(tf.filename())),
              verilog_source_file);
  }

  // Re-opening same file, changes nothing
  {
    const auto status_or_file2 = project.OpenIncludedFile(basename);
    VerilogSourceFile *verilog_source_file2 = *status_or_file2;
    EXPECT_EQ(verilog_source_file2, verilog_source_file);
    EXPECT_TRUE(verilog_source_file2->Status().ok());
  }

  // includes aren't required to be parse-able, so just open
  EXPECT_TRUE(verilog_source_file->Open().ok());
  EXPECT_FALSE(verilog_source_file->GetContent().empty());

  // re-opening the file changes nothing
  EXPECT_TRUE(verilog_source_file->Open().ok());
  EXPECT_FALSE(verilog_source_file->GetContent().empty());
}

TEST(VerilogProjectTest, OpenVirtualIncludeFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, "srcs");
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  VerilogProject project(sources_dir, {includes_dir});

  constexpr std::string_view text("`define FOO 1\n");
  const std::string basename = "virtual_include_file1";
  const std::string full_path = JoinPath(includes_dir, basename);
  // The virtual file is added by its full path. But the include is opened by
  // the basename.
  project.AddVirtualFile(full_path, text);

  const auto status_or_file = project.OpenIncludedFile(basename);
  VerilogSourceFile *verilog_source_file = *status_or_file;
  EXPECT_TRUE(verilog_source_file->Status().ok());
  EXPECT_EQ(verilog_source_file->ReferencedPath(), full_path);
  EXPECT_EQ(verilog_source_file->ResolvedPath(), full_path);
  EXPECT_EQ(project.LookupRegisteredFile(basename), verilog_source_file);
  {  // const-lookup overload
    const VerilogProject &cproject(project);
    EXPECT_EQ(cproject.LookupRegisteredFile(basename), verilog_source_file);
  }

  // Re-opening same file, changes nothing
  {
    const auto status_or_file2 = project.OpenIncludedFile(basename);
    VerilogSourceFile *verilog_source_file2 = *status_or_file2;
    EXPECT_EQ(verilog_source_file2, verilog_source_file);
    EXPECT_TRUE(verilog_source_file2->Status().ok());
  }

  // includes aren't required to be parse-able, so just open
  EXPECT_TRUE(verilog_source_file->Open().ok());
  EXPECT_FALSE(verilog_source_file->GetContent().empty());

  // re-opening the file changes nothing
  EXPECT_TRUE(verilog_source_file->Open().ok());
  EXPECT_FALSE(verilog_source_file->GetContent().empty());
}

TEST(VerilogProjectTest, TranslationUnitNotFound) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, "srcs");
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  VerilogProject project(sources_dir, {includes_dir});

  constexpr std::string_view text("module m;\nendmodule\n");
  // deliberately plant this file in the includes dir != sources dir
  const ScopedTestFile tf(includes_dir, text);
  {
    const auto status_or_file =
        project.OpenTranslationUnit(Basename(tf.filename()));
    EXPECT_FALSE(status_or_file.ok());
  }
  {  // try again, still fail
    const auto status_or_file =
        project.OpenTranslationUnit(Basename(tf.filename()));
    EXPECT_FALSE(status_or_file.ok());
  }
}

TEST(VerilogProjectTest, IncludeFileNotFound) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, "srcs");
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  VerilogProject project(sources_dir, {includes_dir});

  constexpr std::string_view text("module m;\nendmodule\n");
  // deliberately plant this file in the sources dir != include dir
  const ScopedTestFile tf(sources_dir, text);
  {
    const auto status_or_file =
        project.OpenIncludedFile(Basename(tf.filename()));
    EXPECT_FALSE(status_or_file.ok());
  }
  {  // try again, still fail
    const auto status_or_file =
        project.OpenIncludedFile(Basename(tf.filename()));
    EXPECT_FALSE(status_or_file.ok());
  }
}

TEST(VerilogProjectTest, AddVirtualFile) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, "srcs");
  const std::string includes_dir = JoinPath(tempdir, "includes");
  EXPECT_TRUE(CreateDir(sources_dir).ok());
  EXPECT_TRUE(CreateDir(includes_dir).ok());
  VerilogProject project(sources_dir, {includes_dir});

  const std::string file_path = "/some/file";
  const std::string file_content = "virtual file content";
  project.AddVirtualFile(file_path, file_content);

  auto *stored_file = project.LookupRegisteredFile(file_path);
  ASSERT_NE(stored_file, nullptr);
  EXPECT_TRUE(stored_file->Open().ok());
  EXPECT_TRUE(stored_file->Status().ok());
  EXPECT_EQ(stored_file->GetContent(), file_content);
}

}  // namespace
}  // namespace verilog
