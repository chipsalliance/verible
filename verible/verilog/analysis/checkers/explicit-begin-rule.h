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

#ifndef VERIBLE_VERILOG_ANALYSIS_CHECKERS_EXPLICIT_BEGIN_RULE_H_
#define VERIBLE_VERILOG_ANALYSIS_CHECKERS_EXPLICIT_BEGIN_RULE_H_

#include <set>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

using verible::TokenInfo;

// Detects whether if and for loop statements use verilog block statements
// (begin/end)
class ExplicitBeginRule : public verible::TokenStreamLintRule {
 public:
  using rule_type = verible::TokenStreamLintRule;

  static const LintRuleDescriptor &GetDescriptor();

  absl::Status Configure(absl::string_view configuration) final;

  void HandleToken(const verible::TokenInfo &token) final;

  verible::LintRuleStatus Report() const final;

 private:
  bool HandleTokenStateMachine(const TokenInfo &token);

  bool IsTokenEnabled(const TokenInfo &token);

  // States of the internal token-based analysis.
  enum class State {
    kNormal,
    kInAlways,
    kInCondition,
    kInElse,
    kExpectBegin,
    kConstraint,
    kInlineConstraint
  };

  // Internal lexical analysis state.
  State state_{State::kNormal};

  // Level of nested parenthesis when analysing conditional expressions
  int condition_expr_level_{0};

  // Level inside a constraint expression
  int constraint_expr_level_{0};

  // Configuration
  bool if_enable_{true};
  bool else_enable_{true};
  bool always_enable_{true};
  bool always_comb_enable_{true};
  bool always_latch_enable_{true};
  bool always_ff_enable_{true};
  bool for_enable_{true};
  bool forever_enable_{true};
  bool foreach_enable_{true};
  bool while_enable_{true};
  bool initial_enable_{true};

  // Token that requires blocking statement.
  verible::TokenInfo start_token_{verible::TokenInfo::EOFToken()};

  // Collection of found violations.
  std::set<verible::LintViolation> violations_;
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_ENDIF_COMMENT_RULE_H_