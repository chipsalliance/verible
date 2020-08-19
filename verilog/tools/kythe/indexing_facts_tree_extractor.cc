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

#include <iostream>
#include <string>

#include "common/text/tree_context_visitor.h"
#include "common/text/tree_utils.h"
#include "indexing_facts_tree_extractor.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/module.h"
#include "verilog/CST/net.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace kythe {

namespace {

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

  for (const verible::SymbolPtr& child : node.children()) {
    if (!child) continue;
    if (child->Kind() == verible::SymbolKind::kNode) {
      DebugSyntaxTree(verible::SymbolCastToNode(*child));
    } else {
      DebugSyntaxTree(verible::SymbolCastToLeaf(*child));
    }
  }

  LOG(INFO) << "End Node";
}
}  // namespace

IndexingFactNode ExtractOneFile(absl::string_view content,
                                absl::string_view filename, int& exit_status,
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

  return BuildIndexingFactsTree(syntax_tree, analyzer->Data().Contents(),
                                filename);
}

IndexingFactNode BuildIndexingFactsTree(
    const verible::ConcreteSyntaxTree& syntax_tree, absl::string_view base,
    absl::string_view file_name) {
  IndexingFactsTreeExtractor visitor(base, file_name);
  if (syntax_tree == nullptr) {
    return visitor.GetRoot();
  }

  const verible::SyntaxTreeNode& root = verible::SymbolCastToNode(*syntax_tree);
  DebugSyntaxTree(root);
  root.Accept(&visitor);

  return visitor.GetRoot();
};

void IndexingFactsTreeExtractor::Visit(const verible::SyntaxTreeNode& node) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  switch (tag) {
    case NodeEnum ::kDescriptionList: {
      const AutoPop p(&facts_tree_context_, &GetRoot());
      TreeContextVisitor::Visit(node);
      break;
    }
    case NodeEnum::kModuleDeclaration: {
      ExtractModule(node);
      break;
    }
    case NodeEnum::kDataDeclaration: {
      ExtractModuleInstantiation(node);
      break;
    }
    case NodeEnum::kModulePortDeclaration: {
      ExtractInputOutputDeclaration(node);
      break;
    }
    case NodeEnum ::kNetDeclaration: {
      ExtractNetDeclaration(node);
      break;
    }
    default: {
      TreeContextVisitor::Visit(node);
    }
  }
}

void IndexingFactsTreeExtractor::ExtractModule(
    const verible::SyntaxTreeNode& node) {
  IndexingNodeData module_node_data(IndexingFactType::kModule);
  IndexingFactNode module_node(module_node_data);

  {
    const AutoPop p(&facts_tree_context_, &module_node);
    ExtractModuleHeader(node);
    ExtractModuleEnd(node);

    const verible::SyntaxTreeNode& module_item_list = GetModuleItemList(node);
    Visit(module_item_list);
  }

  facts_tree_context_.top().NewChild(module_node);
}

void IndexingFactsTreeExtractor::ExtractModuleHeader(
    const verible::SyntaxTreeNode& node) {
  const verible::TokenInfo& module_name_token = GetModuleNameToken(node);
  const Anchor module_name_anchor(module_name_token, context_.base);

  facts_tree_context_.top().Value().AppendAnchor(module_name_anchor);

  const verible::SyntaxTreeNode* port_list = GetModulePortDeclarationList(node);

  if (port_list != nullptr) {
    std::vector<verible::TreeSearchMatch> port_names =
        FindAllUnqualifiedIds(*port_list);

    for (const verible::TreeSearchMatch& port : port_names) {
      const verible::SyntaxTreeLeaf* leaf = GetIdentifier(*port.match);
      const Anchor port_name_anchor(leaf->get(), context_.base);

      IndexingNodeData module_port(IndexingFactType::kVariableDefinition);
      IndexingFactNode port_node(module_port);
      port_node.Value().AppendAnchor(port_name_anchor);

      facts_tree_context_.top().NewChild(port_node);
    }
  }
}

void IndexingFactsTreeExtractor::ExtractInputOutputDeclaration(
    const verible::SyntaxTreeNode& node) {
  std::vector<verible::TreeSearchMatch> port_names =
      FindAllUnqualifiedIds(node);

  for (const verible::TreeSearchMatch& port : port_names) {
    const verible::SyntaxTreeLeaf* leaf = GetIdentifier(*port.match);
    const Anchor port_name_anchor(leaf->get(), context_.base);

    IndexingNodeData module_port(IndexingFactType::kVariableReference);
    IndexingFactNode port_node(module_port);
    port_node.Value().AppendAnchor(port_name_anchor);

    facts_tree_context_.top().NewChild(port_node);
  }
}

void IndexingFactsTreeExtractor::ExtractModuleEnd(
    const verible::SyntaxTreeNode& node) {
  const verible::TokenInfo* module_name = GetModuleEndLabel(node);

  if (module_name != nullptr) {
    const Anchor module_end_anchor(*module_name, context_.base);
    facts_tree_context_.top().Value().AppendAnchor(module_end_anchor);
  }
};

void IndexingFactsTreeExtractor::ExtractModuleInstantiation(
    const verible::SyntaxTreeNode& node) {
  std::vector<verible::TreeSearchMatch> gate_instances =
      GetListOfGateInstanceFromDataDeclaration(node);

  const verible::TokenInfo& type =
      GetTypeTokenInfoFromModuleInstantiation(node);
  const Anchor type_anchor(type, context_.base);

  for (const verible::TreeSearchMatch& instance : gate_instances) {
    IndexingNodeData indexing_node_data(IndexingFactType::kModuleInstance);

    const verible::TokenInfo& variable_name =
        GetModuleInstanceNameTokenInfoFromGateInstance(*instance.match);
    const Anchor variable_name_anchor(variable_name, context_.base);
    indexing_node_data.AppendAnchor(type_anchor);
    indexing_node_data.AppendAnchor(variable_name_anchor);

    std::vector<verible::TreeSearchMatch> port_names =
        FindAllUnqualifiedIds(*instance.match);

    for (const verible::TreeSearchMatch& port : port_names) {
      const verible::SyntaxTreeLeaf* leaf = GetIdentifier(*port.match);
      const Anchor port_name_anchor(leaf->get(), context_.base);

      indexing_node_data.AppendAnchor(port_name_anchor);
    }

    IndexingFactNode module_instance(indexing_node_data);
    facts_tree_context_.top().NewChild(module_instance);
  }
}
void IndexingFactsTreeExtractor::ExtractNetDeclaration(
    const verible::SyntaxTreeNode& node) {
  const std::vector<const verible::TokenInfo*> identifiers =
      GetIdentifiersFromNetDeclaration(node);

  for (const verible::TokenInfo* wire_token_info : identifiers) {
    IndexingNodeData indexing_node_data(IndexingFactType::kVariableDefinition);
    const Anchor wire_name_anchor(*wire_token_info, context_.base);
    indexing_node_data.AppendAnchor(wire_name_anchor);

    IndexingFactNode wire_name(indexing_node_data);
    facts_tree_context_.top().NewChild(wire_name);
  }
}

}  // namespace kythe
}  // namespace verilog
