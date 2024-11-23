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

#include "verible/verilog/analysis/checkers/explicit-task-lifetime-rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/tasks.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Register ExplicitTaskLifetimeRule
VERILOG_REGISTER_LINT_RULE(ExplicitTaskLifetimeRule);

static constexpr absl::string_view kMessage =
    "Explicitly define static or automatic lifetime for non-class tasks";

const LintRuleDescriptor &ExplicitTaskLifetimeRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "explicit-task-lifetime",
      .topic = "function-task-explicit-lifetime",
      .desc =
          "Checks that every task declared outside of a class is declared "
          "with an explicit lifetime (static or automatic).",
  };
  return d;
}

static const Matcher &TaskMatcher() {
  static const Matcher matcher(NodekTaskDeclaration());
  return matcher;
}

void ExplicitTaskLifetimeRule::HandleSymbol(const verible::Symbol &symbol,
                                            const SyntaxTreeContext &context) {
  // Don't need to check for lifetime declaration if context is inside a class
  if (ContextIsInsideClass(context)) return;

  verible::matcher::BoundSymbolManager manager;
  if (TaskMatcher().Matches(symbol, &manager)) {
    // If task id is qualified, it is an out-of-line
    // class task definition, which is also exempt.
    const auto *task_id = GetTaskId(symbol);
    if (IdIsQualified(*task_id)) return;

    // Make sure the lifetime was set
    if (GetTaskLifetime(symbol) == nullptr) {
      // Point to the task id.
      const verible::TokenInfo token(SymbolIdentifier,
                                     verible::StringSpanOfSymbol(*task_id));
      violations_.insert(LintViolation(token, kMessage, context));
    }
  }
}

LintRuleStatus ExplicitTaskLifetimeRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
