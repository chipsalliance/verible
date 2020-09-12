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

#include <iostream>
#include <memory>
#include <sstream>  // IWYU pragma: keep  // for ostringstream
#include <string>   // for string, allocator, etc
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/util/init_command_line.h"
#include "common/util/logging.h"  // for operator<<, LOG, LogMessage, etc
#include "verilog/analysis/verilog_linter.h"
#include "verilog/analysis/verilog_linter_configuration.h"

// Reminder: The linter service expects the program to return 0 unless
// there is a fatal error, regardless of parse/lint status.
// The following flags should only be used in contexts where returning
// nonzero is applicable (e.g. running as a presubmit test, or a user wants it).
ABSL_FLAG(bool, parse_fatal, false,
          "If true, exit nonzero if there are any syntax errors.");
ABSL_FLAG(bool, lint_fatal, false,
          "If true, exit nonzero if linter finds violations.");
ABSL_FLAG(std::string, help_rules, "",
          "[all|<rule-name>], print the description of one rule/all rules "
          "and exit immediately.");
ABSL_FLAG(
    bool, generate_markdown, false,
    "If true, print the description of every rule formatted for the "
    "markdown and exit immediately. Intended for the output to be written "
    "to a snippet of markdown.");

using verilog::LinterConfiguration;

int main(int argc, char** argv) {
  const auto usage =
      absl::StrCat("usage: ", argv[0], " [options] <file> [<file>...]");
  const auto args = verible::InitCommandLine(usage, &argc, &argv);

  std::string help_flag = absl::GetFlag(FLAGS_help_rules);
  if (!help_flag.empty()) {
    verilog::GetLintRuleDescriptionsHelpFlag(&std::cout, help_flag);
    return 0;
  }

  // In documentation generation mode, print documentation and exit immediately.
  bool generate_markdown_flag = absl::GetFlag(FLAGS_generate_markdown);
  if (generate_markdown_flag) {
    verilog::GetLintRuleDescriptionsMarkdown(&std::cout);
    return 0;
  }

  int exit_status = 0;
  // All positional arguments are file names.  Exclude program name.
  for (const auto filename :
       verible::make_range(args.begin() + 1, args.end())) {
    // Copy configuration, so that it can be locally modified per file.
    const LinterConfiguration config(
        verilog::LinterConfigurationFromFlags(filename));

    const int lint_status = verilog::LintOneFile(
        &std::cout, filename, config, absl::GetFlag(FLAGS_parse_fatal),
        absl::GetFlag(FLAGS_lint_fatal));
    exit_status = std::max(lint_status, exit_status);
  }  // for each file

  return exit_status;
}
