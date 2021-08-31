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

// verilog_lint is a command-line utility to check Verilog syntax
// and style compliance for the given file.
//
// Example usage:
// verilog_lint files...

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/util/enum_flags.h"
#include "common/util/file_util.h"
#include "common/util/init_command_line.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "verilog/analysis/verilog_linter.h"
#include "verilog/analysis/verilog_linter_configuration.h"

// Autofix mode
//   kNo           disable autofixes
//   kYes          apply all autofixes
//   kInteractive  ask what to do on each violation with autofix available
enum class AutofixMode { kNo, kYes, kInteractive };

static const verible::EnumNameMap<AutofixMode> kAutofixModeEnumStringMap = {
    {"no", AutofixMode::kNo},
    {"yes", AutofixMode::kYes},
    {"interactive", AutofixMode::kInteractive},
};

std::ostream& operator<<(std::ostream& stream, AutofixMode mode) {
  return kAutofixModeEnumStringMap.Unparse(mode, stream);
}

std::string AbslUnparseFlag(const AutofixMode& mode) {
  std::ostringstream stream;
  kAutofixModeEnumStringMap.Unparse(mode, stream);
  return stream.str();
}

bool AbslParseFlag(absl::string_view text, AutofixMode* mode,
                   std::string* error) {
  return kAutofixModeEnumStringMap.Parse(text, mode, error, "--autofix value");
}

// LINT.IfChange

ABSL_FLAG(bool, check_syntax, true,
          "If true, check for lexical and syntax errors, otherwise ignore.");
ABSL_FLAG(bool, parse_fatal, true,
          "If true, exit nonzero if there are any syntax errors.");
ABSL_FLAG(bool, lint_fatal, true,
          "If true, exit nonzero if linter finds violations.");
ABSL_FLAG(std::string, help_rules, "",
          "[all|<rule-name>], print the description of one rule/all rules "
          "and exit immediately.");
ABSL_FLAG(
    bool, generate_markdown, false,
    "If true, print the description of every rule formatted for the "
    "Markdown and exit immediately. Intended for the output to be written "
    "to a snippet of Markdown.");

ABSL_FLAG(bool, show_diagnostic_context, false,
          "prints an additional "
          "line on which the diagnostic was found,"
          "followed by a line with a position marker");

ABSL_FLAG(AutofixMode, autofix, AutofixMode::kNo,
          "[yes|no|interactive], autofix mode.");
ABSL_FLAG(std::string, autofix_output_file, "",
          "File to write a patch with autofixes to. If not set autofixes are "
          "applied directly to the analyzed file. Relevant only when "
          "--autofix option is enabled.");
ABSL_FLAG(std::string, lint_rule_citations, "",
          "Path to lint rule citations to overwrite. "
          "Please refer to the README file for information about its format.");
// LINT.ThenChange(README.md)

using verilog::CustomCitationMap;
using verilog::LinterConfiguration;

// LintOneFile returns 0, 1, or 2
static const int kAutofixErrorExitStatus = 3;

int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  std::string custom_citations_file = absl::GetFlag(FLAGS_lint_rule_citations);
  std::string content;
  CustomCitationMap citations;
  if (!custom_citations_file.empty()) {
    const absl::Status config_read_status =
        verible::file::GetContents(custom_citations_file, &content);
    if (!config_read_status.ok()) return -1;
    citations = verilog::ParseCitations(content);
  }

  std::string help_flag = absl::GetFlag(FLAGS_help_rules);
  if (!help_flag.empty()) {
    verilog::GetLintRuleDescriptionsHelpFlag(&std::cout, help_flag, citations);
    return 0;
  }

  // In documentation generation mode, print documentation and exit immediately.
  bool generate_markdown_flag = absl::GetFlag(FLAGS_generate_markdown);
  if (generate_markdown_flag) {
    verilog::GetLintRuleDescriptionsMarkdown(&std::cout, citations);
    return 0;
  }

  int exit_status = 0;

  AutofixMode autofix_mode = absl::GetFlag(FLAGS_autofix);
  const std::string autofix_output_file =
      absl::GetFlag(FLAGS_autofix_output_file);

  std::unique_ptr<std::ostream> autofix_output_stream;
  if (autofix_mode != AutofixMode::kNo && !autofix_output_file.empty()) {
    autofix_output_stream.reset(new std::ofstream(autofix_output_file));
    if (!autofix_output_stream->good()) {
      LOG(ERROR) << "Failed to create/open output patch file: "
                 << autofix_output_file;
      LOG(WARNING) << "Disabling autofixing.";
      autofix_output_stream.reset();
      autofix_mode = AutofixMode::kNo;
      exit_status = kAutofixErrorExitStatus;
    }
  }

  std::unique_ptr<verilog::ViolationHandler> violation_handler;
  switch (autofix_mode) {
    case AutofixMode::kNo:
      violation_handler.reset(new verilog::ViolationPrinter(&std::cout));
      break;
    case AutofixMode::kYes:
      violation_handler.reset(new verilog::ViolationFixer(
          &std::cout, autofix_output_stream.get(),
          [](const verible::LintViolation&, absl::string_view) {
            return verilog::ViolationFixer::AnswerChoice::kApplyAll;
          }));
      break;
    case AutofixMode::kInteractive:
      violation_handler.reset(
          new verilog::ViolationFixer(&std::cout, autofix_output_stream.get()));
      break;
  }

  // All positional arguments are file names.  Exclude program name.
  for (const absl::string_view filename :
       verible::make_range(args.begin() + 1, args.end())) {
    // Copy configuration, so that it can be locally modified per file.
    const LinterConfiguration config(
        verilog::LinterConfigurationFromFlags(filename));

    const int lint_status = verilog::LintOneFile(
        &std::cout, filename, config, violation_handler.get(),
        absl::GetFlag(FLAGS_check_syntax), absl::GetFlag(FLAGS_parse_fatal),
        absl::GetFlag(FLAGS_lint_fatal),
        absl::GetFlag(FLAGS_show_diagnostic_context));
    exit_status = std::max(lint_status, exit_status);
  }  // for each file

  return exit_status;
}
