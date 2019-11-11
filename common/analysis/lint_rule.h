// Copyright 2017-2019 The Verible Authors.
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

// LintRule is an abstract class from which a broad class of
// structure-dependent linter rules can be derived.

#ifndef VERIBLE_COMMON_ANALYSIS_LINT_RULE_H_
#define VERIBLE_COMMON_ANALYSIS_LINT_RULE_H_

#include "common/analysis/lint_rule_status.h"

namespace verible {

// LintRule is an abstract base class that represent a single linter rule.
class LintRule {
 public:
  virtual ~LintRule() {}

  // Report() returns a LintRuleStatus, which summarizes the results so
  // far of running the LintRule.
  virtual LintRuleStatus Report() const = 0;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_LINT_RULE_H_
