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
#include "common/text/tree_context_visitor.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/module.h"
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

  for (const auto& child : node.children()) {
    if (child) {
      if (child->Kind() == verible::SymbolKind::kNode) {
        DebugSyntaxTree(verible::SymbolCastToNode(*child));
      } else {
        DebugSyntaxTree(verible::SymbolCastToLeaf(*child));
      }
    }
  }

  VLOG(1) << "End Node";
}
}  // namespace

void IndexingFactsTreeExtractor::Visit(const verible::SyntaxTreeNode& node) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
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

  return BuildIndexingFactsTree(verible::SymbolCastToNode(*syntax_tree),
                                analyzer->Data().Contents());
}

IndexingFactNode BuildIndexingFactsTree(const verible::SyntaxTreeNode& root,
                                        absl::string_view base) {
  DebugSyntaxTree(root);

  IndexingNodeData file_node_data(IndexingFactType::kFile);
  IndexingFactNode indexing_fact_root(file_node_data);

  IndexingFactsTreeExtractor visitor(base, indexing_fact_root);
  root.Accept(&visitor);

  return indexing_fact_root;
};

void IndexingFactsTreeExtractor::ExtractModule(
    const verible::SyntaxTreeNode& node) {
  auto module_node_data = IndexingNodeData(IndexingFactType::kModule);
  auto module_node = IndexingFactNode(module_node_data);

  auto parent = facts_tree_context_.back();
  const AutoPop p(&facts_tree_context_, &module_node);

  ExtractModuleHeader(node);

  const auto& module_item_list = GetModuleItemList(node);
  Visit(module_item_list);

  ExtractModuleEnd(node);

  parent->NewChild(module_node);
}

void IndexingFactsTreeExtractor::ExtractModuleHeader(
    const verible::SyntaxTreeNode& node) {
  const auto& module_name_token = GetModuleNameToken(node);
  auto module_name_anchor = Anchor(module_name_token, base_);

  facts_tree_context_.back()->Value().AppendAnchor(module_name_anchor);
}

void IndexingFactsTreeExtractor::ExtractModuleEnd(
    const verible::SyntaxTreeNode& node) {
  const auto module_name = GetModuleNameTokenAfterModuleEnd(node);

  if (module_name != nullptr) {
    Anchor module_end_anchor = Anchor(*module_name, base_);
    facts_tree_context_.back()->Value().AppendAnchor(module_end_anchor);
  }
};

void IndexingFactsTreeExtractor::ExtractModuleInstantiation(
    const verible::SyntaxTreeNode& node) {
  const auto& type =
      GetTypeTokenInfoOfModuleInstantiationFromModuleDeclaration(node);
  auto type_anchor = Anchor(type, base_);

  const auto& variable_name =
      GetModuleInstanceNameTokenInfoFromDataDeclaration(node);
  Anchor variable_name_anchor = Anchor(variable_name, base_);

  auto indexingNodeData = IndexingNodeData(IndexingFactType::kModuleInstance);
  indexingNodeData.AppendAnchor(type_anchor);
  indexingNodeData.AppendAnchor(variable_name_anchor);

  auto module_instance = IndexingFactNode(indexingNodeData);
  facts_tree_context_.back()->NewChild(module_instance);
}

}  // namespace kythe
}  // namespace verilog
