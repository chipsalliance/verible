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
#include <stack>
#include <string>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/text/token_info.h"
#include "verilog/analysis/descriptions.h"

namespace verilog {
namespace analysis {

// Detects whether a Verilog `endif directive is followed by a comment that
// matches the opening `ifdef or `ifndef.
//
// Accepted examples:
//   if (FOO) begin
//
//   else begin
//
//   else if (FOO) begin
//
//   for (FOO) begin
//
// Rejected examples:
//   if (FOO)
//       BAR
//
//   else
//       BAR
//
//   else if (FOO)
//       BAR
//
//   for (FOO) begin
//       BAR
//
//
class ExplicitBeginRule : public verible::TokenStreamLintRule {
 public:
  using rule_type = verible::TokenStreamLintRule;

  static const LintRuleDescriptor& GetDescriptor();

  ExplicitBeginRule() : state_(State::kNormal), condition_expr_level_(0) {}

  void HandleToken(const verible::TokenInfo& token) final;

  verible::LintRuleStatus Report() const final;

 private:
  // States of the internal token-based analysis.
  enum class State { kNormal, kInCondition, kInElse, kExpectBegin };

  // Internal lexical analysis state.
  State state_;

  // Level of nested parenthesis when analizing conditional expressions.
  int condition_expr_level_;

  // Token information for the last seen block opening (for/if/else).
  verible::TokenInfo last_condition_start_ = verible::TokenInfo::EOFToken();
  verible::TokenInfo end_of_condition_statement_ =
      verible::TokenInfo::EOFToken();

  // Collection of found violations.
  std::set<verible::LintViolation> violations_;

  void trigger_violation_();
};

}  // namespace analysis
}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_CHECKERS_ENDIF_COMMENT_RULE_H_
