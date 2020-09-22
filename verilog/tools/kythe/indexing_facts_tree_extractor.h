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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_

#include "absl/strings/string_view.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/tree_context_visitor.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/indexing_facts_tree_context.h"

namespace verilog {
namespace kythe {

// This class is used for traversing CST and extracting different indexing
// facts from CST nodes and constructs a tree of indexing facts.
class IndexingFactsTreeExtractor : public verible::TreeContextVisitor {
 public:
  IndexingFactsTreeExtractor(absl::string_view base,
                             absl::string_view file_name)
      : context_(verible::TokenInfo::Context(base)) {
    root_.Value().AppendAnchor(Anchor(file_name, 0, base.size()));
    root_.Value().AppendAnchor(Anchor(base, 0, base.size()));
  }

  void Visit(const verible::SyntaxTreeLeaf& leaf) override;
  void Visit(const verible::SyntaxTreeNode& node) override;

  IndexingFactNode& GetRoot() { return root_; }

 private:
  // Extracts modules and creates its corresponding fact tree.
  void ExtractModule(const verible::SyntaxTreeNode& module_declaration_node);

  // Extracts modules instantiations and creates its corresponding fact tree.
  void ExtractModuleInstantiation(
      const verible::SyntaxTreeNode& data_declaration_node,
      const std::vector<verible::TreeSearchMatch>& gate_instances);

  // Extracts endmodule and creates its corresponding fact tree.
  void ExtractModuleEnd(const verible::SyntaxTreeNode& module_declaration_node);

  // Extracts modules headers and creates its corresponding fact tree.
  void ExtractModuleHeader(const verible::SyntaxTreeNode& module_header_node);

  // Extracts modules ports and creates its corresponding fact tree.
  void ExtractModulePort(const verible::SyntaxTreeNode& module_port_node,
                         bool has_propagated_type);

  // Extracts "a" from input a, output a and creates its corresponding fact
  // tree.
  void ExtractInputOutputDeclaration(
      const verible::SyntaxTreeNode& module_port_declaration_node);

  // Extracts "a" from wire a and creates its corresponding fact tree.
  void ExtractNetDeclaration(
      const verible::SyntaxTreeNode& net_declaration_node);

  // Extract package declarations and creates its corresponding facts tree.
  void ExtractPackageDeclaration(
      const verible::SyntaxTreeNode& package_declaration_node);

  // Extract macro definitions and explores its arguments and creates its
  // corresponding facts tree.
  void ExtractMacroDefinition(
      const verible::SyntaxTreeNode& preprocessor_definition);

  // Extract macro calls and explores its arguments and creates its
  // corresponding facts tree.
  void ExtractMacroCall(const verible::SyntaxTreeNode& macro_call);

  // Extract macro names from kMacroIdentifiers which are considered references
  // to macros and creates its corresponding facts tree.
  void ExtractMacroReference(const verible::SyntaxTreeLeaf& macro_identifier);

  // Extracts function and creates its corresponding fact tree.
  void ExtractFunctionDeclaration(
      const verible::SyntaxTreeNode& function_declaration_node);

  // Extracts task and creates its corresponding fact tree.
  void ExtractTaskDeclaration(
      const verible::SyntaxTreeNode& task_declaration_node);

  // Extracts function or task call and creates its corresponding fact tree.
  void ExtractFunctionOrTaskCall(
      const verible::SyntaxTreeNode& function_call_node);

  // Extracts function or task call tagged with kMethodCalLExtension (treated as
  // kFunctionOrTaskCall in facts tree) creates its corresponding fact tree.
  void ExtractMethodCallExtension(
      const verible::SyntaxTreeNode& call_extension_node);

  // Extracts function or task ports and parameters.
  void ExtractFunctionTaskPort(
      const verible::SyntaxTreeNode& function_declaration_node);

  // Extracts classes and creates its corresponding fact tree.
  void ExtractClassDeclaration(
      const verible::SyntaxTreeNode& class_declaration);

  // Extracts class instances and creates its corresponding fact tree.
  void ExtractClassInstances(
      const verible::SyntaxTreeNode& data_declaration,
      const std::vector<verible::TreeSearchMatch>& register_variables);

  // Extracts primitive types declarations and creates its corresponding fact
  // tree.
  void ExtractPrimitiveVariables(
      const verible::SyntaxTreeNode& enclosing_node,
      const std::vector<verible::TreeSearchMatch>& variable_matches);

  // Extracts leaves tagged with SymbolIdentifier and creates its facts tree.
  // This should only be reached in case of free variable references.
  // e.g assign out = in & in2.
  // Other extraction functions should terminate in case the inner
  // SymbolIdentifiers are extracted.
  void ExtractSymbolIdentifier(const verible::SyntaxTreeLeaf& unqualified_id);

  // Extracts parameter declarations and creates its corresponding fact tree.
  void ExtractParam(const verible::SyntaxTreeNode& param_declaration);

  // Extracts module instantiation named ports and creates its corresponding
  // fact tree.
  void ExtractModuleNamedPort(const verible::SyntaxTreeNode& actual_named_port);

  // Extracts package imports and creates its corresponding fact tree.
  void ExtractPackageImport(const verible::SyntaxTreeNode& package_import_item);

  // Extracts qualified ids and creates its corresponding fact tree.
  // e.g pkg::member or class::member.
  void ExtractQualifiedId(const verible::SyntaxTreeNode& qualified_id);

  // The Root of the constructed tree
  IndexingFactNode root_{IndexingNodeData(IndexingFactType::kFile)};

  // Used for getting token offsets in code text.
  verible::TokenInfo::Context context_;

  // Keeps track of facts tree ancestors as the visitor traverses CST.
  IndexingFactsTreeContext facts_tree_context_;
};

// Given a verilog file returns the extracted indexing facts tree.
IndexingFactNode ExtractOneFile(absl::string_view content,
                                absl::string_view filename, int& exit_status,
                                bool& parse_ok);

// Given a root to CST this function traverses the tree and extracts and
// constructs the indexing facts tree.
IndexingFactNode BuildIndexingFactsTree(
    const verible::ConcreteSyntaxTree& syntax_tree, absl::string_view base,
    absl::string_view file_name);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
