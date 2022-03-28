// Copyright 2017-2021 The Verible Authors.
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

#include "common/analysis/violation_handler.h"

#include "absl/status/status.h"
#include "common/strings/diff.h"
#include "common/util/file_util.h"
#include "common/util/user_interaction.h"

namespace verible {
namespace {

void PrintFix(std::ostream& stream, absl::string_view text,
              const verible::AutoFix& fix) {
  std::string after = fix.Apply(text);
  verible::LineDiffs diff(text, after);

  verible::LineDiffsToUnifiedDiff(stream, diff, 1);
}

void PrintFixAlternatives(std::ostream& stream, absl::string_view text,
                          const std::vector<verible::AutoFix>& fixes) {
  const bool print_alternative_number = fixes.size() > 1;
  for (size_t i = 0; i < fixes.size(); ++i) {
    if (print_alternative_number) {
      stream << verible::term::inverse(absl::StrCat(
          "[ ", (i + 1), ". Alternative ", fixes[i].Description(), " ]\n"));
    } else {
      stream << verible::term::inverse(
          absl::StrCat("[ ", fixes[i].Description(), " ]\n"));
    }
    PrintFix(stream, text, fixes[i]);
  }
}

}  // namespace

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

void ViolationWaiverPrinter::HandleViolations(
    const std::set<LintViolationWithStatus>& violations, absl::string_view base,
    absl::string_view path) {
  verible::LintStatusFormatter formatter(base);
  for (auto violation : violations) {
    formatter.FormatViolation(message_stream_, *violation.violation, base, path,
                              violation.status->url,
                              violation.status->lint_rule_name);
    (*message_stream_) << std::endl;

    formatter.FormatViolationWaiver(waiver_stream_, *violation.violation, base,
                                    path, violation.status->lint_rule_name);
    (*waiver_stream_) << std::endl;
  }
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
  (*message_stream_) << violation_message.str() << std::endl;

  if (violation.autofixes.empty()) {
    return;
  }

  static std::string_view previous_fix_conflict =
      "The fix conflicts with "
      "previously applied fixes, rejecting.\n";

  Answer answer;
  for (bool first_round = true; /**/; first_round = false) {
    if (ultimate_answer_.choice != AnswerChoice::kUnknown) {
      answer = ultimate_answer_;
    } else if (auto found = rule_answers_.find(rule_name);
               found != rule_answers_.end()) {
      answer = found->second;
      // If the ApplyAll specifies alternative not available here, use first.
      if (answer.alternative >= violation.autofixes.size()) {
        answer.alternative = 0;
      }
    } else {
      if (is_interactive_ && first_round) {  // Show the user what is available.
        PrintFixAlternatives(*message_stream_, base, violation.autofixes);
      }
      answer = answer_chooser_(violation, rule_name);
    }

    switch (answer.choice) {
      case AnswerChoice::kApplyAll:
        ultimate_answer_ = {AnswerChoice::kApply};
        [[fallthrough]];
      case AnswerChoice::kApplyAllForRule:
        rule_answers_[rule_name] = {AnswerChoice::kApply, answer.alternative};
        [[fallthrough]];
      case AnswerChoice::kApply:  // Apply fix chosen in the alternatative
        if (answer.alternative >= violation.autofixes.size())
          continue;  // ask again.
        if (!fix->AddEdits(violation.autofixes[answer.alternative].Edits())) {
          *message_stream_ << previous_fix_conflict;
        }
        break;
      case AnswerChoice::kRejectAll:
        ultimate_answer_ = {AnswerChoice::kReject};
        [[fallthrough]];
      case AnswerChoice::kRejectAllForRule:
        rule_answers_[rule_name] = {AnswerChoice::kReject};
        [[fallthrough]];
      case AnswerChoice::kReject:
        return;

      case AnswerChoice::kPrintFix:
        PrintFixAlternatives(*message_stream_, base, violation.autofixes);
        continue;
      case AnswerChoice::kPrintAppliedFixes:
        PrintFix(*message_stream_, base, *fix);
        continue;

      default:
        continue;
    }

    break;
  }
}

ViolationFixer::Answer ViolationFixer::InteractiveAnswerChooser(
    const verible::LintViolation& violation, absl::string_view rule_name) {
  static absl::string_view fixed_help_message =
      "n - reject fix\n"
      "a - apply this and all remaining fixes for violations of this rule\n"
      "d - reject this and all remaining fixes for violations of this rule\n"
      "A - apply this and all remaining fixes\n"
      "D - reject this and all remaining fixes\n"
      "p - show fix\n"
      "P - show fixes applied in this file so far\n"
      "? - print this help and prompt again\n";

  const size_t fix_count = violation.autofixes.size();
  std::string help_message;
  std::string alternative_list;  // Show alternatives in the short-menu.
  if (fix_count > 1) {
    help_message =
        absl::StrCat("y - apply first fix\n[1-", fix_count,
                     "] - apply given alternative\n", fixed_help_message);
    for (size_t i = 0; i < fix_count; ++i)
      absl::StrAppend(&alternative_list, (i + 1), ",");
  } else {
    help_message = absl::StrCat("y - apply fix\n", fixed_help_message);
  }

  for (;;) {
    const char c = verible::ReadCharFromUser(
        std::cin, std::cerr, verible::IsInteractiveTerminalSession(),
        verible::term::bold("Autofix is available. Apply? [" +
                            alternative_list + "y,n,a,d,A,D,p,P,?] "));

    // Single character digit chooses the available alternative.
    if (c >= '1' && c <= '9' &&
        c < static_cast<char>('1' + violation.autofixes.size())) {
      return {AnswerChoice::kApply, static_cast<size_t>(c - '1')};
    }

    switch (c) {
      case 'y':
        return {AnswerChoice::kApply, 0};

        // TODO(hzeller): Should we provide a way to choose 'all for rule'
        // including an alternative ? Maybe with a two-letter response
        // such as 1a, 2a, 3a ? Current assumption of interaction is
        // single character.
      case 'a':
        return {AnswerChoice::kApplyAllForRule};
      case 'A':
        return {AnswerChoice::kApplyAll};  // No alternatives
      case 'n':
        return {AnswerChoice::kReject};
      case 'd':
        return {AnswerChoice::kRejectAllForRule};
      case 'D':
        return {AnswerChoice::kRejectAll};

      case '\0':
        // EOF: received when too few "answers" have been piped to stdin.
        std::cerr << "Received EOF while there are questions left. "
                  << "Rejecting all remaining fixes." << std::endl;
        return {AnswerChoice::kRejectAll};

      case 'p':
        return {AnswerChoice::kPrintFix};
      case 'P':
        return {AnswerChoice::kPrintAppliedFixes};

      case '\n':
        continue;

      case '?':
      default:
        std::cerr << help_message << std::endl;
        continue;
    }
  }
}

}  // namespace verible
