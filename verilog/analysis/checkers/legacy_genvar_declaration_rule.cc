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

#include "verilog/analysis/checkers/legacy_genvar_declaration_rule.h"

#include <set>
#include <string_view>

#include "common/analysis/lint_rule_status.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

VERILOG_REGISTER_LINT_RULE(LegacyGenvarDeclarationRule);

using verible::LintRuleStatus;
using verible::LintViolation;

static constexpr std::string_view kMessage =
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
