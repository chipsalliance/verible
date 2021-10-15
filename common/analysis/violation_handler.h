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

#include <ostream>
#include <set>

#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"

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
      const std::set<verible::LintViolationWithStatus>& violations,
      absl::string_view base, absl::string_view path) = 0;
};

// ViolationHandler that prints all violations in a form of user-friendly
// messages.
class ViolationPrinter : public ViolationHandler {
 public:
  explicit ViolationPrinter(std::ostream* stream)
      : stream_(stream), formatter_(nullptr) {}

  void HandleViolations(
      const std::set<verible::LintViolationWithStatus>& violations,
      absl::string_view base, absl::string_view path) final;

 protected:
  std::ostream* const stream_;
  verible::LintStatusFormatter* formatter_;
};

// ViolationHandler that prints all violations in Reviewdog Diagnostic Format
class RDJsonPrinter : public ViolationHandler {
 public:
  explicit RDJsonPrinter(std::ostream* stream)
      : stream_(stream), formatter_(nullptr) {}

  void HandleViolations(
      const std::set<verible::LintViolationWithStatus>& violations,
      absl::string_view base, absl::string_view path) final;

 private:
  std::ostream* const stream_;
  verible::LintStatusFormatter* formatter_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_VIOLATION_HANDLER_H_
