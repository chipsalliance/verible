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
#include "verilog/CST/declaration.h"
#include "verilog/CST/module.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace kythe {

void DebugSyntaxTree(const verible::SyntaxTreeLeaf& leaf) {
  LOG(INFO) << "Start Leaf";
  LOG(INFO) << verilog::NodeEnumToString(
                   static_cast<verilog::NodeEnum>(leaf.Tag().tag))
            << " <<>> " << leaf.Tag().tag << " " << leaf.get();
  LOG(INFO) << "End Leaf";
}

void DebugSyntaxTree(const verible::SyntaxTreeNode& node) {
  LOG(INFO) << "Start Node";
  LOG(INFO) << verilog::NodeEnumToString(
                   static_cast<verilog::NodeEnum>(node.Tag().tag))
            << "  " << node.children().size();

  for (const auto& child : node.children()) {
    if (child) {
      if (child->Kind() == verible::SymbolKind::kNode) {
        DebugSyntaxTree(verible::SymbolCastToNode(*child));
      } else {
        DebugSyntaxTree(verible::SymbolCastToLeaf(*child));
      }
    }
  }

  LOG(INFO) << "End Node";
}

IndexingFactNode ExtractOneFile(absl::string_view content,
                                absl::string_view filename,
                                int& exit_status,
                                bool& parse_ok) {
  const auto analyzer =
      verilog::VerilogAnalyzer::AnalyzeAutomaticMode(content, filename);
  const auto lex_status = ABSL_DIE_IF_NULL(analyzer)->LexStatus();
  const auto parse_status = analyzer->ParseStatus();
  if (!lex_status.ok() || !parse_status.ok()) {
    const std::vector<std::string> syntax_error_messages(
        analyzer->LinterTokenErrorMessages());
    for (const auto& message : syntax_error_messages) {
      std::cout << message << std::endl;
    }
    exit_status = 1;
  }
  parse_ok = parse_status.ok();

  const auto& text_structure = analyzer->Data();
  const auto& syntax_tree = text_structure.SyntaxTree();

  return BuildIndexingFactsTree(verible::SymbolCastToNode(*syntax_tree),
                                analyzer->Data().Contents());
}

IndexingFactNode BuildIndexingFactsTree(const verible::SyntaxTreeNode& root,
                                        absl::string_view base) {
  DebugSyntaxTree(root);

  IndexingNodeData file_node_data(IndexingFactType::kFile);

  IndexingFactNode indexing_fact_root(file_node_data);
  Extract(root, indexing_fact_root, base);

  return indexing_fact_root;
};

void Extract(const verible::SyntaxTreeNode& node,
             IndexingFactNode& parent,
             absl::string_view base) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);

  switch (tag) {
    case NodeEnum::kModuleDeclaration: {
      return ExtractModule(node, parent, base);
    }
    case NodeEnum::kDataDeclaration: {
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

void ExtractModule(const verible::SyntaxTreeNode& node,
                   IndexingFactNode& parent,
                   absl::string_view base) {
  auto module_node_data = IndexingNodeData(IndexingFactType::kModule);
  auto module_node = IndexingFactNode(module_node_data);

  ExtractModuleHeader(node, module_node.Value(), base);

  const auto& module_item_list = GetModuleItemList(node);
  Extract(verible::SymbolCastToNode(module_item_list), module_node, base);

  ExtractModuleEnd(node, module_node.Value(), base);

  parent.NewChild(module_node);
}

void ExtractModuleHeader(const verible::SyntaxTreeNode& node,
                         IndexingNodeData& parent,
                         absl::string_view base) {
  const auto& module_name_token = GetModuleNameToken(node);
  auto moduleNameAnchor = Anchor(module_name_token, base);

  parent.AppendAnchor(moduleNameAnchor);
}

void ExtractModuleEnd(const verible::SyntaxTreeNode& node,
                      IndexingNodeData& parent,
                      absl::string_view base) {
  const auto module_name = GetModuleNameTokenAfterModuleEnd(node);

  if (module_name != nullptr) {
    Anchor moduleEndAnchor = Anchor(*module_name, base);
    parent.AppendAnchor(moduleEndAnchor);
  }
};

void ExtractModuleInstantiation(const verible::SyntaxTreeNode& node,
                                IndexingFactNode& parent,
                                absl::string_view base) {
  const auto& type =
      GetTypeTokenInfoOfModuleInstantiationFromModuleDeclaration(node);
  auto type_anchor = Anchor(type, base);

  const auto& variable_name =
      GetModuleInstanceNameTokenInfoFromDataDeclaration(node);
  Anchor variable_name_anchor = Anchor(variable_name, base);

  auto indexingNodeData = IndexingNodeData(IndexingFactType::kModuleInstance);
  indexingNodeData.AppendAnchor(type_anchor);
  indexingNodeData.AppendAnchor(variable_name_anchor);

  auto moduleInstance = IndexingFactNode(indexingNodeData);
  parent.NewChild(moduleInstance);
}

void Extract(const verible::SyntaxTreeLeaf&,
             IndexingFactNode&,
             absl::string_view) {}

}  // namespace kythe
}  // namespace verilog
