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

#include "verilog/analysis/verilog_analyzer.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/casts.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/analysis/file_analyzer.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/constants.h"
#include "common/text/symbol.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/text/token_info_test_util.h"
#include "common/text/token_stream_view.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "common/util/status.h"
#include "verilog/analysis/verilog_excerpt_parse.h"
#include "verilog/parser/verilog_token_enum.h"

#undef EXPECT_OK
#define EXPECT_OK(value) EXPECT_TRUE((value).ok())
#undef ASSERT_OK
#define ASSERT_OK(value) ASSERT_TRUE((value).ok())

namespace verilog {
namespace {

using testing::SizeIs;
using verible::AnalysisPhase;
using verible::ConcreteSyntaxTree;
using verible::down_cast;
using verible::FindFirstSubtree;
using verible::Symbol;
using verible::SymbolPtr;
using verible::SyntaxTreeLeaf;
using verible::TokenInfo;
using verible::TokenInfoTestData;

bool TreeContainsToken(const ConcreteSyntaxTree& tree, const TokenInfo& token) {
  const auto* matching_leaf =
      FindFirstSubtree(tree.get(), [&](const Symbol& symbol) {
        if (symbol.Kind() != verible::SymbolKind::kLeaf) return false;
        const auto* leaf_ptr = down_cast<const SyntaxTreeLeaf*>(&symbol);
        return leaf_ptr->get() == token;
      });
  return matching_leaf != nullptr;
}

void DiagnosticMessagesContainFilename(const VerilogAnalyzer& analyzer,
                                       absl::string_view filename) {
  const std::vector<std::string> syntax_error_messages(
      analyzer.LinterTokenErrorMessages());
  for (const auto& message : syntax_error_messages) {
    EXPECT_TRUE(absl::StrContains(message, filename));
  }
}

// AnalyzeVerilog tests:
// More extensive tests are in verilog_parser_unittest.cc.

TEST(AnalyzeVerilogTest, EmptyText) {
  const auto analyzer_ptr = absl::make_unique<VerilogAnalyzer>("", "<noname>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->Analyze());
}

// The following tests check Verilog lexer returns proper diagnostics:

// Tests that invalid symbol identifier is rejected.
TEST(AnalyzeVerilogLexerTest, RejectsBadId) {
  const auto analyzer_ptr = absl::make_unique<VerilogAnalyzer>(
      "module 321foo;\nendmodule\n", "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Tokenize();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->LexStatus().ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kLexPhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<noname>");
}

// Tests that invalid macro identifier is rejected.
TEST(AnalyzeVerilogLexerTest, RejectsMacroBadId) {
  const auto analyzer_ptr =
      absl::make_unique<VerilogAnalyzer>("`321foo(a, b, c)\n", "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Tokenize();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->LexStatus().ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kLexPhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<noname>");
}

// The following tests check that standalone Verilog expression parsing work.
// More extensive tests are in verilog_parser_unittest.cc.

TEST(AnalyzeVerilogExpressionTest, ParsesZero) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("0", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("\"\"", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesNonEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("\"nevermore.\"", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesBinaryOp) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("a+b", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesParenBinaryOp) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("(a+b)", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesUnfinishedOp) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("a+", "<file>");
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>");
}

TEST(AnalyzeVerilogExpressionTest, Unbalanced) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("(a+c", "<file>");
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>");
}

TEST(AnalyzeVerilogExpressionTest, ParsesConcat) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("{cde, fgh, ijk}", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesFunctionCall) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("average(1, 2, \"five\")", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesMacroCall) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("`MACRO(a+b, 1)", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesMacroCallWithBadId) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("`MACRO(a+b, 1bad_id)", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, RejectsModuleItemAttack) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("a; wire foo", "<file>");
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(2));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>");
}

// The following tests check that standalone Verilog module-body parsing works.
// More extensive tests are in verilog_parser_unittest.cc.
TEST(AnalyzeVerilogModuleBodyTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogModuleBody("", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesWireDeclarations) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogModuleBody("wire fire;", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesInstance) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogModuleBody("type_of_thing #(16) foo(a, b);", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesInitialBlock) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogModuleBody(
      "initial begin\n"
      "  a = 1;\n"
      "end",
      "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesMultipleItems) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogModuleBody(
      "wire [7:0] bar;\n"
      "initial begin\n"
      "  a = 1;\n"
      "end",
      "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

// The following tests check for class-body parsing.
TEST(AnalyzeVerilogClassBodyTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogClassBody("", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, ParsesIntegerField) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogClassBody("integer foo;", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, ParsesMethod) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogClassBody(
      "virtual function bar();\nendfunction\n", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, ParsesConstructor) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogClassBody(
      "function new();\nx = 1;\nendfunction : new\n", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, RejectsModuleItem) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogClassBody("initial begin\nend\n", "<file>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  const auto& first_reject = rejects.front();
  EXPECT_EQ(first_reject.phase, AnalysisPhase::kParsePhase);
  EXPECT_EQ(first_reject.token_info.text, "initial");
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>");
}

// The following tests check for package-body parsing.
TEST(AnalyzeVerilogPackageBodyTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogPackageBody("", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogPackageBodyTest, ParsesExportDeclaration) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogPackageBody("export foo::bar;\n", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogPackageBodyTest, ParsesParameter) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogPackageBody("parameter int kFoo = 42;\n", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, RejectsWireDeclaration) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogClassBody("wire [3:0] bar;\n", "<file>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>");
}

// The following tests verify that parser mode selection works.
TEST(AnalyzeVerilogAutomaticMode, NormalModeEmptyText) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode("", "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, NormalModeModule) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode("module rrr;\nendmodule\n",
                                            "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, NormalModeModuleInvalidSelection) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: does-not-exist-mode\n"
          "module rrr;\nendmodule\n",
          "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, StatementsMode) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-statements\n"
          "foo_bar();\n"
          "if (1) begin\n"
          "  x = 0;\n"
          "end\n",
          "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, ModuleBodyMode) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-module-body\n"
          "wire x;\n"
          "initial begin\n"
          "  x = 0;\n"
          "end\n",
          "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, ClassBodyMode) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-class-body\n"
          "function new();\n"
          "  x = 0;\n"
          "endfunction\n",
          "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, PackageBodyMode) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-package-body\n"
          "export xx::*;\n",
          "<file>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

// Tests that automatic mode parsing can detect that some first failing
// keywords will trigger (successful) re-parsing as a module-body.
TEST(AnalyzeVerilogAutomaticMode, InferredModuleBodyMode) {
  constexpr const char* test_cases[] = {
      "always @(posedge clk) begin x<=y; end\n",
      "initial begin x = 0; end;\n",
  };
  for (const char* code : test_cases) {
    std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
        VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>");
    EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus()) << "code was:\n"
                                                             << code;
  }
}

struct TestCase {
  const char* code;
  bool valid;
};

// Tests that various invalid input does not crash.
TEST(AnalyzeVerilogAutomaticMode, InvalidInputs) {
  constexpr TestCase test_cases[] = {
      {"`s(\n", false},
      {"`s(}\n", false},
      {"`s(};\n", false},
      {"`s(};if\n", false},
      {"`s(};if(\n", false},
      {"`s(};if(k\n", false},
      {"`s(};if(k)\n",
       // valid because it is macro call is un-expanded, closed at ')'
       true},
      {"`s(};if(k);\n", true},
  };
  for (const auto& test : test_cases) {
    std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
        VerilogAnalyzer::AnalyzeAutomaticMode(test.code, "<file>");
    EXPECT_EQ(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus().ok(), test.valid)
        << "code was:\n"
        << test.code;
  }
}

// Tests that when retrying parsing in a different mode fails, we get the result
// of the analyzer that got further before the first syntax error.
TEST(AnalyzeVerilogAutomaticMode, InferredModuleBodyModeFarthestFirstError) {
  const TokenInfoTestData test{
      "always @(posedge clk) begin ", {TK_module, "module"}, " x<=y; end\n"};
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(test.code, "<file>");
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  ASSERT_FALSE(status.ok());
  const auto& rejects = analyzer_ptr->GetRejectedTokens();
  ASSERT_FALSE(rejects.empty());
  const auto& token_info = rejects.front().token_info;
  // Expect the first syntax error of the retried parsing:
  const auto expected_tokens =
      test.FindImportantTokens(analyzer_ptr->Data().Contents());
  ASSERT_EQ(expected_tokens.size(), 1);
  EXPECT_EQ(token_info, expected_tokens.front());
}

// The following tests cover integration between parsing Verilog
// and verible::FileAnalyzer::FocusOnSubtreeSpanningSubstring
// and verible::FileAnalyzer::ExpandSubtrees.
// This is done in lieu of hand-crafting fake FileAnalyzer objects
// which would be very tedious without using a real parser.
// TODO(b/69043298): implement test utilites for building fake FileAnalyzer
// objects (with coherent token stream and syntax tree) *without* relying
// on a real language parser.

// Test that an empty macro arg doesn't expand.
TEST(VerilogAnalyzerExpandsMacroArgsTest, NoArg) {
  const TokenInfoTestData test = {{MacroCallId, "`FOOBAR"}, "()\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that a space macro arg doesn't expand.
TEST(VerilogAnalyzerExpandsMacroArgsTest, SpaceArg) {
  const TokenInfoTestData test = {"  ", {MacroCallId, "`FOOBAR"}, "(      )\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that comma-separated blanks don't expand.
TEST(VerilogAnalyzerExpandsMacroArgsTest, CommaSeparatedBlankArg) {
  const TokenInfoTestData test = {"`FOOBAR( ", ',', " )\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that a non-expression macro arg doesn't expand.
TEST(VerilogAnalyzerExpandsMacroArgsTest, NonExprArg) {
  // 'module' is a Verilog keyword, but macro argument text remains unlexed
  // when it does not parse as an expression.
  const TokenInfoTestData test = {"`FOOBAR(", {MacroArg, "module"}, ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an integer expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, IntegerArg) {
  const TokenInfoTestData test = {"`FOO(", {TK_DecNumber, "123"}, ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an identifier expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, IdentifierArg) {
  const TokenInfoTestData test = {"`FOO(", {SymbolIdentifier, "bar"}, ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an macro id macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, MacroIdentifierArg) {
  const TokenInfoTestData test = {"`FOO(", {MacroIdentifier, "`bar"}, ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that a string expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, StringArg) {
  const TokenInfoTestData test = {
      "`FOO(", {TK_StringLiteral, "\"hello\""}, ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an binary expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, BinaryExprArg) {
  const TokenInfoTestData test = {
      "`FOO(", {TK_DecNumber, "1"}, '+', {TK_DecNumber, "3"}, ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 3);
  for (const auto search_token : search_tokens) {
    EXPECT_TRUE(TreeContainsToken(tree, search_token));
  }
}

// Test that a function call expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, FunctionCallArg) {
  const TokenInfoTestData test = {"`FOO(", {SymbolIdentifier, "square_root"},
                                  '(',     {TK_DecNumber, "0"},
                                  ')',     ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 4);
  for (const auto search_token : search_tokens) {
    EXPECT_TRUE(TreeContainsToken(tree, search_token));
  }
}

// Test that a list of expression macro args expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, MultipleExpressions) {
  const TokenInfoTestData test = {
      "`FOO(", {TK_DecNumber, "9"}, ",", {SymbolIdentifier, "aa"}, ")\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 2);
  for (const auto search_token : search_tokens) {
    EXPECT_TRUE(TreeContainsToken(tree, search_token));
  }
}

// Test that nested macro calls expand properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, MacroCall) {
  const TokenInfoTestData test = {
      "`FOO(", {MacroCallId, "`BAR"}, "(", {SymbolIdentifier, "abc"}, "))\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 2);
  for (const auto search_token : search_tokens) {
    EXPECT_TRUE(TreeContainsToken(tree, search_token));
  }
}

// Test that deeply nested macro calls expand properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, MacroCallNested) {
  const TokenInfoTestData test = {
      "`FOO(", {MacroCallId, "`BAR"},     "(",    {MacroCallId, "`BAZ"},
      "(",     {SymbolIdentifier, "abc"}, ")))\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 3);
  for (const auto search_token : search_tokens) {
    EXPECT_TRUE(TreeContainsToken(tree, search_token));
  }
}

// Test that deeply nested macro calls expand properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, MultipleMacroCalls) {
  const TokenInfoTestData test = {"`FOO(", {MacroCallId, "`BAR"},
                                  "(",     {TK_DecNumber, "33"},
                                  ")",     ',',
                                  " ",     {MacroCallId, "`BAZ"},
                                  "(",     {SymbolIdentifier, "abc"},
                                  "))\n"};
  const auto analyzer =
      absl::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree& tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 5);
  for (const auto search_token : search_tokens) {
    EXPECT_TRUE(TreeContainsToken(tree, search_token));
  }
}

// Helper class for testing internals.
class VerilogAnalyzerInternalsTest : public testing::Test,
                                     public VerilogAnalyzer {
 public:
  VerilogAnalyzerInternalsTest() : VerilogAnalyzer("", "") {}
};

// Tests that parser-selection directive is properly detected.
TEST_F(VerilogAnalyzerInternalsTest, ScanParsingModeDirective) {
  const std::pair<std::string, absl::string_view> test_cases[] = {
      // code, expected parsing mode
      {"", ""},
      {"\n", ""},
      {"// nothing here\n", ""},
      {"/* or here */\n", ""},
      {"// verilog_syntax: super-mode\n", "super-mode"},
      {"    //    verilog_syntax:    sub-mode  \n", "sub-mode"},
      {"//verilog_syntax:    sub-mode // blah\n", "sub-mode"},
      {"// verilogsyntax: foo-mode\n", ""},  // not spelled right
      {"// VerilogSyntax: bar-mode\n", ""},  // not spelled right
      {"/*verilog_syntax: foo-mode*/\n", "foo-mode"},
      {"/*    verilog_syntax:    foo-mode      */\n", "foo-mode"},
      {"\n\n\n// verilog_syntax: super-mode\n", "super-mode"},
      {"// verilog_syntax: alpha-mode\n"  // first wins
       "// verilog_syntax: beta-mode\n",
       "alpha-mode"},
      {"module foo;\n"  // stops scanning after real tokens
       "endmodule\n"
       "// verilog_syntax: beta-mode\n",
       ""},
      {"package foo;\n"  // stops scanning after real tokens
       "// verilog_syntax: delta-mode\n"
       "endpackage\n",
       ""},
      {"// regular comment\n"
       "// verilog_syntax: beta-mode\n",
       "beta-mode"},
      {"`ifndef FOO\n"  // typical include guard
       "`define FOO  // this is FOO\n"
       "// verilog_syntax: gamma-mode\n"  // still scans up to here
       "`endif  // FOO\n",
       "gamma-mode"},
      {"`ifdef FOO\n"  // typical include guard
       "`undef FOO\n"
       "`elsif BAR\n"
       "`else\n"
       "// verilog_syntax: turbo-mode\n"  // still scans up to here
       "`endif\n",
       "turbo-mode"},
      {"`MACRO\n"
       "// verilog_syntax: moody-mode\n",
       ""},
      {"`MACRO_CALL(arg1, arg2) // macro me\n"
       "// verilog_syntax: evil-mode\n",
       ""},
  };
  for (const auto& test : test_cases) {
    VerilogAnalyzer analyzer(test.first, "<file>");
    const auto lexer_status = analyzer.Tokenize();
    EXPECT_OK(lexer_status);
    absl::string_view mode =
        ScanParsingModeDirective(analyzer.Data().TokenStream());
    EXPECT_EQ(mode, test.second) << " mismatched mode with input:\n"
                                 << test.first;
  }
}

template <typename T>
void _LexTestCases(const T& test_cases,
                   std::vector<std::unique_ptr<VerilogAnalyzer>>* analyzers) {
  for (auto test : test_cases) {
    analyzers->emplace_back(absl::make_unique<VerilogAnalyzer>(test, ""));
  }
  for (auto& analyzer : *analyzers) {
    ASSERT_OK(ABSL_DIE_IF_NULL(analyzer)->Tokenize());
  }
}

bool _EquivalentTokenStreams(const VerilogAnalyzer& a1,
                             const VerilogAnalyzer& a2,
                             std::ostream* errstream = &std::cout) {
  const auto& tokens1(a1.Data().TokenStream());
  const auto& tokens2(a2.Data().TokenStream());
  const bool eq = LexicallyEquivalent(tokens1, tokens2, errstream);
  // Check that commutative comparison yields same result.
  // Don't bother with the error stream.
  const bool commutative = LexicallyEquivalent(tokens2, tokens1);
  EXPECT_EQ(eq, commutative);
  return eq;
}

TEST(LexicallyEquivalentTest, Spaces) {
  const char* kTestCases[] = {
      "",
      " ",
      "\n",
      "\t",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[3]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[1], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[1], *analyzers[3]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[2], *analyzers[3]));
}

TEST(LexicallyEquivalentTest, ShortSequences) {
  const char* kTestCases[] = {
      "1",
      "2",
      "1;",
      "1 ;",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[3]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[2]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[3]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[2], *analyzers[3]));
}

TEST(LexicallyEquivalentTest, Identifiers) {
  const char* kTestCases[] = {
      "foo bar;",
      "   foo\t\tbar    ;   ",
      "foobar;",  // only 2 tokens
      "foo bar\n;\n",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[3]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[2]));
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[1], *analyzers[3]));
  EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[2], *analyzers[3]));
}

TEST(LexicallyEquivalentTest, Keyword) {
  const char* kTestCases[] = {
      "wire foo;",
      "  wire  \n\t\t   foo  ;\n",
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  _LexTestCases(absl::MakeSpan(kTestCases), &analyzers);
  EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1]));
}

TEST(LexicallyEquivalentTest, Comments) {
  const char* kTestCases[] = {
      "// comment1\n",        //
      "// comment1 \n",       //
      "//    comment1\n",     //
      "   //    comment1\n",  // same as [2]
      "// comment2\n",        //
      "/* comment1 */\n",     //
      "/*  comment1  */\n",   //
  };
  // Lex all of the test cases once.
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  // At some point in the future when token-reflowing is implemented, these
  // will need to become smarter checks.
  // For now, they only check for exact match.
  for (size_t i = 0; i < span.size(); ++i) {
    for (size_t j = i + 1; j < span.size(); ++j) {
      if (i == 2 && j == 3) {
        EXPECT_TRUE(_EquivalentTokenStreams(*analyzers[i], *analyzers[j]));
      } else {
        EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[i], *analyzers[j]));
      }
    }
  }
}

TEST(LexicallyEquivalentTest, DiagnosticLength) {
  const char* kTestCases[] = {
      "module foo\n",
      "module foo;\n",
  };
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1], &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 3 vs. 4"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[1], *analyzers[0], &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 4 vs. 3"));
    EXPECT_TRUE(absl::StrContains(errs.str(), "First mismatched token [2]:"));
  }
}

