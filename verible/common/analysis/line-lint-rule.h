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

// LineLintRule represents a line-based scanner for detecting lint
// violations.  It scans one line at a time, and may update internal state
// held by any subclasses.

#ifndef VERIBLE_COMMON_ANALYSIS_LINE_LINT_RULE_H_
#define VERIBLE_COMMON_ANALYSIS_LINE_LINT_RULE_H_

#include <string_view>

#include "verible/common/analysis/lint-rule.h"

namespace verible {

class LineLintRule : public LintRule {
 public:
  ~LineLintRule() override = default;  // not yet final

  // Scans a single line during analysis.
  virtual void HandleLine(std::string_view line) = 0;

  // Analyze the final state of the rule, after the last line has been read.
  virtual void Finalize() {}
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_LINE_LINT_RULE_H_
