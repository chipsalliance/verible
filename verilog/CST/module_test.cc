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

// Unit tests for module-related concrete-syntax-tree functions.
//
// Testing strategy:
// The point of these tests is to validate the structure that is assumed
// about module declaration nodes and the structure that is actually
// created by the parser, so test *should* use the parser-generated
// syntax trees, as opposed to hand-crafted/mocked syntax trees.

#include "verilog/CST/module.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_info_test_util.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;
using verible::SyntaxTreeNode;
using verible::TokenInfoTestData;

TEST(FindAllModuleDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(module_declarations.empty());
}

TEST(FindAllModuleDeclarationsTest, NonModule) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(module_declarations.empty());
}

TEST(FindAllModuleDeclarationsTest, OneModule) {
  VerilogAnalyzer analyzer("module mod; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_declarations.size(), 1);
}

TEST(FindAllModuleDeclarationsTest, MultiModules) {
  VerilogAnalyzer analyzer(R"(
module mod1;
endmodule
package p;
endpackage
module mod2(input foo);
endmodule
class c;
endclass
)",
                           "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations =
      FindAllModuleDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(module_declarations.size(), 2);
}

TEST(GetModuleNameTokenTest, RootIsNotAModule) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  // CHECK should fail: root node is a description list, not a module.
  // If this happens, it is a programmer error, not user error.
  EXPECT_DEATH(GetModuleNameToken(*ABSL_DIE_IF_NULL(root)),
               "kDescriptionList vs. kModuleDeclaration");
}

TEST(GetModuleNameTokenTest, ValidModule) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  EXPECT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto module_declarations = FindAllModuleDeclarations(*root);
  EXPECT_EQ(module_declarations.size(), 1);
  const auto& module_node =
      down_cast<const SyntaxTreeNode&>(*module_declarations.front().match);
  // Root node is a description list, not a module.
  const auto& token = GetModuleNameToken(module_node);
  EXPECT_EQ(token.text(), "foo");
}

TEST(GetModulePortDeclarationListTest, NoPorts) {
  const verible::TokenInfoTestData kTestCases[] = {
      // No () ports lists.
      {"module m;\nendmodule\n"},
      {"module m\t;  \n  endmodule\n"},
      {"module m;\nfunction f;\nendfunction\nendmodule\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto modules = FindAllModuleDeclarations(*root);
    ASSERT_EQ(modules.size(), 1);
    const auto& module = *modules.front().match;
    const auto* return_type = GetModulePortParenGroup(module);
    EXPECT_EQ(return_type, nullptr);
  }
}

TEST(GetModulePortDeclarationListTest, WithPorts) {
  constexpr int kTag = 1;  // value not important
  const verible::TokenInfoTestData kTestCases[] = {
      {"module m", {kTag, "()"}, ";\nendmodule\n"},
      {"module m    ", {kTag, "()"}, "   ;\nendmodule\n"},
      {"module m", {kTag, "(input clk)"}, ";\nendmodule\n"},
      {"module m", {kTag, "(  input   clk  )"}, ";\nendmodule\n"},
      {"module m", {kTag, "(\ninput   clk\n)"}, ";\nendmodule\n"},
      {"module m", {kTag, "(\ninput   clk,\noutput foo\n)"}, ";\nendmodule\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const absl::string_view code_copy(analyzer.Data().Contents());
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto modules = FindAllModuleDeclarations(*root);
    ASSERT_EQ(modules.size(), 1);
    const auto& module = *modules.front().match;
    const auto* ports = GetModulePortParenGroup(module);
    ASSERT_NE(ports, nullptr);
    const auto ports_span = verible::StringSpanOfSymbol(*ports);
    EXPECT_FALSE(ports_span.empty());

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts = test.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text();
    ASSERT_TRUE(verible::IsSubRange(ports_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(ports_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(ports_span, expected_span));
  }
}

}  // namespace
}  // namespace verilog
