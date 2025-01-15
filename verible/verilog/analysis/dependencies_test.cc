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

#include "verible/verilog/analysis/dependencies.h"

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/symbol-table.h"
#include "verible/verilog/analysis/verilog-project.h"

namespace verilog {
namespace {

using testing::ElementsAre;
using verible::file::Basename;
using verible::file::CreateDir;
using verible::file::JoinPath;
using verible::file::testing::ScopedTestFile;

TEST(FileDependenciesTest, EmptyData) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());
  VerilogProject project(sources_dir, {/* no include paths */});

  // no files
  SymbolTable symbol_table(&project);

  {  // pre-build should be empty
    FileDependencies file_deps(symbol_table);
    EXPECT_TRUE(file_deps.Empty()) << file_deps;
  }

  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty());

  {  // post-build should be empty
    FileDependencies file_deps(symbol_table);
    EXPECT_TRUE(file_deps.Empty()) << file_deps;
  }
}

TEST(FileDependenciesTest, OneFileNoDeps) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  // None of these test cases will yield any inter-file deps.
  constexpr std::string_view kTestCases[] = {
      "",
      // one module
      "module mmm;\n"
      "endmodule\n",
      // two modules, with dependency
      "module mmm;\n"
      "endmodule\n"
      "module ppp;\n"
      "  mmm mmm_inst();\n"
      "endmodule\n",
      // parameters with dependency
      "localparam int foo = 0;\n"
      "localparam int bar = foo + 1;\n",
  };

  for (const auto &code : kTestCases) {
    VLOG(1) << "code: " << code;
    VerilogProject project(sources_dir, {/* no include paths */});

    ScopedTestFile tf(sources_dir, code);
    {
      const auto status_or_file =
          project.OpenTranslationUnit(Basename(tf.filename()));
      ASSERT_TRUE(status_or_file.ok()) << status_or_file.status().message();
    }

    SymbolTable symbol_table(&project);
    std::vector<absl::Status> build_diagnostics;
    symbol_table.Build(&build_diagnostics);
    EXPECT_TRUE(build_diagnostics.empty());

    // post-build should be empty
    FileDependencies file_deps(symbol_table);
    EXPECT_TRUE(file_deps.Empty()) << file_deps;
  }
}

TEST(FileDependenciesTest, TwoFilesNoDeps) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include paths */});

  ScopedTestFile tf1(sources_dir, "localparam int foo = 0;");
  const auto status_or_file1 =
      project.OpenTranslationUnit(Basename(tf1.filename()));

  ScopedTestFile tf2(sources_dir, "localparam int bar = 2;");
  const auto status_or_file2 =
      project.OpenTranslationUnit(Basename(tf2.filename()));

  SymbolTable symbol_table(&project);
  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty());

  // post-build should be empty
  FileDependencies file_deps(symbol_table);
  EXPECT_TRUE(file_deps.Empty()) << file_deps;
}

struct SymbolTableDebug {
  const SymbolTable &symbol_table;
};

static std::ostream &operator<<(std::ostream &stream,
                                const SymbolTableDebug &p) {
  stream << "Definitions:" << std::endl;
  p.symbol_table.PrintSymbolDefinitions(stream) << std::endl;
  stream << "References:" << std::endl;
  return p.symbol_table.PrintSymbolReferences(stream) << std::endl;
}

