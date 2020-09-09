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

#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"

#include <iostream>
#include <string>

#include "common/text/tree_context_visitor.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/class.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/module.h"
#include "verilog/CST/net.h"
#include "verilog/CST/port.h"
#include "verilog/CST/tasks.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace kythe {

namespace {

using verible::SyntaxTreeNode;
using verible::TreeSearchMatch;

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

  const SyntaxTreeNode& root = verible::SymbolCastToNode(*syntax_tree);
  root.Accept(&visitor);

  return visitor.GetRoot();
}

void IndexingFactsTreeExtractor::Visit(const SyntaxTreeNode& node) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  switch (tag) {
    case NodeEnum ::kDescriptionList: {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &GetRoot());
      TreeContextVisitor::Visit(node);
      break;
    }
    case NodeEnum::kModuleDeclaration: {
      ExtractModule(node);
      break;
    }
    case NodeEnum::kDataDeclaration: {
      // For module instantiations
      const std::vector<TreeSearchMatch> gate_instances =
          FindAllGateInstances(node);
      if (!gate_instances.empty()) {
        ExtractModuleInstantiation(node, gate_instances);
        break;
      }

      // For bit, int and classes
      const std::vector<TreeSearchMatch> register_variables =
          FindAllRegisterVariables(node);
      if (!register_variables.empty()) {
        // for classes.
        const std::vector<TreeSearchMatch> class_instances =
            verible::SearchSyntaxTree(node, NodekClassNew());
        if (!class_instances.empty()) {
          ExtractClassInstances(node, register_variables);
          break;
        }
      }

      break;
    }
    case NodeEnum::kPortDeclaration:
    case NodeEnum::kPortReference: {
      ExtractModulePort(node);
      break;
    }
    case NodeEnum::kIdentifierUnpackedDimensions: {
      ExtractInputOutputDeclaration(node);
      break;
    }
    case NodeEnum ::kNetDeclaration: {
      ExtractNetDeclaration(node);
      break;
    }
    case NodeEnum::kFunctionDeclaration: {
      ExtractFunctionDeclaration(node);
      break;
    }
    case NodeEnum::kTaskDeclaration: {
      ExtractTaskDeclaration(node);
      break;
    }
    case NodeEnum::kFunctionCall: {
      ExtractFunctionOrTaskCall(node);
      break;
    }
    case NodeEnum::kClassDeclaration: {
      ExtractClassDeclaration(node);
      break;
    }
    default: {
      TreeContextVisitor::Visit(node);
    }
  }
}

void IndexingFactsTreeExtractor::ExtractModule(
    const SyntaxTreeNode& module_declaration_node) {
  IndexingNodeData module_node_data(IndexingFactType::kModule);
  IndexingFactNode module_node(module_node_data);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &module_node);
    ExtractModuleHeader(module_declaration_node);
    ExtractModuleEnd(module_declaration_node);

    const SyntaxTreeNode& module_item_list =
        GetModuleItemList(module_declaration_node);
    Visit(module_item_list);
  }

  facts_tree_context_.top().NewChild(module_node);
}

void IndexingFactsTreeExtractor::ExtractModuleHeader(
    const SyntaxTreeNode& module_header_node) {
  const verible::TokenInfo& module_name_token =
      GetModuleNameToken(module_header_node);
  const Anchor module_name_anchor(module_name_token, context_.base);

  facts_tree_context_.top().Value().AppendAnchor(module_name_anchor);

  // Extracting module ports e.g. (input a, input b).
  // Ports are treated as children of the module.
  const SyntaxTreeNode* port_list =
      GetModulePortDeclarationList(module_header_node);

  if (port_list == nullptr) {
    return;
  }

  Visit(*port_list);
}

void IndexingFactsTreeExtractor::ExtractModulePort(
    const verible::SyntaxTreeNode& module_port_node) {
  const auto tag = static_cast<verilog::NodeEnum>(module_port_node.Tag().tag);

  // TODO(minatoma): Fix case like:
  // module m(input a, b); --> b is treated as a reference but should be a
  // definition.

  // For extracting cases like:
  // module m(input a, input b);
  if (tag == NodeEnum::kPortDeclaration) {
    const verible::SyntaxTreeLeaf* leaf =
        GetIdentifierFromModulePortDeclaration(module_port_node);

    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(leaf->get(), context_.base)},
                         IndexingFactType::kVariableDefinition));
  }
  // For extracting Non-ANSI style ports:
  // module m(a, b);
  else {
    const verible::SyntaxTreeLeaf* leaf =
        GetIdentifierFromPortReference(module_port_node);

    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(leaf->get(), context_.base)},
                         IndexingFactType::kVariableReference));
  }
}

