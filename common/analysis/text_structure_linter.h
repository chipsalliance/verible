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

// TextStructureLinter runs a set of analyses on a token stream (from lexer).
// This linter performs a single pass over the input stream.
// Each rule is expected to respond to tokens one-at-a-time.

#ifndef VERIBLE_COMMON_ANALYSIS_TEXT_STRUCTURE_LINTER_H_
#define VERIBLE_COMMON_ANALYSIS_TEXT_STRUCTURE_LINTER_H_

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/text_structure_lint_rule.h"
#include "common/text/text_structure.h"

namespace verible {

class TextStructureLinter {
 public:
  // Analyzes a sequence of tokens.
  void Lint(const TextStructureView &, std::string_view);

  // Transfers ownership of rule into this Linter
  void AddRule(std::unique_ptr<TextStructureLintRule> rule) {
    rules_.emplace_back(std::move(rule));
  }

  // Aggregates results of each held LintRule
  std::vector<LintRuleStatus> ReportStatus() const;

 private:
  // List of rules that the linter is using. Rules are responsible for tracking
  // their own internal state.
  std::vector<std::unique_ptr<TextStructureLintRule>> rules_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_TEXT_STRUCTURE_LINTER_H_
