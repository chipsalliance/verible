// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/CST/functions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/identifier.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::down_cast;

TEST(FindAllFunctionDeclarationsTest, EmptySource) {
  VerilogAnalyzer analyzer("", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations =
      FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(function_declarations.empty());
}

TEST(FindAllFunctionDeclarationsTest, OnlyClass) {
  VerilogAnalyzer analyzer("class foo; endclass", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations =
      FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(function_declarations.empty());
}

TEST(FindAllFunctionDeclarationsTest, OnlyModule) {
  VerilogAnalyzer analyzer("module foo; endmodule", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations =
      FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_TRUE(function_declarations.empty());
}

TEST(FindAllFunctionDeclarationsTest, OneFunction) {
  VerilogAnalyzer analyzer("function foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations =
      FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
  ASSERT_EQ(function_declarations.size(), 1);
}

TEST(FindAllFunctionDeclarationsTest, TwoFunctions) {
  VerilogAnalyzer analyzer(R"(
function foo(); endfunction
function foo2(); endfunction
)",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations =
      FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(function_declarations.size(), 2);
}

TEST(FindAllFunctionDeclarationsTest, FunctionInsideClass) {
  VerilogAnalyzer analyzer("class bar; function foo(); endfunction endclass",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations =
      FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(function_declarations.size(), 1);
}

TEST(FindAllFunctionDeclarationsTest, FunctionInsideModule) {
  VerilogAnalyzer analyzer("module bar; function foo(); endfunction endmodule",
                           "");
  ASSERT_OK(analyzer.Analyze());
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations =
      FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));
  EXPECT_EQ(function_declarations.size(), 1);
}

// TODO(kathuriac): Add test case for function inside cross_body_item see
// (verilog.y)

TEST(GetFunctionHeaderTest, Header) {
  const char* kTestCases[] = {
      "function foo(); endfunction",
      "class c; function foo(); endfunction endclass",
      "module m; function foo(); endfunction endmodule",
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test, "");
    ASSERT_OK(analyzer.Analyze());
    // Root node is a description list, not a function.
    const auto& root = analyzer.Data().SyntaxTree();
    const auto function_declarations = FindAllFunctionDeclarations(*root);
    ASSERT_EQ(function_declarations.size(), 1);
    const auto& function_node = down_cast<const verible::SyntaxTreeNode&>(
        *function_declarations.front().match);
    GetFunctionHeader(function_node);
    // Reaching here is success, function includes internal checks already.
  }
}

TEST(GetFunctionLifetimeTest, NoLifetimeDeclared) {
  VerilogAnalyzer analyzer("function foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto& function_node = down_cast<const verible::SyntaxTreeNode&>(
      *function_declarations.front().match);
  const auto* lifetime = GetFunctionLifetime(function_node);
  EXPECT_EQ(lifetime, nullptr);
}

TEST(GetFunctionLifetimeTest, StaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("function static foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto& function_node = down_cast<const verible::SyntaxTreeNode&>(
      *function_declarations.front().match);
  const auto* lifetime = GetFunctionLifetime(function_node);
  const auto* leaf = down_cast<const verible::SyntaxTreeLeaf*>(lifetime);
  EXPECT_EQ(leaf->get().token_enum, TK_static);
}

TEST(GetFunctionLifetimeTest, AutomaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("function automatic foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.e
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto& function_node = down_cast<const verible::SyntaxTreeNode&>(
      *function_declarations.front().match);
  const auto* lifetime = GetFunctionLifetime(function_node);
  const auto* leaf = down_cast<const verible::SyntaxTreeLeaf*>(lifetime);
  EXPECT_EQ(leaf->get().token_enum, TK_automatic);
}

TEST(GetFunctionIdTest, UnqualifiedIds) {
  const std::pair<std::string, std::vector<absl::string_view>> kTestCases[] = {
      {"function foo(); endfunction", {"foo"}},
      {"function automatic bar(); endfunction", {"bar"}},
      {"function static baz(); endfunction", {"baz"}},
      {"package p; function foo(); endfunction endpackage", {"foo"}},
      {"class c; function zoo(); endfunction endclass", {"zoo"}},
      {"function myclass::foo(); endfunction", {"myclass", "foo"}},
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    // Root node is a description list, not a function.
    const auto& root = analyzer.Data().SyntaxTree();
    const auto function_declarations = FindAllFunctionDeclarations(*root);
    ASSERT_EQ(function_declarations.size(), 1);
    const auto& function_node = down_cast<const verible::SyntaxTreeNode&>(
        *function_declarations.front().match);
    const auto* function_id = GetFunctionId(function_node);
    const auto ids = FindAllUnqualifiedIds(*function_id);
    std::vector<absl::string_view> got_ids;
    for (const auto& id : ids) {
      const verible::SyntaxTreeLeaf* base = GetIdentifier(*id.match);
      got_ids.push_back(ABSL_DIE_IF_NULL(base)->get().text);
    }
    EXPECT_EQ(got_ids, test.second);
  }
}

TEST(GetFunctionIdTest, QualifiedIds) {
  const std::pair<std::string, int> kTestCases[] = {
      {"function foo(); endfunction", 0},
      {"function myclass::foo(); endfunction", 1},
  };
  for (const auto test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "");
    ASSERT_OK(analyzer.Analyze());
    // Root node is a description list, not a function.
    const auto& root = analyzer.Data().SyntaxTree();
    const auto function_declarations = FindAllFunctionDeclarations(*root);
    ASSERT_EQ(function_declarations.size(), 1);
    const auto& function_node = down_cast<const verible::SyntaxTreeNode&>(
        *function_declarations.front().match);
    const auto* function_id = GetFunctionId(function_node);
    const auto ids = FindAllQualifiedIds(*function_id);
    EXPECT_EQ(ids.size(), test.second);
  }
}

}  // namespace
}  // namespace verilog
