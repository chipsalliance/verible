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

#include "verible/verilog/CST/package.h"

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllPackageDeclarations(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekPackageDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllPackageImportItems(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekPackageImportItem());
}

const verible::TokenInfo *GetPackageNameToken(const verible::Symbol &s) {
  const auto *package_name = GetPackageNameLeaf(s);
  if (!package_name) return nullptr;
  return &GetPackageNameLeaf(s)->get();
}

const verible::SyntaxTreeLeaf *GetPackageNameLeaf(const verible::Symbol &s) {
  return verible::GetSubtreeAsLeaf(s, NodeEnum::kPackageDeclaration, 2);
}

const verible::SyntaxTreeLeaf *GetPackageNameEndLabel(
    const verible::Symbol &package_declaration) {
  const auto *label_node = verible::GetSubtreeAsSymbol(
      package_declaration, NodeEnum::kPackageDeclaration, 6);
  if (label_node == nullptr) {
    return nullptr;
  }
  return verible::GetSubtreeAsLeaf(verible::SymbolCastToNode(*label_node),
                                   NodeEnum::kLabel, 1);
}

const verible::Symbol *GetPackageItemList(
    const verible::Symbol &package_declaration) {
  return verible::GetSubtreeAsSymbol(package_declaration,
                                     NodeEnum::kPackageDeclaration, 4);
}

const verible::SyntaxTreeNode *GetScopePrefixFromPackageImportItem(
    const verible::Symbol &package_import_item) {
  return verible::GetSubtreeAsNode(package_import_item,
                                   NodeEnum::kPackageImportItem, 0,
                                   NodeEnum::kScopePrefix);
}

const verible::SyntaxTreeLeaf *GetImportedPackageName(
    const verible::Symbol &package_import_item) {
  const auto *prefix = GetScopePrefixFromPackageImportItem(package_import_item);
  if (!prefix) return nullptr;
  return verible::GetSubtreeAsLeaf(*prefix, NodeEnum::kScopePrefix, 0);
}

const verible::SyntaxTreeLeaf *GeImportedItemNameFromPackageImportItem(
    const verible::Symbol &package_import_item) {
  const verible::SyntaxTreeLeaf *imported_item = verible::GetSubtreeAsLeaf(
      package_import_item, NodeEnum::kPackageImportItem, 1);

  if (!imported_item || imported_item->get().token_enum() !=
                            verilog_tokentype::SymbolIdentifier) {
    return nullptr;
  }

  return imported_item;
}

}  // namespace verilog
