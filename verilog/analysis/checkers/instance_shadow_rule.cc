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

#include "verilog/analysis/checkers/instance_shadow_rule.h"

#include <set>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/analysis/citation.h"
#include "common/analysis/lint_rule_status.h"
#include "common/analysis/matcher/bound_symbol_manager.h"
#include "common/analysis/matcher/core_matchers.h"
#include "common/analysis/matcher/matcher.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/descriptions.h"
#include "verilog/analysis/lint_rule_registry.h"

namespace verilog {
namespace analysis {

using verible::GetStyleGuideCitation;
using verible::LintRuleStatus;
using verible::LintViolation;
using verible::SyntaxTreeContext;
using verible::matcher::Matcher;

// Register InstanceShadowRule
VERILOG_REGISTER_LINT_RULE(InstanceShadowRule);

absl::string_view InstanceShadowRule::Name() { return "instance-shadowing"; }
const char InstanceShadowRule::kTopic[] = "mark-shadowed-instances";
const char InstanceShadowRule::kMessage[] =
    "Instance shadows the already declared variable";

std::string InstanceShadowRule::GetDescription(
    DescriptionType description_type) {
  return absl::StrCat(
      "Checks that any defined variable does not shadow other variable in the "
      "same scope",
      GetStyleGuideCitation(kTopic), ".");
}

static const Matcher& InstanceShadowMatcher() {
  static const Matcher matcher(SymbolIdentifierLeaf());
  return matcher;
}
static bool isInAllowedNode(const SyntaxTreeContext& ctx) {
  return ctx.IsInside(NodeEnum::kSeqBlock) ||
         ctx.IsInside(NodeEnum::kGenvarDeclaration) ||
         ctx.IsInside(NodeEnum::kReference);
}

void InstanceShadowRule::HandleSymbol(const verible::Symbol& symbol,
                                      const SyntaxTreeContext& context) {
  verible::matcher::BoundSymbolManager manager;
  bool omit_node = false;
  if (InstanceShadowMatcher().Matches(symbol, &manager)) {
    const auto& labels = FindAllSymbolIdentifierLeafs(symbol);
    if (labels.empty()) return;
    // if the considered symbol name is not a declaration
    if (context.IsInside(NodeEnum::kReference)) return;

    const auto& rcontext = reversed_view(context);
    const auto& rdirectParent = *std::next(rcontext.begin());
    // we are looking for the potential labels that might overlap the considered
    // declaration. We are searching all the labels within the visible scope
    // until we find the node or we reach the top of the scope
    for (auto rc = rcontext.begin(); rc != rcontext.end(); rc++) {
      const auto& node = *rc;

      for (const auto& ch : node->children()) {
        if (ch == nullptr) {
          continue;
        }
        const auto& overlappingLabels = FindAllSymbolIdentifierLeafs(*ch);
        for (auto& omatch : overlappingLabels) {
          const auto& overlappingLabel = SymbolCastToLeaf(*omatch.match);
          // variable in different scopes or this is not a
          // vulnerable declaration
          if (isInAllowedNode(omatch.context)) {
            omit_node = true;
            break;
          }
          const auto& label = SymbolCastToLeaf(*labels[0].match);
          // if found label has the same adress as considered label
          // we found the same node so we don't
          // want to look further
          if (omatch.match == labels[0].match) {
            omit_node = true;
            break;
          }
          // if considered label is the last node
          // this is the ending node with label
          if (rdirectParent->children().back().get() ==
              node->children().back().get()) {
            return;
          }

          if (overlappingLabel.get().text() == label.get().text()) {
            violations_.insert(LintViolation(symbol, kMessage, context));
            return;
          }
        }
        if (omit_node) {
          omit_node = false;
          break;
        }
      }
    }
  }
}

LintRuleStatus InstanceShadowRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}
}  // namespace analysis
}  // namespace verilog
