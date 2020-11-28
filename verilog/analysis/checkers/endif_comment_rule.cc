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

#include "verilog/analysis/checkers/endif_comment_rule.h"

#include <deque>
#include <set>
#include <stack>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/token_stream_lint_rule.h"
#include "common/strings/comment_utils.h"
#include "common/text/token_info.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;
using verible::TokenStreamLintRule;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(EndifCommentRule);

absl::string_view EndifCommentRule::Name() { return "endif-comment"; }
const char EndifCommentRule::kTopic[] = "endif-comment";
const char EndifCommentRule::kMessage[] =
    "`endif should be followed on the same line by a comment that matches the "
    "opening `ifdef/`ifndef.";

std::string EndifCommentRule::GetDescription(DescriptionType description_type) {
  return absl::StrCat("Checks that a Verilog ",
                      Codify("`endif", description_type),
                      " directive is followed by a comment that matches the "
                      "name of the opening ",
                      Codify("`ifdef", description_type), " or ",
                      Codify("`ifndef", description_type), ". See ",
                      GetStyleGuideCitation(kTopic), ".");
}

void EndifCommentRule::HandleToken(const TokenInfo& token) {
  // Responds to a token by updating the state of the analysis.
  switch (state_) {
    case State::kNormal: {
      // Only changes state on `ifdef/`ifndef/`endif tokens;
      // all others are ignored in this analysis.
      // Notably, `else and `elsif are neither examined nor used.
      switch (token.token_enum()) {
        case PP_ifdef:  // fall-through
        case PP_ifndef:
          state_ = State::kExpectPPIdentifier;
          break;
        case PP_endif:
          last_endif_ = token;
          state_ = State::kExpectEndifComment;
          break;
        default:
          break;
      }
      break;
    }
    case State::kExpectPPIdentifier: {
      // Expecting the argument to `ifdef/`ifndef.
      switch (token.token_enum()) {
        case PP_Identifier:
          conditional_scopes_.push(token);
          state_ = State::kNormal;
          break;
        // Anything other than whitespace/comment would be an error,
        // but is already diagnosed during preprocessing.
        default:
          break;
      }
      break;
    }
    case State::kExpectEndifComment: {
      if (conditional_scopes_.empty()) break;  // unbalanced
      // Checking for comment immediately following `endif.
      // Matching comment must be on the same line as the `endif
      const absl::string_view expect = conditional_scopes_.top().text();
      switch (token.token_enum()) {
        case TK_SPACE:  // stay in the same state
          break;
        case TK_COMMENT_BLOCK:
        case TK_EOL_COMMENT: {
          // check comment text, unwrap comment, unpad whitespace.
          // allow either // COND or /* COND */
          const absl::string_view contents =
              verible::StripCommentAndSpacePadding(token.text());
          if (contents != expect) {
            violations_.insert(LintViolation(
                last_endif_, absl::StrCat(kMessage, " (", expect, ")")));
          }
          conditional_scopes_.pop();
          state_ = State::kNormal;
          break;
        }
        default:
          // includes TK_NEWLINE and TK_EOF.
          violations_.insert(LintViolation(
              last_endif_, absl::StrCat(kMessage, " (", expect, ")")));
          conditional_scopes_.pop();
          state_ = State::kNormal;
          break;
      }
    }
  }  // switch (state_)
}

LintRuleStatus EndifCommentRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