TEST(FileDependenciesTest, TwoFilesWithParamDepAtRootScope) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include paths */});

  ScopedTestFile tf1(sources_dir, "localparam int zzz = 0;\n");
  const auto status_or_file1 =
      project.OpenTranslationUnit(Basename(tf1.filename()));
  const VerilogSourceFile *file1 = *status_or_file1;

  ScopedTestFile tf2(sources_dir, "localparam int yyy = zzz * 2;\n");
  const auto status_or_file2 =
      project.OpenTranslationUnit(Basename(tf2.filename()));
  const VerilogSourceFile *file2 = *status_or_file2;

  SymbolTable symbol_table(&project);
  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty());

  FileDependencies file_deps(symbol_table);

  // "zzz" is defined in the same $root scope, but different files,
  // so we expect a dependency edge.
  EXPECT_FALSE(file_deps.Empty()) << file_deps;
  EXPECT_EQ(file_deps.file_deps.size(), 1) << SymbolTableDebug{symbol_table};
  const auto found_ref = file_deps.file_deps.find(file2);
  ASSERT_NE(found_ref, file_deps.file_deps.end())
      << file2->GetTextStructure()->Contents();
  const auto found_def = found_ref->second.find(file1);
  ASSERT_NE(found_def, found_ref->second.end())
      << file1->GetTextStructure()->Contents();
  EXPECT_THAT(found_def->second, ElementsAre("zzz"));
}

TEST(FileDependenciesTest, TwoFilesWithParamDep) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include paths */});

  ScopedTestFile tf1(sources_dir,
                     "localparam int foo = 0;\n"
                     "package p_pkg;\n"
                     "  localparam int goo = 1;\n"
                     "endpackage\n");
  const auto status_or_file1 =
      project.OpenTranslationUnit(Basename(tf1.filename()));
  const VerilogSourceFile *file1 = *status_or_file1;

  ScopedTestFile tf2(sources_dir,
                     "localparam int bar = foo - 2;\n"
                     "localparam int baz = p_pkg::goo;\n");
  const auto status_or_file2 =
      project.OpenTranslationUnit(Basename(tf2.filename()));
  const VerilogSourceFile *file2 = *status_or_file2;

  SymbolTable symbol_table(&project);
  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty());

  FileDependencies file_deps(symbol_table);

  EXPECT_FALSE(file_deps.Empty()) << file_deps;
  EXPECT_EQ(file_deps.file_deps.size(), 1) << SymbolTableDebug{symbol_table};
  const auto found_ref = file_deps.file_deps.find(file2);
  ASSERT_NE(found_ref, file_deps.file_deps.end())
      << file2->GetTextStructure()->Contents();
  const auto found_def = found_ref->second.find(file1);
  ASSERT_NE(found_def, found_ref->second.end())
      << file1->GetTextStructure()->Contents();
  EXPECT_THAT(found_def->second, ElementsAre("foo", "p_pkg"));
}

TEST(FileDependenciesTest, TwoFilesWithCyclicDep) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include paths */});

  ScopedTestFile tf1(sources_dir,
                     "localparam int foo = 0;\n"
                     "package p_pkg;\n"
                     "  localparam int goo = bar;\n"
                     "endpackage\n");
  const auto status_or_file1 =
      project.OpenTranslationUnit(Basename(tf1.filename()));
  const VerilogSourceFile *file1 = *status_or_file1;

  ScopedTestFile tf2(sources_dir,
                     "localparam int bar = foo - 2;\n"
                     "localparam int baz = p_pkg::goo;\n");
  const auto status_or_file2 =
      project.OpenTranslationUnit(Basename(tf2.filename()));
  const VerilogSourceFile *file2 = *status_or_file2;

  SymbolTable symbol_table(&project);
  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty());

  FileDependencies file_deps(symbol_table);

  EXPECT_FALSE(file_deps.Empty()) << file_deps;
  {  // dependencies in one direction
    const auto found_ref = file_deps.file_deps.find(file2);
    ASSERT_NE(found_ref, file_deps.file_deps.end())
        << file2->GetTextStructure()->Contents();
    const auto found_def = found_ref->second.find(file1);
    ASSERT_NE(found_def, found_ref->second.end())
        << file1->GetTextStructure()->Contents();
    EXPECT_THAT(found_def->second, ElementsAre("foo", "p_pkg"));
  }
  {  // dependencies in reverse direction
    const auto found_ref = file_deps.file_deps.find(file1);
    ASSERT_NE(found_ref, file_deps.file_deps.end())
        << file1->GetTextStructure()->Contents();
    const auto found_def = found_ref->second.find(file2);
    ASSERT_NE(found_def, found_ref->second.end())
        << file2->GetTextStructure()->Contents();
    EXPECT_THAT(found_def->second, ElementsAre("bar"));
  }
}

