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

#include "verilog/analysis/checkers/legacy-generate-region-rule.h"

#include <set>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/analysis/lint-rule-status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/matcher.h"
#include "common/strings/naming_utils.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(LegacyGenerateRegionRule);

using verible::FindFirstSubtree;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::matcher::EqualTagPredicate;

static constexpr absl::string_view kMessage = "Do not use generate regions.";

const LintRuleDescriptor& LegacyGenerateRegionRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "legacy-generate-region",
      .topic = "generate-constructs",
      .desc = "Checks that there are no generate regions.",
  };
  return d;
}

void LegacyGenerateRegionRule::HandleNode(
    const verible::SyntaxTreeNode& node,
    const verible::SyntaxTreeContext& context) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  if (tag == NodeEnum::kGenerateRegion) {
    const auto* generate_keyword = ABSL_DIE_IF_NULL(FindFirstSubtree(
        &node, EqualTagPredicate<verible::SymbolKind::kLeaf, verilog_tokentype,
                                 verilog_tokentype::TK_generate>));
    const auto& leaf = verible::SymbolCastToLeaf(*generate_keyword);
    violations_.insert(LintViolation(leaf.get(), kMessage));
  }
}

LintRuleStatus LegacyGenerateRegionRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
