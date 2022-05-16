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

#include "verilog/analysis/checkers/explicit_begin_rule.h"

#include <deque>
#include <set>
#include <stack>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/strings/comment_utils.h"
#include "common/text/token_info.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;
using verible::TokenStreamLintRule;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ExplicitBeginRule);

static const char kMessage[] =
    "All block construct shall explicitely use begin/end";

const LintRuleDescriptor& ExplicitBeginRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "explicit-begin",
      .topic = "explicit-begin",
      .desc =
          "Checks that a Verilog ``begin`` directive follows all "
          "if, else and for loops.",
  };
  return d;
}

void ExplicitBeginRule::HandleToken(const TokenInfo& token) {
  // Responds to a token by updating the state of the analysis.
  bool raise_violation = false;
  switch (state_) {
    case State::kNormal: {
      // On if/else/for tokens;
      // Also, skip all conditional statement after a for or a if
      // Then, expect a `begin` token or record a violation.
      switch (token.token_enum()) {
        case TK_if:
        case TK_for:
          condition_expr_level_ = 0;
          last_condition_start_ = token;
          state_ = State::kInCondition;
          break;
        case TK_else:
          last_condition_start_ = token;
          end_of_condition_statement_ = token;
          state_ = State::kInElse;
          break;
        default:
          break;
      }  // switch (token)
      break;
    }
    case State::kInElse: {
      // If we are in a else statement, we can either have a if (and need to
      // skip cond. statement) Or directly wait for a begin. We make use of the
      // boolean raise_violation in order to avoid code duplication.
      switch (token.token_enum()) {
        case TK_SPACE:  // stay in the same state
          break;
        case TK_COMMENT_BLOCK:
        case TK_EOL_COMMENT:
          break;
        case TK_if:
        case TK_for:
          condition_expr_level_ = 0;
          last_condition_start_ = token;
          state_ = State::kInCondition;
          break;
        case TK_begin:
          state_ = State::kNormal;
          break;
        default:
          raise_violation = true;
          break;
      }  // switch (token)
      break;
    }
    case State::kInCondition: {
      if (token.text() == "(") {
        condition_expr_level_++;
      } else if (token.text() == ")") {
        condition_expr_level_--;
        if (condition_expr_level_ == 0) {
          end_of_condition_statement_ = token;
          state_ = State::kExpectBegin;
        }
      }
      break;
    }
    case State::kExpectBegin: {
      switch (token.token_enum()) {
        case TK_SPACE:  // stay in the same state
          break;
        case TK_COMMENT_BLOCK:
        case TK_EOL_COMMENT:
          break;
        case TK_begin:
          // If we got our begin token, we go back to normal status
          state_ = State::kNormal;
          break;
        default: {
          raise_violation = true;
          break;
        }
      }  // switch (token)
      break;
    }
  }  // switch (state_)

  if (raise_violation) {
    violations_.insert(LintViolation(
        last_condition_start_, absl::StrCat(kMessage),
        {AutoFix(
            "Insert begin",
            {end_of_condition_statement_,
             absl::StrCat(end_of_condition_statement_.text(), " begin")})}));

    // Once the violation is raised, we go back to a normal, default, state
    condition_expr_level_ = 0;
    state_ = State::kNormal;
    raise_violation = false;
  }
}

LintRuleStatus ExplicitBeginRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
