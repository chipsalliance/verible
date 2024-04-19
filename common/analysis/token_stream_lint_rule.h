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

// TokenStreamLintRule represents a token-based scanner for detecing lint
// violations.  It scans one token at a time, and may update internal state
// held by any subclasses.

#ifndef VERIBLE_COMMON_ANALYSIS_TOKEN_STREAM_LINT_RULE_H_
#define VERIBLE_COMMON_ANALYSIS_TOKEN_STREAM_LINT_RULE_H_

#include "common/analysis/lint_rule.h"
#include "common/text/token_info.h"

namespace verible {

class TokenStreamLintRule : public LintRule {
 public:
  ~TokenStreamLintRule() override = default;  // not yet final

  // Scans a single token during analysis.
  virtual void HandleToken(const TokenInfo &token) = 0;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_TOKEN_STREAM_LINT_RULE_H_
