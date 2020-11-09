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

#include "verilog/analysis/checkers/v2001_generate_begin_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintViolation;
using verible::matcher::Matcher;

// Register the lint rule
VERILOG_REGISTER_LINT_RULE(V2001GenerateBeginRule);

absl::string_view V2001GenerateBeginRule::Name() {
  return "v2001-generate-begin";
}
const char V2001GenerateBeginRule::kTopic[] = "generate-constructs";
const char V2001GenerateBeginRule::kMessage[] =
    "Do not begin a generate block inside a generate region.";

static const Matcher& GenerateRegionMatcher() {
  static const Matcher matcher(
      NodekGenerateRegion(HasGenerateBlock().Bind("block")));
  return matcher;
}

std::string V2001GenerateBeginRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that there are no generate-begin blocks inside a generate "
      "region. See ",
      GetStyleGuideCitation(kTopic), ".");
}

void V2001GenerateBeginRule::HandleSymbol(
    const verible::Symbol& symbol, const verible::SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  if (GenerateRegionMatcher().Matches(symbol, &manager)) {
    if (const auto* block = manager.GetAs<verible::SyntaxTreeNode>("block")) {
      violations_.insert(LintViolation(verible::GetLeftmostLeaf(*block)->get(),
                                       kMessage, context));
    }
  }
}

verible::LintRuleStatus V2001GenerateBeginRule::Report() const {
  return verible::LintRuleStatus(violations_, Name(),
                                 GetStyleGuideCitation(kTopic));
}

}  // namespace analysis
}  // namespace verilog
