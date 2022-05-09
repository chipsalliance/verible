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

#include "common/analysis/token_stream_linter.h"

#include <vector>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/text/token_stream_view.h"
#include "common/util/logging.h"

namespace verible {

void TokenStreamLinter::Lint(const TokenSequence& tokens) {
  VLOG(1) << "TokenStreamLinter analyzing tokens with " << rules_.size()
          << " rules.";
  for (const auto& token : tokens) {
    for (const auto& rule : rules_) {
      ABSL_DIE_IF_NULL(rule)->HandleToken(token);
    }
  }
}

std::vector<LintRuleStatus> TokenStreamLinter::ReportStatus() const {
  std::vector<LintRuleStatus> status;
  status.reserve(rules_.size());
  for (const auto& rule : rules_) {
    status.push_back(ABSL_DIE_IF_NULL(rule)->Report());
  }
  return status;
}

}  // namespace verible
