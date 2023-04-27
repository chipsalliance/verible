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

#include "common/analysis/line_linter.h"

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/line-lint-rule.h"
#include "common/analysis/lint-rule-status.h"
#include "common/util/logging.h"

namespace verible {

void LineLinter::Lint(const std::vector<absl::string_view>& lines) {
  VLOG(1) << "LineLinter analyzing lines with " << rules_.size() << " rules.";
  for (const auto& line : lines) {
    for (const auto& rule : rules_) {
      ABSL_DIE_IF_NULL(rule)->HandleLine(line);
    }
  }
  for (const auto& rule : rules_) {
    rule->Finalize();
  }
}

std::vector<LintRuleStatus> LineLinter::ReportStatus() const {
  std::vector<LintRuleStatus> status;
  status.reserve(rules_.size());
  for (const auto& rule : rules_) {
    status.push_back(ABSL_DIE_IF_NULL(rule)->Report());
  }
  return status;
}

}  // namespace verible
