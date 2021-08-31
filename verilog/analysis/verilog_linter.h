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
#include "absl/status/statusor.h"
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
// the structure used for custom citation
using CustomCitationMap = std::map<absl::string_view, std::string>;

struct LintViolationWithStatus {
  const verible::LintViolation* violation;
  const verible::LintRuleStatus* status;

  LintViolationWithStatus(const verible::LintViolation* v,
                          const verible::LintRuleStatus* s)
      : violation(v), status(s) {}

  bool operator<(const LintViolationWithStatus& r) const {
    // compares addresses which correspond to locations within the same string
    return violation->token.text().data() < r.violation->token.text().data();
  }
};

// Returns violations from multiple `LintRuleStatus`es sorted by position
// of their occurrence in source code.
std::set<LintViolationWithStatus> GetSortedViolations(
    const std::vector<verible::LintRuleStatus>& statuses);

// Interface for implementing violation handlers.
//
// The linting process produces a list of violations found in source code. Those
// violations are then sorted and passed to `HandleViolations()` method of an
// instance passed to LintOneFile().
class ViolationHandler {
 public:
  virtual ~ViolationHandler() = default;

  // This method is called with a list of sorted violations found in file
  // located at `path`. It can be called multiple times with statuses generated
  // from different files. `base` contains source code from the file.
  virtual void HandleViolations(
      const std::set<LintViolationWithStatus>& violations,
      absl::string_view base, absl::string_view path) = 0;
};

// Checks a single file for Verilog style lint violations.
// This is suitable for calling from main().
// 'stream' is used for printing potential syntax errors (if 'check_syntax' is
// true).
// 'filename' is the path to the file to analyze.
// 'config' controls lint rules for analysis.
// 'violation_handler' controls what to do with violations.
// If 'check_syntax' is true, report lexical and syntax errors.
// If 'parse_fatal' is true, abort after encountering syntax errors, else
// continue to analyze the salvaged code structure.
// If 'lint_fatal' is true, exit nonzero on finding lint violations.
// Returns an exit_code like status where 0 means success, 1 means some
// errors were found (syntax, lint), and anything else is a fatal error.
int LintOneFile(std::ostream* stream, absl::string_view filename,
                const LinterConfiguration& config,
                ViolationHandler* violation_handler, bool check_syntax,
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

// ViolationHandler that prints all violations in a form of user-friendly
// messages.
class ViolationPrinter : public ViolationHandler {
 public:
  ViolationPrinter(std::ostream* stream)
      : stream_(stream), formatter_(nullptr) {}

  void HandleViolations(const std::set<LintViolationWithStatus>& violations,
                        absl::string_view base, absl::string_view path) final;

 protected:
  std::ostream* const stream_;
  verible::LintStatusFormatter* formatter_;
};

// ViolationHandler that prints all violations and gives an option to fix those
// that have autofixes available.
//
// By default, when violation has an autofix available, ViolationFixer asks an
// user what to do. The answers can be provided by AnswerChooser callback passed
// to the constructor as the answer_chooser parameter. The callback is called
// once for each fixable violation with a current violation object and a
// violated rule name as arguments, and must return one of the values from
// AnswerChoice enum.
//
// When the constructor's patch_stream parameter is not null, the fixes are
// written to specified stream in unified diff format. Otherwise the fixes are
// applied directly to the source file.
//
// The HandleLintRuleStatuses method can be called multiple times with statuses
// generated from different files. The state of answers like "apply all for
// rule" or "apply all" is kept between the calls.
class ViolationFixer : public ViolationHandler {
 public:
  enum class AnswerChoice {
    kUnknown,
    kApply,              // apply fix
    kReject,             // reject fix
    kApplyAllForRule,    // apply this and all remaining fixes for violations
                         // of this rule
    kRejectAllForRule,   // reject this and all remaining fixes for violations
                         // of this rule
    kApplyAll,           // apply this and all remaining fixes
    kRejectAll,          // reject this and all remaining fixes
    kPrintFix,           // show fix
    kPrintAppliedFixes,  // show fixes applied so far
  };

  using AnswerChooser = std::function<AnswerChoice(
      const verible::LintViolation&, absl::string_view)>;

  ViolationFixer(std::ostream* stream, std::ostream* patch_stream = nullptr,
                 AnswerChooser answer_chooser = InteractiveAnswerChooser)
      : stream_(stream),
        patch_stream_(patch_stream),
        ultimate_answer_(AnswerChoice::kUnknown),
        rule_answers_(),
        answer_chooser_(answer_chooser) {}

  void HandleViolations(const std::set<LintViolationWithStatus>& violations,
                        absl::string_view base, absl::string_view path) final;

 protected:
  void HandleViolation(const verible::LintViolation& violation,
                       absl::string_view base, absl::string_view path,
                       absl::string_view url, absl::string_view rule_name,
                       const verible::LintStatusFormatter& formatter,
                       verible::AutoFix* fix);

  static AnswerChoice InteractiveAnswerChooser(
      const verible::LintViolation& violation, absl::string_view rule_name);

  void CommitFixes(absl::string_view source_content,
                   absl::string_view source_path,
                   const verible::AutoFix& fix) const;

  std::ostream* const stream_;
  std::ostream* const patch_stream_;
  AnswerChoice ultimate_answer_;
  std::map<absl::string_view, AnswerChoice> rule_answers_;
  const AnswerChooser answer_chooser_;
};

// VerilogLintTextStructure analyzes Verilog syntax tree for style violations
// and syntactically detectable pitfalls.
//
// The configuration of this function is controlled by flags:
//   FLAGS_ruleset, FLAGS_rules
//
// Args:
//   stream: the output stream where diagnostics are captured.
//     Writing anything to this stream means that the input contains
//     some lint violation.
//   filename: (optional) name of input file, that can appear in logs.
//   text_structure: contains the syntax tree that will be lint-analyzed.
//   show_context: print additional line with vulnerable code
//
// Returns:
//   Vector of LintRuleStatuses on success, otherwise error code.
absl::StatusOr<std::vector<verible::LintRuleStatus>> VerilogLintTextStructure(
    absl::string_view filename, const LinterConfiguration& config,
    const verible::TextStructureView& text_structure,
    bool show_context = false);

// Prints the rule, description and default_enabled.
absl::Status PrintRuleInfo(std::ostream*,
                           const analysis::LintRuleDescriptionsMap&,
                           absl::string_view);

// Outputs the descriptions for every rule for the --help_rules flag.
// When custom citations are delivered substitute chosen rule description
// with citations specified by user
void GetLintRuleDescriptionsHelpFlag(std::ostream*, absl::string_view,
                                     const CustomCitationMap&);

// Outputs the descriptions for every rule, formatted for markdown.
// When custom citations are delivered substitute chosen rule description
// with citations specified by user
void GetLintRuleDescriptionsMarkdown(std::ostream*, const CustomCitationMap&);

// Parse the file with custom citations
CustomCitationMap ParseCitations(absl::string_view content);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_H_
