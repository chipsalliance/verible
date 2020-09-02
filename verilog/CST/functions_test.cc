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

#include "verilog/CST/functions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/analysis/syntax_tree_search_test_utils.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_info_test_util.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/CST/identifier.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/parser/verilog_token_enum.h"

#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::CheckSymbolAsLeaf;
using verible::SymbolCastToNode;
using verible::SyntaxTreeSearchTestCase;
using verible::TreeSearchMatch;

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
    const auto& function_node =
        SymbolCastToNode(*function_declarations.front().match);
    GetFunctionHeader(function_node);
    // Reaching here is success, function includes internal checks already.
    // TODO(b/151371397): verify substring range
  }
}

TEST(GetFunctionLifetimeTest, NoLifetimeDeclared) {
  VerilogAnalyzer analyzer("function foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto& function_node =
      SymbolCastToNode(*function_declarations.front().match);
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
  const auto& function_node =
      SymbolCastToNode(*function_declarations.front().match);
  const auto* lifetime = GetFunctionLifetime(function_node);
  CheckSymbolAsLeaf(*ABSL_DIE_IF_NULL(lifetime), TK_static);
  // TODO(b/151371397): verify substring range
}

TEST(GetFunctionLifetimeTest, AutomaticLifetimeDeclared) {
  VerilogAnalyzer analyzer("function automatic foo(); endfunction", "");
  ASSERT_OK(analyzer.Analyze());
  // Root node is a description list, not a function.e
  const auto& root = analyzer.Data().SyntaxTree();
  const auto function_declarations = FindAllFunctionDeclarations(*root);
  ASSERT_EQ(function_declarations.size(), 1);
  const auto& function_node =
      SymbolCastToNode(*function_declarations.front().match);
  const auto* lifetime = GetFunctionLifetime(function_node);
  CheckSymbolAsLeaf(*ABSL_DIE_IF_NULL(lifetime), TK_automatic);
  // TODO(b/151371397): verify substring range
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
    const auto& function_node =
        SymbolCastToNode(*function_declarations.front().match);
    const auto* function_id = GetFunctionId(function_node);
    const auto ids = FindAllUnqualifiedIds(*function_id);
    std::vector<absl::string_view> got_ids;
    for (const auto& id : ids) {
      const verible::SyntaxTreeLeaf* base = GetIdentifier(*id.match);
      got_ids.push_back(ABSL_DIE_IF_NULL(base)->get().text());
    }
    EXPECT_EQ(got_ids, test.second);
    // TODO(b/151371397): verify substring range
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
    const auto& function_node =
        SymbolCastToNode(*function_declarations.front().match);
    const auto* function_id = GetFunctionId(function_node);
    const auto ids = FindAllQualifiedIds(*function_id);
    EXPECT_EQ(ids.size(), test.second);
    // TODO(b/151371397): verify substring range
  }
}

struct SubtreeTestData {
  NodeEnum expected_construct;
  verible::TokenInfoTestData token_data;
};

TEST(GetFunctionReturnTypeTest, NoReturnType) {
  const SubtreeTestData kTestCases[] = {
      {NodeEnum::kFunctionDeclaration, {"function f;endfunction\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"package p;\nfunction f;\nendfunction\nendpackage\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"class c;\nfunction f;\nendfunction\nendclass\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"module m;\nfunction f;\nendfunction\nendmodule\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = FindAllFunctionDeclarations(*root);
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* return_type = GetFunctionReturnType(statement);
    // Expect a type node, even when type is implicit or empty.
    ASSERT_NE(return_type, nullptr);
    const auto return_type_span = verible::StringSpanOfSymbol(*return_type);
    EXPECT_TRUE(return_type_span.empty());
  }
}

TEST(GetFunctionReturnTypeTest, WithReturnType) {
  constexpr int kTag = 1;  // value not important
  const SubtreeTestData kTestCases[] = {
      {NodeEnum::kFunctionDeclaration,
       {"function ", {kTag, "void"}, " f;endfunction\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"package p;\nfunction ",
        {kTag, "int"},
        " f;\nendfunction\nendpackage\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"class c;\nfunction ",
        {kTag, "foo_pkg::bar_t"},
        " f;\nendfunction\nendclass\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"module m;\nfunction ",
        {kTag, "foo#(bar)"},
        " f;\nendfunction\nendmodule\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const absl::string_view code_copy(analyzer.Data().Contents());
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = FindAllFunctionDeclarations(*root);
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* return_type = GetFunctionReturnType(statement);
    ASSERT_NE(return_type, nullptr);
    const auto return_type_span = verible::StringSpanOfSymbol(*return_type);
    EXPECT_FALSE(return_type_span.empty());

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts =
        test.token_data.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text();
    ASSERT_TRUE(verible::IsSubRange(return_type_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(return_type_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(return_type_span, expected_span));
  }
}

TEST(GetFunctionFormalPortsGroupTest, NoFormalPorts) {
  const SubtreeTestData kTestCases[] = {
      {NodeEnum::kFunctionDeclaration, {"function f;endfunction\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"package p;\nfunction f;\nendfunction\nendpackage\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"class c;\nfunction f;\nendfunction\nendclass\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"module m;\nfunction f;\nendfunction\nendmodule\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = FindAllFunctionDeclarations(*root);
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* return_type = GetFunctionFormalPortsGroup(statement);
    EXPECT_EQ(return_type, nullptr);
  }
}

TEST(GetFunctionFormalPortsGroupTest, WithFormalPorts) {
  constexpr int kTag = 1;  // value not important
  const SubtreeTestData kTestCases[] = {
      {NodeEnum::kFunctionDeclaration,
       {"function f", {kTag, "()"}, ";endfunction\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"package p;\nfunction f",
        {kTag, "(string s)"},
        ";\nendfunction\nendpackage\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"class c;\nfunction f",
        {kTag, "(int i, string s)"},
        ";\nendfunction\nendclass\n"}},
      {NodeEnum::kFunctionDeclaration,
       {"module m;\nfunction f",
        {kTag, "(input logic foo, bar)"},
        ";\nendfunction\nendmodule\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const absl::string_view code_copy(analyzer.Data().Contents());
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = FindAllFunctionDeclarations(*root);
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* port_formals = GetFunctionFormalPortsGroup(statement);
    ASSERT_NE(port_formals, nullptr);
    const auto port_formals_span = verible::StringSpanOfSymbol(*port_formals);
    EXPECT_FALSE(port_formals_span.empty());

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts =
        test.token_data.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text();
    ASSERT_TRUE(verible::IsSubRange(port_formals_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(port_formals_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(port_formals_span, expected_span));
  }
}

TEST(GetFunctionHeaderTest, GetFunctionName) {
  constexpr int kTag = 1;  // value doesn't matter
  const SyntaxTreeSearchTestCase kTestCases[] = {
      {"function ",
       {kTag, "foo"},
       "();\n endfunction\n function ",
       {kTag, "bar"},
       "()\n; endfunction"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllFunctionDeclarations(*ABSL_DIE_IF_NULL(root));

    std::vector<TreeSearchMatch> types;
    for (const auto& decl : decls) {
      const auto* type = GetFunctionName(*decl.match);
      types.push_back(TreeSearchMatch{type, {/* ignored context */}});
    }

    std::ostringstream diffs;
    EXPECT_TRUE(test.ExactMatchFindings(types, code_copy, &diffs))
        << "failed on:\n"
        << code << "\ndiffs:\n"
        << diffs.str();
  }
}

}  // namespace
}  // namespace verilog
