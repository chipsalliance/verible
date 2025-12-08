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

#include "verible/verilog/analysis/checkers/generate-label-rule.h"

#include <set>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/core-matchers.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(GenerateLabelRule);

static constexpr std::string_view kMessage =
    "All generate block statements must have a label";

const LintRuleDescriptor &GenerateLabelRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "generate-label",
      .topic = "generate-statements",
      .desc = "Checks that every generate block statement is labeled.",
  };
  return d;
}

// Matches against generate blocks that do not have a label
//
// For example:
//   if (TypeIsPosedge) begin
//      always @(posedge clk) foo <= bar;
//    end
//
static const Matcher &BlockMatcher() {
  static const Matcher matcher(
      NodekGenerateBlock(verible::matcher::Unless(HasBeginLabel())));
  return matcher;
}

void GenerateLabelRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (BlockMatcher().Matches(symbol, &manager)) {
    violations_.insert(verible::LintViolation(symbol, kMessage, context));
  }
}

verible::LintRuleStatus GenerateLabelRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
