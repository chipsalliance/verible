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

#include "verilog/CST/type.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllDataTypeDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekDataType());
}

std::vector<verible::TreeSearchMatch> FindAllTypeDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekTypeDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllEnumDataTypeDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekEnumDataType());
}

std::vector<verible::TreeSearchMatch> FindAllStructDataTypeDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekStructDataType());
}

std::vector<verible::TreeSearchMatch> FindAllUnionDataTypeDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekUnionDataType());
}

bool IsStorageTypeOfDataTypeSpecified(const verible::Symbol& symbol) {
  const auto* storage =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kDataType, 0);
  return (storage != nullptr);
}

const verible::SyntaxTreeLeaf* GetIdentifierFromTypeDeclaration(
    const verible::Symbol& symbol) {
  // For enum, struct and union identifier is found at the same position
  const auto* identifier_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kTypeDeclaration, 3);
  return AutoUnwrapIdentifier(*ABSL_DIE_IF_NULL(identifier_symbol));
}

}  // namespace verilog
