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

#include "verible/verilog/analysis/checkers/instance-shadow-rule.h"

#include <iterator>
#include <set>
#include <sstream>

#include "absl/strings/string_view.h"
#include "verible/common/analysis/citation.h"
#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/matcher/bound-symbol-manager.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/iterator-adaptors.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

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

const LintRuleDescriptor &InstanceShadowRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "instance-shadowing",
      .topic = "mark-shadowed-instances",
      .desc =
          "Warns if there are multiple declarations in the same scope "
          "that shadow each other with the same name."};
  return d;
}

static const Matcher &InstanceShadowMatcher() {
  static const Matcher matcher(SymbolIdentifierLeaf());
  return matcher;
}

static bool isInAllowedNode(const SyntaxTreeContext &ctx) {
  return ctx.IsInside(NodeEnum::kSeqBlock) ||
         ctx.IsInside(NodeEnum::kGenvarDeclaration) ||
         ctx.IsInside(NodeEnum::kReference);  // ||
  //  ctx.IsInside(NodeEnum::kModportSimplePort);
}

void InstanceShadowRule::HandleSymbol(const verible::Symbol &symbol,
                                      const SyntaxTreeContext &context) {
  verible::matcher::BoundSymbolManager manager;
  bool omit_node = false;
  if (!InstanceShadowMatcher().Matches(symbol, &manager)) {
    return;
  }
  const auto labels = FindAllSymbolIdentifierLeafs(symbol);
  if (labels.empty()) return;
  // if the considered symbol name is not a declaration
  if (context.IsInside(NodeEnum::kReference)) return;

  // if the considered symbol name is in a modport, discard it
  if (context.IsInside(NodeEnum::kModportSimplePort) ||
      context.IsInside(NodeEnum::kModportClockingPortsDeclaration)) {
    return;
  }

  // TODO: don't latch on to K&R-Style form in which the same symbol shows
  // up twice.

  const auto rcontext = reversed_view(context);
  const auto *const rdirectParent = *std::next(rcontext.begin());
  // we are looking for the potential labels that might overlap the considered
  // declaration. We are searching all the labels within the visible scope
  // until we find the node or we reach the top of the scope
  for (const auto *node : rcontext) {
    for (const verible::SymbolPtr &child : node->children()) {
      if (child == nullptr) {
        continue;
      }
      const auto overlappingLabels = FindAllSymbolIdentifierLeafs(*child);
      for (const verible::TreeSearchMatch &omatch : overlappingLabels) {
        const auto &overlappingLabel = SymbolCastToLeaf(*omatch.match);
        // variable in different scopes or this is not a
        // vulnerable declaration
        if (isInAllowedNode(omatch.context)) {
          omit_node = true;
          break;
        }
        const auto &label = SymbolCastToLeaf(*labels[0].match);
        // if found label has the same adress as considered label
        // we found the same node so we don't
        // want to look further
        if (omatch.match == labels[0].match) {
          omit_node = true;
          break;
        }
        // if considered label is the last node
        // this is the ending node with label
        if (rdirectParent->back() == node->back()) {
          return;
        }

        if (overlappingLabel.get().text() == label.get().text()) {
          std::stringstream ss;
          ss << "Symbol `" << overlappingLabel.get().text()
             << "` is shadowing symbol `" << label.get().text()
             << "` defined at @";
          violations_.insert(LintViolation(symbol, ss.str(), context, {},
                                           {overlappingLabel.get()}));
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

LintRuleStatus InstanceShadowRule::Report() const {
  return LintRuleStatus(violations_, Name(), GetStyleGuideCitation(kTopic));
}
}  // namespace analysis
}  // namespace verilog