void IndexingFactsTreeExtractor::ExtractInputOutputDeclaration(
    const verible::SyntaxTreeNode& identifier_unpacked_dimension) {
  const verible::SyntaxTreeLeaf* port_name_leaf =
      GetSymbolIdentifierFromIdentifierUnpackedDimensions(
          identifier_unpacked_dimension);

  facts_tree_context_.top().NewChild(
      IndexingNodeData({Anchor(port_name_leaf->get(), context_.base)},
                       IndexingFactType::kVariableDefinition));
}

void IndexingFactsTreeExtractor::ExtractModuleEnd(
    const SyntaxTreeNode& module_declaration_node) {
  const verible::TokenInfo* module_name =
      GetModuleEndLabel(module_declaration_node);

  if (module_name != nullptr) {
    const Anchor module_end_anchor(*module_name, context_.base);
    facts_tree_context_.top().Value().AppendAnchor(module_end_anchor);
  }
}

// TODO(minatoma): consider this case:
//  foo_module foo_instance(id1[id2],id3[id4]);  // where instance is
//  "foo_instance(...)"
void IndexingFactsTreeExtractor::ExtractModuleInstantiation(
    const SyntaxTreeNode& data_declaration_node,
    const std::vector<TreeSearchMatch>& gate_instances) {
  const verible::TokenInfo& type =
      GetTypeTokenInfoFromDataDeclaration(data_declaration_node);
  const Anchor type_anchor(type, context_.base);

  // Module instantiations (data declarations) may declare multiple instances
  // sharing the same type in a single statement e.g. bar b1(), b2().
  //
  // Loop through each instance and associate each declared id with the same
  // type and create its corresponding facts tree node.
  for (const TreeSearchMatch& instance : gate_instances) {
    IndexingNodeData indexing_node_data(IndexingFactType::kModuleInstance);

    const verible::TokenInfo& variable_name =
        GetModuleInstanceNameTokenInfoFromGateInstance(*instance.match);
    const Anchor variable_name_anchor(variable_name, context_.base);
    indexing_node_data.AppendAnchor(type_anchor);
    indexing_node_data.AppendAnchor(variable_name_anchor);

    std::vector<TreeSearchMatch> port_names =
        FindAllUnqualifiedIds(*instance.match);

    // Module ports are treated as anchors in instantiations.
    for (const TreeSearchMatch& port : port_names) {
      const verible::SyntaxTreeLeaf* leaf = GetIdentifier(*port.match);
      const Anchor port_name_anchor(leaf->get(), context_.base);

      indexing_node_data.AppendAnchor(port_name_anchor);
    }

    facts_tree_context_.top().NewChild(indexing_node_data);
  }
}

void IndexingFactsTreeExtractor::ExtractNetDeclaration(
    const SyntaxTreeNode& net_declaration_node) {
  // Nets are treated as children of the enclosing parent.
  // Net declarations may declare multiple instances sharing the same type in a
  // single statement.
  const std::vector<const verible::TokenInfo*> identifiers =
      GetIdentifiersFromNetDeclaration(net_declaration_node);

  // Loop through each instance and associate each declared id with the same
  // type.
  for (const verible::TokenInfo* wire_token_info : identifiers) {
    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(*wire_token_info, context_.base)},
                         IndexingFactType::kVariableDefinition));
  }
}

void IndexingFactsTreeExtractor::ExtractFunctionDeclaration(
    const verible::SyntaxTreeNode& function_declaration_node) {
  IndexingNodeData function_node_data(IndexingFactType::kFunctionOrTask);
  IndexingFactNode function_node(function_node_data);

  // Extract function name.
  const auto* function_name_leaf = GetFunctionName(function_declaration_node);
  const Anchor function_name_anchor(function_name_leaf->get(), context_.base);
  function_node.Value().AppendAnchor(function_name_anchor);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);
    // Extract function ports.
    ExtractFunctionTaskPort(function_declaration_node);

    // Extract function body.
    const verible::SyntaxTreeNode& function_body =
        GetFunctionBlockStatementList(function_declaration_node);
    Visit(function_body);
  }

  facts_tree_context_.top().NewChild(function_node);
}

