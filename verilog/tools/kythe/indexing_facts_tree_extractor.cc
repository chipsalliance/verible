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

#include "indexing_facts_tree_extractor.h"

IndexingFactNode IndexingFactsTreeExtractor::ConstructIndexingFactsTree(
    const verible::SyntaxTreeNode& root,
    absl::string_view base) {
  IndexingFactNode indexing_fact_root =
      IndexingFactNode(IndexingFactType::kFile);

  Extract(root, indexing_fact_root, base);

  return indexing_fact_root;
};

void IndexingFactsTreeExtractor::Extract(const verible::SyntaxTreeNode& node,
                                         IndexingFactNode& parent,
                                         absl::string_view base) {
  switch (node.Tag().tag) {
    case static_cast<int>(verilog::NodeEnum::kModuleDeclaration): {
      return ExtractModule(node, parent, base);
    }
    case static_cast<int>(verilog::NodeEnum::kDataDeclaration): {
      return ExtractModuleInstantiation(node, parent, base);
    }
    default: {
      for (const auto& child : node.children()) {
        if (child) {
          Extract(verible::SymbolCastToNode(*child), parent, base);
        }
      }
    }
  }
}

void IndexingFactsTreeExtractor::ExtractModule(
    const verible::SyntaxTreeNode& node,
    IndexingFactNode& parent,
    absl::string_view base) {
  IndexingFactNode module = IndexingFactNode(IndexingFactType::kModule);

  const auto& moduleHeader =
      GetChildByTag(node, verilog::NodeEnum::kModuleHeader);
  ExtractModuleHeader(verible::SymbolCastToNode(*moduleHeader), module, base);

  const auto& moduleItemList =
      GetChildByTag(node, verilog::NodeEnum::kModuleItemList);
  Extract(verible::SymbolCastToNode(*moduleItemList), module, base);

  const auto& moduleEnd = GetChildByTag(node, verilog::NodeEnum::kLabel);
  if (moduleEnd != nullptr) {
    ExtractModuleEnd(verible::SymbolCastToNode(*moduleEnd), module, base);
  }

  parent.AppendChild(module);
}

void IndexingFactsTreeExtractor::ExtractModuleInstantiation(
    const verible::SyntaxTreeNode& node,
    IndexingFactNode& parent,
    absl::string_view base) {
  const auto& instantiationBase =
      GetChildByTag(node, verilog::NodeEnum::kInstantiationBase);

  const auto& instantiationType =
      GetChildByTag(verible::SymbolCastToNode(*instantiationBase),
                    verilog::NodeEnum::kInstantiationType);

  const auto& type = verible::SymbolCastToLeaf(
      *GetFirstChildByTag(verible::SymbolCastToNode(*instantiationType),
                          verilog::NodeEnum::kNetVariableDeclarationAssign));

  Anchor typeAnchor = Anchor(type.get(), base);

  const auto& variableList =
      GetChildByTag(verible::SymbolCastToNode(*instantiationBase),
                    verilog::NodeEnum::kGateInstanceRegisterVariableList);
  const auto& variableName = verible::SymbolCastToLeaf(
      *GetFirstChildByTag(verible::SymbolCastToNode(*variableList),
                          verilog::NodeEnum::kNetVariableDeclarationAssign));
  Anchor variableNameAnchor = Anchor(variableName.get(), base);

  IndexingFactNode moduleInstance =
      IndexingFactNode(IndexingFactType::kModuleInstance);
  moduleInstance.AppendAnchor(typeAnchor);
  moduleInstance.AppendAnchor(variableNameAnchor);

  parent.AppendChild(moduleInstance);
}

void IndexingFactsTreeExtractor::ExtractModuleEnd(
    const verible::SyntaxTreeNode& node,
    IndexingFactNode& parent,
    absl::string_view base) {
  const auto& moduleEndKeyword = verible::SymbolCastToLeaf(
      *GetChildByTag(node, verilog::NodeEnum::kNetVariableDeclarationAssign));

  Anchor moduleEndAnchor = Anchor(moduleEndKeyword.get(), base);
  parent.AppendAnchor(moduleEndAnchor);
};

void IndexingFactsTreeExtractor::ExtractModuleHeader(
    const verible::SyntaxTreeNode& node,
    IndexingFactNode& parent,
    absl::string_view base) {
  const auto& moduleName = verible::SymbolCastToLeaf(
      *GetChildByTag(node, verilog::NodeEnum::kNetVariableDeclarationAssign));

  Anchor moduleNameAnchor = Anchor(moduleName.get(), base);
  parent.AppendAnchor(moduleNameAnchor);
}

const verible::Symbol* IndexingFactsTreeExtractor::GetChildByTag(
    const verible::SyntaxTreeNode& root,
    verilog::NodeEnum tag) {
  for (const auto& child : root.children()) {
    if (child) {
      if (child->Tag().tag == static_cast<int>(tag)) {
        return child.get();
      }
    }
  }
  return nullptr;
}

const verible::Symbol* IndexingFactsTreeExtractor::GetFirstChildByTag(
    const verible::SyntaxTreeNode& root,
    verilog::NodeEnum tag) {
  const auto* target = GetChildByTag(root, tag);
  if (target == nullptr) {
    for (const auto& child : root.children()) {
      if (child && child->Kind() == verible::SymbolKind::kNode) {
        target = GetFirstChildByTag(verible::SymbolCastToNode(*child), tag);
        if (target != nullptr) {
          return target;
        }
      }
    }
  }
  return target;
}