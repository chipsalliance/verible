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

#include "verilog/analysis/symbol_table.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/util/logging.h"
#include "verilog/analysis/verilog_project.h"

namespace verilog {
namespace {

using testing::HasSubstr;

// An in-memory source file that doesn't require file-system access,
// nor create temporary files.
// TODO: if this is useful elsewhere, move to a test-only library.
class TestVerilogSourceFile : public VerilogSourceFile {
 public:
  // filename can be fake, it is not used to open any file.
  TestVerilogSourceFile(absl::string_view filename, absl::string_view contents)
      : VerilogSourceFile(filename, filename), contents_for_open_(contents) {}

  // Load text into analyzer structure without actually opening a file.
  absl::Status Open() override {
    analyzed_structure_ = ABSL_DIE_IF_NULL(
        absl::make_unique<VerilogAnalyzer>(contents_for_open_, ResolvedPath()));
    state_ = State::kOpened;
    status_ = absl::OkStatus();
    return status_;
  }

 private:
  const absl::string_view contents_for_open_;
};

TEST(BuildSymbolTableTest, InvalidSyntax) {
  TestVerilogSourceFile src("foobar.sv", "module;\nendmodule\n");
  const auto status = src.Parse();
  EXPECT_FALSE(status.ok());
  SymbolTable symbol_table(nullptr);

  const auto diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_TRUE(symbol_table.Root().Children().empty());
  EXPECT_TRUE(diagnostics.empty()) << "Unexpected diagnostic:\n"
                                   << diagnostics.front().message();
}

TEST(BuildSymbolTableTest, ModuleDeclarationSingle) {
  TestVerilogSourceFile src("foobar.sv", "module m;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto diagnostics = BuildSymbolTable(src, &symbol_table);

  const auto found = root_symbol.Find("m");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "m");
  const SymbolInfo& module_symbol(found->second.Value());
  EXPECT_EQ(module_symbol.type, SymbolType::kModule);
  EXPECT_EQ(module_symbol.file_origin, &src);
  EXPECT_EQ(module_symbol.declared_type,
            nullptr);  // there is no module meta-type
  EXPECT_TRUE(diagnostics.empty()) << "Unexpected diagnostic:\n"
                                   << diagnostics.front().message();
}

TEST(BuildSymbolTableTest, ModuleDeclarationMultiple) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m1;\nendmodule\n"
                            "module m2;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto diagnostics = BuildSymbolTable(src, &symbol_table);

  const absl::string_view expected_modules[] = {"m1", "m2"};
  for (const auto& expected_module : expected_modules) {
    const auto found = root_symbol.Find(expected_module);
    ASSERT_NE(found, root_symbol.end());
    EXPECT_EQ(found->first, expected_module);
    const SymbolInfo& module_symbol(found->second.Value());
    EXPECT_EQ(module_symbol.type, SymbolType::kModule);
    EXPECT_EQ(module_symbol.file_origin, &src);
    EXPECT_EQ(module_symbol.declared_type,
              nullptr);  // there is no module meta-type
    EXPECT_TRUE(diagnostics.empty()) << "Unexpected diagnostic:\n"
                                     << diagnostics.front().message();
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationDuplicate) {
  TestVerilogSourceFile src("foobar.sv",
                            "module mm;\nendmodule\n"
                            "module mm;\nendmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto diagnostics = BuildSymbolTable(src, &symbol_table);

  const absl::string_view expected_module("mm");
  const auto found = root_symbol.Find(expected_module);
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, expected_module);
  const SymbolInfo& module_symbol(found->second.Value());
  EXPECT_EQ(module_symbol.type, SymbolType::kModule);
  EXPECT_EQ(module_symbol.file_origin, &src);
  EXPECT_EQ(module_symbol.declared_type,
            nullptr);  // there is no module meta-type
  ASSERT_EQ(diagnostics.size(), 1);
  EXPECT_EQ(diagnostics.front().code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(diagnostics.front().message(),
              HasSubstr("\"mm\" is already defined in the $root scope"));
}

TEST(BuildSymbolTableTest, ModuleDeclarationNested) {
  TestVerilogSourceFile src("foobar.sv",
                            "module m_outer;\n"
                            "module m_inner;\n"
                            "endmodule\n"
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto diagnostics = BuildSymbolTable(src, &symbol_table);

  EXPECT_TRUE(diagnostics.empty()) << "Unexpected diagnostic:\n"
                                   << diagnostics.front().message();
  const auto found = root_symbol.Find("m_outer");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "m_outer");
  const SymbolTableNode& outer_module_node(found->second);
  {
    const SymbolInfo& module_symbol(outer_module_node.Value());
    EXPECT_EQ(module_symbol.type, SymbolType::kModule);
    EXPECT_EQ(module_symbol.file_origin, &src);
    EXPECT_EQ(module_symbol.declared_type,
              nullptr);  // there is no module meta-type
  }

  const auto found_inner = outer_module_node.Find("m_inner");
  {
    ASSERT_NE(found_inner, outer_module_node.end());
    EXPECT_EQ(found_inner->first, "m_inner");
    const SymbolTableNode& inner_module_node(found_inner->second);
    const SymbolInfo& module_symbol(inner_module_node.Value());
    EXPECT_EQ(module_symbol.type, SymbolType::kModule);
    EXPECT_EQ(module_symbol.file_origin, &src);
    EXPECT_EQ(module_symbol.declared_type,
              nullptr);  // there is no module meta-type
  }
}

TEST(BuildSymbolTableTest, ModuleDeclarationNestedDuplicate) {
  TestVerilogSourceFile src("foobar.sv",
                            "module outer;\n"
                            "module mm;\nendmodule\n"
                            "module mm;\nendmodule\n"  // dupe
                            "endmodule\n");
  const auto status = src.Parse();
  ASSERT_TRUE(status.ok()) << status.message();
  SymbolTable symbol_table(nullptr);
  const SymbolTableNode& root_symbol(symbol_table.Root());

  const auto diagnostics = BuildSymbolTable(src, &symbol_table);

  const auto found = root_symbol.Find("outer");
  ASSERT_NE(found, root_symbol.end());
  EXPECT_EQ(found->first, "outer");
  ASSERT_EQ(diagnostics.size(), 1);
  EXPECT_EQ(diagnostics.front().code(), absl::StatusCode::kAlreadyExists);
  EXPECT_THAT(diagnostics.front().message(),
              HasSubstr("\"mm\" is already defined in the $root::outer scope"));
}

}  // namespace
}  // namespace verilog
