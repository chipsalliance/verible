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

#include "verible/verilog/analysis/verilog-analyzer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verible/common/analysis/file-analyzer.h"
#include "verible/common/strings/display-utils.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/text/token-info-test-util.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-excerpt-parse.h"
#include "verible/verilog/parser/verilog-token-enum.h"

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

static constexpr verilog::VerilogPreprocess::Config kDefaultPreprocess;

bool TreeContainsToken(const ConcreteSyntaxTree &tree, const TokenInfo &token) {
  const auto *matching_leaf =
      FindFirstSubtree(tree.get(), [&](const Symbol &symbol) {
        if (symbol.Kind() != verible::SymbolKind::kLeaf) return false;
        const auto *leaf_ptr = down_cast<const SyntaxTreeLeaf *>(&symbol);
        return leaf_ptr->get() == token;
      });
  return matching_leaf != nullptr;
}

void DiagnosticMessagesContainFilename(const VerilogAnalyzer &analyzer,
                                       absl::string_view filename,
                                       bool with_diagnostic_context) {
  const std::vector<std::string> syntax_error_messages(
      analyzer.LinterTokenErrorMessages(with_diagnostic_context));
  for (const auto &message : syntax_error_messages) {
    EXPECT_TRUE(absl::StrContains(message, filename));
  }
}

// AnalyzeVerilog tests:
// More extensive tests are in verilog_parser_unittest.cc.

TEST(AnalyzeVerilogTest, EmptyText) {
  const auto analyzer_ptr = std::make_unique<VerilogAnalyzer>("", "<noname>");
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->Analyze());
}

// The following tests check Verilog lexer returns proper diagnostics:

// Tests that invalid symbol identifier is rejected.
TEST(AnalyzeVerilogLexerTest, RejectsBadId) {
  const auto analyzer_ptr = std::make_unique<VerilogAnalyzer>(
      "module 321foo;\nendmodule\n", "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Tokenize();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->LexStatus().ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kLexPhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<noname>", false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<noname>", true);
}

// Tests that invalid macro identifier is rejected.
TEST(AnalyzeVerilogLexerTest, RejectsMacroBadId) {
  const auto analyzer_ptr =
      std::make_unique<VerilogAnalyzer>("`321foo(a, b, c)\n", "<noname>");
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->Tokenize();
  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(analyzer_ptr->LexStatus().ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kLexPhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<noname>", false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<noname>", true);
}

// The following tests check that standalone Verilog expression parsing work.
// More extensive tests are in verilog_parser_unittest.cc.

TEST(AnalyzeVerilogExpressionTest, ParsesZero) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("0", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("\"\"", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesNonEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("\"nevermore.\"", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesBinaryOp) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("a+b", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesParenBinaryOp) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("(a+b)", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesUnfinishedOp) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("a+", "<file>", kDefaultPreprocess);
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", true);
}

TEST(AnalyzeVerilogExpressionTest, Unbalanced) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("(a+c", "<file>", kDefaultPreprocess);
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", true);
}

TEST(AnalyzeVerilogExpressionTest, ParsesConcat) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("{cde, fgh, ijk}", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesFunctionCall) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogExpression(
      "average(1, 2, \"five\")", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesMacroCall) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("`MACRO(a+b, 1)", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, ParsesMacroCallWithBadId) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogExpression(
      "`MACRO(a+b, 1bad_id)", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogExpressionTest, RejectsModuleItemAttack) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogExpression("a; wire foo", "<file>", kDefaultPreprocess);
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1))
      << "got: [\n"
      << verible::SequenceFormatter(rejects, "\n") << "\n]";
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", true);
}

// The following tests check that standalone Verilog module-body parsing works.
// More extensive tests are in verilog_parser_unittest.cc.
TEST(AnalyzeVerilogModuleBodyTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogModuleBody("", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesWireDeclarations) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogModuleBody("wire fire;", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesInstance) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogModuleBody(
      "type_of_thing #(16) foo(a, b);", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesInitialBlock) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogModuleBody(
      "initial begin\n"
      "  a = 1;\n"
      "end",
      "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogModuleBodyTest, ParsesMultipleItems) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogModuleBody(
      "wire [7:0] bar;\n"
      "initial begin\n"
      "  a = 1;\n"
      "end",
      "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

// The following tests check for class-body parsing.
TEST(AnalyzeVerilogClassBodyTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogClassBody("", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, ParsesIntegerField) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogClassBody("integer foo;", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, ParsesMethod) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogClassBody(
      "virtual function bar();\nendfunction\n", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, ParsesConstructor) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogClassBody("function new();\nx = 1;\nendfunction : new\n",
                              "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, RejectsModuleItem) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogClassBody(
      "initial begin\nend\n", "<file>", kDefaultPreprocess);
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  const auto &first_reject = rejects.front();
  EXPECT_EQ(first_reject.phase, AnalysisPhase::kParsePhase);
  EXPECT_EQ(first_reject.token_info.text(), "initial");
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", true);
}

// The following tests check for package-body parsing.
TEST(AnalyzeVerilogPackageBodyTest, ParsesEmptyString) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      AnalyzeVerilogPackageBody("", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogPackageBodyTest, ParsesExportDeclaration) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogPackageBody(
      "export foo::bar;\n", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogPackageBodyTest, ParsesParameter) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogPackageBody(
      "parameter int kFoo = 42;\n", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogClassBodyTest, RejectsWireDeclaration) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogClassBody(
      "wire [3:0] bar;\n", "<file>", kDefaultPreprocess);
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  EXPECT_THAT(rejects, SizeIs(1));
  EXPECT_EQ(rejects.front().phase, AnalysisPhase::kParsePhase);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, "<file>", true);
}

