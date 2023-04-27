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

#include "verilog/analysis/checkers/case-missing-default-rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/lint-rule-status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintRuleStatus;
using verible::LintViolation;
using verible::matcher::Matcher;

// Register CaseMissingDefaultRule
VERILOG_REGISTER_LINT_RULE(CaseMissingDefaultRule);

static constexpr absl::string_view kMessage =
    "Explicitly define a default case for every case statement.";

const LintRuleDescriptor& CaseMissingDefaultRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "case-missing-default",
      .topic = "case-statements",
      .desc = "Checks that a default case-item is always defined.",
  };
  return d;
}

static const Matcher& CaseMatcher() {
  static const Matcher matcher(
      NodekCaseItemList(verible::matcher::Unless(HasDefaultCase())));
  return matcher;
}

void CaseMissingDefaultRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (context.DirectParentIs(NodeEnum::kCaseStatement) &&
      CaseMatcher().Matches(symbol, &manager)) {
    violations_.insert(LintViolation(symbol, kMessage, context));
  }
}

LintRuleStatus CaseMissingDefaultRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
