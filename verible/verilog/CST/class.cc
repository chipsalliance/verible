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

#include "verible/verilog/CST/class.h"

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

using verible::Symbol;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllClassDeclarations(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekClassDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllClassConstructors(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekClassConstructor());
}

std::vector<verible::TreeSearchMatch> FindAllHierarchyExtensions(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekHierarchyExtension());
}

const verible::SyntaxTreeNode *GetClassHeader(
    const verible::Symbol &class_symbol) {
  return verible::GetSubtreeAsNode(class_symbol, NodeEnum::kClassDeclaration, 0,
                                   NodeEnum::kClassHeader);
}

const verible::SyntaxTreeLeaf *GetClassName(
    const verible::Symbol &class_declaration) {
  const auto *header_node = GetClassHeader(class_declaration);
  if (!header_node) return nullptr;
  const verible::SyntaxTreeLeaf *name_leaf =
      verible::GetSubtreeAsLeaf(*header_node, NodeEnum::kClassHeader, 3);
  return name_leaf;
}

const verible::SyntaxTreeNode *GetExtendedClass(
    const verible::Symbol &class_declaration) {
  const auto *class_header = GetClassHeader(class_declaration);
  if (!class_header) return nullptr;
  const auto *extends_list =
      verible::GetSubtreeAsSymbol(*class_header, NodeEnum::kClassHeader, 5);
  if (!extends_list) return nullptr;
  return verible::GetSubtreeAsNode(*extends_list, NodeEnum::kExtendsList, 1);
}

const verible::SyntaxTreeLeaf *GetClassEndLabel(
    const verible::Symbol &class_declaration) {
  const auto *label_node = verible::GetSubtreeAsSymbol(
      class_declaration, NodeEnum::kClassDeclaration, 3);
  if (label_node == nullptr) {
    return nullptr;
  }
  return verible::GetSubtreeAsLeaf(verible::SymbolCastToNode(*label_node),
                                   NodeEnum::kLabel, 1);
}

const verible::SyntaxTreeNode *GetClassItemList(
    const verible::Symbol &class_declaration) {
  return verible::GetSubtreeAsNode(
      class_declaration, NodeEnum::kClassDeclaration, 1, NodeEnum::kClassItems);
}

const verible::SyntaxTreeLeaf *GetUnqualifiedIdFromHierarchyExtension(
    const verible::Symbol &hierarchy_extension) {
  const verible::SyntaxTreeNode *unqualified = verible::GetSubtreeAsNode(
      hierarchy_extension, NodeEnum::kHierarchyExtension, 1,
      NodeEnum::kUnqualifiedId);
  if (!unqualified) return nullptr;
  return AutoUnwrapIdentifier(*unqualified);
}

const verible::SyntaxTreeNode *GetParamDeclarationListFromClassDeclaration(
    const verible::Symbol &class_declaration) {
  const auto *header_node = GetClassHeader(class_declaration);
  if (!header_node) return nullptr;
  const verible::Symbol *param_declaration_list =
      verible::GetSubtreeAsSymbol(*header_node, NodeEnum::kClassHeader, 4);
  return verible::CheckOptionalSymbolAsNode(
      param_declaration_list, NodeEnum::kFormalParameterListDeclaration);
}

const verible::SyntaxTreeNode *GetClassConstructorStatementList(
    const verible::Symbol &class_constructor) {
  return verible::GetSubtreeAsNode(class_constructor,
                                   NodeEnum::kClassConstructor, 2);
}

const verible::SyntaxTreeLeaf *GetNewKeywordFromClassConstructor(
    const verible::Symbol &class_constructor) {
  const verible::SyntaxTreeNode *constructor_prototype =
      verible::GetSubtreeAsNode(class_constructor, NodeEnum::kClassConstructor,
                                1, NodeEnum::kClassConstructorPrototype);
  if (!constructor_prototype) return nullptr;
  return verible::GetSubtreeAsLeaf(*constructor_prototype,
                                   NodeEnum::kClassConstructorPrototype, 1);
}

}  // namespace verilog
