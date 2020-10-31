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

#include "verilog/CST/class.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

using verible::Symbol;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllClassDeclarations(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekClassDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllClassConstructors(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekClassConstructor());
}

std::vector<verible::TreeSearchMatch> FindAllHierarchyExtensions(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekHierarchyExtension());
}

const verible::SyntaxTreeNode& GetClassHeader(
    const verible::Symbol& class_symbol) {
  return verible::GetSubtreeAsNode(class_symbol, NodeEnum::kClassDeclaration, 0,
                                   NodeEnum::kClassHeader);
}

const verible::SyntaxTreeLeaf& GetClassName(
    const verible::Symbol& class_declaration) {
  const auto& header_node = GetClassHeader(class_declaration);
  const auto& name_leaf =
      verible::GetSubtreeAsLeaf(header_node, NodeEnum::kClassHeader, 3);
  return name_leaf;
}

const verible::SyntaxTreeNode* GetExtendedClass(
    const verible::Symbol& class_declaration) {
  const auto& class_header = GetClassHeader(class_declaration);
  const auto* extends_list =
      verible::GetSubtreeAsSymbol(class_header, NodeEnum::kClassHeader, 5);
  if (extends_list == nullptr) {
    return nullptr;
  }
  return &verible::GetSubtreeAsNode(*extends_list, NodeEnum::kExtendsList, 1);
}

const verible::SyntaxTreeLeaf* GetClassEndLabel(
    const verible::Symbol& class_declaration) {
  const auto* label_node = verible::GetSubtreeAsSymbol(
      class_declaration, NodeEnum::kClassDeclaration, 3);
  if (label_node == nullptr) {
    return nullptr;
  }
  const auto& class_name = verible::GetSubtreeAsLeaf(
      verible::SymbolCastToNode(*label_node), NodeEnum::kLabel, 1);
  return &class_name;
}

const verible::SyntaxTreeNode& GetClassItemList(
    const verible::Symbol& class_declaration) {
  return verible::GetSubtreeAsNode(
      class_declaration, NodeEnum::kClassDeclaration, 1, NodeEnum::kClassItems);
}

const verible::SyntaxTreeLeaf& GetUnqualifiedIdFromHierarchyExtension(
    const verible::Symbol& hierarchy_extension) {
  return *AutoUnwrapIdentifier(verible::GetSubtreeAsNode(
      hierarchy_extension, NodeEnum::kHierarchyExtension, 1,
      NodeEnum::kUnqualifiedId));
}

const verible::SyntaxTreeNode* GetParamDeclarationListFromClassDeclaration(
    const verible::Symbol& class_declaration) {
  const auto& header_node = GetClassHeader(class_declaration);
  const verible::Symbol* param_declaration_list =
      verible::GetSubtreeAsSymbol(header_node, NodeEnum::kClassHeader, 4);
  return verible::CheckOptionalSymbolAsNode(
      param_declaration_list, NodeEnum::kFormalParameterListDeclaration);
}

const verible::SyntaxTreeNode& GetClassConstructorStatementList(
    const verible::Symbol& class_constructor) {
  return verible::GetSubtreeAsNode(class_constructor,
                                   NodeEnum::kClassConstructor, 2);
}

const verible::SyntaxTreeLeaf& GetNewKeywordFromClassConstructor(
    const verible::Symbol& class_constructor) {
  const verible::SyntaxTreeNode& constructor_prototype =
      verible::GetSubtreeAsNode(class_constructor, NodeEnum::kClassConstructor,
                                1, NodeEnum::kClassConstructorPrototype);

  return verible::GetSubtreeAsLeaf(constructor_prototype,
                                   NodeEnum::kClassConstructorPrototype, 1);
}

}  // namespace verilog
