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

// LintRule is an abstract class from which a broad class of
// structure-dependent linter rules can be derived.

#ifndef VERIBLE_COMMON_ANALYSIS_LINT_RULE_H_
#define VERIBLE_COMMON_ANALYSIS_LINT_RULE_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"

namespace verible {

// LintRule is an abstract base class that represent a single linter rule.
class LintRule {
 public:
  virtual ~LintRule() = default;

  // If there is a configuration string for this rule, it will be passed to this
  // rule before use. This is a single string and the rule is free to impose
  // its own style of configuration (might be more formalized later).
  //
  // Returns OK-status if  configuration could be parsed successfully;
  // on failure, the Error-status will contain a message.
  // By default, rules don't accept any configuration, so only an empty
  // configuration is valid.
  virtual absl::Status Configure(absl::string_view configuration) {
    if (configuration.empty()) return absl::OkStatus();
    return absl::InvalidArgumentError("Rule does not support configuration.");
  }

  // Report() returns a LintRuleStatus, which summarizes the results so
  // far of running the LintRule.
  virtual LintRuleStatus Report() const = 0;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_LINT_RULE_H_
