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
#include "verilog/analysis/verilog_project.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/indexing_facts_tree_context.h"

namespace verilog {
namespace kythe {

// This class is used for traversing CST and extracting different indexing
// facts from CST nodes and constructs a tree of indexing facts.
class IndexingFactsTreeExtractor : public verible::TreeContextVisitor {
 public:
  IndexingFactsTreeExtractor(IndexingFactNode& file_list_facts_tree,
                             const VerilogSourceFile& source_file,
                             VerilogProject* project)
      : context_(verible::TokenInfo::Context(
            source_file.GetTextStructure()->Contents())),
        file_list_facts_tree_(file_list_facts_tree),
        source_file_(source_file),
        project_(project) {
    const absl::string_view base = source_file.GetTextStructure()->Contents();
    // Create the Anchors for file path node.
    root_.Value().AppendAnchor(
        Anchor(source_file.ResolvedPath(), 0, base.size()));
    // Create the Anchors for text (code) node.
    root_.Value().AppendAnchor(Anchor(base, 0, base.size()));
  }

  void Visit(const verible::SyntaxTreeLeaf& leaf) override;
  void Visit(const verible::SyntaxTreeNode& node) override;

  IndexingFactNode& GetRoot() { return root_; }

 private:  // methods
  // Extracts facts from module, intraface and program declarations.
  void ExtractModuleOrInterfaceOrProgram(
      const verible::SyntaxTreeNode& declaration_node,
      IndexingFactType node_type);

  // Extracts modules instantiations and creates its corresponding fact tree.
  void ExtractModuleInstantiation(
      const verible::SyntaxTreeNode& data_declaration_node,
      const std::vector<verible::TreeSearchMatch>& gate_instances);

  // Extracts endmodule, endinterface, endprogram and creates its corresponding
  // fact tree.
  void ExtractModuleOrInterfaceOrProgramEnd(
      const verible::SyntaxTreeNode& module_declaration_node);

  // Extracts module, interface, program headers and creates its corresponding
  // fact tree.
  void ExtractModuleOrInterfaceOrProgramHeader(
      const verible::SyntaxTreeNode& module_declaration_node);

  // Extracts modules ports and creates its corresponding fact tree.
  // "has_propagated_type" determines if this port is "Non-ANSI" or not.
  // e.g "module m(a, b, input c, d)" starting from "c" "has_propagated_type"
  // will be true.
  void ExtractModulePort(const verible::SyntaxTreeNode& module_port_node,
                         bool has_propagated_type);

  // Extracts "a" from "input a", "output a" and creates its corresponding fact
  // tree.
  void ExtractInputOutputDeclaration(
      const verible::SyntaxTreeNode& identifier_unpacked_dimensions);

  // Extracts "a" from "wire a" and creates its corresponding fact tree.
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

  // Extract macro names from "kMacroIdentifiers" which are considered
  // references to macros and creates its corresponding facts tree.
  void ExtractMacroReference(const verible::SyntaxTreeLeaf& macro_identifier);

  // Extracts Include statements and creates its corresponding fact tree.
  void ExtractInclude(const verible::SyntaxTreeNode& preprocessor_include);

  // Extracts function and creates its corresponding fact tree.
  void ExtractFunctionDeclaration(
      const verible::SyntaxTreeNode& function_declaration_node);

  // Extracts class constructor and creates its corresponding fact tree.
  void ExtractClassConstructor(
      const verible::SyntaxTreeNode& class_constructor);

  // Extracts task and creates its corresponding fact tree.
  void ExtractTaskDeclaration(
      const verible::SyntaxTreeNode& task_declaration_node);

  // Extracts function or task call and creates its corresponding fact tree.
  void ExtractFunctionOrTaskCall(
      const verible::SyntaxTreeNode& function_call_node);

  // Extracts function or task call tagged with "kMethodCallExtension" (treated
  // as kFunctionOrTaskCall in facts tree) and creates its corresponding fact
  // tree.
  void ExtractMethodCallExtension(
      const verible::SyntaxTreeNode& call_extension_node);

  // Extracts members tagged with "kHierarchyExtension" (treated as
  // kMemberReference in facts tree) and creates its corresponding fact tree.
  void ExtractMemberExtension(
      const verible::SyntaxTreeNode& hierarchy_extension_node);

  // Extracts function or task ports and parameters.
  void ExtractFunctionOrTaskOrConstructorPort(
      const verible::SyntaxTreeNode& function_declaration_node);

  // Extracts classes and creates its corresponding fact tree.
  void ExtractClassDeclaration(
      const verible::SyntaxTreeNode& class_declaration);

