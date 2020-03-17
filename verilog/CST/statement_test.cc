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

#include "verilog/CST/statement.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_info_test_util.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/range.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())

namespace verilog {
namespace {

using verible::SymbolKind;
using verible::SymbolTag;

struct ControlStatementTestData {
  NodeEnum expected_construct;
  verible::TokenInfoTestData token_data;
};

TEST(GetAnyControlStatementBodyTest, Various) {
  constexpr int kTag = 1;  // value doesn't matter
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kGenerateIfClause,
       {"module m;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "foo bar;"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},
      {NodeEnum::kGenerateIfClause,
       {"module m;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "begin\nfoo bar;end"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},
      {NodeEnum::kGenerateElseClause,
       {"module m;\n"
        "  if (expr)\n"
        "   foo bar;\n"
        "  else \n",
        {kTag, "bar#(1)   foo;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kGenerateElseClause,
       {"module m;\n"
        "  if (expr)\n"
        "   foo bar;\n"
        "  else \n",
        {kTag, "begin \nbar#(1)   foo; baz bam();\nend"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kLoopGenerateConstruct,
       {"module m;\n"
        "  for (genvar i=0; i<N; ++i)\n"
        "   ",
        {kTag, "foo#(.N(i)) bar;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kLoopGenerateConstruct,
       {"module m;\n"
        "  for (genvar i=0; i<N; ++i)\n"
        "   ",
        {kTag, "begin:l1\n      foo#(.N(i)) bar;\n  end : l1"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kIfClause,
       {"function f;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endfunction\n"}},
      {NodeEnum::kIfClause,
       {"task t;\n"
        "  if (expr)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},
      {NodeEnum::kElseClause,
       {"task t;\n"
        "  if (expr)\n"
        "   foo =bar;\n"
        "\n"
        "  else\n",
        {kTag, "bar=foo;"},
        "endtask\n"}},
      {NodeEnum::kElseClause,
       {"function f;\n"
        "  if (expr)\n"
        "   foo =bar;\n"
        "  else\n",
        {kTag, "begin:bb bar=foo(baz);\n\nend :\nbb"},
        "\nendfunction\n"}},
      {NodeEnum::kForLoopStatement,
       {"function f;\n"
        "  for (int j=N; expr; --j)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForLoopStatement,
       {"task t;\n"
        "  for (int j=N; expr; --j)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kDoWhileLoopStatement,
       {"function f;\n"
        "   do\n",
        {kTag, "foo=bar;"},
        "  while (expr);\n"
        "endfunction\n"}},
      {NodeEnum::kDoWhileLoopStatement,
       {"task t;\n"
        "  do ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "  while (expr);\n"
        "endtask\n"}},
      {NodeEnum::kForeverLoopStatement,
       {"function f;\n"
        "  forever\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForeverLoopStatement,
       {"task t;\n"
        "  forever\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kForeachLoopStatement,
       {"function f;\n"
        "  foreach (x[i])\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kForeachLoopStatement,
       {"task t;\n"
        "  foreach (x[i])\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kRepeatLoopStatement,
       {"function f;\n"
        "  repeat (8)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kRepeatLoopStatement,
       {"task t;\n"
        "  repeat (9)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kWhileLoopStatement,
       {"function f;\n"
        "  while (expr)\n"
        "   ",
        {kTag, "foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kWhileLoopStatement,
       {"task t;\n"
        "  while (expr)\n"
        "   ",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endtask\n"}},
      {NodeEnum::kProceduralTimingControlStatement,
       {"module  m;\n"
        "  always @(negedge c)\n",
        {kTag, "foo=bar;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kProceduralTimingControlStatement,
       {"module  m;\n"
        "  always @(negedge c)\n",
        {kTag, "begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "endmodule\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = verible::SearchSyntaxTree(
        *ABSL_DIE_IF_NULL(root),
        verible::matcher::DynamicTagMatchBuilder(SymbolTag{
            SymbolKind::kNode, static_cast<int>(test.expected_construct)})());
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* body = GetAnyControlStatementBody(statement);
    ASSERT_NE(body, nullptr);
    const auto body_span = verible::StringSpanOfSymbol(*body);
    // Verify that type span the specified range of text.

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts =
        test.token_data.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    ASSERT_TRUE(verible::IsSubRange(body_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(body_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(body_span, expected_span));
  }
}

TEST(GetAnyConditionalIfClauseTest, Various) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr) foo bar;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr) foo bar;"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause),
         "if (expr) begin\nfoo bar;end"},
        "\n"
        "  else \n"
        "   bar foo;\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr ) foo=bar;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr ) foo=bar;"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"task t;\n",
        {static_cast<int>(NodeEnum::kIfClause),
         "if  (expr)begin\nfoo=bar; bar=1;\nend"},
        "\n"
        "  else \n"
        "   bar=foo;\n"
        "endtask\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = verible::SearchSyntaxTree(
        *ABSL_DIE_IF_NULL(root),
        verible::matcher::DynamicTagMatchBuilder(SymbolTag{
            SymbolKind::kNode, static_cast<int>(test.expected_construct)})());
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* clause = GetAnyConditionalIfClause(statement);
    ASSERT_NE(clause, nullptr);
    const auto clause_span = verible::StringSpanOfSymbol(*clause);
    // Verify that type span the specified range of text.

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts =
        test.token_data.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    EXPECT_EQ(expected_excerpts.front().token_enum, clause->Tag().tag);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    ASSERT_TRUE(verible::IsSubRange(clause_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(clause_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(clause_span, expected_span));
  }
}

TEST(GetAnyConditionalElseClauseTest, NoElseClause) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        {static_cast<int>(NodeEnum::kGenerateIfClause), "if (expr) foo bar;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        {static_cast<int>(NodeEnum::kIfClause), "if ( expr ) foo=bar;"},
        "\n"
        "endfunction\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = verible::SearchSyntaxTree(
        *ABSL_DIE_IF_NULL(root),
        verible::matcher::DynamicTagMatchBuilder(SymbolTag{
            SymbolKind::kNode, static_cast<int>(test.expected_construct)})());
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* clause = GetAnyConditionalElseClause(statement);
    EXPECT_EQ(clause, nullptr);
  }
}

TEST(GetAnyConditionalElseClauseTest, HaveElseClause) {
  const ControlStatementTestData kTestCases[] = {
      // each of these test cases should match exactly one statement body
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        "if (expr) foo bar;\n",
        {static_cast<int>(NodeEnum::kGenerateElseClause),
         "else \n"
         "   bar foo;"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalGenerateConstruct,
       {"module m;\n",
        "if (expr) foo bar;\n",
        {static_cast<int>(NodeEnum::kGenerateElseClause),
         "else \n"
         "   begin bar foo;\nend"},
        "\n"
        "endmodule\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        "if ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   bar=foo;"},
        "\n"
        "endfunction\n"}},
      {NodeEnum::kConditionalStatement,
       {"function f;\n",
        "if ( expr ) foo=bar;\n",
        {static_cast<int>(NodeEnum::kElseClause),
         "else \n"
         "   begin\nbar=foo;\nend"},
        "\n"
        "endfunction\n"}},
  };
  for (const auto& test : kTestCases) {
    const absl::string_view code(test.token_data.code);
    VerilogAnalyzer analyzer(code, "test-file");
    const auto code_copy = analyzer.Data().Contents();
    ASSERT_OK(analyzer.Analyze()) << "failed on:\n" << code;
    const auto& root = analyzer.Data().SyntaxTree();

    const auto statements = verible::SearchSyntaxTree(
        *ABSL_DIE_IF_NULL(root),
        verible::matcher::DynamicTagMatchBuilder(SymbolTag{
            SymbolKind::kNode, static_cast<int>(test.expected_construct)})());
    ASSERT_EQ(statements.size(), 1);
    const auto& statement = *statements.front().match;
    const auto* clause = GetAnyConditionalElseClause(statement);
    ASSERT_NE(clause, nullptr);
    const auto clause_span = verible::StringSpanOfSymbol(*clause);
    // Verify that type span the specified range of text.

    // TODO(b/151371397): Refactor this test code along with
    // common/analysis/linter_test_util.h to be able to compare set-symmetric
    // differences in 'findings'.

    // Find the tokens, rebased into the other buffer.
    const auto expected_excerpts =
        test.token_data.FindImportantTokens(code_copy);
    ASSERT_EQ(expected_excerpts.size(), 1);
    EXPECT_EQ(expected_excerpts.front().token_enum, clause->Tag().tag);
    // Compare the string_views and their exact spans.
    const auto expected_span = expected_excerpts.front().text;
    ASSERT_TRUE(verible::IsSubRange(clause_span, code_copy));
    ASSERT_TRUE(verible::IsSubRange(expected_span, code_copy));
    EXPECT_EQ(clause_span, expected_span);
    EXPECT_TRUE(verible::BoundsEqual(clause_span, expected_span));
  }
}

}  // namespace
}  // namespace verilog
