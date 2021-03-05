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

// This tests the VerilogLinter class and its associated functions, end-to-end.
//
// Tests for individual lint rules can be found in
// verilog/analysis/checkers/.

#include "verilog/analysis/verilog_linter.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "common/util/file_util.h"
#include "common/util/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "verilog/analysis/default_rules.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_linter_configuration.h"

namespace verilog {
namespace {

using ::testing::EndsWith;
using ::testing::StartsWith;
using verible::file::testing::ScopedTestFile;

class DefaultLinterConfigTestFixture {
 public:
  // Test code using the default rule set.
  DefaultLinterConfigTestFixture() : config_() {
    config_.UseRuleSet(RuleSet::kDefault);
  }

 protected:
  LinterConfiguration config_;
};

class LintOneFileTest : public DefaultLinterConfigTestFixture,
                        public testing::Test {
 public:
  LintOneFileTest() = default;
};

// Tests that nonexistent file is handled as a fatal error.
TEST_F(LintOneFileTest, FileNotFound) {
  std::ostringstream output;
  const int exit_code = LintOneFile(&output, "FileNotFound.sv", config_, true,
                                    false, false, false);
  EXPECT_EQ(exit_code, 2);
}

// Tests that clean code exits 0 (success).
TEST_F(LintOneFileTest, LintCleanFiles) {
  constexpr absl::string_view kTestCases[] = {
      "",  // empty file
      "\n",
      "class foo;\n"
      "endclass : foo\n",
  };
  for (const auto test_code : kTestCases) {
    const ScopedTestFile temp_file(testing::TempDir(), test_code);
    {
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        true, false, false, false);
      EXPECT_EQ(exit_code, 0);
      EXPECT_TRUE(output.str().empty());  // silence
    }
    {  // enable additional error context printing
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        true, false, false, true);
      EXPECT_EQ(exit_code, 0);
      EXPECT_TRUE(output.str().empty());  // silence
    }
  }
}

// Tests that invalid code is handled according to 'parse_fatal' parameter.
TEST_F(LintOneFileTest, SyntaxError) {
  constexpr absl::string_view kTestCases[] = {
      "class foo;\n",                     // no endclass
      "endclass : foo\n",                 // no begin class
      "module 444bad_name; endmodule\n",  // lexical error
  };
  for (const auto test_code : kTestCases) {
    const ScopedTestFile temp_file(testing::TempDir(), test_code);
    {  // continue even with syntax error
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        true, false, false, false);
      EXPECT_EQ(exit_code, 0);
      EXPECT_FALSE(output.str().empty());
    }
    {  // continue even with syntax error with additional error context
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        true, false, false, true);
      EXPECT_EQ(exit_code, 0);
      EXPECT_FALSE(output.str().empty());
    }
    {  // abort on syntax error
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        true, true, false, false);
      EXPECT_EQ(exit_code, 1);
      EXPECT_FALSE(output.str().empty());
    }
    {  // ignore syntax error
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        false, false, false, false);
      EXPECT_EQ(exit_code, 0);
      EXPECT_TRUE(output.str().empty());  // silence
    }
  }
}

TEST_F(LintOneFileTest, LintError) {
  constexpr absl::string_view kTestCases[] = {
      "task automatic foo;\n"
      "  $psprintf(\"blah\");\n"  // forbidden function
      "endtask\n",
  };
  for (const auto test_code : kTestCases) {
    const ScopedTestFile temp_file(testing::TempDir(), test_code);
    {  // continue even with lint error
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        true, false, false, false);
      EXPECT_EQ(exit_code, 0) << "output:\n" << output.str();
      EXPECT_FALSE(output.str().empty());
    }
    {  // abort on lint error
      std::ostringstream output;
      const int exit_code = LintOneFile(&output, temp_file.filename(), config_,
                                        true, false, true, false);
      EXPECT_EQ(exit_code, 1) << "output:\n" << output.str();
      EXPECT_FALSE(output.str().empty());
    }
  }
}

