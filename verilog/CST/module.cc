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

#include "verilog/CST/module.h"

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

using verible::Symbol;
using verible::SyntaxTreeNode;
using verible::TokenInfo;

std::vector<verible::TreeSearchMatch> FindAllModuleDeclarations(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekModuleDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllInterfaceDeclarations(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekInterfaceDeclaration());
}

const SyntaxTreeNode& GetModuleHeader(const Symbol& module_symbol) {
  return verible::GetSubtreeAsNode(module_symbol, NodeEnum::kModuleDeclaration,
                                   0, NodeEnum::kModuleHeader);
}

const SyntaxTreeNode& GetInterfaceHeader(const Symbol& module_symbol) {
  return verible::GetSubtreeAsNode(module_symbol,
                                   NodeEnum::kInterfaceDeclaration, 0,
                                   NodeEnum::kModuleHeader);
}

const TokenInfo& GetModuleNameToken(const Symbol& s) {
  const auto& header_node = GetModuleHeader(s);
  const auto& name_leaf =
      verible::GetSubtreeAsLeaf(header_node, NodeEnum::kModuleHeader, 2);
  return name_leaf.get();
}

const TokenInfo& GetInterfaceNameToken(const Symbol& s) {
  const auto& header_node = GetInterfaceHeader(s);
  const auto& name_leaf =
      verible::GetSubtreeAsLeaf(header_node, NodeEnum::kModuleHeader, 2);
  return name_leaf.get();
}

const SyntaxTreeNode* GetModulePortParenGroup(
    const Symbol& module_declaration) {
  const auto& header_node = GetModuleHeader(module_declaration);
  const auto* ports =
      verible::GetSubtreeAsSymbol(header_node, NodeEnum::kModuleHeader, 5);
  return verible::CheckOptionalSymbolAsNode(ports, NodeEnum::kParenGroup);
}

const SyntaxTreeNode* GetModulePortDeclarationList(
    const Symbol& module_declaration) {
  const auto* paren_group = GetModulePortParenGroup(module_declaration);
  if (verible::CheckOptionalSymbolAsNode(paren_group, NodeEnum::kParenGroup) ==
      nullptr) {
    return nullptr;
  }
  return &verible::GetSubtreeAsNode(*paren_group, NodeEnum::kParenGroup, 1,
                                    NodeEnum::kPortDeclarationList);
}

const TokenInfo* GetModuleEndLabel(const verible::Symbol& s) {
  const auto* label_node =
      verible::GetSubtreeAsSymbol(s, NodeEnum::kModuleDeclaration, 3);
  if (label_node == nullptr) {
    return nullptr;
  }
  const auto& module_name = verible::GetSubtreeAsLeaf(
      verible::SymbolCastToNode(*label_node), NodeEnum::kLabel, 1);
  return &module_name.get();
}

const verible::SyntaxTreeNode& GetModuleItemList(
    const verible::Symbol& module_declaration) {
  return verible::GetSubtreeAsNode(module_declaration,
                                   NodeEnum::kModuleDeclaration, 1,
                                   NodeEnum::kModuleItemList);
}

const verible::SyntaxTreeNode* GetParamDeclarationListFromModuleDeclaration(
    const verible::Symbol& module_declaration) {
  const auto& header_node = GetModuleHeader(module_declaration);
  const verible::Symbol* param_declaration_list =
      verible::GetSubtreeAsSymbol(header_node, NodeEnum::kModuleHeader, 4);
  return verible::CheckOptionalSymbolAsNode(
      param_declaration_list, NodeEnum::kFormalParameterListDeclaration);
}

const verible::SyntaxTreeNode* GetParamDeclarationListFromInterfaceDeclaration(
    const verible::Symbol& interface_declaration) {
  const auto& header_node = GetInterfaceHeader(interface_declaration);
  const verible::Symbol* param_declaration_list =
      verible::GetSubtreeAsSymbol(header_node, NodeEnum::kModuleHeader, 4);
  return verible::CheckOptionalSymbolAsNode(
      param_declaration_list, NodeEnum::kFormalParameterListDeclaration);
}

}  // namespace verilog
