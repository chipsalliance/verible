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

#include "verilog/CST/declaration.h"

#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/text/token_info_test_util.h"
#include "common/text/tree_utils.h"
#include "common/util/range.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace {

TEST(FindAllDataDeclarations, CountMatches) {
  constexpr std::pair<absl::string_view, int> kTestCases[] = {
      {"", 0},
      {"module m;\nendmodule\n", 0},
      {"class c;\nendclass\n", 0},
      {"function f;\nendfunction\n", 0},
      {"package p;\nendpackage\n", 0},
      {"task t;\nendtask\n", 0},
      {"foo bar;\n", 1},
      {"foo bar, baz;\n", 1},
      {"foo bar; foo baz;\n", 2},
      {"module m;\n"
       "  foo bar, baz;\n"
       "endmodule\n",
       1},
      {"module m;\n"
       "  foo bar;\n"
       "  foo baz;\n"
       "endmodule\n",
       2},
  };
  for (const auto& test : kTestCases) {
    VerilogAnalyzer analyzer(test.first, "test-file");
    ASSERT_OK(analyzer.Analyze());
    const auto& root = analyzer.Data().SyntaxTree();
    const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));
    EXPECT_EQ(decls.size(), test.second);
  }
}

TEST(GetQualifiersOfDataDeclarationTest, NoQualifiers) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::TokenInfoTestData kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {{kTag, "foo bar;"}, "\n"},
      {"module m;\n",
       {kTag, "foo bar;"},
       "\n"
       "endmodule\n"},
      {"class c;\n",
       {kTag, "int foo;"},
       "\n"
       "endclass\n"},
      {"package p;\n",
       {kTag, "int foo;"},
       "\n"
       "endpackage\n"},
      {"function f;\n",
       {kTag, "logic bar;"},
       "\n"
       "endfunction\n"},
      {"task t;\n",
       {kTag, "logic bar;"},
       "\n"
       "endtask\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(decls.size(), 1);
    const auto& decl = *decls.front().match;
    const auto decl_span = verible::StringSpanOfSymbol(decl);
    const auto* quals = GetQualifiersOfDataDeclaration(decl);
    // Verify that quals is either nullptr or empty or contains only nullptrs.
    if (quals != nullptr) {
      for (const auto& child : quals->children()) {
        EXPECT_EQ(child, nullptr)
            << "unexpected:\n"
            << verible::RawTreePrinter(*child) << "\nfailed on:\n"
            << code;
      }
    }
    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts = test.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    ASSERT_TRUE(verible::IsSubRange(decl_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(decl_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(decl_span, expected_span));
  }
}

TEST(GetTypeOfDataDeclarationTest, ExplicitTypes) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::TokenInfoTestData kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {{kTag, "foo"}, " bar;\n"},
      {{kTag, "foo"}, " bar, baz;\n"},
      {"const ", {kTag, "foo"}, " bar;\n"},
      {"const ", {kTag, "foo#(1)"}, " bar;\n"},
      {"const ", {kTag, "foo#(.N(1))"}, " bar;\n"},
      {"const ", {kTag, "foo#(1, 2, 3)"}, " bar;\n"},
      {"static ", {kTag, "foo"}, " bar;\n"},
      {"var static ", {kTag, "foo"}, " bar;\n"},
      {"automatic ", {kTag, "foo"}, " bar;\n"},
      {"class c;\n",
       {kTag, "int"},
       " foo;\n"
       "endclass\n"},
      {"class c;\n"
       "const static ",
       {kTag, "int"},
       " foo;\n"
       "endclass\n"},
      {"class c;\n"
       "function f;\n"
       "const ",
       {kTag, "int"},
       " foo;\n"
       "endfunction\n"
       "endclass\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(decls.size(), 1);
    const auto& decl = *decls.front().match;
    const auto& type = GetTypeOfDataDeclaration(decl);
    const auto type_span = verible::StringSpanOfSymbol(type);
    // Verify that type span the specified range of text.

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts = test.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    ASSERT_TRUE(verible::IsSubRange(type_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(type_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(type_span, expected_span));
  }
}

TEST(GetQualifiersOfDataDeclarationTest, SomeQualifiers) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::TokenInfoTestData kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {{kTag, "const"}, " foo bar;\n"},
      {{kTag, "const"}, " foo#(1) bar;\n"},
      {{kTag, "const"}, " foo bar, baz;\n"},
      {{kTag, "static"}, " foo bar;\n"},
      {{kTag, "automatic"}, " foo bar;\n"},
      {{kTag, "var"}, " foo bar;\n"},
      {{kTag, "var static"}, " foo bar;\n"},
      {{kTag, "const static"}, " foo bar;\n"},
      {"class c;\n",
       {kTag, "const static"},
       " int foo;\n"
       "endclass\n"},
      {"class c;\n",
       {kTag, "const"},
       " int foo;\n"
       "endclass\n"},
      {"class c;\n"
       "function f;\n",
       {kTag, "const"},
       " int foo;\n"
       "endfunction\n"
       "endclass\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(decls.size(), 1);
    const auto& decl = *decls.front().match;
    const auto* type = GetQualifiersOfDataDeclaration(decl);
    ASSERT_NE(type, nullptr) << "decl:\n" << verible::RawTreePrinter(decl);
    const auto type_span = verible::StringSpanOfSymbol(*type);
    // Verify that type span the specified range of text.

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts = test.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    ASSERT_TRUE(verible::IsSubRange(type_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(type_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(type_span, expected_span));
  }
}

TEST(GetInstanceListFromDataDeclarationTest, InstanceLists) {
  constexpr int kTag = 1;  // value doesn't matter
  const verible::TokenInfoTestData kTestCases[] = {
      // each of these test cases should match exactly one data declaration
      // and have no qualifiers
      {"foo ", {kTag, "bar"}, ";\n"},
      {"foo ", {kTag, "bar = 0"}, ";\n"},
      {"foo ", {kTag, "bar, baz"}, ";\n"},
      {"foo ", {kTag, "bar = 1, baz = 2"}, ";\n"},
      {"foo#(1) ", {kTag, "bar"}, ";\n"},
      {"foo#(1,2) ", {kTag, "bar,baz,bam"}, ";\n"},
      {"const foo ", {kTag, "bar = 0"}, ";\n"},
      {"static foo ", {kTag, "bar = 0"}, ";\n"},
      {"class c;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endclass\n"},
      {"class c;\n"
       "  foo ",
       {kTag, "barr, bazz"},
       ";\n"
       "endclass\n"},
      {"class c;\n"
       "  const int ",
       {kTag, "barr, bazz"},
       ";\n"
       "endclass\n"},
      {"class c;\n"
       "  const int ",
       {kTag, "barr=3, bazz=4"},
       ";\n"
       "endclass\n"},
      {"function f;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endfunction\n"},
      {"function f;\n"
       "  foo ",
       {kTag, "bar, baz"},
       ";\n"
       "endfunction\n"},
      {"task t;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endtask\n"},
      {"task t;\n"
       "  foo ",
       {kTag, "bar, baz"},
       ";\n"
       "endtask\n"},
      {"package p;\n"
       "  foo ",
       {kTag, "bar"},
       ";\n"
       "endpackage\n"},
      {"package p;\n"
       "  foo ",
       {kTag, "bar, baz"},
       ";\n"
       "endpackage\n"},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto decls = FindAllDataDeclarations(*ABSL_DIE_IF_NULL(root));
    ASSERT_EQ(decls.size(), 1);
    const auto& decl = *decls.front().match;
    const auto& insts = GetInstanceListFromDataDeclaration(decl);
    const auto insts_span = verible::StringSpanOfSymbol(insts);
    // Verify that insts span the specified range of text.

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts = test.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    ASSERT_TRUE(verible::IsSubRange(insts_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(insts_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(insts_span, expected_span));
  }
}

}  // namespace
}  // namespace verilog
