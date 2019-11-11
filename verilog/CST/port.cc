// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/CST/port.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "common/util/logging.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

using verible::Symbol;
using verible::SymbolKind;
using verible::SyntaxTreeLeaf;

std::vector<verible::TreeSearchMatch> FindAllModulePortDeclarations(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekPortDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllTaskFunctionPortDeclarations(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekPortItem());
}

const SyntaxTreeLeaf* GetIdentifierFromModulePortDeclaration(
    const Symbol& symbol) {
  // Assert that symbol is a port declaration.
  CHECK_EQ(symbol.Kind(), SymbolKind::kNode);
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kPortDeclaration);

  // Get the identifier symbol
  constexpr auto kIdentifierIdx = 3;
  const auto& node = verible::SymbolCastToNode(symbol);
  const auto* identifier_symbol = node[kIdentifierIdx].get();

  return AutoUnwrapIdentifier(*ABSL_DIE_IF_NULL(identifier_symbol));
}

static const Symbol* GetTypeIdDimensionsFromTaskFunctionPortItem(
    const Symbol& symbol) {
  CHECK_EQ(symbol.Kind(), SymbolKind::kNode);
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kPortItem);
  const auto& node = verible::SymbolCastToNode(symbol);
  return node[1].get();
}

const Symbol* GetTypeOfTaskFunctionPortItem(const verible::Symbol& symbol) {
  const Symbol* type_id_dimensions =
      ABSL_DIE_IF_NULL(GetTypeIdDimensionsFromTaskFunctionPortItem(symbol));
  const auto& node = verible::SymbolCastToNode(*type_id_dimensions);
  const Symbol* data_type = node[0].get();
  const auto& data_type_node = verible::SymbolCastToNode(*data_type);
  CHECK_EQ(NodeEnum(data_type_node.Tag().tag), NodeEnum::kDataType);
  return data_type;
}

const SyntaxTreeLeaf* GetIdentifierFromTaskFunctionPortItem(
    const verible::Symbol& symbol) {
  const Symbol* type_id_dimensions =
      ABSL_DIE_IF_NULL(GetTypeIdDimensionsFromTaskFunctionPortItem(symbol));
  const auto& node = verible::SymbolCastToNode(*type_id_dimensions);
  return AutoUnwrapIdentifier(*ABSL_DIE_IF_NULL(node[1].get()));
}

}  // namespace verilog
