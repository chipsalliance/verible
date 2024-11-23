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

#include "verible/verilog/CST/net.h"

#include <vector>

#include "verible/common/analysis/matcher/inner-match-handlers.h"
#include "verible/common/analysis/matcher/matcher.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

using verible::Symbol;
using verible::TokenInfo;

std::vector<verible::TreeSearchMatch> FindAllNetDeclarations(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekNetDeclaration());
}

// Helper predicate to match all types of applicable nets
static bool ExpectedTagPredicate(const Symbol &symbol) {
  verible::SymbolTag var_symbol = {verible::SymbolKind::kNode,
                                   static_cast<int>(NodeEnum::kNetVariable)};
  verible::SymbolTag assign_symbol = {
      verible::SymbolKind::kNode,
      static_cast<int>(NodeEnum::kNetDeclarationAssignment)};

  // This exploits the fact that net identifiers can be found in:
  // - kNetVariable, e.g.:
  //     module top; wire x; endmodule;
  //
  // - as well as kNetDeclarationAssignment, e.g.:
  //     module top; wire x = 1; endmodule;

  return symbol.Tag() == var_symbol || symbol.Tag() == assign_symbol;
}

const verible::SyntaxTreeLeaf *GetNameLeafOfNetVariable(
    const verible::Symbol &net_variable) {
  return verible::GetSubtreeAsLeaf(net_variable, NodeEnum::kNetVariable, 0);
}

const verible::SyntaxTreeLeaf *GetNameLeafOfRegisterVariable(
    const verible::Symbol &register_variable) {
  return verible::GetSubtreeAsLeaf(register_variable,
                                   NodeEnum::kRegisterVariable, 0);
}

std::vector<const TokenInfo *> GetIdentifiersFromNetDeclaration(
    const Symbol &symbol) {
  // TODO: re-implement this without search, instead using direct access for
  // efficiency.
  std::vector<const TokenInfo *> identifiers;

  auto matcher = verible::matcher::Matcher(ExpectedTagPredicate,
                                           verible::matcher::InnerMatchAll);

  std::vector<verible::TreeSearchMatch> identifier_nodes =
      SearchSyntaxTree(symbol, matcher);

  for (auto &id : identifier_nodes) {
    const auto *identifier = SymbolCastToNode(*id.match)[0].get();
    if (!identifier) continue;
    const auto *identifier_leaf = AutoUnwrapIdentifier(*identifier);
    if (!identifier_leaf) continue;

    identifiers.push_back(&identifier_leaf->get());
  }

  return identifiers;
}

}  // namespace verilog
