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

#include "verible/verilog/analysis/checkers/forbid-defparam-rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ForbidDefparamRule);

static constexpr absl::string_view kMessage = "Do not use defparam.";

const LintRuleDescriptor &ForbidDefparamRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "forbid-defparam",
      .topic = "module-instantiation",
      .desc = "Do not use defparam.",
  };
  return d;
}

// Matches the defparam construct.
static const Matcher &OverrideMatcher() {
  static const Matcher matcher(NodekParameterOverride());
  return matcher;
}

void ForbidDefparamRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  if (OverrideMatcher().Matches(symbol, &manager)) {
    const verible::SyntaxTreeLeaf *defparam =
        GetSubtreeAsLeaf(symbol, NodeEnum::kParameterOverride, 0);
    if (defparam) {
      const auto &defparam_token = defparam->get();
      CHECK_EQ(defparam_token.token_enum(), TK_defparam);
      violations_.insert(
          verible::LintViolation(defparam_token, kMessage, context));
    }
  }
}

verible::LintRuleStatus ForbidDefparamRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
