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

#include "verilog/analysis/checkers/module-begin-block-rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound-symbol-manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax-tree-context.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(ModuleBeginBlockRule);

static constexpr absl::string_view kMessage =
    "Module-level begin-end blocks are not LRM-valid syntax.";

const LintRuleDescriptor& ModuleBeginBlockRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "module-begin-block",
      .topic = "floating-begin-end-blocks",
      .desc =
          "Checks that there are no begin-end blocks declared at the module "
          "level.",
  };
  return d;
}

// Matches begin-end blocks at the module-item level.
static const Matcher& BlockMatcher() {
  static const Matcher matcher(NodekModuleBlock());
  return matcher;
}

void ModuleBeginBlockRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (BlockMatcher().Matches(symbol, &manager)) {
    violations_.insert(verible::LintViolation(symbol, kMessage, context));
  }
}

verible::LintRuleStatus ModuleBeginBlockRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
