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

#include "verilog/CST/package.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllPackageDeclarations(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekPackageDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllPackageImportItems(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekPackageImportItem());
}

const verible::TokenInfo& GetPackageNameToken(const verible::Symbol& s) {
  const auto& name_node = GetPackageNameLeaf(s);
  return name_node.get();
}

const verible::SyntaxTreeLeaf& GetPackageNameLeaf(const verible::Symbol& s) {
  return verible::GetSubtreeAsLeaf(s, NodeEnum::kPackageDeclaration, 2);
}

const verible::SyntaxTreeLeaf* GetPackageNameEndLabel(
    const verible::Symbol& package_declaration) {
  const auto* label_node = verible::GetSubtreeAsSymbol(
      package_declaration, NodeEnum::kPackageDeclaration, 6);
  if (label_node == nullptr) {
    return nullptr;
  }
  const auto& package_name = verible::GetSubtreeAsLeaf(
      verible::SymbolCastToNode(*label_node), NodeEnum::kLabel, 1);
  return &package_name;
}

const verible::Symbol* GetPackageItemList(
    const verible::Symbol& package_declaration) {
  return verible::GetSubtreeAsSymbol(package_declaration,
                                     NodeEnum::kPackageDeclaration, 4);
}

const verible::SyntaxTreeNode& GetScopePrefixFromPackageImportItem(
    const verible::Symbol& package_import_item) {
  return verible::GetSubtreeAsNode(package_import_item,
                                   NodeEnum::kPackageImportItem, 0,
                                   NodeEnum::kScopePrefix);
}

const verible::SyntaxTreeLeaf& GetImportedPackageName(
    const verible::Symbol& package_import_item) {
  return verible::GetSubtreeAsLeaf(
      GetScopePrefixFromPackageImportItem(package_import_item),
      NodeEnum::kScopePrefix, 0);
}

const verible::SyntaxTreeLeaf* GeImportedItemNameFromPackageImportItem(
    const verible::Symbol& package_import_item) {
  const auto& imported_item = verible::GetSubtreeAsLeaf(
      package_import_item, NodeEnum::kPackageImportItem, 1);

  if (imported_item.get().token_enum() != verilog_tokentype::SymbolIdentifier) {
    return nullptr;
  }

  return &imported_item;
}

}  // namespace verilog
