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
#include "verilog/CST/module.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace kythe {

namespace {

template <class V, class T>
class AutoPopBack {
 public:
  AutoPopBack(V* v, T* t) : vec_(v) { vec_->push_back(t); }
  ~AutoPopBack() { vec_->pop_back(); }

 private:
  V* vec_;
};

using AutoPop = AutoPopBack<IndexingFactsTreeContext, IndexingFactNode>;

void DebugSyntaxTree(const verible::SyntaxTreeLeaf& leaf) {
  VLOG(1) << "Start Leaf";
  VLOG(1) << verilog::NodeEnumToString(
                 static_cast<verilog::NodeEnum>(leaf.Tag().tag))
          << " <<>> " << leaf.Tag().tag << " " << leaf.get();
  VLOG(1) << "End Leaf";
}

void DebugSyntaxTree(const verible::SyntaxTreeNode& node) {
  VLOG(1) << "Start Node";
  VLOG(1) << verilog::NodeEnumToString(
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

  VLOG(1) << "End Node";
}
}  // namespace

void IndexingFactsTreeExtractor::Visit(const verible::SyntaxTreeNode& node) {
  const verilog::NodeEnum tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  switch (tag) {
    case NodeEnum::kModuleDeclaration: {
      ExtractModule(node);
      break;
    }
    case NodeEnum::kDataDeclaration: {
      ExtractModuleInstantiation(node);
      break;
    }
    default: {
      TreeContextVisitor::Visit(node);
    }
  }
}

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

  facts_tree_context_.back()->NewChild(module_node);
}

void IndexingFactsTreeExtractor::ExtractModuleHeader(
    const verible::SyntaxTreeNode& node) {
  const verible::TokenInfo& module_name_token = GetModuleNameToken(node);
  const Anchor module_name_anchor(module_name_token, context_.base);

  facts_tree_context_.back()->Value().AppendAnchor(module_name_anchor);
}

void IndexingFactsTreeExtractor::ExtractModuleEnd(
    const verible::SyntaxTreeNode& node) {
  const verible::TokenInfo* module_name = GetModuleEndLabel(node);

  if (module_name != nullptr) {
    const Anchor module_end_anchor(*module_name, context_.base);
    facts_tree_context_.back()->Value().AppendAnchor(module_end_anchor);
  }
}

void IndexingFactsTreeExtractor::ExtractModuleInstantiation(
    const verible::SyntaxTreeNode& node) {
  const verible::TokenInfo& type =
      GetTypeTokenInfoFromModuleInstantiation(node);
  const Anchor type_anchor(type, context_.base);

  const verible::TokenInfo& variable_name =
      GetModuleInstanceNameTokenInfoFromDataDeclaration(node);
  const Anchor variable_name_anchor(variable_name, context_.base);

  IndexingNodeData indexing_node_data(IndexingFactType::kModuleInstance);
  indexing_node_data.AppendAnchor(type_anchor);
  indexing_node_data.AppendAnchor(variable_name_anchor);

  IndexingFactNode module_instance(indexing_node_data);
  facts_tree_context_.back()->NewChild(module_instance);
}

}  // namespace kythe
}  // namespace verilog