TEST(AnalyzeVerilogLibraryMapTest, ParsesLibraryDeclaration) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogLibraryMap(
      "library foo bar/*.v;", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogLibraryMapTest, ParsesLibraryInclude) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr = AnalyzeVerilogLibraryMap(
      "include /foo/bar/?.v;", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

// The following tests verify that parser mode selection works.
TEST(AnalyzeVerilogAutomaticMode, NormalModeEmptyText) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode("", "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, NormalModeModule) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode("module rrr;\nendmodule\n",
                                            "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, StatementsModeSingleDirective) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-statements", "<file>",
          kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, ModuleBodyModeSingleDirective) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-module-body", "<file>",
          kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, ClassBodyModeSingleDirective) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-class-body", "<file>",
          kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, PackageBodyModeSingleDirective) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-package-body", "<file>",
          kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, LibraryMapModeSingleDirective) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-library-map", "<file>",
          kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, SingleDirectiveInvalidSelection) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: does-not-exist-mode", "<file>",
          kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, ExpressionModeSingleDirective) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-expression", "<file>",
          kDefaultPreprocess);
  EXPECT_FALSE(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus().ok());
}

TEST(AnalyzeVerilogAutomaticMode, PropertySpecModeSingleDirective) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-property-spec", "<file>",
          kDefaultPreprocess);
  EXPECT_FALSE(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus().ok());
}

TEST(AnalyzeVerilogAutomaticMode, NormalModeModuleInvalidSelection) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: does-not-exist-mode\n"
          "module rrr;\nendmodule\n",
          "<file>", kDefaultPreprocess);
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
          "<file>", kDefaultPreprocess);
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
          "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, ModuleBodyModeSyntaxError) {
  const absl::string_view filename = "wirefile.sv";
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-module-body\n"
          "wire wire;\n",
          filename, kDefaultPreprocess);
  const auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  EXPECT_FALSE(status.ok());
  DiagnosticMessagesContainFilename(*analyzer_ptr, filename, false);
  DiagnosticMessagesContainFilename(*analyzer_ptr, filename, true);
}

TEST(AnalyzeVerilogAutomaticMode, ClassBodyMode) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-class-body\n"
          "function new();\n"
          "  x = 0;\n"
          "endfunction\n",
          "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, PackageBodyMode) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-package-body\n"
          "export xx::*;\n",
          "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

TEST(AnalyzeVerilogAutomaticMode, PropertySpecMode) {
  std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
      VerilogAnalyzer::AnalyzeAutomaticMode(
          "// verilog_syntax: parse-as-property-spec\n"
          "bb|=>cc\n",
          "<file>", kDefaultPreprocess);
  EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus());
}

// Tests that automatic mode parsing can detect that some first failing
// keywords will trigger (successful) re-parsing as a module-body.
TEST(AnalyzeVerilogAutomaticMode, InferredModuleBodyMode) {
  constexpr const char *test_cases[] = {
      "always @(posedge clk) begin x<=y; end\n",
      "initial begin x = 0; end;\n",
  };
  for (const char *code : test_cases) {
    std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
        VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>",
                                              kDefaultPreprocess);
    EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus()) << "code was:\n"
                                                             << code;
  }
}

TEST(AnalyzeVerilogAutomaticMode, AutomaticWithFallback) {
  static constexpr verilog::VerilogPreprocess::Config kNoBranchFilter{
      .filter_branches = false,
  };
  static constexpr verilog::VerilogPreprocess::Config kWithBranchFilter{
      .filter_branches = true,
  };

  // Test cases that are known to syntax error without branch filter enabled.
  constexpr absl::string_view test_cases[] = {
      R"(
module foo();
  always @(*) begin
    if (a) bar();
`ifdef SOME_MACRO
    else if (b) baz();
`endif
    else qux();
  end
endmodule
)"};
  for (const absl::string_view code : test_cases) {
    const auto should_fail =
        VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>", kNoBranchFilter);
    const auto should_succeed = VerilogAnalyzer::AnalyzeAutomaticMode(
        code, "<file>", kWithBranchFilter);
    // Verify that it will fail in one parse mode, then succeed with branch
    // filtering enabled
    EXPECT_FALSE(should_fail->ParseStatus().ok());
    EXPECT_OK(should_succeed->ParseStatus());

    // Also, fallback should succeed with that.
    const auto with_fallback =
        VerilogAnalyzer::AnalyzeAutomaticPreprocessFallback(code, "<file>");
    EXPECT_OK(should_succeed->ParseStatus());
  }
}

// Tests that automatic mode parsing can detect that some first failing
// keywords will trigger (successful) re-parsing as a library map.
TEST(AnalyzeVerilogAutomaticMode, InferredLibraryMapMode) {
  constexpr const char *test_cases[] = {
      "library foolib bar/*.vg;\n",
      "include bar/*.vg;\n",
      // config_declaration, followed by library declaration
      "config cfg;\n"
      "  design foo.bar;\n"
      "endconfig\n"
      "library foolib bar/*.vg -incdir inky/;\n",
      // config_declaration, followed by library include
      "config cfg;\n"
      "  design foo.bar;\n"
      "endconfig\n"
      "include foo_inc/bar/...;\n",
  };
  for (const char *code : test_cases) {
    std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
        VerilogAnalyzer::AnalyzeAutomaticMode(code, "<file>",
                                              kDefaultPreprocess);
    EXPECT_OK(ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus()) << "code was:\n"
                                                             << code;
  }
}

struct TestCase {
  const char *code;
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
  for (const auto &test : test_cases) {
    std::unique_ptr<VerilogAnalyzer> analyzer_ptr =
        VerilogAnalyzer::AnalyzeAutomaticMode(test.code, "<file>",
                                              kDefaultPreprocess);
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
      VerilogAnalyzer::AnalyzeAutomaticMode(test.code, "<file>",
                                            kDefaultPreprocess);
  auto status = ABSL_DIE_IF_NULL(analyzer_ptr)->ParseStatus();
  ASSERT_FALSE(status.ok());
  const auto &rejects = analyzer_ptr->GetRejectedTokens();
  ASSERT_FALSE(rejects.empty());
  const auto &token_info = rejects.front().token_info;
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that a space macro arg doesn't expand.
TEST(VerilogAnalyzerExpandsMacroArgsTest, SpaceArg) {
  const TokenInfoTestData test = {"  ", {MacroCallId, "`FOOBAR"}, "(      )\n"};
  const auto analyzer =
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that comma-separated blanks don't expand.
TEST(VerilogAnalyzerExpandsMacroArgsTest, CommaSeparatedBlankArg) {
  const TokenInfoTestData test = {"`FOOBAR( ", ',', " )\n"};
  const auto analyzer =
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an integer expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, IntegerArg) {
  const TokenInfoTestData test = {"`FOO(", {TK_DecNumber, "123"}, ")\n"};
  const auto analyzer =
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an identifier expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, IdentifierArg) {
  const TokenInfoTestData test = {"`FOO(", {SymbolIdentifier, "bar"}, ")\n"};
  const auto analyzer =
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an macro id macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, MacroIdentifierArg) {
  const TokenInfoTestData test = {"`FOO(", {MacroIdentifier, "`bar"}, ")\n"};
  const auto analyzer =
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
  const auto search_tokens =
      test.FindImportantTokens(analyzer->Data().Contents());
  ASSERT_EQ(search_tokens.size(), 1);
  EXPECT_TRUE(TreeContainsToken(tree, search_tokens[0]));
}

// Test that an eval string expression macro arg expands properly.
TEST(VerilogAnalyzerExpandsMacroArgsTest, EvalStringArg) {
  const TokenInfoTestData test = {
      "`FOO(", {TK_EvalStringLiteral, "`\"`hello(world)`\""}, ")\n"};
  const auto analyzer =
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
      std::make_unique<VerilogAnalyzer>(test.code, "<<inline>>");
  EXPECT_OK(analyzer->Analyze());
  const ConcreteSyntaxTree &tree = analyzer->SyntaxTree();
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
  for (const auto &test : test_cases) {
    VerilogAnalyzer analyzer(test.first, "<file>", kDefaultPreprocess);
    const auto lexer_status = analyzer.Tokenize();
    EXPECT_OK(lexer_status);
    absl::string_view mode =
        ScanParsingModeDirective(analyzer.Data().TokenStream());
    EXPECT_EQ(mode, test.second) << " mismatched mode with input:\n"
                                 << test.first;
  }
}

}  // namespace
}  // namespace verilog
