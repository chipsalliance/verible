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
#include <array>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/violation_handler.h"
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
using verible::ViolationFixer;
using verible::ViolationPrinter;
using verible::file::GetContentAsString;
using verible::file::testing::ScopedTestFile;

class DefaultLinterConfigTestFixture {
 public:
  // Test code using the default rule set.
  DefaultLinterConfigTestFixture() { config_.UseRuleSet(RuleSet::kDefault); }

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
  ViolationPrinter violation_printer(&output);
  const int exit_code =
      LintOneFile(&output, "FileNotFound.sv", config_, &violation_printer, true,
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
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, true, false, false, false);
      EXPECT_EQ(exit_code, 0);
      EXPECT_TRUE(output.str().empty());  // silence
    }
    {  // enable additional error context printing
      std::ostringstream output;
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, true, false, false, true);
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
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, true, false, false, false);
      EXPECT_EQ(exit_code, 0);
      EXPECT_FALSE(output.str().empty());
    }
    {  // continue even with syntax error with additional error context
      std::ostringstream output;
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, true, false, false, true);
      EXPECT_EQ(exit_code, 0);
      EXPECT_FALSE(output.str().empty());
    }
    {  // abort on syntax error
      std::ostringstream output;
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, true, true, false, false);
      EXPECT_EQ(exit_code, 1);
      EXPECT_FALSE(output.str().empty());
    }
    {  // ignore syntax error
      std::ostringstream output;
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, false, false, false, false);
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
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, true, false, false, false);
      EXPECT_EQ(exit_code, 0) << "output:\n" << output.str();
      EXPECT_FALSE(output.str().empty());
    }
    {  // abort on lint error
      std::ostringstream output;
      ViolationPrinter violation_printer(&output);
      const int exit_code =
          LintOneFile(&output, temp_file.filename(), config_,
                      &violation_printer, true, false, true, false);
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
      absl::string_view filename, absl::string_view content) const {
    // Run the analyzer to produce a syntax tree from source code.
    const auto analyzer = std::make_unique<VerilogAnalyzer>(content, filename);
    const absl::Status status = ABSL_DIE_IF_NULL(analyzer)->Analyze();
    std::ostringstream diagnostics;
    if (!status.ok()) {
      bool with_diagnostic_contex = false;
      const std::vector<std::string> syntax_error_messages(
          analyzer->LinterTokenErrorMessages(with_diagnostic_contex));
      for (const auto &message : syntax_error_messages) {
        diagnostics << message << std::endl;
      }

      with_diagnostic_contex = true;
      const std::vector<std::string> syntax_error_messages_with_context(
          analyzer->LinterTokenErrorMessages(with_diagnostic_contex));
      for (const auto &message : syntax_error_messages_with_context) {
        diagnostics << message << std::endl;
      }
    }

    const auto &text_structure = analyzer->Data();

    // For testing purposes we want the status returned to reflect
    // lint success, so as long as we have a syntax tree (even if there
    // are errors), run the lint checks.
    const absl::StatusOr<std::vector<verible::LintRuleStatus>> lint_result =
        VerilogLintTextStructure(filename, config_, text_structure);
    verilog::ViolationPrinter violation_printer(&diagnostics);
    const std::set<verible::LintViolationWithStatus> violations =
        GetSortedViolations(lint_result.value());
    violation_printer.HandleViolations(violations, text_structure.Contents(),
                                       filename);
    return {lint_result.status(), diagnostics.str()};
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
      "bad.sv:2:3-11: $psprintf is a forbidden system function "
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
              StartsWith("long.sv:2:101-114: Line length exceeds "
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
  EXPECT_THAT(
      diagnostics.second,
      StartsWith("module-body.sv:3:101-114: Line length exceeds max: "));
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

TEST(VerilogLinterDocumentationTest, PrintLintRuleFile) {
  auto config_or = verilog::LinterConfigurationFromFlags("");
  ASSERT_TRUE(config_or.ok());

  const LinterConfiguration &config = *config_or;

  // Generate Line Rule File
  std::ostringstream stream;
  verilog::GetLintRuleFile(&stream, config);

  std::string generated_default_rules_str = stream.str();

  // Spot-check a few patterns, must mostly make sure generation
  // works without any fatal errors.
  // NOTE: This will break if/when the rules change so this part
  // of the test is not ideal.
  EXPECT_THAT(generated_default_rules_str, testing::HasSubstr("always-comb"));
  EXPECT_THAT(
      generated_default_rules_str,
      testing::HasSubstr("module-filename=allow-dash-for-underscore:false"));
  EXPECT_THAT(generated_default_rules_str,
              testing::HasSubstr("-forbid-negative-array-dim"));

  // Roundtrip test, first parse the rules
  absl::StatusOr<std::string> generated_default_rules_or =
      generated_default_rules_str;
  RuleBundle parsed_rule_bundle;
  std::string error;
  parsed_rule_bundle.ParseConfiguration(*generated_default_rules_or, '\n',
                                        &error);
  EXPECT_TRUE(error.empty());

  // Convert the parsed_rule_bundle back to a string
  std::string unparsed_rule_bundle =
      parsed_rule_bundle.UnparseConfiguration('\n', false);

  // compare the unparsed_rule_bundle to the generated_default_rules_str
  // NOTE: Triming the extra extra white space from the generated output
  // as it has some extra return characters to make things look pretty on print
  EXPECT_EQ(unparsed_rule_bundle,
            std::string(absl::StripTrailingAsciiWhitespace(
                generated_default_rules_str)));
}

class ViolationFixerTest : public testing::Test {
 public:
  ViolationFixerTest() {
    config_.UseRuleSet(RuleSet::kNone);
    config_.TurnOn("forbid-consecutive-null-statements");
    config_.TurnOn("no-trailing-spaces");
    config_.TurnOn("posix-eof");
  }

 protected:
  LinterConfiguration config_;

  absl::Status LintAnalyzeFixText(absl::string_view content,
                                  ViolationFixer *violation_fixer,
                                  std::string *fixed_content) const {
    const ScopedTestFile temp_file(testing::TempDir(), content);

    // Run the analyzer to produce a syntax tree from source code.
    const auto analyzer =
        std::make_unique<VerilogAnalyzer>(content, temp_file.filename());
    const absl::Status status = ABSL_DIE_IF_NULL(analyzer)->Analyze();

    const auto &text_structure = analyzer->Data();

    const absl::StatusOr<std::vector<verible::LintRuleStatus>> lint_result =
        VerilogLintTextStructure(temp_file.filename(), config_, text_structure);

    const std::set<verible::LintViolationWithStatus> violations =
        GetSortedViolations(lint_result.value());
    violation_fixer->HandleViolations(violations, text_structure.Contents(),
                                      temp_file.filename());

    if (auto content_or = GetContentAsString(temp_file.filename());
        content_or.ok()) {
      *fixed_content = std::move(*content_or);
    }
    return lint_result.status();
  }

  void DoFixerTest(
      std::initializer_list<ViolationFixer::Answer> choices,
      std::initializer_list<absl::string_view> expected_fixed_sources) const {
    static constexpr std::array<const absl::string_view, 3> input_sources{
        // Input source 0:
        // :2:10: no-trailing-spaces
        // :3:10: forbid-consecutive-null-statements
        // :4:10: forbid-consecutive-null-statements
        // :4:11: no-trailing-spaces
        // :5:10: forbid-consecutive-null-statements
        // :6:10: forbid-consecutive-null-statements
        // :7:10: no-trailing-spaces
        // :7:14: posix-eof
        "module Autofix;    \n"
        "  wire a;;\n"
        "  wire b;;  \n"
        "  wire c;;\n"
        "  wire d;;\n"
        "endmodule    ",

        // Input source 1:
        // (no issues)
        "module AutofixTwo;\n"
        "endmodule\n",

        // Input source 2:
        // :1:21: forbid-consecutive-null-statements
        // :2:10: no-trailing-spaces
        "module AutofixThree;;\n"
        "  wire a;   \n"
        "endmodule\n",
    };
    EXPECT_EQ(expected_fixed_sources.size(), input_sources.size());

    std::initializer_list<ViolationFixer::Answer>::iterator choice_it;
    const ViolationFixer::AnswerChooser answer_chooser =
        [&choice_it, &choices](const verible::LintViolation &,
                               absl::string_view) {
          EXPECT_NE(choice_it, choices.end())
              << "AnswerChooser called more times than expected.";
          return *choice_it++;
        };

    // In-place fixing
    {
      choice_it = choices.begin();
      // intentionally unopened, diagnostics are discarded
      std::ofstream diagnostics;
      ViolationFixer violation_fixer(&diagnostics, nullptr, answer_chooser);
      std::vector<std::string> fixed_sources(input_sources.size());

      for (size_t i = 0; i < input_sources.size(); ++i) {
        const absl::string_view input_source = input_sources[i];
        std::string &fixed_source = fixed_sources[i];

        const absl::Status status =
            LintAnalyzeFixText(input_source, &violation_fixer, &fixed_source);
        EXPECT_TRUE(status.ok());
      }

      EXPECT_EQ(choice_it, choices.end())
          << "AnswerChooser called fewer times than expected.";

      for (size_t i = 0; i < input_sources.size(); ++i) {
        const std::string &fixed_source = fixed_sources[i];
        const absl::string_view expected_fixed_source =
            *(expected_fixed_sources.begin() + i);

        EXPECT_EQ(fixed_source, expected_fixed_source);
      }
    }

    // Patch generation
    {
      choice_it = choices.begin();
      // intentionally unopened, diagnostics are discarded
      std::ofstream diagnostics;
      std::ostringstream patch;
      ViolationFixer violation_fixer(&diagnostics, &patch, answer_chooser);
      std::vector<std::string> fixed_sources(input_sources.size());

      for (size_t i = 0; i < input_sources.size(); ++i) {
        const absl::string_view input_source = input_sources[i];
        std::string &fixed_source = fixed_sources[i];

        const absl::Status status =
            LintAnalyzeFixText(input_source, &violation_fixer, &fixed_source);
        EXPECT_TRUE(status.ok());
      }

      EXPECT_EQ(choice_it, choices.end())
          << "AnswerChooser called fewer times than expected.";

      bool expect_empty_patch = true;

      for (size_t i = 0; i < input_sources.size(); ++i) {
        const absl::string_view input_source = input_sources[i];
        const std::string &fixed_source = fixed_sources[i];
        const absl::string_view expected_fixed_source =
            *(expected_fixed_sources.begin() + i);

        EXPECT_EQ(input_source, fixed_source);
        if (input_source != expected_fixed_source) {
          expect_empty_patch = false;
        }
      }

      EXPECT_EQ(patch.str().empty(), expect_empty_patch);
    }
  }
};

TEST_F(ViolationFixerTest, ApplyAll) {
  DoFixerTest(
      {
          {ViolationFixer::AnswerChoice::kApplyAll, 0},
      },
      {
          "module Autofix;\n"
          "  wire a;\n"
          "  wire b;\n"
          "  wire c;\n"
          "  wire d;\n"
          "endmodule\n",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;\n"
          "  wire a;\n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, RejectAll) {
  DoFixerTest(
      {
          {ViolationFixer::AnswerChoice::kRejectAll, 0},
      },
      {
          "module Autofix;    \n"
          "  wire a;;\n"
          "  wire b;;  \n"
          "  wire c;;\n"
          "  wire d;;\n"
          "endmodule    ",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;;\n"
          "  wire a;   \n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, Reject) {
  DoFixerTest(
      {
          {ViolationFixer::AnswerChoice::kReject, 0},
          {ViolationFixer::AnswerChoice::kApplyAll, 0},
      },
      {
          "module Autofix;    \n"
          "  wire a;\n"
          "  wire b;\n"
          "  wire c;\n"
          "  wire d;\n"
          "endmodule\n",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;\n"
          "  wire a;\n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, Apply) {
  DoFixerTest(
      {
          {ViolationFixer::AnswerChoice::kApply, 0},
          {ViolationFixer::AnswerChoice::kRejectAll, 0},
      },
      {
          "module Autofix;\n"
          "  wire a;;\n"
          "  wire b;;  \n"
          "  wire c;;\n"
          "  wire d;;\n"
          "endmodule    ",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;;\n"
          "  wire a;   \n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, ApplyAllForRule) {
  DoFixerTest(
      {
          // Input source 0:
          // :2:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kApplyAllForRule},
          // :3:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          // :4:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          // :4:11: no-trailing-spaces
          // AUTOMATICALLY APPLIED due to kApplyAllForRule
          // :5:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          // :6:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          // :7:10: no-trailing-spaces
          // AUTOMATICALLY APPLIED due to kApplyAllForRule
          // :7:14: posix-eof
          {ViolationFixer::AnswerChoice::kReject},
          // Input source 2:
          // :1:21: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          // :2:10: no-trailing-spaces
          // AUTOMATICALLY APPLIED due to kApplyAllForRule
      },
      {
          "module Autofix;\n"
          "  wire a;;\n"
          "  wire b;;\n"
          "  wire c;;\n"
          "  wire d;;\n"
          "endmodule",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;;\n"
          "  wire a;\n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, RejectAllForRule) {
  DoFixerTest(
      {
          // Input source 0:
          // :2:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kRejectAllForRule},
          // :3:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          // :4:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          // :4:11: no-trailing-spaces
          // AUTOMATICALLY REJECTED due to kApplyAllForRule
          // :5:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          // :6:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          // :7:10: no-trailing-spaces
          // AUTOMATICALLY REJECTED due to kApplyAllForRule
          // :7:14: posix-eof
          {ViolationFixer::AnswerChoice::kApply},
          // Input source 2:
          // :1:21: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          // :2:10: no-trailing-spaces
          // AUTOMATICALLY REJECTED due to kApplyAllForRule
      },
      {
          "module Autofix;    \n"
          "  wire a;\n"
          "  wire b;  \n"
          "  wire c;\n"
          "  wire d;\n"
          "endmodule    \n",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;\n"
          "  wire a;   \n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, RejectAllForRuleApplyAllForRule) {
  DoFixerTest(
      {
          // Input source 0:
          // :2:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kRejectAllForRule},
          // :3:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApplyAllForRule},
          // :4:10: forbid-consecutive-null-statements
          // AUTOMATICALLY APPLIED due to kApplyAllForRule
          // :4:11: no-trailing-spaces
          // AUTOMATICALLY REJECTED due to kApplyAllForRule
          // :5:10: forbid-consecutive-null-statements
          // AUTOMATICALLY APPLIED due to kApplyAllForRule
          // :6:10: forbid-consecutive-null-statements
          // AUTOMATICALLY APPLIED due to kApplyAllForRule
          // :7:10: no-trailing-spaces
          // AUTOMATICALLY REJECTED due to kApplyAllForRule
          // :7:14: posix-eof
          {ViolationFixer::AnswerChoice::kReject},
          // Input source 2:
          // :1:21: forbid-consecutive-null-statements
          // AUTOMATICALLY APPLIED due to kApplyAllForRule
          // :2:10: no-trailing-spaces
          // AUTOMATICALLY REJECTED due to kApplyAllForRule
      },
      {
          "module Autofix;    \n"
          "  wire a;\n"
          "  wire b;  \n"
          "  wire c;\n"
          "  wire d;\n"
          "endmodule    ",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;\n"
          "  wire a;   \n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, PrintFix) {
  // Just checks that kPrintFix doesn't affect choices
  DoFixerTest(
      {
          // Input source 0:
          // :2:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintFix},
          {ViolationFixer::AnswerChoice::kPrintFix},
          // :3:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          // :4:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          {ViolationFixer::AnswerChoice::kPrintFix},
          // :4:11: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintFix},
          // :5:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          // :6:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintFix},
          {ViolationFixer::AnswerChoice::kPrintFix},
          // :7:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kApply},
          // :7:14: posix-eof
          {ViolationFixer::AnswerChoice::kReject},
          {ViolationFixer::AnswerChoice::kPrintFix},
          // Input source 2:
          // :1:21: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintFix},
          // :2:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kReject},
      },
      {
          "module Autofix;\n"
          "  wire a;;\n"
          "  wire b;;\n"
          "  wire c;\n"
          "  wire d;\n"
          "endmodule",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;\n"
          "  wire a;   \n"
          "endmodule\n",
      });
}

TEST_F(ViolationFixerTest, PrintAppliedFixes) {
  // Just checks that kPrintAppliedFixes doesn't affect choices
  DoFixerTest(
      {
          // Input source 0:
          // :2:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          // :3:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kReject},
          // :4:10: forbid-consecutive-null-staments
          {ViolationFixer::AnswerChoice::kReject},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          // :4:11: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          // :5:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          // :6:10: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          // :7:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kApply},
          // :7:14: posix-eof
          {ViolationFixer::AnswerChoice::kReject},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          // Input source 2:
          // :1:21: forbid-consecutive-null-statements
          {ViolationFixer::AnswerChoice::kApply},
          {ViolationFixer::AnswerChoice::kPrintAppliedFixes},
          // :2:10: no-trailing-spaces
          {ViolationFixer::AnswerChoice::kReject},
      },
      {
          "module Autofix;\n"
          "  wire a;;\n"
          "  wire b;;\n"
          "  wire c;\n"
          "  wire d;\n"
          "endmodule",

          "module AutofixTwo;\n"
          "endmodule\n",

          "module AutofixThree;\n"
          "  wire a;   \n"
          "endmodule\n",
      });
}

}  // namespace
}  // namespace verilog