  // Extracts class instances and creates its corresponding fact tree.
  void ExtractClassInstances(
      const verible::SyntaxTreeNode& data_declaration,
      const std::vector<verible::TreeSearchMatch>& class_instances);

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

  // Extracts pure virtual functions and creates its corresponding fact tree.
  void ExtractPureVirtualFunction(
      const verible::SyntaxTreeNode& function_prototype);

  // Extracts pure virtual tasks and creates its corresponding fact tree.
  void ExtractPureVirtualTask(const verible::SyntaxTreeNode& task_prototype);

  // Extracts function header and creates its corresponding fact tree.
  void ExtractFunctionHeader(const verible::SyntaxTreeNode& function_header,
                             IndexingFactNode& function_node);

  // Extracts task header and creates its corresponding fact tree.
  void ExtractTaskHeader(const verible::SyntaxTreeNode& task_header,
                         IndexingFactNode& task_node);

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
  // e.g "some_type var1;"
  void ExtractTypedVariableDefinition(
      const verible::Symbol& type_identifier,
      const std::vector<verible::TreeSearchMatch>& variables_matched);

  // Extracts leaves tagged with SymbolIdentifier and creates its facts
  // tree. This should only be reached in case of free variable references.
  // e.g "assign out = in & in2."
  // Other extraction functions should terminate in case the inner
  // SymbolIdentifiers are extracted.
  void ExtractSymbolIdentifier(
      const verible::SyntaxTreeLeaf& symbol_identifier);

  // Extracts nodes tagged with "kUnqualifiedId".
  void ExtractUnqualifiedId(const verible::SyntaxTreeNode& unqualified_id);

  // Extracts parameter declarations and creates its corresponding fact tree.
  void ExtractParamDeclaration(
      const verible::SyntaxTreeNode& param_declaration);

  // Extracts module instantiation named ports and creates its corresponding
  // fact tree.
  void ExtractModuleNamedPort(const verible::SyntaxTreeNode& actual_named_port);

  // Extracts package imports and creates its corresponding fact tree.
  void ExtractPackageImport(const verible::SyntaxTreeNode& package_import_item);

  // Extracts qualified ids and creates its corresponding fact tree.
  // e.g "pkg::member" or "class::member".
  void ExtractQualifiedId(const verible::SyntaxTreeNode& qualified_id);

  // Extracts initializations in for loop and creates its corresponding fact
  // tree. e.g from "for(int i = 0, j = k; ...)" extracts "i", "j" and "k".
  void ExtractForInitialization(
      const verible::SyntaxTreeNode& for_initialization);

  // Extracts param references and the actual references names.
  // e.g from "counter #(.N(r))" extracts "N".
  void ExtractParamByName(const verible::SyntaxTreeNode& param_by_name);

  // Extracts new scope and assign unique id to it.
  // specifically, intended for conditional/loop generate constructs.
  void ExtractAnonymousScope(const verible::SyntaxTreeNode& node);

  // Determines how to deal with the given data declaration node as it may be
  // module instance, class instance or primitive variable.
  void ExtractDataDeclaration(const verible::SyntaxTreeNode& data_declaration);

  // Moves the anchors and children from the the last extracted node in
  // "facts_tree_context_", adds them to the new_node and pops remove the last
  // extracted node.
  void MoveAndDeleteLastExtractedNode(IndexingFactNode& new_node);

 private:  // data members
  // The Root of the constructed facts tree.
  IndexingFactNode root_{IndexingNodeData(IndexingFactType::kFile)};

  // Used for getting token offsets in code text.
  // TODO(fangism): if a string_view is enough, get it from source_file_.
  verible::TokenInfo::Context context_;

  // Keeps track of indexing facts tree ancestors as the visitor traverses CST.
  IndexingFactsTreeContext facts_tree_context_;

  // "IndexingFactNode" with tag kFileList which holds the extracted indexing
  // facts trees of the files in the ordered file list. The extracted files will
  // be children of this node and ordered as they are given in the ordered file
  // list.
  IndexingFactNode& file_list_facts_tree_;

  // The current file being extracted.
  const VerilogSourceFile& source_file_;

  // The project configuration used to find included files.
  VerilogProject* const project_;

  // Counter used as an id for the anonymous scopes.
  int next_anonymous_id = 0;
};

// Given a set of SystemVerilog project files, extracts and returns the
// IndexingFactsTree for the given files.
// The returned Root will have the files as children and they will retain their
// original ordering from the file list.
IndexingFactNode ExtractFiles(absl::string_view file_list_path,
                              VerilogProject* project,
                              const std::vector<std::string>& file_names);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
