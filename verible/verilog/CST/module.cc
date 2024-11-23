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

#include "verible/verilog/CST/module.h"

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

using verible::Symbol;
using verible::SyntaxTreeNode;
using verible::TokenInfo;

std::vector<verible::TreeSearchMatch> FindAllModuleDeclarations(
    const Symbol &root) {
  return SearchSyntaxTree(root, NodekModuleDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllModuleHeaders(const Symbol &root) {
  return SearchSyntaxTree(root, NodekModuleHeader());
}

std::vector<verible::TreeSearchMatch> FindAllInterfaceDeclarations(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekInterfaceDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllProgramDeclarations(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekProgramDeclaration());
}

bool IsModuleOrInterfaceOrProgramDeclaration(
    const SyntaxTreeNode &declaration) {
  return declaration.MatchesTagAnyOf({NodeEnum::kModuleDeclaration,
                                      NodeEnum::kInterfaceDeclaration,
                                      NodeEnum::kProgramDeclaration});
}

const SyntaxTreeNode *GetModuleHeader(const Symbol &module_declaration) {
  if (module_declaration.Kind() != verible::SymbolKind::kNode) return nullptr;
  const SyntaxTreeNode &module_node =
      verible::SymbolCastToNode(module_declaration);
  if (!IsModuleOrInterfaceOrProgramDeclaration(module_node)) return nullptr;
  if (module_node.empty()) return nullptr;
  return &verible::SymbolCastToNode(*module_node[0].get());
}

const SyntaxTreeNode *GetInterfaceHeader(const Symbol &module_symbol) {
  return verible::GetSubtreeAsNode(module_symbol,
                                   NodeEnum::kInterfaceDeclaration, 0,
                                   NodeEnum::kModuleHeader);
}

const verible::SyntaxTreeLeaf *GetModuleName(const Symbol &s) {
  const auto *header_node = GetModuleHeader(s);
  if (!header_node) return nullptr;
  return verible::GetSubtreeAsLeaf(*header_node, NodeEnum::kModuleHeader, 2);
}

const TokenInfo *GetInterfaceNameToken(const Symbol &s) {
  const auto *header_node = GetInterfaceHeader(s);
  if (!header_node) return nullptr;
  const verible::SyntaxTreeLeaf *name_leaf =
      verible::GetSubtreeAsLeaf(*header_node, NodeEnum::kModuleHeader, 2);
  return name_leaf ? &name_leaf->get() : nullptr;
}

const SyntaxTreeNode *GetModulePortParenGroup(
    const Symbol &module_declaration) {
  const auto *header_node = GetModuleHeader(module_declaration);
  if (!header_node) return nullptr;
  const auto *ports =
      verible::GetSubtreeAsSymbol(*header_node, NodeEnum::kModuleHeader, 5);
  return verible::CheckOptionalSymbolAsNode(ports, NodeEnum::kParenGroup);
}

const SyntaxTreeNode *GetModulePortDeclarationList(
    const Symbol &module_declaration) {
  const auto *paren_group = GetModulePortParenGroup(module_declaration);
  if (verible::CheckOptionalSymbolAsNode(paren_group, NodeEnum::kParenGroup) ==
      nullptr) {
    return nullptr;
  }
  return verible::GetSubtreeAsNode(*paren_group, NodeEnum::kParenGroup, 1,
                                   NodeEnum::kPortDeclarationList);
}

const verible::SyntaxTreeLeaf *GetModuleEndLabel(
    const verible::Symbol &module_declaration) {
  const SyntaxTreeNode &module_node =
      verible::SymbolCastToNode(module_declaration);
  CHECK(IsModuleOrInterfaceOrProgramDeclaration(module_node));

  const auto *label_node = module_node[3].get();
  if (label_node == nullptr) {
    return nullptr;
  }
  return verible::GetSubtreeAsLeaf(verible::SymbolCastToNode(*label_node),
                                   NodeEnum::kLabel, 1);
}

const verible::SyntaxTreeNode *GetModuleItemList(
    const verible::Symbol &module_declaration) {
  if (module_declaration.Kind() != verible::SymbolKind::kNode) return nullptr;
  const SyntaxTreeNode &module_node =
      verible::SymbolCastToNode(module_declaration);
  if (!IsModuleOrInterfaceOrProgramDeclaration(module_node)) return nullptr;
  if (module_node.size() < 2) return nullptr;
  verible::Symbol *item = module_node[1].get();
  return item ? &verible::SymbolCastToNode(*item) : nullptr;
}

const verible::SyntaxTreeNode *GetParamDeclarationListFromModuleDeclaration(
    const verible::Symbol &module_declaration) {
  const auto *header_node = GetModuleHeader(module_declaration);
  if (!header_node) return nullptr;
  const verible::Symbol *param_declaration_list =
      verible::GetSubtreeAsSymbol(*header_node, NodeEnum::kModuleHeader, 4);
  return verible::CheckOptionalSymbolAsNode(
      param_declaration_list, NodeEnum::kFormalParameterListDeclaration);
}

const verible::SyntaxTreeNode *GetParamDeclarationListFromInterfaceDeclaration(
    const verible::Symbol &interface_declaration) {
  const auto *header_node = GetInterfaceHeader(interface_declaration);
  if (!header_node) return nullptr;
  const verible::Symbol *param_declaration_list =
      verible::GetSubtreeAsSymbol(*header_node, NodeEnum::kModuleHeader, 4);
  return verible::CheckOptionalSymbolAsNode(
      param_declaration_list, NodeEnum::kFormalParameterListDeclaration);
}

}  // namespace verilog
