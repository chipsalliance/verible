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

#include "verilog/CST/net.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

using verible::Symbol;
using verible::TokenInfo;

std::vector<verible::TreeSearchMatch> FindAllNetDeclarations(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekNetDeclaration());
}

// Helper predicate to match all types of applicable nets
static bool ExpectedTagPredicate(const Symbol& symbol) {
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

const verible::SyntaxTreeLeaf& GetNameLeafOfNetVariable(
    const verible::Symbol& net_variable) {
  return verible::GetSubtreeAsLeaf(net_variable, NodeEnum::kNetVariable, 0);
}

const verible::SyntaxTreeLeaf& GetNameLeafOfRegisterVariable(
    const verible::Symbol& register_variable) {
  return verible::GetSubtreeAsLeaf(register_variable,
                                   NodeEnum::kRegisterVariable, 0);
}

std::vector<const TokenInfo*> GetIdentifiersFromNetDeclaration(
    const Symbol& symbol) {
  // TODO: re-implement this without search, instead using direct access for
  // efficiency.
  std::vector<const TokenInfo*> identifiers;

  auto matcher = verible::matcher::Matcher(ExpectedTagPredicate,
                                           verible::matcher::InnerMatchAll);

  std::vector<verible::TreeSearchMatch> identifier_nodes =
      SearchSyntaxTree(symbol, matcher);

  for (auto& id : identifier_nodes) {
    const auto* identifier = SymbolCastToNode(*id.match)[0].get();

    const auto* identifier_leaf =
        AutoUnwrapIdentifier(*ABSL_DIE_IF_NULL(identifier));

    if (identifier_leaf == nullptr) {
      continue;
    }

    identifiers.push_back(&identifier_leaf->get());
  }

  return identifiers;
}

}  // namespace verilog
