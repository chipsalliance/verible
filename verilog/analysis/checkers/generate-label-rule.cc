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

#include "verilog/analysis/checkers/generate-label-rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/lint-rule-status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(GenerateLabelRule);

static constexpr absl::string_view kMessage =
    "All generate block statements must have a label";

const LintRuleDescriptor& GenerateLabelRule::GetDescriptor() {
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
static const Matcher& BlockMatcher() {
  static const Matcher matcher(
      NodekGenerateBlock(verible::matcher::Unless(HasBeginLabel())));
  return matcher;
}

void GenerateLabelRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
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
