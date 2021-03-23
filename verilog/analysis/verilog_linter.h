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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/analysis/line_linter.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/lint_waiver.h"
#include "common/analysis/syntax_tree_linter.h"
#include "common/analysis/text_structure_linter.h"
#include "common/analysis/token_stream_linter.h"
#include "common/strings/line_column_map.h"
#include "common/text/text_structure.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/analysis/verilog_linter_configuration.h"

namespace verilog {

// Checks a single file for Verilog style lint violations.
// This is suitable for calling from main().
// Diagnostics are printed to 'stream'.
// 'filename' is the path to the file to analyze.
// 'config' controls lint rules for analysis.
// If 'check_syntax' is true, report lexical and syntax errors.
// If 'parse_fatal' is true, abort after encountering syntax errors, else
// continue to analyze the salvaged code structure.
// If 'lint_fatal' is true, exit nonzero on finding lint violations.
// Returns an exit_code like status where 0 means success, 1 means some
// errors were found (syntax, lint), and anything else is a fatal error.
int LintOneFile(std::ostream* stream, absl::string_view filename,
                const LinterConfiguration& config, bool check_syntax,
                bool parse_fatal, bool lint_fatal, bool show_context = false);

// VerilogLinter analyzes a TextStructureView of Verilog source code.
// This uses syntax-tree based analyses and lexical token-stream analyses.
class VerilogLinter {
 public:
  VerilogLinter();

  // Configures the internal linters, enabling select rules.
  absl::Status Configure(const LinterConfiguration& configuration,
                         absl::string_view lintee_filename);

  // Analyzes text structure.
  void Lint(const verible::TextStructureView& text_structure,
            absl::string_view filename);

  // Reports lint findings.
  std::vector<verible::LintRuleStatus> ReportStatus(
      const verible::LineColumnMap&, absl::string_view text_base);

 private:
  // Line based linter.
  verible::LineLinter line_linter_;

  // Token-based linter.
  verible::TokenStreamLinter token_stream_linter_;

  // Syntax-tree based linter.
  verible::SyntaxTreeLinter syntax_tree_linter_;

  // TextStructure-based linter.
  verible::TextStructureLinter text_structure_linter_;

  // Tracks the set of waived lines per rule.
  verible::LintWaiverBuilder lint_waiver_;
};

// Creates a linter configuration from global flags.
// If --rules_config_search is configured, uses the given
// start file to look up the directory chain.
LinterConfiguration LinterConfigurationFromFlags(
    absl::string_view linting_start_file = ".");

// Expands linter configuration from a text file
absl::Status AppendLinterConfigurationFromFile(
    LinterConfiguration* config, absl::string_view config_filename);

// VerilogLintTextStructure analyzes Verilog syntax tree for style violations
// and syntactically detectable pitfalls.
//
// The configuration of this function is controlled by flags:
//   FLAGS_ruleset, FLAGS_rules.
//
// Args:
//   stream: the output stream where diagnostics are captured.
//     Writing anything to this stream means that the input contains
//     some lint violation.
//   filename: (optional) name of input file, that can appear in logs.
//   contents: source text that corresponds to the syntax tree argument,
//     and its sole purpose is to provide a translation from byte-offset to
//     line-column in diagnostics.
//   text_structure: contains the syntax tree that will be lint-analyzed.
//   show_context: print additional line with vulnerable code
//
// Returns:
//   absl::Status that reflects whether linter linter ran successfully.
absl::Status VerilogLintTextStructure(
    std::ostream* stream, const std::string& filename,
    const std::string& contents, const LinterConfiguration& config,
    const verible::TextStructureView& text_structure,
    bool show_context = false);

// Prints the rule, description and default_enabled.
absl::Status PrintRuleInfo(std::ostream*,
                           const analysis::LintRuleDescriptionsMap&,
                           absl::string_view);

// Outputs the descriptions for every rule for the --help_rules flag.
void GetLintRuleDescriptionsHelpFlag(std::ostream*, absl::string_view);

// Outputs the descriptions for every rule, formatted for markdown.
void GetLintRuleDescriptionsMarkdown(std::ostream*);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_H_