TEST(LexicallyEquivalentTest, DiagnosticLengthTrimEnd) {
  const char* kTestCases[] = {
      "module foo;",
  };
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  // Make a copy of token sequences and trim off the $end.
  verible::TokenSequence longer(analyzers[0]->Data().TokenStream());
  ASSERT_EQ(longer.back().token_enum, verible::TK_EOF);
  longer.pop_back();
  verible::TokenSequence shorter(longer);
  shorter.pop_back();
  {
    std::ostringstream errs;
    EXPECT_FALSE(LexicallyEquivalent(shorter, longer, &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 2 vs. 3"));
    EXPECT_TRUE(
        absl::StrContains(errs.str(), "First excess token in right sequence:"));
  }
  {
    std::ostringstream errs;
    EXPECT_FALSE(LexicallyEquivalent(longer, shorter, &errs));
    EXPECT_TRUE(absl::StartsWith(
        errs.str(), "Mismatch in token sequence lengths: 3 vs. 2"));
    EXPECT_TRUE(
        absl::StrContains(errs.str(), "First excess token in left sequence:"));
  }
}

TEST(LexicallyEquivalentTest, DiagnosticMismatch) {
  const char* kTestCases[] = {
      "module foo;\n",
      "module bar;\n",
      "module foo,\n",
  };
  std::vector<std::unique_ptr<VerilogAnalyzer>> analyzers;
  auto span = absl::MakeSpan(kTestCases);
  _LexTestCases(span, &analyzers);
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[1], &errs));
    EXPECT_TRUE(absl::StartsWith(errs.str(), "First mismatched token [1]:"));
  }
  {
    std::ostringstream errs;
    EXPECT_FALSE(_EquivalentTokenStreams(*analyzers[0], *analyzers[2], &errs));
    EXPECT_TRUE(absl::StartsWith(errs.str(), "First mismatched token [2]:"));
  }
}

}  // namespace
}  // namespace verilog
