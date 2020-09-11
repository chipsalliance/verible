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

#include "verilog/analysis/checkers/forbid_consecutive_null_statements_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;

// Register ForbidConsecutiveNullStatementsRule
VERILOG_REGISTER_LINT_RULE(ForbidConsecutiveNullStatementsRule);

absl::string_view ForbidConsecutiveNullStatementsRule::Name() {
  return "forbid-consecutive-null-statements";
}
const char ForbidConsecutiveNullStatementsRule::kTopic[] =
    "redundant-semicolons";
const char ForbidConsecutiveNullStatementsRule::kMessage[] =
    "Do not use consecutive null statements like \';;\'.";

std::string ForbidConsecutiveNullStatementsRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat("Checks that there are no occurrences of ",
                      "consecutive null statements like ",
                      Codify(";;", description_type));
}

void ForbidConsecutiveNullStatementsRule::HandleLeaf(
    const verible::SyntaxTreeLeaf& leaf, const SyntaxTreeContext& context) {
  if (context.IsInside(NodeEnum::kForSpec)) {
    // for loops are allowed to be: for (;;)
    state_ = State::kNormal;
  } else {
    switch (state_) {
      case State::kNormal: {
        if (leaf.Tag().tag == ';') {
          state_ = State::kExpectNonSemicolon;
        }
        break;
      }

      case State::kExpectNonSemicolon: {
        if (leaf.Tag().tag == ';') {
          violations_.insert(LintViolation(leaf, kMessage, context));
        } else {
          state_ = State::kNormal;
        }
        break;
      }
    }
  }
}

LintRuleStatus ForbidConsecutiveNullStatementsRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
