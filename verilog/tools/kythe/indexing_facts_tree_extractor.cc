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
#include "verilog/CST/declaration.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/macro.h"
#include "verilog/CST/module.h"
#include "verilog/CST/net.h"
#include "verilog/CST/port.h"
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
}

void IndexingFactsTreeExtractor::Visit(const verible::SyntaxTreeNode& node) {
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
    case NodeEnum::kPreprocessorDefine: {
      ExtractMacroDefinition(node);
      break;
    }
    case NodeEnum::kMacroCall: {
      ExtractMacroCall(node);
      break;
    }
    // Special case toa handle tags in leaves.
    // e.g leaves tagged with MacroIdentifier.
    case NodeEnum::kUnqualifiedId: {
      const verible::SyntaxTreeLeaf* leaf =
          ABSL_DIE_IF_NULL(AutoUnwrapIdentifier(node));

      if (leaf->get().token_enum() == verilog_tokentype::MacroIdentifier) {
        ExtractMacroReference(*leaf);
        break;
      }

      break;
    }
    default: {
      TreeContextVisitor::Visit(node);
    }
  }
}

void IndexingFactsTreeExtractor::ExtractModule(
    const verible::SyntaxTreeNode& module_declaration_node) {
  IndexingNodeData module_node_data(IndexingFactType::kModule);
  IndexingFactNode module_node(module_node_data);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &module_node);
    ExtractModuleHeader(module_declaration_node);
    ExtractModuleEnd(module_declaration_node);

    const verible::SyntaxTreeNode& module_item_list =
        GetModuleItemList(module_declaration_node);
    Visit(module_item_list);
  }

  facts_tree_context_.top().NewChild(module_node);
}

void IndexingFactsTreeExtractor::ExtractModuleHeader(
    const verible::SyntaxTreeNode& module_header_node) {
  const verible::TokenInfo& module_name_token =
      GetModuleNameToken(module_header_node);
  const Anchor module_name_anchor(module_name_token, context_.base);

  facts_tree_context_.top().Value().AppendAnchor(module_name_anchor);

  // TODO(minatoma): consider this case: module m(a, b);
  // Extracting module ports e.g. (input a, input b).
  // Ports are treated as children of the module.
  const verible::SyntaxTreeNode* port_list =
      GetModulePortDeclarationList(module_header_node);

  if (port_list == nullptr) {
    return;
  }

  const std::vector<verible::TreeSearchMatch> port_names =
      FindAllModulePortDeclarations(*port_list);

  for (const verible::TreeSearchMatch& port : port_names) {
    const verible::SyntaxTreeLeaf* leaf =
        GetIdentifierFromModulePortDeclaration(*port.match);

    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(leaf->get(), context_.base)},
                         IndexingFactType::kVariableDefinition));
  }
}

void IndexingFactsTreeExtractor::ExtractInputOutputDeclaration(
    const verible::SyntaxTreeNode& module_port_declaration_node) {
  const std::vector<verible::TreeSearchMatch> port_names =
      FindAllUnqualifiedIds(module_port_declaration_node);

  // In case we have input a, b.
  // Loop through each port name and create its own node in facts tree.
  for (const verible::TreeSearchMatch& port : port_names) {
    const verible::SyntaxTreeLeaf* leaf = GetIdentifier(*port.match);

    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(leaf->get(), context_.base)},
                         IndexingFactType::kVariableReference));
  }
}

void IndexingFactsTreeExtractor::ExtractModuleEnd(
    const verible::SyntaxTreeNode& module_declaration_node) {
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
    const verible::SyntaxTreeNode& data_declaration_node) {
  const verible::TokenInfo& type =
      GetTypeTokenInfoFromModuleInstantiation(data_declaration_node);
  const Anchor type_anchor(type, context_.base);

  // Module instantiations (data declarations) may declare multiple instances
  // sharing the same type in a single statement e.g. bar b1(), b2().
  const std::vector<verible::TreeSearchMatch> gate_instances =
      GetListOfGateInstanceFromDataDeclaration(data_declaration_node);

  // Loop through each instance and associate each declared id with the same
  // type and create its corresponding facts tree node.
  for (const verible::TreeSearchMatch& instance : gate_instances) {
    IndexingNodeData indexing_node_data(IndexingFactType::kModuleInstance);

    const verible::TokenInfo& variable_name =
        GetModuleInstanceNameTokenInfoFromGateInstance(*instance.match);
    const Anchor variable_name_anchor(variable_name, context_.base);
    indexing_node_data.AppendAnchor(type_anchor);
    indexing_node_data.AppendAnchor(variable_name_anchor);

    std::vector<verible::TreeSearchMatch> port_names =
        FindAllUnqualifiedIds(*instance.match);

    // Module ports are treated as anchors in instantiations.
    for (const verible::TreeSearchMatch& port : port_names) {
      const verible::SyntaxTreeLeaf* leaf = GetIdentifier(*port.match);
      const Anchor port_name_anchor(leaf->get(), context_.base);

      indexing_node_data.AppendAnchor(port_name_anchor);
    }

    facts_tree_context_.top().NewChild(indexing_node_data);
  }
}

void IndexingFactsTreeExtractor::ExtractNetDeclaration(
    const verible::SyntaxTreeNode& net_declaration_node) {
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

void IndexingFactsTreeExtractor::ExtractMacroDefinition(
    const verible::SyntaxTreeNode& preprocessor_definition) {
  const verible::SyntaxTreeLeaf& macro_name =
      GetMacroName(preprocessor_definition);
  facts_tree_context_.top().NewChild(IndexingNodeData(
      {Anchor(macro_name.get(), context_.base)}, IndexingFactType::kMacro));
}

void IndexingFactsTreeExtractor::ExtractMacroCall(
    const verible::SyntaxTreeNode& macro_call) {
  const verible::TokenInfo& macro_call_name_token = GetMacroCallId(macro_call);

  IndexingFactNode macro_node(
      IndexingNodeData({Anchor(macro_call_name_token, context_.base)},
                       IndexingFactType::kMacroCall));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &macro_node);

    const verible::SyntaxTreeNode& macro_call_args =
        GetMacroCallArgs(macro_call);
    Visit(macro_call_args);
  }

  facts_tree_context_.top().NewChild(macro_node);
}

void IndexingFactsTreeExtractor::ExtractMacroReference(
    const verible::SyntaxTreeLeaf& macro_identifier) {
  facts_tree_context_.top().NewChild(
      IndexingNodeData({Anchor(macro_identifier.get(), context_.base)},
                       IndexingFactType::kMacroCall));
}

}  // namespace kythe
}  // namespace verilog
