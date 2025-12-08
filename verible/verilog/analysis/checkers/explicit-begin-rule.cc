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

#include "verible/verilog/analysis/checkers/explicit-begin-rule.h"

#include <set>
#include <string_view>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/text/config-utils.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;
using verible::TokenStreamLintRule;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ExplicitBeginRule);

static const char kMessage[] =
    " block constructs shall explicitly use begin/end.";

const LintRuleDescriptor &ExplicitBeginRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "explicit-begin",
      .topic = "explicit-begin",
      .desc =
          "Checks that a Verilog ``begin`` directive follows all "
          "if, else, always, always_comb, always_latch, always_ff, "
          "for, forever, foreach, while and initial statements.",
      .param =
          {
              {"if_enable", "true",
               "All if statements require an explicit begin-end block"},
              {"else_enable", "true",
               "All else statements require an explicit begin-end block"},
              {"always_enable", "true",
               "All always statements require an explicit begin-end block"},
              {"always_comb_enable", "true",
               "All always_comb statements require an explicit begin-end "
               "block"},
              {"always_latch_enable", "true",
               "All always_latch statements require an explicit begin-end "
               "block"},
              {"always_ff_enable", "true",
               "All always_ff statements require an explicit begin-end block"},
              {"for_enable", "true",
               "All for statements require an explicit begin-end block"},
              {"forever_enable", "true",
               "All forever statements require an explicit begin-end block"},
              {"foreach_enable", "true",
               "All foreach statements require an explicit begin-end block"},
              {"while_enable", "true",
               "All while statements require an explicit begin-end block"},
              {"initial_enable", "true",
               "All initial statements require an explicit begin-end block"},
          },
  };
  return d;
}

absl::Status ExplicitBeginRule::Configure(std::string_view configuration) {
  using verible::config::SetBool;
  return verible::ParseNameValues(
      configuration,
      {
          {"if_enable", SetBool(&if_enable_)},
          {"else_enable", SetBool(&else_enable_)},
          {"always_enable", SetBool(&always_enable_)},
          {"always_comb_enable", SetBool(&always_comb_enable_)},
          {"always_latch_enable", SetBool(&always_latch_enable_)},
          {"always_ff_enable", SetBool(&always_ff_enable_)},
          {"for_enable", SetBool(&for_enable_)},
          {"forever_enable", SetBool(&forever_enable_)},
          {"foreach_enable", SetBool(&foreach_enable_)},
          {"while_enable", SetBool(&while_enable_)},
          {"initial_enable", SetBool(&initial_enable_)},
      });
}

bool ExplicitBeginRule::IsTokenEnabled(const TokenInfo &token) {
  switch (token.token_enum()) {
    case TK_if:
      return if_enable_;
    case TK_else:
      return else_enable_;
    case TK_always:
      return always_enable_;
    case TK_always_comb:
      return always_comb_enable_;
    case TK_always_latch:
      return always_latch_enable_;
    case TK_always_ff:
      return always_ff_enable_;
    case TK_for:
      return for_enable_;
    case TK_forever:
      return forever_enable_;
    case TK_foreach:
      return foreach_enable_;
    case TK_while:
      return while_enable_;
    case TK_initial:
      return initial_enable_;
    default:
      return false;
  }
}

