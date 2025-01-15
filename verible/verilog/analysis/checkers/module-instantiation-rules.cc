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

#include "verible/verilog/analysis/checkers/module-instantiation-rules.h"

#include <algorithm>
#include <memory>
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
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/context-functions.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/descriptions.h"
#include "verible/verilog/analysis/lint-rule-registry.h"

namespace verilog {
namespace analysis {

using verible::down_cast;
using verible::matcher::Matcher;

// Register the linter rule
VERILOG_REGISTER_LINT_RULE(ModuleParameterRule);
VERILOG_REGISTER_LINT_RULE(ModulePortRule);

const LintRuleDescriptor &ModuleParameterRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "module-parameter",
      .topic = "module-instantiation",
      .desc =
          "Checks that module instantiations with more than one parameter "
          "are passed in as named parameters, rather than positional "
          "parameters.",
  };
  return d;
}

const LintRuleDescriptor &ModulePortRule::GetDescriptor() {
  static const LintRuleDescriptor d{
      .name = "module-port",
      .topic = "module-instantiation",
      .desc =
          "Checks that module instantiations with more than one port are "
          "passed in as named ports, rather than positional ports.",
  };
  return d;
}

// Matches against a gate instance with a port list and bind that port list
// to "list".
// For example:
//   foo bar (port1, port2);
// Here, the node representing "port1, port2" will be bound to "list"
static const Matcher &InstanceMatcher() {
  static const Matcher matcher(
      NodekGateInstance(GateInstanceHasPortList().Bind("list")));
  return matcher;
}

// Matches against a parameter list that has positional parameters
// For examples:
//   foo #(1, 2) bar;
// Here, the node representing "1, 2" will be bound to "list".
static const Matcher &ParamsMatcher() {
  static const Matcher matcher(NodekActualParameterList(
      ActualParameterListHasPositionalParameterList().Bind("list")));
  return matcher;
}

static bool IsComma(const verible::Symbol &symbol) {
  if (symbol.Kind() == verible::SymbolKind::kLeaf) {
    const auto *leaf = down_cast<const verible::SyntaxTreeLeaf *>(&symbol);
    if (leaf) return leaf->get().token_enum() == ',';
  }
  return false;
}

static bool IsAnyPort(const verible::Symbol *symbol) {
  if (symbol->Kind() == verible::SymbolKind::kNode) {
    const auto *node = down_cast<const verible::SyntaxTreeNode *>(symbol);
    return node->MatchesTag(NodeEnum::kActualNamedPort) ||
           node->MatchesTag(NodeEnum::kActualPositionalPort);
  }
  return false;
}

//
// ModuleParameterRule Implementation
//

void ModuleParameterRule::HandleSymbol(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context) {
  static constexpr std::string_view kMessage =
      "Pass named parameters for parameterized module instantiations with "
      "more than one parameter";

  // Syntactically, class instances are indistinguishable from module instances
  // (they look like generic types), however, module instances can only occur
  // inside module definitions.  Anywhere outside of a module can be skipped.
  if (!ContextIsInsideModule(context)) return;

  verible::matcher::BoundSymbolManager manager;
  if (ParamsMatcher().Matches(symbol, &manager)) {
    if (const auto *list = manager.GetAs<verible::SyntaxTreeNode>("list")) {
      const auto &children = list->children();
      auto parameter_count = std::count_if(
          children.begin(), children.end(),
          [](const verible::SymbolPtr &n) { return n ? !IsComma(*n) : false; });

      // One positional parameter is permitted, but any more require all
      // parameters to be named.
      if (parameter_count > 1) {  // Determine the spanning location
        const auto *leaf_ptr = verible::GetLeftmostLeaf(*list);
        const verible::TokenInfo token = ABSL_DIE_IF_NULL(leaf_ptr)->get();
        violations_.insert(verible::LintViolation(token, kMessage, context));
      }
    }
  }
}

verible::LintRuleStatus ModuleParameterRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

//
// ModulePortRule Implementation
//

void ModulePortRule::HandleSymbol(const verible::Symbol &symbol,
                                  const verible::SyntaxTreeContext &context) {
  static constexpr std::string_view kMessage =
      "Use named ports for module instantiation with "
      "more than one port";

  verible::matcher::BoundSymbolManager manager;

  if (InstanceMatcher().Matches(symbol, &manager)) {
    if (const auto *port_list_node =
            manager.GetAs<verible::SyntaxTreeNode>("list")) {
      // Don't know how to handle unexpected non-portlist, so proceed
      if (!port_list_node->MatchesTag(NodeEnum::kPortActualList)) return;

      if (!IsPortListCompliant(*port_list_node)) {
        // Determine the leftmost location
        const auto *leaf_ptr = verible::GetLeftmostLeaf(*port_list_node);
        const verible::TokenInfo token = ABSL_DIE_IF_NULL(leaf_ptr)->get();
        violations_.insert(verible::LintViolation(token, kMessage, context));
      }
    }
  }
}

// Checks that if node violates this rule
// Returns true if node has either 1 or 0 ports or
// contains all named ports
// Returns false if node has 2 or more children and at least
// one child is an unnamed positional port
/* static */ bool ModulePortRule::IsPortListCompliant(
    const verible::SyntaxTreeNode &port_list_node) {
  const auto &children = port_list_node.children();
  auto port_count = std::count_if(
      children.begin(), children.end(),
      [](const verible::SymbolPtr &n) { return IsAnyPort(n.get()); });

  // Do not enforce rule if node has 0 or 1 ports
  if (port_count <= 1) {
    return true;
  }

  for (const auto &child : port_list_node.children()) {
    if (child == nullptr) continue;

    // If child is a node, then it must be a kPort
    if (child->Kind() == verible::SymbolKind::kNode) {
      const auto *child_node =
          down_cast<const verible::SyntaxTreeNode *>(child.get());
      if (child_node->MatchesTag(NodeEnum::kActualPositionalPort)) return false;
    }
  }

  return true;
}

verible::LintRuleStatus ModulePortRule::Report() const {
  return verible::LintRuleStatus(violations_, GetDescriptor());
}

}  // namespace analysis
}  // namespace verilog
