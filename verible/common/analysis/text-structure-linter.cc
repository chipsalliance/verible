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

#include "verible/common/analysis/text-structure-linter.h"

#include <vector>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/text-structure-lint-rule.h"
#include "verible/common/text/text-structure.h"
#include "verible/common/util/logging.h"

namespace verible {

void TextStructureLinter::Lint(const TextStructureView &text_structure,
                               absl::string_view filename) {
  VLOG(1) << "TextStructureLinter analyzing text with " << rules_.size()
          << " rules.";
  for (const auto &rule : rules_) {
    ABSL_DIE_IF_NULL(rule)->Lint(text_structure, filename);
  }
}

std::vector<LintRuleStatus> TextStructureLinter::ReportStatus() const {
  std::vector<LintRuleStatus> status;
  status.reserve(rules_.size());
  for (const auto &rule : rules_) {
    status.push_back(ABSL_DIE_IF_NULL(rule)->Report());
  }
  return status;
}

}  // namespace verible
