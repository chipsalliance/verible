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

#include <initializer_list>

#include "absl/status/status.h"
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
  IndexingFactsTreeExtractor(
      absl::string_view base, absl::string_view file_name,
      IndexingFactNode& file_list_facts_tree,
      std::map<std::string, std::string>& extracted_files,
      const std::vector<std::string>& include_dir_paths)
      : context_(verible::TokenInfo::Context(base)),
        file_list_facts_tree_(file_list_facts_tree),
        extracted_files_(extracted_files),
        include_dir_paths_(include_dir_paths) {
    root_.Value().AppendAnchor(Anchor(file_name, 0, base.size()));
    root_.Value().AppendAnchor(Anchor(base, 0, base.size()));
  }

  void Visit(const verible::SyntaxTreeLeaf& leaf) override;
  void Visit(const verible::SyntaxTreeNode& node) override;

  IndexingFactNode& GetRoot() { return root_; }

 private:
  // Extracts facts from module, intraface and program declarations.
  void ExtractModuleOrInterfaceOrProgram(
      const verible::SyntaxTreeNode& declaration_node,
      IndexingFactNode& facts_node);

  // Extracts modules and creates its corresponding fact tree.
  void ExtractModule(const verible::SyntaxTreeNode& module_declaration_node);

  // Extracts interfaces and creates its corresponding fact tree.
  void ExtractInterface(
      const verible::SyntaxTreeNode& interface_declaration_node);

  // Extracts programs and creates its corresponding fact tree.
  void ExtractProgram(const verible::SyntaxTreeNode& program_declaration_node);

  // Extracts modules instantiations and creates its corresponding fact tree.
  void ExtractModuleInstantiation(
      const verible::SyntaxTreeNode& data_declaration_node,
      const std::vector<verible::TreeSearchMatch>& gate_instances);

  // Extracts endmodule and creates its corresponding fact tree.
  void ExtractModuleEnd(const verible::SyntaxTreeNode& module_declaration_node);

  // Extracts modules headers and creates its corresponding fact tree.
  void ExtractModuleHeader(
      const verible::SyntaxTreeNode& module_declaration_node);

  // Extracts modules ports and creates its corresponding fact tree.
  void ExtractModulePort(const verible::SyntaxTreeNode& module_port_node,
                         bool has_propagated_type);

  // Extracts variable dimensions and creates its corresponding fact tree.
  // e.g x[i] ==> extracts "[i]".
  void ExtractSelectVariableDimension(
      const verible::SyntaxTreeNode& variable_dimension);

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

  // Extracts Include statements and creates its corresponding fact tree.
  void ExtractInclude(const verible::SyntaxTreeNode& preprocessor_include);

  // Extracts function and creates its corresponding fact tree.
  void ExtractFunctionDeclaration(
      const verible::SyntaxTreeNode& function_declaration_node);

  // Extracts task and creates its corresponding fact tree.
  void ExtractTaskDeclaration(
      const verible::SyntaxTreeNode& task_declaration_node);

  // Extracts function or task call and creates its corresponding fact tree.
  void ExtractFunctionOrTaskCall(
      const verible::SyntaxTreeNode& function_call_node);

  // Extracts function or task call tagged with kMethodCallExtension (treated as
  // kFunctionOrTaskCall in facts tree) and creates its corresponding fact tree.
  void ExtractMethodCallExtension(
      const verible::SyntaxTreeNode& call_extension_node);

  // Extracts members tagged with kHierarchyExtension (treated as
  // kMemberReference in facts tree) and creates its corresponding fact tree.
  void ExtractMemberExtension(
      const verible::SyntaxTreeNode& hierarchy_extension_node);

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

  // Extracts primitive types declarations tagged with kRegisterVariable and
  // creates its corresponding fact tree.
  void ExtractRegisterVariable(
      const verible::SyntaxTreeNode& register_variable);

  // Extracts primitive types declarations tagged with
  // kVariableDeclarationAssignment and creates its corresponding fact tree.
  void ExtractVariableDeclarationAssignment(
      const verible::SyntaxTreeNode& variable_declaration_assignment);

  // Extracts enum name and creates its corresponding fact tree.
  void ExtractEnumName(const verible::SyntaxTreeNode& enum_name);

  // Extracts type declaration preceeded with "typedef" and creates its
  // corresponding fact tree.
  void ExtractTypeDeclaration(const verible::SyntaxTreeNode& type_declaration);

  // Extracts enum type declaration preceeded with "typedef" and creates its
  // corresponding fact tree.
  void ExtractEnumTypeDeclaration(
      const verible::SyntaxTreeNode& enum_type_declaration);

  // Extracts struct type declaration preceeded with "typedef" and creates its
  // corresponding fact tree.
  void ExtractStructUnionTypeDeclaration(
      const verible::SyntaxTreeNode& type_declaration,
      const verible::SyntaxTreeNode& struct_type);

  // Extracts struct declaration and creates its corresponding fact tree.
  void ExtractStructUnionDeclaration(
      const verible::SyntaxTreeNode& struct_type,
      const std::vector<verible::TreeSearchMatch>& variables_matched);

  // Extracts struct and union members and creates its corresponding fact tree.
  void ExtractDataTypeImplicitIdDimensions(
      const verible::SyntaxTreeNode& data_type_implicit_id_dimensions);

  // Extracts variable definitions preceeded with some data type and creates its
  // corresponding fact tree.
  // e.g some_type var1;
  void ExtractTypedVariableDefinition(
      const verible::SyntaxTreeLeaf& type_identifier,
      const std::vector<verible::TreeSearchMatch>& variables_matched);

  // Extracts leaves tagged with SymbolIdentifier and creates its facts
  // tree. This should only be reached in case of free variable references.
  // e.g assign out = in & in2.
  // Other extraction functions should terminate in case the inner
  // SymbolIdentifiers are extracted.
  void ExtractSymbolIdentifier(const verible::SyntaxTreeLeaf& unqualified_id);

  // Extracts parameter declarations and creates its corresponding fact tree.
  void ExtractParamDeclaration(
      const verible::SyntaxTreeNode& param_declaration);

  // Extracts module instantiation named ports and creates its corresponding
  // fact tree.
  void ExtractModuleNamedPort(const verible::SyntaxTreeNode& actual_named_port);

  // Extracts package imports and creates its corresponding fact tree.
  void ExtractPackageImport(const verible::SyntaxTreeNode& package_import_item);

  // Extracts qualified ids and creates its corresponding fact tree.
  // e.g pkg::member or class::member.
  void ExtractQualifiedId(const verible::SyntaxTreeNode& qualified_id);

  // Extracts initializations in for loop and creates its corresponding fact
  // tree. e.g for(int i = 0, j = k; ...) extracts "i", "j" and "k".
  void ExtractForInitialization(
      const verible::SyntaxTreeNode& for_initialization);

  // Extracts param references and the actual references names.
  // e.g counter #(.N(r)) extracts "N".
  void ExtractParamByName(const verible::SyntaxTreeNode& param_by_name);

  // Determines how to deal with the given data declaration node as it may be
  // module instance, class instance or primitive variable.
  void ExtractDataDeclaration(const verible::SyntaxTreeNode& data_declaration);

  // Copies the anchors and children from the the last sibling of
  // facts_tree_context_, adds them to the new_node and pops that sibling.
  void MoveAndDeleteLastSibling(IndexingFactNode& new_node);

  // The Root of the constructed tree
  IndexingFactNode root_{IndexingNodeData(IndexingFactType::kFile)};

  // Used for getting token offsets in code text.
  verible::TokenInfo::Context context_;

  // Keeps track of facts tree ancestors as the visitor traverses CST.
  IndexingFactsTreeContext facts_tree_context_;

  // IndexingFactNode with tag kFileList which holds the extracted facts trees
  // of the files in the ordered file list.
  // The extracted files will be children of this node and ordered as they are
  // given in the ordered file list.
  IndexingFactNode& file_list_facts_tree_;

  // Maps every file name to its file path.
  // Used to avoid extracting some file more than one time.
  // Key "filename", Value "file_path" for that file.
  std::map<std::string, std::string>& extracted_files_;

  // Holds the paths of the directories used to look for the included
  // files.
  const std::vector<std::string>& include_dir_paths_;
};

// Given the ordered SystemVerilog files, Extracts and returns the
// IndexingFactsTree from the given files.
// The returned Root will have the files as children and they will retain their
// original ordering from the file list.
IndexingFactNode ExtractFiles(
    const std::vector<std::string>& ordered_file_list,
    std::vector<absl::Status>& exit_status, absl::string_view file_list_dir,
    const std::vector<std::string>& include_dir_paths);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
