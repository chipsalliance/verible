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

#include "verible/verilog/analysis/checkers/endif-comment-rule.h"

#include <set>
#include <stack>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/token-stream-lint-rule.h"
#include "verible/common/strings/comment-utils.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::TokenInfo;
using verible::TokenStreamLintRule;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(EndifCommentRule);

static constexpr absl::string_view kMessage =
    "`endif should be followed on the same line by a comment that matches the "
    "opening `ifdef/`ifndef.";

const LintRuleDescriptor &EndifCommentRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "endif-comment",
      .topic = "endif-comment",
      .desc =
          "Checks that a Verilog `` `endif`` directive is followed by a "
          "comment that matches the name of the opening "
          "`` `ifdef`` or `` `ifndef``.",
  };
  return d;
}

void EndifCommentRule::HandleToken(const TokenInfo &token) {
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
        default: {
          auto endif_end =
              last_endif_.text().substr(last_endif_.text().size(), 0);

          // The whitespace before the comment is something that should come
          // from the formatter configuration so that we don't emit code that
          // then would be complained about by the formatter.
          // Or, make this a configuration option in the lint rule (probably
          // best, as we don't intermingle formatter/linter).
          // TODO(hzeller) Until that is happening just comment here.
          constexpr int kSpacesBeforeComment = 2;
          std::string ws(kSpacesBeforeComment, ' ');

          // TOOD(hzeller): rethink the 'alternative fixes' thing. Problem is,
          // that it makes it impossible to run this unattended as that choice
          // would've to be made at apply time (and the chooser doesn't provide
          // that yet, and it would probably complicate the UI a lot when it
          // will be added).
          //
          // Maybe better: only have _one_ alternative and pre-configure the
          // rule to do that.
          // So with the above our rule needs two configurations.
          //   autofix_whitespace
          //   autofix_block_comment

          // includes TK_NEWLINE and TK_EOF.
          violations_.insert(LintViolation(
              last_endif_, absl::StrCat(kMessage, " (", expect, ")"),
              {
                  AutoFix("Insert // comment",
                          {endif_end, absl::StrCat(ws, "// ", expect)}),
                  AutoFix("Insert /* comment */",
                          {endif_end, absl::StrCat(ws, "/* ", expect, " */")}),
              }));
          conditional_scopes_.pop();
          state_ = State::kNormal;
          break;
        }
      }
    }
  }  // switch (state_)
}

LintRuleStatus EndifCommentRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