class VerilogLinterTest : public DefaultLinterConfigTestFixture,
                          public testing::Test {
 public:
  // Test code using the default rule set.
  VerilogLinterTest() = default;

 protected:
  // Returns diagnostic text from analyzing source code.
  std::pair<absl::Status, std::string> LintAnalyzeText(
      const std::string& filename, const std::string& content) const {
    // Run the analyzer to produce a syntax tree from source code.
    const auto analyzer = absl::make_unique<VerilogAnalyzer>(content, filename);
    const absl::Status status = ABSL_DIE_IF_NULL(analyzer)->Analyze();
    std::ostringstream diagnostics;
    if (!status.ok()) {
      bool with_diagnostic_contex = false;
      const std::vector<std::string> syntax_error_messages(
          analyzer->LinterTokenErrorMessages(with_diagnostic_contex));
      for (const auto& message : syntax_error_messages) {
        diagnostics << message << std::endl;
      }

      with_diagnostic_contex = true;
      const std::vector<std::string> syntax_error_messages_with_context(
          analyzer->LinterTokenErrorMessages(with_diagnostic_contex));
      for (const auto& message : syntax_error_messages_with_context) {
        diagnostics << message << std::endl;
      }
    }
    // For testing purposes we want the status returned to reflect
    // lint success, so as long as we have a syntax tree (even if there
    // are errors), run the lint checks.
    const absl::Status lint_status = VerilogLintTextStructure(
        &diagnostics, filename, content, config_, analyzer->Data(), false);
    return {lint_status, diagnostics.str()};
  }
};

// This test verifies that VerilogLintTextStructure runs on an empty tree.
TEST_F(VerilogLinterTest, AnonymousEmptyTree) {
  const auto diagnostics = LintAnalyzeText("", "");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// that is lint-clean.
TEST_F(VerilogLinterTest, NoLintViolation) {
  const auto diagnostics = LintAnalyzeText("good.sv",
                                           "task automatic foo;\n"
                                           "  $display(\"blah\");\n"
                                           "endtask\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one syntax tree lint finding.
TEST_F(VerilogLinterTest, KnownTreeLintViolation) {
  const auto diagnostics = LintAnalyzeText("bad.sv",
                                           "task automatic foo;\n"
                                           "  $psprintf(\"blah\");\n"
                                           "endtask\n");
  EXPECT_TRUE(diagnostics.first.ok());
  const auto expected =
      "bad.sv:2:3: $psprintf is a forbidden system function "
      "or task, please use $sformatf instead";
  EXPECT_THAT(diagnostics.second, StartsWith(expected));
  EXPECT_THAT(diagnostics.second, EndsWith("[invalid-system-task-function]\n"));
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one syntax tree lint finding that is waived (next-line).
TEST_F(VerilogLinterTest, KnownTreeLintViolationWaivedNextLine) {
  const auto diagnostics =
      LintAnalyzeText("bad.sv",
                      "task automatic foo;\n"
                      "  // verilog_lint: waive invalid-system-task-function\n"
                      "  $psprintf(\"blah\");\n"
                      "endtask\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one syntax tree lint finding that is waived (same-line).
TEST_F(VerilogLinterTest, KnownTreeLintViolationWaivedSameLine) {
  const auto diagnostics =
      LintAnalyzeText("bad.sv",
                      "task automatic foo;\n"
                      "  $psprintf(\"blah\");  // verilog_lint: waive "
                      "invalid-system-task-function\n"
                      "endtask\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one syntax tree lint finding that is waived (line-range).
TEST_F(VerilogLinterTest, KnownTreeLintViolationWaivedLineRange) {
  const auto diagnostics = LintAnalyzeText(
      "bad.sv",
      "task automatic foo;\n"
      "  // verilog_lint: waive-start invalid-system-task-function\n"
      "  $psprintf(\"blah\");\n"
      "  // verilog_lint: waive-end invalid-system-task-function\n"
      "endtask\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one token stream lint finding.
TEST_F(VerilogLinterTest, KnownTokenStreamLintViolation) {
  using analysis::kDefaultRuleSet;
  // TODO(fangism): Remove this conditional check or choose a different
  // token-stream based lint rule that is enabled by default.
  // (Just so happens that we may not want endif-comment enabled by default.)
  if (std::find(std::begin(kDefaultRuleSet), std::end(kDefaultRuleSet),
                "endif-comment") != std::end(kDefaultRuleSet)) {
    const auto diagnostics = LintAnalyzeText("endif.sv",
                                             "`ifdef SIM\n"
                                             "module foo;\n"
                                             "endmodule\n"
                                             "`endif\n");
    static const auto expect_message =
        "endif.sv:4:1: `endif should be followed on the same line by a "
        "comment that matches the opening `ifdef/`ifndef. (SIM) ";
    EXPECT_TRUE(diagnostics.first.ok());
    EXPECT_THAT(diagnostics.second, StartsWith(expect_message));
    EXPECT_THAT(diagnostics.second, EndsWith("[endif-comment]\n"));
  }
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one line-lint-rule finding.
TEST_F(VerilogLinterTest, KnownLineLintViolation) {
  const auto diagnostics = LintAnalyzeText("tab.sv",
                                           "`include \"blah.svh\";\n"
                                           "\n"
                                           "module\ttab;\n"  // bad tab here
                                           "endmodule\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_THAT(diagnostics.second,
              StartsWith("tab.sv:3:7: Use spaces, not tabs."));
  EXPECT_THAT(diagnostics.second, EndsWith("[no-tabs]\n"));
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one text structure lint rule finding (line-length).
TEST_F(VerilogLinterTest, KnownTextStructureLintViolation) {
  const auto diagnostics = LintAnalyzeText(
      "long.sv",
      "module long;\n"
      "initial xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx = "
      "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy[777777777777];\n"
      "endmodule\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_THAT(diagnostics.second,
              StartsWith("long.sv:2:101: Line length exceeds "
                         "max: 100; is: 114"));
  EXPECT_THAT(diagnostics.second, EndsWith("[line-length]\n"));
}

// This test verifies that VerilogLintTextStructure runs on complete source,
// with one waived lint rule finding (line-length).
TEST_F(VerilogLinterTest, KnownTextStructureLintViolationWaived) {
  const auto diagnostics = LintAnalyzeText(
      "long.sv",
      "module long;\n"
      "initial xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx = "
      "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy[777777777777];  "
      "// verilog_lint: waive line-length\n"
      "endmodule\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

// This test verifies that VerilogLintTextStructure runs on a module-body
// source, with one lint rule finding (line-length).
TEST_F(VerilogLinterTest, ModuleBodyLineLength) {
  const auto diagnostics = LintAnalyzeText(
      "module-body.sv",
      "// verilog_syntax: parse-as-module-body\n"
      "\n"
      "initial xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx = "
      "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy[777777777777];\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_THAT(diagnostics.second,
              StartsWith("module-body.sv:3:101: Line length exceeds max: "));
  EXPECT_THAT(diagnostics.second, EndsWith("[line-length]\n"));
}

// This test verifies that VerilogLintTextStructure runs on a module-body
// source, with one waived lint rule finding (line-length).
TEST_F(VerilogLinterTest, ModuleBodyLineLengthWaived) {
  const auto diagnostics = LintAnalyzeText(
      "module-body.sv",
      "// verilog_syntax: parse-as-module-body\n"
      "\n"
      "initial xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx = "
      "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy[777777777777];  "
      "// verilog_lint: waive line-length\n");
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

TEST_F(VerilogLinterTest, MultiByteUTF8CharactersAreOnlyCountedOnce) {
  // Typical comment that might be found in verilog: some ASCII-art diagram
  // except that the 'ˉ'-'overscore' is actually a two-byte UTF8 character.
  constexpr char comment_with_utf8[] =
      "module utf8_short;\n"
      R"(initial a = 42; // __/ˉˉˉˉˉˉˉˉˉ\___/ˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉˉ\___/ˉˉˉˉˉ)"
      "\nendmodule\n";

  const auto diagnostics = LintAnalyzeText("utf8_short.sv", comment_with_utf8);
  EXPECT_TRUE(diagnostics.first.ok());
  EXPECT_EQ(diagnostics.second, "");
}

TEST(VerilogLinterDocumentationTest, AllRulesHelpDescriptions) {
  std::ostringstream stream;
  verilog::GetLintRuleDescriptionsHelpFlag(&stream, "all");
  // Spot-check a few patterns, must mostly make sure generation
  // works without any fatal errors.
  EXPECT_TRUE(absl::StrContains(stream.str(), "line-length"));
  EXPECT_TRUE(absl::StrContains(stream.str(), "posix-eof"));
  EXPECT_TRUE(absl::StrContains(stream.str(), "Enabled by default:"));
}

TEST(VerilogLinterDocumentationTest, AllRulesMarkdown) {
  std::ostringstream stream;
  verilog::GetLintRuleDescriptionsMarkdown(&stream);
  // Spot-check a few patterns, must mostly make sure generation
  // works without any fatal errors.
  EXPECT_TRUE(absl::StrContains(stream.str(), "line-length"));
  EXPECT_TRUE(absl::StrContains(stream.str(), "posix-eof"));
  EXPECT_TRUE(absl::StrContains(stream.str(), "Enabled by default:"));
}

}  // namespace
}  // namespace verilog
