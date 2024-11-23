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

#include "verible/verilog/analysis/checkers/forbid-consecutive-null-statements-rule.h"

#include <set>
#include <string>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::AutoFix;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;

// Register ForbidConsecutiveNullStatementsRule
VERILOG_REGISTER_LINT_RULE(ForbidConsecutiveNullStatementsRule);

static constexpr absl::string_view kMessage =
    "Do not use consecutive null statements like \';;\'.";

const LintRuleDescriptor &ForbidConsecutiveNullStatementsRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "forbid-consecutive-null-statements",
      .topic = "redundant-semicolons",
      .desc =
          "Checks that there are no occurrences of "
          "consecutive null statements like `;;`",
  };
  return d;
}

void ForbidConsecutiveNullStatementsRule::HandleLeaf(
    const verible::SyntaxTreeLeaf &leaf, const SyntaxTreeContext &context) {
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
          violations_.insert(LintViolation(
              leaf, kMessage, context,
              {AutoFix("Remove superfluous semicolon", {leaf.get(), ""})}));
        } else {
          state_ = State::kNormal;
        }
        break;
      }
    }
  }
}

LintRuleStatus ForbidConsecutiveNullStatementsRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