bool ExplicitBeginRule::HandleTokenStateMachine(const TokenInfo &token) {
  // Responds to a token by updating the state of the analysis.
  bool raise_violation = false;
  switch (state_) {
    case State::kNormal: {
      // Special handling for constraints
      switch (token.token_enum()) {
        case TK_constraint: {
          constraint_expr_level_ = 0;
          state_ = State::kConstraint;
          return false;
        }
        case TK_with: {
          constraint_expr_level_ = 0;
          state_ = State::kInlineConstraint;
          return false;
        }
        default: {
          // Do nothing
          break;
        }
      }

      if (!IsTokenEnabled(token)) {
        break;
      }

      switch (token.token_enum()) {
        // After token expect "begin"
        case TK_always_comb:
          ABSL_FALLTHROUGH_INTENDED;
        case TK_always_latch:
          ABSL_FALLTHROUGH_INTENDED;
        case TK_forever:
          ABSL_FALLTHROUGH_INTENDED;
        case TK_initial:
          start_token_ = token;
          state_ = State::kExpectBegin;
          break;
        // After token expect a "condition" followed by "begin". NOTE: there may
        // be tokens prior to the condition (like in an "always_ff" statement)
        // and these are all ignored.
        case TK_if:
          ABSL_FALLTHROUGH_INTENDED;
        case TK_always_ff:
          ABSL_FALLTHROUGH_INTENDED;
        case TK_for:
          ABSL_FALLTHROUGH_INTENDED;
        case TK_foreach:
          ABSL_FALLTHROUGH_INTENDED;
        case TK_while:
          condition_expr_level_ = 0;
          start_token_ = token;
          state_ = State::kInCondition;
          break;
        // always gets special handling, as somtimes there is a "condition" or
        // not before a "begin".
        case TK_always:
          condition_expr_level_ = 0;
          start_token_ = token;
          state_ = State::kInAlways;
          break;
        // else is also special as "if" or "begin" can follow
        case TK_else:
          start_token_ = token;
          state_ = State::kInElse;
          break;
        default:
          // Do nothing
          break;
      }
      break;
    }
    case State::kInAlways: {
      // always is a little more complicated in that it can be immediately
      // followed by a "begin" or followed by some special characters ("@" or
      // "*") and maybe a condition.
      switch (token.token_enum()) {
        case '@':
          break;
        case '*':
          break;
        case TK_begin:
          state_ = State::kNormal;
          break;
        case '(':
          condition_expr_level_++;
          state_ = State::kInCondition;
          break;
        default:
          raise_violation = true;
          break;
      }
      break;
    }
    case State::kInElse: {
      // An else statement can be followed by either a begin or an if.
      switch (token.token_enum()) {
        case TK_if:
          if (if_enable_) {
            condition_expr_level_ = 0;
            start_token_ = token;
            state_ = State::kInCondition;
          } else {
            state_ = State::kNormal;
          }
          break;
        case TK_begin:
          state_ = State::kNormal;
          break;
        default:
          raise_violation = true;
          break;
      }
      break;
    }
    case State::kInCondition: {
      // The last token expects a condition statement enclosed in a pair of
      // parentheses "()". This process also ignores any tokens between the last
      // token and the opening parentheses which simplifies "always_ff".
      switch (token.token_enum()) {
        case '(': {
          condition_expr_level_++;
          break;
        }
        case ')': {
          condition_expr_level_--;
          if (condition_expr_level_ == 0) {
            state_ = State::kExpectBegin;
          }
          break;
        }
        default: {
          // throw away everything else
          break;
        }
      }
      break;
    }
    case State::kExpectBegin: {
      // The next token must be a "begin"
      switch (token.token_enum()) {
        case TK_begin:
          // If we got our begin token, we go back to normal status
          state_ = State::kNormal;
          break;
        default: {
          raise_violation = true;
          break;
        }
      }
      break;
    }
    case State::kInlineConstraint: {
      switch (token.token_enum()) {
        case '{': {
          // An InlineConstraint
          constraint_expr_level_++;
          state_ = State::kConstraint;
          break;
        }
        default: {
          // throw away everything else
          state_ = State::kNormal;
          break;
        }
      }
    }
      ABSL_FALLTHROUGH_INTENDED;
    case State::kConstraint: {
      // System Verilog constraints are special and use curly braces {}
      // instead of begin-end. So ignore all constraints
      switch (token.token_enum()) {
        case '{': {
          constraint_expr_level_++;
          break;
        }
        case '}': {
          constraint_expr_level_--;
          if (constraint_expr_level_ == 0) {
            state_ = State::kNormal;
          }
          break;
        }
        default: {
          // throw away everything else
          break;
        }
      }
      break;
    }
    default: {
      // Do nothing
      break;
    }
  }  // switch (state_)

  if (raise_violation) {
    violations_.insert(LintViolation(
        start_token_, absl::StrCat(start_token_.text(), kMessage,
                                   " Expected begin, got ", token.text())));

    // Once the violation is raised, we go back to a normal, default, state
    state_ = State::kNormal;
  }

  return raise_violation;
}

void ExplicitBeginRule::HandleToken(const TokenInfo &token) {
  // Ignore all white space and comments and return immediately
  switch (token.token_enum()) {
    case TK_SPACE:
      return;
    case TK_NEWLINE:
      return;
    case TK_COMMENT_BLOCK:
      return;
    case TK_EOL_COMMENT:
      return;
    default:
      break;
  }

  bool retry = HandleTokenStateMachine(token);

  // If this token raises a violation, it was because the statemachine was
  // expecting a begin token. This token may expect a begin token too, so
  // check it.
  // Consider: forever if(a) #10; else #20; // 3 violations could be raised
  // [forever, if, else].
  if (retry) {
    HandleTokenStateMachine(token);
  }
}

LintRuleStatus ExplicitBeginRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog