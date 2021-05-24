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

#include "verilog/analysis/verilog_linter.h"

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "common/analysis/line_lint_rule.h"
#include "common/analysis/line_linter.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/lint_waiver.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/analysis/syntax_tree_linter.h"
#include "common/analysis/text_structure_lint_rule.h"
#include "common/analysis/text_structure_linter.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/analysis/token_stream_linter.h"
#include "common/strings/diff.h"
#include "common/strings/line_column_map.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/text_structure.h"
#include "common/text/token_info.h"
#include "common/util/file_util.h"
#include "common/util/logging.h"
#include "common/util/user_interaction.h"
#include "verilog/analysis/default_rules.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/analysis/verilog_analyzer.h"
#include "verilog/analysis/verilog_linter_configuration.h"
#include "verilog/analysis/verilog_linter_constants.h"
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

// TODO(hzeller): make --rules repeatable and cumulative

ABSL_FLAG(verilog::RuleBundle, rules, {},
          "Comma-separated of lint rules to enable.  "
          "No prefix or a '+' prefix enables it, '-' disable it. "
          "Configuration values for each rules placed after '=' character.");
ABSL_FLAG(std::string, rules_config, "",
          "Path to lint rules configuration file. "
          "Disables --rule_config_search if set.");
ABSL_FLAG(bool, rules_config_search, false,
          "Look for lint rules configuration file '.rules.verible_lint' "
          "searching upward from the location of each analyzed file.");
ABSL_FLAG(verilog::RuleSet, ruleset, verilog::RuleSet::kDefault,
          "[default|all|none], the base set of rules used by linter");
ABSL_FLAG(std::string, waiver_files, "",
          "Path to waiver config files (comma-separated). "
          "Please refer to the README file for information about its format.");

namespace verilog {

using verible::LineColumnMap;
using verible::LintRuleStatus;
using verible::LintWaiver;
using verible::TextStructureView;
using verible::TokenInfo;

std::set<LintViolationWithStatus> GetSortedViolations(
    const std::vector<LintRuleStatus>& statuses) {
  std::set<LintViolationWithStatus> violations;

  for (auto& status : statuses) {
    for (auto& violation : status.violations) {
      violations.insert(LintViolationWithStatus(&violation, &status));
    }
  }

  return violations;
}

void ViolationPrinter::HandleViolations(
    const std::set<LintViolationWithStatus>& violations, absl::string_view base,
    absl::string_view path) {
  verible::LintStatusFormatter formatter(base);
  for (auto violation : violations) {
    formatter.FormatViolation(stream_, *violation.violation, base, path,
                              violation.status->url,
                              violation.status->lint_rule_name);
    (*stream_) << std::endl;
  }
}

static void PrintFix(std::ostream& stream, absl::string_view text,
                     const verible::AutoFix& fix) {
  std::string after = fix.Apply(text);
  verible::LineDiffs diff(text, after);

  verible::LineDiffsToUnifiedDiff(stream, diff, 1);
}

void ViolationFixer::CommitFixes(absl::string_view source_content,
                                 absl::string_view source_path,
                                 const verible::AutoFix& fix) const {
  if (fix.Edits().empty()) {
    return;
  }
  std::string fixed_content = fix.Apply(source_content);

  if (patch_stream_) {
    verible::LineDiffs diff(source_content, fixed_content);
    verible::LineDiffsToUnifiedDiff(*patch_stream_, diff, 1, source_path);
  } else {
    const absl::Status write_status =
        verible::file::SetContents(source_path, fixed_content);
    if (!write_status.ok()) {
      LOG(ERROR) << "Failed to write fixes to file '" << source_path
                 << "': " << write_status.ToString();
      return;
    }
  }
}

void ViolationFixer::HandleViolations(
    const std::set<LintViolationWithStatus>& violations, absl::string_view base,
    absl::string_view path) {
  verible::AutoFix fix;
  verible::LintStatusFormatter formatter(base);
  for (auto violation : violations) {
    HandleViolation(*violation.violation, base, path, violation.status->url,
                    violation.status->lint_rule_name, formatter, &fix);
  }

  CommitFixes(base, path, fix);
}

void ViolationFixer::HandleViolation(
    const verible::LintViolation& violation, absl::string_view base,
    absl::string_view path, absl::string_view url, absl::string_view rule_name,
    const verible::LintStatusFormatter& formatter, verible::AutoFix* fix) {
  std::stringstream violation_message;
  formatter.FormatViolation(&violation_message, violation, base, path, url,
                            rule_name);
  (*stream_) << violation_message.str() << std::endl;

  if (violation.autofixes.empty()) {
    return;
  }

  AnswerChoice answer;
  for (;;) {
    if (ultimate_answer_ != AnswerChoice::kUnknown) {
      answer = ultimate_answer_;
    } else if (auto found = rule_answers_.find(rule_name);
               found != rule_answers_.end()) {
      answer = found->second;
    } else {
      answer = answer_chooser_(violation, rule_name);
    }

    switch (answer) {
      case AnswerChoice::kApplyAll:
        ultimate_answer_ = AnswerChoice::kApply;
        [[fallthrough]];
      case AnswerChoice::kApplyAllForRule:
        rule_answers_[rule_name] = AnswerChoice::kApply;
        [[fallthrough]];
      case AnswerChoice::kApply:
        // Apply first available fix
        if (!fix->AddEdits(violation.autofixes[0].Edits())) {
          VLOG(2) << "The fix conflicts with previously applied fixes, "
                     "rejecting.";
        } else {
          (*stream_) << "(fixed)" << std::endl;
        }
        break;

      case AnswerChoice::kRejectAll:
        ultimate_answer_ = AnswerChoice::kReject;
        [[fallthrough]];
      case AnswerChoice::kRejectAllForRule:
        rule_answers_[rule_name] = AnswerChoice::kReject;
        [[fallthrough]];
      case AnswerChoice::kReject:
        return;

      case AnswerChoice::kPrintFix:
        // Print first available fix
        PrintFix(*stream_, base, violation.autofixes[0]);
        continue;
      case AnswerChoice::kPrintAppliedFixes:
        PrintFix(*stream_, base, *fix);
        continue;

      default:
        continue;
    }

    break;
  }
}

ViolationFixer::AnswerChoice ViolationFixer::InteractiveAnswerChooser(
    const verible::LintViolation& violation, absl::string_view rule_name) {
  static absl::string_view help_message =
      "y - apply fix\n"
      "n - reject fix\n"
      "a - apply this and all remaining fixes for violations of this rule\n"
      "d - reject this and all remaining fixes for violations of this rule\n"
      "A - apply this and all remaining fixes\n"
      "D - reject this and all remaining fixes\n"
      "p - show fix\n"
      "P - show fixes applied in this file so far\n"
      "? - print this help and prompt again\n";

  for (;;) {
    const char c = verible::ReadCharFromUser(
        std::cin, std::cerr, verible::IsInteractiveTerminalSession(),
        "Autofix is available. Apply? [y,n,a,d,A,D,p,P,?] ");

    switch (c) {
      case 'y':
        return AnswerChoice::kApply;
      case 'a':
        return AnswerChoice::kApplyAllForRule;
      case 'A':
        return AnswerChoice::kApplyAll;

      case 'n':
        return AnswerChoice::kReject;
      case 'd':
        return AnswerChoice::kRejectAllForRule;
      case 'D':
        return AnswerChoice::kRejectAll;

      case '\0':
        // EOF: received when too few "answers" have been piped to stdin.
        LOG(WARNING) << "Received EOF while there are questions left. "
                        "Rejecting all remaining fixes.";
        return AnswerChoice::kRejectAll;

      case 'p':
        return AnswerChoice::kPrintFix;
      case 'P':
        return AnswerChoice::kPrintAppliedFixes;

      case '\n':
        continue;

      case '?':
      default:
        std::cerr << help_message << std::endl;
        continue;
    }

    break;
  }
}

// Return code useful to be used in main:
//  0: success
//  1: linting error (if parse_fatal == true)
//  2..: other fatal issues such as file not found.
int LintOneFile(std::ostream* stream, absl::string_view filename,
                const LinterConfiguration& config,
                ViolationHandler* violation_handler, bool check_syntax,
                bool parse_fatal, bool lint_fatal, bool show_context) {
  std::string content;
  const absl::Status content_status =
      verible::file::GetContents(filename, &content);
  if (!content_status.ok()) {
    LOG(ERROR) << "Can't read '" << filename
               << "': " << content_status.message();
    return 2;
  }

  // Lex and parse the contents of the file.
  const auto analyzer =
      VerilogAnalyzer::AnalyzeAutomaticMode(content, filename);
  if (check_syntax) {
    const auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
    const auto parse_status = analyzer->ParseStatus();
    if (!lex_status.ok() || !parse_status.ok()) {
      const std::vector<std::string> syntax_error_messages(
          analyzer->LinterTokenErrorMessages(show_context));
      for (const auto& message : syntax_error_messages) {
        *stream << message << std::endl;
      }
      if (parse_fatal) {
        return 1;
        // With syntax-error recovery, one can still continue to analyze a
        // partial syntax tree.
      }
    }
  }

  // Analyze the parsed structure for lint violations.
  const auto& text_structure = analyzer->Data();
  const auto linter_result =
      VerilogLintTextStructure(filename, config, text_structure, show_context);
  if (!linter_result.ok()) {
    // Something went wrong with running the lint analysis itself.
    LOG(ERROR) << "Fatal error: " << linter_result.status().message();
    return 2;
  }

  const std::vector<LintRuleStatus> linter_statuses =
      std::move(linter_result.value());

  size_t total_violations = 0;
  for (const auto& rule_status : linter_statuses) {
    total_violations += rule_status.violations.size();
  }

  if (total_violations == 0) {
    VLOG(1) << "No lint violations found." << std::endl;
  } else {
    VLOG(1) << "Lint Violations (" << total_violations << "): " << std::endl;

    absl::string_view text_base = text_structure.Contents();

    const std::set<LintViolationWithStatus> violations =
        GetSortedViolations(linter_statuses);
    violation_handler->HandleViolations(violations, text_base, filename);
    if (lint_fatal) {
      return 1;
    }
  }

  return 0;
}

VerilogLinter::VerilogLinter()
    : lint_waiver_(
          [](const TokenInfo& t) {
            return IsComment(verilog_tokentype(t.token_enum()));
          },
          [](const TokenInfo& t) {
            return IsWhitespace(verilog_tokentype(t.token_enum()));
          },
          kLinterTrigger, kLinterWaiveLineCommand, kLinterWaiveStartCommand,
          kLinterWaiveStopCommand) {}

absl::Status VerilogLinter::Configure(const LinterConfiguration& configuration,
                                      absl::string_view lintee_filename) {
  if (VLOG_IS_ON(1)) {
    for (const auto& name : configuration.ActiveRuleIds()) {
      LOG(INFO) << "active rule: '" << name << '\'';
    }
  }
  auto text_rules = configuration.CreateTextStructureRules();
  for (auto& rule : text_rules) {
    text_structure_linter_.AddRule(std::move(rule));
  }
  auto line_rules = configuration.CreateLineRules();
  for (auto& rule : line_rules) {
    line_linter_.AddRule(std::move(rule));
  }
  auto token_rules = configuration.CreateTokenStreamRules();
  for (auto& rule : token_rules) {
    token_stream_linter_.AddRule(std::move(rule));
  }
  auto syntax_rules = configuration.CreateSyntaxTreeRules();
  for (auto& rule : syntax_rules) {
    syntax_tree_linter_.AddRule(std::move(rule));
  }

  absl::Status rc = absl::OkStatus();
  for (const auto& waiver_file :
       absl::StrSplit(configuration.external_waivers, ',', absl::SkipEmpty())) {
    std::string content;
    auto status = verible::file::GetContents(waiver_file, &content);
    if (content.empty()) {
      continue;
    }
    if (status.ok()) {
      status = lint_waiver_.ApplyExternalWaivers(
          configuration.ActiveRuleIds(), lintee_filename, waiver_file, content);
    }
    if (!status.ok()) {
      rc.Update(status);
    }
  }

  return rc;
}

void VerilogLinter::Lint(const TextStructureView& text_structure,
                         absl::string_view filename) {
  // Collect all lint waivers in an initial pass.
  lint_waiver_.ProcessTokenRangesByLine(text_structure);

  // Analyze general text structure.
  text_structure_linter_.Lint(text_structure, filename);

  // Analyze lines of text.
  line_linter_.Lint(text_structure.Lines());

  // Analyze token stream.
  token_stream_linter_.Lint(text_structure.TokenStream());

  // Analyze syntax tree.
  const verible::ConcreteSyntaxTree& syntax_tree = text_structure.SyntaxTree();
  if (syntax_tree != nullptr) {
    syntax_tree_linter_.Lint(*syntax_tree);
  }
}

static void AppendLintRuleStatuses(
    const std::vector<LintRuleStatus>& new_statuses,
    const verible::LintWaiver& waivers, const LineColumnMap& line_map,
    absl::string_view text_base,
    std::vector<LintRuleStatus>* cumulative_statuses) {
  for (const auto& status : new_statuses) {
    cumulative_statuses->push_back(status);
    const auto* waived_lines =
        waivers.LookupLineNumberSet(status.lint_rule_name);
    if (waived_lines) {
      cumulative_statuses->back().WaiveViolations(
          [&](const verible::LintViolation& violation) {
            // Lookup the line number on which the offending token resides.
            const size_t offset = violation.token.left(text_base);
            const size_t line = line_map(offset).line;
            // Check that line number against the set of waived lines.
            const bool waived =
                LintWaiver::LineNumberSetContains(*waived_lines, line);
            VLOG(2) << "Violation of " << status.lint_rule_name
                    << " rule on line " << line + 1
                    << (waived ? " is waived." : " is not waived.");
            return waived;
          });
    }
  }
}

std::vector<LintRuleStatus> VerilogLinter::ReportStatus(
    const LineColumnMap& line_map, absl::string_view text_base) {
  std::vector<LintRuleStatus> statuses;
  const verible::LintWaiver& waivers = lint_waiver_.GetLintWaiver();
  AppendLintRuleStatuses(line_linter_.ReportStatus(), waivers, line_map,
                         text_base, &statuses);
  AppendLintRuleStatuses(text_structure_linter_.ReportStatus(), waivers,
                         line_map, text_base, &statuses);
  AppendLintRuleStatuses(token_stream_linter_.ReportStatus(), waivers, line_map,
                         text_base, &statuses);
  AppendLintRuleStatuses(syntax_tree_linter_.ReportStatus(), waivers, line_map,
                         text_base, &statuses);
  return statuses;
}

LinterConfiguration LinterConfigurationFromFlags(
    absl::string_view linting_start_file) {
  LinterConfiguration config;

  const verilog::LinterOptions options = {
      .ruleset = absl::GetFlag(FLAGS_ruleset),
      .rules = absl::GetFlag(FLAGS_rules),
      .config_file = absl::GetFlag(FLAGS_rules_config),
      .rules_config_search = absl::GetFlag(FLAGS_rules_config_search),
      .linting_start_file = std::string(linting_start_file),
      .waiver_files = absl::GetFlag(FLAGS_waiver_files),
  };

  absl::Status config_status = config.ConfigureFromOptions(options);
  if (!config_status.ok()) {
    LOG(WARNING) << "Unable to configure linter for: " << linting_start_file;
  }

  return config;
}

absl::StatusOr<std::vector<LintRuleStatus>> VerilogLintTextStructure(
    absl::string_view filename, const LinterConfiguration& config,
    const TextStructureView& text_structure, bool show_context) {
  // Create the linter, add rules, and run it.
  VerilogLinter linter;
  const absl::Status configuration_status = linter.Configure(config, filename);
  if (!configuration_status.ok()) {
    return configuration_status;
  }

  linter.Lint(text_structure, filename);

  absl::string_view text_base = text_structure.Contents();
  // Each enabled lint rule yields a collection of violations.
  return linter.ReportStatus(text_structure.GetLineColumnMap(), text_base);
}

absl::Status PrintRuleInfo(std::ostream* os,
                           const analysis::LintRuleDescriptionsMap& rule_map,
                           absl::string_view rule_name) {
  constexpr int kRuleWidth = 35;
  constexpr char kFill = ' ';

  const auto it = rule_map.find(rule_name);
  if (it == rule_map.end())
    return absl::NotFoundError(absl::StrCat(
        "Rule: \'", rule_name,
        "\' not found. Please specify a rule name or \"all\" for help on "
        "the rules.\n"));

  // Print description.
  *os << std::left << std::setw(kRuleWidth) << std::setfill(kFill) << rule_name
      << it->second.description << "\n";
  // Print default enabled.
  *os << std::left << std::setw(kRuleWidth) << std::setfill(kFill) << " "
      << "Enabled by default: " << std::boolalpha << it->second.default_enabled
      << "\n\n";
  return absl::OkStatus();
}

static void AppendCitation(const absl::string_view& rule_raw,
                           CustomCitationMap& citations) {
  const size_t colon_pos = rule_raw.find(':');
  if (!colon_pos || colon_pos == absl::string_view::npos) return;
  const size_t rule_id_end = colon_pos;
  const size_t rule_citation_beg = colon_pos + 1;
  absl::string_view rule_id = rule_raw.substr(0, rule_id_end);
  absl::string_view citation =
      rule_raw.substr(rule_citation_beg, rule_raw.size() - rule_citation_beg);
  rule_id = absl::StripAsciiWhitespace(rule_id);
  citation = absl::StripAsciiWhitespace(citation);

  citations[rule_id] =
      absl::StrReplaceAll(citation, {{"\\\r\n", "\r\n"}, {"\\\n", "\n"}});
}

CustomCitationMap ParseCitations(absl::string_view content) {
  CustomCitationMap citations;
  size_t rule_begin = 0;
  size_t rule_end = content.find("\n");
  while (rule_end != std::string::npos) {
    // check unix like escape seq
    if (rule_end == 0 || content[rule_end - 1] == '\\') {
      rule_end = content.find("\n", rule_end + 1);
      continue;
    }
    // check windows like escape seq
    if (rule_end == 1 ||
        (content[rule_end - 2] == '\\' && content[rule_end - 1] == '\r')) {
      rule_end = content.find("\n", rule_end + 1);
      continue;
    }

    auto rule_subs = content.substr(rule_begin, rule_end - rule_begin);
    AppendCitation(rule_subs, citations);
    rule_begin = rule_end + 1;

    rule_end = content.find("\n", rule_begin);
  }
  return citations;
}

void GetLintRuleDescriptionsHelpFlag(std::ostream* os,
                                     absl::string_view flag_value,
                                     const CustomCitationMap& citations) {
  // Set up the map.
  auto rule_map = analysis::GetAllRuleDescriptionsHelpFlag();
  for (const auto& [rule_id, citation] : citations) {
    auto rule = rule_map.find(rule_id);
    if (rule != rule_map.end()) {
      rule->second.description = citation;
    }
  }

  for (const auto& rule_id : analysis::kDefaultRuleSet) {
    rule_map[rule_id].default_enabled = true;
  }

  if (flag_value != "all") {
    const auto status = PrintRuleInfo(os, rule_map, flag_value);
    if (!status.ok()) *os << status.message();
    return;
  }

  // Print all rules.
  for (const auto& rule : rule_map) {
    const auto status = PrintRuleInfo(os, rule_map, rule.first);
    if (!status.ok()) {
      *os << status.message();
      return;
    }
  }
}

void GetLintRuleDescriptionsMarkdown(std::ostream* os,
                                     const CustomCitationMap& citations) {
  auto rule_map = analysis::GetAllRuleDescriptionsMarkdown();
  for (const auto& rule_id : analysis::kDefaultRuleSet) {
    rule_map[rule_id].default_enabled = true;
  }

  for (const auto& [rule_id, citation] : citations) {
    auto rule = rule_map.find(rule_id);
    if (rule != rule_map.end()) {
      rule->second.description = citation;
    }
  }

  for (const auto& rule : rule_map) {
    // Print the rule, description and if it is enabled by default.
    *os << "### " << rule.first << "\n";
    *os << rule.second.description << "\n\n";
    *os << "Enabled by default: " << std::boolalpha
        << rule.second.default_enabled << "\n\n";
  }
}

}  // namespace verilog
