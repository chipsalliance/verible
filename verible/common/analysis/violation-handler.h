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

#ifndef VERIBLE_COMMON_ANALYSIS_VIOLATION_HANDLER_H_
#define VERIBLE_COMMON_ANALYSIS_VIOLATION_HANDLER_H_

#include <cstddef>
#include <functional>
#include <map>
#include <ostream>
#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"

namespace verible {

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
      const std::set<verible::LintViolationWithStatus> &violations,
      absl::string_view base, absl::string_view path) = 0;
};

// ViolationHandler that prints all violations in a form of user-friendly
// messages.
class ViolationPrinter : public ViolationHandler {
 public:
  explicit ViolationPrinter(std::ostream *stream) : stream_(stream) {}

  void HandleViolations(
      const std::set<verible::LintViolationWithStatus> &violations,
      absl::string_view base, absl::string_view path) final;

 protected:
  std::ostream *const stream_;
  verible::LintStatusFormatter *formatter_ = nullptr;
};

// ViolationHandler that prints all violations in a format required by
// --waiver_files flag
class ViolationWaiverPrinter : public ViolationHandler {
 public:
  explicit ViolationWaiverPrinter(std::ostream *message_stream_,
                                  std::ostream *waiver_stream_)
      : message_stream_(message_stream_), waiver_stream_(waiver_stream_) {}

  void HandleViolations(
      const std::set<verible::LintViolationWithStatus> &violations,
      absl::string_view base, absl::string_view path) final;

 protected:
  std::ostream *const message_stream_;
  std::ostream *const waiver_stream_;
  verible::LintStatusFormatter *formatter_ = nullptr;
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
class ViolationFixer : public verible::ViolationHandler {
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

  struct Answer {
    AnswerChoice choice;
    // If there are multiple alternatives for fixes available, this is
    // the one chosen. By default the first one.
    size_t alternative = 0;
  };

  using AnswerChooser =
      std::function<Answer(const verible::LintViolation &, absl::string_view)>;

  // Violation fixer with user-chosen answer chooser.
  ViolationFixer(std::ostream *message_stream, std::ostream *patch_stream,
                 const AnswerChooser &answer_chooser)
      : ViolationFixer(message_stream, patch_stream, answer_chooser, false) {}

  // Violation fixer with interactive answer choice.
  ViolationFixer(std::ostream *message_stream, std::ostream *patch_stream)
      : ViolationFixer(message_stream, patch_stream, InteractiveAnswerChooser,
                       true) {}

  void HandleViolations(
      const std::set<verible::LintViolationWithStatus> &violations,
      absl::string_view base, absl::string_view path) final;

 private:
  ViolationFixer(std::ostream *message_stream, std::ostream *patch_stream,
                 const AnswerChooser &answer_chooser, bool is_interactive)
      : message_stream_(message_stream),
        patch_stream_(patch_stream),
        answer_chooser_(answer_chooser),
        is_interactive_(is_interactive),
        ultimate_answer_({AnswerChoice::kUnknown, 0}) {}

  void HandleViolation(const verible::LintViolation &violation,
                       absl::string_view base, absl::string_view path,
                       absl::string_view url, absl::string_view rule_name,
                       const verible::LintStatusFormatter &formatter,
                       verible::AutoFix *fix);

  static Answer InteractiveAnswerChooser(
      const verible::LintViolation &violation, absl::string_view rule_name);

  void CommitFixes(absl::string_view source_content,
                   absl::string_view source_path,
                   const verible::AutoFix &fix) const;

  std::ostream *const message_stream_;
  std::ostream *const patch_stream_;
  const AnswerChooser answer_chooser_;
  const bool is_interactive_;

  Answer ultimate_answer_;
  std::map<absl::string_view, Answer> rule_answers_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_VIOLATION_HANDLER_H_
