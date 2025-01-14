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

#include "verible/verilog/analysis/checkers/v2001-generate-begin-rule.h"

#include <set>
#include <string>
#include <string_view>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::LintViolation;
using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(V2001GenerateBeginRule);

static constexpr std::string_view kMessage =
    "Do not begin a generate block inside a generate region.";

const LintRuleDescriptor &V2001GenerateBeginRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "v2001-generate-begin",
      .topic = "generate-constructs",
      .desc =
          "Checks that there are no generate-begin blocks inside a "
          "generate region.",
  };
  return d;
}

static const Matcher &GenerateRegionMatcher() {
  static const Matcher matcher(
      NodekGenerateRegion(HasGenerateBlock().Bind("block")));
  return matcher;
}

void V2001GenerateBeginRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (GenerateRegionMatcher().Matches(symbol, &manager)) {
    if (const auto *block = manager.GetAs<verible::SyntaxTreeNode>("block")) {
      violations_.insert(LintViolation(verible::GetLeftmostLeaf(*block)->get(),
                                       kMessage, context));
    }
  }
}

verible::LintRuleStatus V2001GenerateBeginRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
