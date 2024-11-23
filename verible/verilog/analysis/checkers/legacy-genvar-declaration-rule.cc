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

#include "verible/verilog/analysis/checkers/legacy-genvar-declaration-rule.h"

#include <set>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(LegacyGenvarDeclarationRule);

using verible::LintRuleStatus;
using verible::LintViolation;

static constexpr absl::string_view kMessage =
    "Do not use separate genvar declaration.";

const LintRuleDescriptor &LegacyGenvarDeclarationRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "legacy-genvar-declaration",
      .topic = "generate-constructs",
      .desc = "Checks that there are no separate `genvar` declarations.",
  };
  return d;
}

void LegacyGenvarDeclarationRule::HandleNode(
    const verible::SyntaxTreeNode &node,
    const verible::SyntaxTreeContext &context) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  if (tag == NodeEnum::kGenvarDeclaration) {
    const auto identifier_matches = FindAllSymbolIdentifierLeafs(node);
    for (const auto &match : identifier_matches) {
      const auto &leaf = verible::SymbolCastToLeaf(*match.match);
      violations_.insert(LintViolation(leaf.get(), kMessage));
    }
  }
}

LintRuleStatus LegacyGenvarDeclarationRule::Report() const {
  return LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