TEST(FileDependenciesTest, ModuleDiamondDependencies) {
  const auto tempdir = ::testing::TempDir();
  const std::string sources_dir = JoinPath(tempdir, __FUNCTION__);
  ASSERT_TRUE(CreateDir(sources_dir).ok());

  VerilogProject project(sources_dir, {/* no include paths */});

  // 4 files: with a diamond dependency relationship
  ScopedTestFile mmm(sources_dir,
                     "module mmm;\n"
                     "  ppp ppp_i();\n"
                     "  qqq qqq_i();\n"
                     "endmodule\n");
  const auto mmm_status_or_file =
      project.OpenTranslationUnit(Basename(mmm.filename()));
  const VerilogSourceFile *mmm_file = *mmm_status_or_file;

  ScopedTestFile ppp(sources_dir,
                     "module ppp;\n"
                     "  rrr rrr_i();\n"
                     "endmodule\n");
  const auto ppp_status_or_file =
      project.OpenTranslationUnit(Basename(ppp.filename()));
  const VerilogSourceFile *ppp_file = *ppp_status_or_file;

  ScopedTestFile qqq(sources_dir,
                     "module qqq;\n"
                     "  rrr rrr_i();\n"
                     "endmodule\n");
  const auto qqq_status_or_file =
      project.OpenTranslationUnit(Basename(qqq.filename()));
  const VerilogSourceFile *qqq_file = *qqq_status_or_file;

  ScopedTestFile rrr(sources_dir,
                     "module rrr;\n"
                     "  wire w;\n"
                     "endmodule\n");
  const auto rrr_status_or_file =
      project.OpenTranslationUnit(Basename(rrr.filename()));
  const VerilogSourceFile *rrr_file = *rrr_status_or_file;

  SymbolTable symbol_table(&project);
  std::vector<absl::Status> build_diagnostics;
  symbol_table.Build(&build_diagnostics);
  EXPECT_TRUE(build_diagnostics.empty());

  FileDependencies file_deps(symbol_table);

  EXPECT_FALSE(file_deps.Empty()) << file_deps;
  // Verify 4 edges in graph.
  {
    const auto found_ref = file_deps.file_deps.find(mmm_file);
    ASSERT_NE(found_ref, file_deps.file_deps.end())
        << mmm_file->GetTextStructure()->Contents();
    {
      // mmm -> ppp
      const auto found_def = found_ref->second.find(ppp_file);
      ASSERT_NE(found_def, found_ref->second.end())
          << ppp_file->GetTextStructure()->Contents();
      EXPECT_THAT(found_def->second, ElementsAre("ppp"));
    }
    {
      // mmm -> qqq
      const auto found_def = found_ref->second.find(qqq_file);
      ASSERT_NE(found_def, found_ref->second.end())
          << qqq_file->GetTextStructure()->Contents();
      EXPECT_THAT(found_def->second, ElementsAre("qqq"));
    }
  }
  {  // ppp -> rrr
    const auto found_ref = file_deps.file_deps.find(ppp_file);
    ASSERT_NE(found_ref, file_deps.file_deps.end())
        << ppp_file->GetTextStructure()->Contents();

    const auto found_def = found_ref->second.find(rrr_file);
    ASSERT_NE(found_def, found_ref->second.end())
        << rrr_file->GetTextStructure()->Contents();
    EXPECT_THAT(found_def->second, ElementsAre("rrr"));
  }
  {
    // qqq -> rrr
    const auto found_ref = file_deps.file_deps.find(qqq_file);
    ASSERT_NE(found_ref, file_deps.file_deps.end())
        << qqq_file->GetTextStructure()->Contents();

    const auto found_def = found_ref->second.find(rrr_file);
    ASSERT_NE(found_def, found_ref->second.end())
        << rrr_file->GetTextStructure()->Contents();
    EXPECT_THAT(found_def->second, ElementsAre("rrr"));
  }
}

}  // namespace
}  // namespace verilog
