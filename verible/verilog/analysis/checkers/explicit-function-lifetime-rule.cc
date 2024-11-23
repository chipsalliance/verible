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

#include "verible/verilog/analysis/checkers/explicit-function-lifetime-rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/functions.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using Matcher = verible::matcher::Matcher;

// Register ExplicitFunctionLifetimeRule
VERILOG_REGISTER_LINT_RULE(ExplicitFunctionLifetimeRule);

static constexpr absl::string_view kMessage =
    "Explicitly define static or automatic lifetime for non-class functions";

const LintRuleDescriptor &ExplicitFunctionLifetimeRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "explicit-function-lifetime",
      .topic = "function-task-explicit-lifetime",
      .desc =
          "Checks that every function declared outside of a class is "
          "declared with an explicit lifetime (static or automatic).",
  };
  return d;
}

static const Matcher &FunctionMatcher() {
  static const Matcher matcher(NodekFunctionDeclaration());
  return matcher;
}

void ExplicitFunctionLifetimeRule::HandleSymbol(
    const verible::Symbol &symbol, const SyntaxTreeContext &context) {
  // Don't need to check for lifetime declaration if context is inside a class
  if (ContextIsInsideClass(context)) return;

  verible::matcher::BoundSymbolManager manager;
  if (FunctionMatcher().Matches(symbol, &manager)) {
    // If function id is qualified, it is an out-of-line
    // class method definition, which is also exempt.
    const auto *function_id = ABSL_DIE_IF_NULL(GetFunctionId(symbol));
    if (IdIsQualified(*function_id)) return;

    // Make sure the lifetime was set
    if (GetFunctionLifetime(symbol) == nullptr) {
      // Point to the function id.
      const verible::TokenInfo token(SymbolIdentifier,
                                     verible::StringSpanOfSymbol(*function_id));
      violations_.insert(LintViolation(token, kMessage, context));
    }
  }
}

LintRuleStatus ExplicitFunctionLifetimeRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