void IndexingFactsTreeExtractor::ExtractTaskDeclaration(
    const verible::SyntaxTreeNode& task_declaration_node) {
  IndexingNodeData task_node_data(IndexingFactType::kFunctionOrTask);
  IndexingFactNode task_node(task_node_data);

  // Extract task name.
  const auto* task_name_leaf = GetTaskName(task_declaration_node);
  const Anchor task_name_anchor(task_name_leaf->get(), context_.base);
  task_node.Value().AppendAnchor(task_name_anchor);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &task_node);

    // Extract task ports.
    ExtractFunctionTaskPort(task_declaration_node);

    // Extract task body.
    const verible::SyntaxTreeNode& task_body =
        GetTaskStatementList(task_declaration_node);
    Visit(task_body);
  }

  facts_tree_context_.top().NewChild(task_node);
}

void IndexingFactsTreeExtractor::ExtractFunctionTaskPort(
    const verible::SyntaxTreeNode& function_declaration_node) {
  const std::vector<verible::TreeSearchMatch> ports =
      FindAllTaskFunctionPortDeclarations(function_declaration_node);

  for (const verible::TreeSearchMatch& port : ports) {
    const verible::SyntaxTreeLeaf* leaf =
        GetIdentifierFromTaskFunctionPortItem(*port.match);

    // TODO(minatoma): Consider using kPorts or kParam for ports and params
    // instead of variables (same goes for modules).
    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(leaf->get(), context_.base)},
                         IndexingFactType::kVariableDefinition));
  }
}

void IndexingFactsTreeExtractor::ExtractFunctionOrTaskCall(
    const verible::SyntaxTreeNode& function_call_node) {
  IndexingNodeData function_node_data(IndexingFactType::kFunctionCall);
  IndexingFactNode function_node(function_node_data);

  // Extract function or task name.
  const auto* function_name_leaf = GetFunctionCallName(function_call_node);
  const Anchor task_name_anchor(function_name_leaf->get(), context_.base);
  function_node.Value().AppendAnchor(task_name_anchor);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);

    // Extract function or task parameters.
    TreeContextVisitor::Visit(function_call_node);
  }

  facts_tree_context_.top().NewChild(function_node);
}

void IndexingFactsTreeExtractor::ExtractClassDeclaration(
    const SyntaxTreeNode& class_declaration) {
  IndexingNodeData class_node_data(IndexingFactType::kClass);
  IndexingFactNode class_node(class_node_data);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &class_node);
    // Extract class name.
    const verible::SyntaxTreeLeaf& class_name_leaf =
        GetClassName(class_declaration);
    const Anchor class_name_anchor(class_name_leaf.get(), context_.base);
    facts_tree_context_.top().Value().AppendAnchor(class_name_anchor);

    // Extract class name after endclass.
    const verible::SyntaxTreeLeaf* class_end_name =
        GetClassEndLabel(class_declaration);

    if (class_end_name != nullptr) {
      const Anchor class_end_anchor(class_end_name->get(), context_.base);
      facts_tree_context_.top().Value().AppendAnchor(class_end_anchor);
    }

    // Visit class body.
    const SyntaxTreeNode& class_item_list = GetClassItemList(class_declaration);
    Visit(class_item_list);
  }

  facts_tree_context_.top().NewChild(class_node);
}

void IndexingFactsTreeExtractor::ExtractClassInstances(
    const SyntaxTreeNode& data_declaration_node,
    const std::vector<TreeSearchMatch>& register_variables) {
  const verible::TokenInfo& type =
      GetTypeTokenInfoFromDataDeclaration(data_declaration_node);
  const Anchor type_anchor(type, context_.base);

  // Class instances may may appear as multiple instances sharing the same type
  // in a single statement e.g. myClass b1 = new, b2 = new.
  // LRM 8.8 Typed constructor calls
  //
  // Loop through each instance and associate each declared id with the same
  // type and create its corresponding facts tree node.
  for (const TreeSearchMatch& instance : register_variables) {
    IndexingNodeData indexing_node_data(IndexingFactType::kClassInstance);

    const verible::TokenInfo& variable_name =
        GetInstanceNameTokenInfoFromRegisterVariable(*instance.match);

    const Anchor variable_name_anchor(variable_name, context_.base);
    indexing_node_data.AppendAnchor(type_anchor);
    indexing_node_data.AppendAnchor(variable_name_anchor);

    facts_tree_context_.top().NewChild(indexing_node_data);
  }
}

}  // namespace kythe
}  // namespace verilog
