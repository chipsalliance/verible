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

#include "verible/verilog/tools/kythe/indexing-facts-tree-extractor.h"

#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-context-visitor.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/common/util/tree-operations.h"
#include "verible/verilog/CST/class.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/functions.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/macro.h"
#include "verible/verilog/CST/module.h"
#include "verible/verilog/CST/net.h"
#include "verible/verilog/CST/package.h"
#include "verible/verilog/CST/parameters.h"
#include "verible/verilog/CST/port.h"
#include "verible/verilog/CST/statement.h"
#include "verible/verilog/CST/tasks.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/parser/verilog-token-enum.h"
#include "verible/verilog/tools/kythe/indexing-facts-tree-context.h"
#include "verible/verilog/tools/kythe/indexing-facts-tree.h"
#include "verible/verilog/tools/kythe/verilog-extractor-indexing-fact-type.h"

namespace verilog {
namespace kythe {

namespace {

using verible::Symbol;
using verible::SymbolKind;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TokenInfo;
using verible::TreeSearchMatch;

struct VerilogExtractionState {
  // Multi-file tracker.
  VerilogProject *const project;
  // Keep track of which files (translation units, includes) have been
  // extracted.
  std::set<const VerilogSourceFile *> extracted_files;
};

// This class is used for traversing CST and extracting different indexing
// facts from CST nodes and constructs a tree of indexing facts.
class IndexingFactsTreeExtractor : public verible::TreeContextVisitor {
 public:
  IndexingFactsTreeExtractor(IndexingFactNode *file_list_facts_tree,
                             const VerilogSourceFile &source_file,
                             VerilogExtractionState *extraction_state,
                             std::vector<absl::Status> *errors)
      : file_list_facts_tree_(file_list_facts_tree),
        source_file_(source_file),
        extraction_state_(extraction_state),
        errors_(errors) {
    const std::string_view base = source_file_.GetTextStructure()->Contents();
    root_.Value().AppendAnchor(
        // Create the Anchor for file path node.
        Anchor(source_file_.ResolvedPath()),
        // Create the Anchor for text (code) node.
        Anchor(base));
  }

  void Visit(const SyntaxTreeLeaf &leaf) final;
  void Visit(const SyntaxTreeNode &node) final;

  const IndexingFactNode &Root() const { return root_; }
  IndexingFactNode TakeRoot() { return std::move(root_); }

 private:  // methods
  // Extracts facts from module, intraface and program declarations.
  void ExtractModuleOrInterfaceOrProgram(const SyntaxTreeNode &declaration_node,
                                         IndexingFactType node_type);

  // Extracts modules instantiations and creates its corresponding fact tree.
  void ExtractModuleInstantiation(
      const SyntaxTreeNode &data_declaration_node,
      const std::vector<TreeSearchMatch> &gate_instances);

  // Extracts endmodule, endinterface, endprogram and creates its corresponding
  // fact tree.
  void ExtractModuleOrInterfaceOrProgramEnd(
      const SyntaxTreeNode &module_declaration_node);

  // Extracts module, interface, program headers and creates its corresponding
  // fact tree.
  void ExtractModuleOrInterfaceOrProgramHeader(
      const SyntaxTreeNode &module_declaration_node);

  // Extracts modules ports and creates its corresponding fact tree.
  // "has_propagated_type" determines if this port is "Non-ANSI" or not.
  // e.g "module m(a, b, input c, d)" starting from "c" "has_propagated_type"
  // will be true.
  void ExtractModulePort(const SyntaxTreeNode &module_port_node,
                         bool has_propagated_type);

  // Extracts "a" from "input a", "output a" and creates its corresponding fact
  // tree.
  void ExtractInputOutputDeclaration(
      const SyntaxTreeNode &identifier_unpacked_dimensions);

  // Extracts "a" from "wire a" and creates its corresponding fact tree.
  void ExtractNetDeclaration(const SyntaxTreeNode &net_declaration_node);

  // Extract package declarations and creates its corresponding facts tree.
  void ExtractPackageDeclaration(
      const SyntaxTreeNode &package_declaration_node);

  // Extract macro definitions and explores its arguments and creates its
  // corresponding facts tree.
  void ExtractMacroDefinition(const SyntaxTreeNode &preprocessor_definition);

  // Extract macro calls and explores its arguments and creates its
  // corresponding facts tree.
  void ExtractMacroCall(const SyntaxTreeNode &macro_call);

  // Extract macro names from "kMacroIdentifiers" which are considered
  // references to macros and creates its corresponding facts tree.
  void ExtractMacroReference(const SyntaxTreeLeaf &macro_identifier);

  // Extracts Include statements and creates its corresponding fact tree.
  void ExtractInclude(const SyntaxTreeNode &preprocessor_include);

  // Extracts function and creates its corresponding fact tree.
  void ExtractFunctionDeclaration(
      const SyntaxTreeNode &function_declaration_node);

  // Extracts class constructor and creates its corresponding fact tree.
  void ExtractClassConstructor(const SyntaxTreeNode &class_constructor);

  // Extracts task and creates its corresponding fact tree.
  void ExtractTaskDeclaration(const SyntaxTreeNode &task_declaration_node);

  // Extracts function or task call and creates its corresponding fact tree.
  void ExtractFunctionOrTaskCall(const SyntaxTreeNode &function_call_node);

  // Extracts function or task call tagged with "kMethodCallExtension" (treated
  // as kFunctionOrTaskCall in facts tree) and creates its corresponding fact
  // tree.
  void ExtractMethodCallExtension(const SyntaxTreeNode &call_extension_node);

  // Extracts members tagged with "kHierarchyExtension" (treated as
  // kMemberReference in facts tree) and creates its corresponding fact tree.
  void ExtractMemberExtension(const SyntaxTreeNode &hierarchy_extension_node);

  // Extracts function or task ports and parameters.
  void ExtractFunctionOrTaskOrConstructorPort(
      const SyntaxTreeNode &function_declaration_node);

  // Extracts classes and creates its corresponding fact tree.
  void ExtractClassDeclaration(const SyntaxTreeNode &class_declaration);

  // Extracts class instances and creates its corresponding fact tree.
  void ExtractClassInstances(
      const SyntaxTreeNode &data_declaration,
      const std::vector<TreeSearchMatch> &class_instances);

  // Extracts primitive types declarations tagged with kRegisterVariable and
  // creates its corresponding fact tree.
  void ExtractRegisterVariable(const SyntaxTreeNode &register_variable);

  // Extracts primitive types declarations tagged with
  // kVariableDeclarationAssignment and creates its corresponding fact tree.
  void ExtractVariableDeclarationAssignment(
      const SyntaxTreeNode &variable_declaration_assignment);

  // Extracts enum name and creates its corresponding fact tree.
  void ExtractEnumName(const SyntaxTreeNode &enum_name);

  // Extracts type declaration preceeded with "typedef" and creates its
  // corresponding fact tree.
  void ExtractTypeDeclaration(const SyntaxTreeNode &type_declaration);

  // Extracts pure virtual functions and creates its corresponding fact tree.
  void ExtractPureVirtualFunction(const SyntaxTreeNode &function_prototype);

  // Extracts pure virtual tasks and creates its corresponding fact tree.
  void ExtractPureVirtualTask(const SyntaxTreeNode &task_prototype);

  // Extracts function header and creates its corresponding fact tree.
  void ExtractFunctionHeader(const SyntaxTreeNode &function_header,
                             IndexingFactNode *function_node);

  // Extracts task header and creates its corresponding fact tree.
  void ExtractTaskHeader(const SyntaxTreeNode &task_header,
                         IndexingFactNode *task_node);

  // Extracts enum type declaration preceeded with "typedef" and creates its
  // corresponding fact tree.
  void ExtractEnumTypeDeclaration(const SyntaxTreeNode &enum_type_declaration);

  // Extracts struct type declaration preceeded with "typedef" and creates its
  // corresponding fact tree.
  void ExtractStructUnionTypeDeclaration(const SyntaxTreeNode &type_declaration,
                                         const SyntaxTreeNode &struct_type);

  // Extracts struct declaration and creates its corresponding fact tree.
  void ExtractStructUnionDeclaration(
      const SyntaxTreeNode &struct_type,
      const std::vector<TreeSearchMatch> &variables_matched);

  // Extracts struct and union members and creates its corresponding fact tree.
  void ExtractDataTypeImplicitIdDimensions(
      const SyntaxTreeNode &data_type_implicit_id_dimensions);

  // Extracts variable definitions preceeded with some data type and creates its
  // corresponding fact tree.
  // e.g "some_type var1;"
  void ExtractTypedVariableDefinition(
      const Symbol &type_identifier,
      const std::vector<TreeSearchMatch> &variables_matched);

  // Extracts leaves tagged with SymbolIdentifier and creates its facts
  // tree. This should only be reached in case of free variable references.
  // e.g "assign out = in & in2."
  // Other extraction functions should terminate in case the inner
  // SymbolIdentifiers are extracted.
  void ExtractSymbolIdentifier(const SyntaxTreeLeaf &symbol_identifier);

  // Extracts nodes tagged with "kUnqualifiedId".
  void ExtractUnqualifiedId(const SyntaxTreeNode &unqualified_id);

  // Extracts parameter declarations and creates its corresponding fact tree.
  void ExtractParamDeclaration(const SyntaxTreeNode &param_declaration);

  // Extracts module instantiation named ports and creates its corresponding
  // fact tree.
  void ExtractModuleNamedPort(const SyntaxTreeNode &actual_named_port);

  // Extracts package imports and creates its corresponding fact tree.
  void ExtractPackageImport(const SyntaxTreeNode &package_import_item);

  // Extracts qualified ids and creates its corresponding fact tree.
  // e.g "pkg::member" or "class::member".
  void ExtractQualifiedId(const SyntaxTreeNode &qualified_id);

  // Extracts initializations in for loop and creates its corresponding fact
  // tree. e.g from "for(int i = 0, j = k; ...)" extracts "i", "j" and "k".
  void ExtractForInitialization(const SyntaxTreeNode &for_initialization);

  // Extracts param references and the actual references names.
  // e.g from "counter #(.N(r))" extracts "N".
  void ExtractParamByName(const SyntaxTreeNode &param_by_name);

  // Extracts new scope and assign unique id to it.
  // specifically, intended for conditional/loop generate constructs.
  void ExtractAnonymousScope(const SyntaxTreeNode &node);

  // Determines how to deal with the given data declaration node as it may be
  // module instance, class instance or primitive variable.
  void ExtractDataDeclaration(const SyntaxTreeNode &data_declaration);

  // Moves the anchors and children from the the last extracted node in
  // "facts_tree_context_", adds them to the new_node and pops remove the last
  // extracted node.
  void MoveAndDeleteLastExtractedNode(IndexingFactNode *new_node);

  std::string_view FileContent() {
    return source_file_.GetTextStructure()->Contents();
  }

 private:  // data members
  // The Root of the constructed facts tree.
  IndexingFactNode root_{IndexingNodeData(IndexingFactType::kFile)};

  // Keeps track of indexing facts tree ancestors as the visitor traverses CST.
  IndexingFactsTreeContext facts_tree_context_;

  // "IndexingFactNode" with tag kFileList which holds the extracted indexing
  // facts trees of the files in the ordered file list. The extracted files will
  // be children of this node and ordered as they are given in the ordered file
  // list.
  IndexingFactNode *file_list_facts_tree_;

  // The current file being extracted.
  const VerilogSourceFile &source_file_;

  // The project configuration used to find included files.
  VerilogExtractionState *const extraction_state_;

  // Processing errors.
  std::vector<absl::Status> *errors_;

  // Counter used as an id for the anonymous scopes.
  int next_anonymous_id = 0;
};

// Given a root to CST this function traverses the tree, extracts and constructs
// the indexing facts tree for one file.
IndexingFactNode BuildIndexingFactsTree(
    IndexingFactNode *file_list_facts_tree,
    const VerilogSourceFile &source_file,
    VerilogExtractionState *extraction_state,
    std::vector<absl::Status> *errors) {
  VLOG(1) << __FUNCTION__ << ": file: " << source_file;
  IndexingFactsTreeExtractor visitor(file_list_facts_tree, source_file,
                                     extraction_state, errors);

  if (source_file.Status().ok()) {
    const auto &syntax_tree = source_file.GetTextStructure()->SyntaxTree();
    if (syntax_tree != nullptr) {
      VLOG(2) << "syntax:\n" << verible::RawTreePrinter(*syntax_tree);
      syntax_tree->Accept(&visitor);
    }
  }
  const PrintableIndexingFactNode debug_node(
      visitor.Root(), source_file.GetTextStructure()->Contents());
  VLOG(2) << "built facts tree: " << debug_node;
  return visitor.TakeRoot();
}

}  // namespace

IndexingFactNode ExtractFiles(std::string_view file_list_path,
                              VerilogProject *project,
                              const std::vector<std::string> &file_names,
                              std::vector<absl::Status> *errors) {
  VLOG(1) << __FUNCTION__;
  // Open all of the translation units.
  for (std::string_view file_name : file_names) {
    const auto status_or_file = project->OpenTranslationUnit(file_name);
    if (!status_or_file.ok()) {
      if (errors != nullptr) {
        errors->push_back(status_or_file.status());
      } else {
        LOG(ERROR) << "Failed to open file " << file_name << ": "
                   << status_or_file.status();
      }
    }
    // For now, collect all diagnostics at the end.
    // TODO(fangism): offer a mode to exit-early if there are file-not-found
    // or read-permission issues (fail-fast, alert-user).
  }

  // Create a node to hold the path and root of the ordered file list, group
  // all the files and acts as a ordered file list of these files.
  IndexingFactNode file_list_facts_tree(
      IndexingNodeData(IndexingFactType::kFileList, Anchor(file_list_path),
                       Anchor(project->TranslationUnitRoot())));

  VerilogExtractionState project_extraction_state{project};

  // pre-allocate file nodes with the number of translation units
  file_list_facts_tree.Children().reserve(file_names.size());
  for (std::string_view file_name : file_names) {
    auto *translation_unit = project->LookupRegisteredFile(file_name);
    if (translation_unit == nullptr) continue;
    const auto parse_status = translation_unit->Parse();
    // status is also stored in translation_unit for later retrieval.
    if (parse_status.ok()) {
      file_list_facts_tree.Children().push_back(
          BuildIndexingFactsTree(&file_list_facts_tree, *translation_unit,
                                 &project_extraction_state, errors));
    } else {
      if (errors != nullptr) {
        errors->push_back(parse_status);
      } else {
        LOG(WARNING) << "Failed to parse file " << file_name << ": "
                     << parse_status;
      }
    }
    project->RemoveRegisteredFile(file_name);
  }
  VLOG(1) << "end of " << __FUNCTION__;
  return file_list_facts_tree;
}

void IndexingFactsTreeExtractor::Visit(const SyntaxTreeNode &node) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  VLOG(3) << __FUNCTION__ << ", tag: " << tag;
  switch (tag) {
    case NodeEnum ::kDescriptionList: {
      // Adds the current root to facts tree context to keep track of the parent
      // node so that it can be used to construct the tree and add children to
      // it.
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &root_);
      TreeContextVisitor::Visit(node);
      break;
    }
    case NodeEnum::kInterfaceDeclaration: {
      ExtractModuleOrInterfaceOrProgram(node, IndexingFactType::kInterface);
      break;
    }
    case NodeEnum::kModuleDeclaration: {
      ExtractModuleOrInterfaceOrProgram(node, IndexingFactType::kModule);
      break;
    }
    case NodeEnum::kProgramDeclaration: {
      ExtractModuleOrInterfaceOrProgram(node, IndexingFactType::kProgram);
      break;
    }
    case NodeEnum::kDataDeclaration: {
      ExtractDataDeclaration(node);
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
    case NodeEnum::kPackageDeclaration: {
      ExtractPackageDeclaration(node);
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
    case NodeEnum::kFunctionDeclaration: {
      ExtractFunctionDeclaration(node);
      break;
    }
    case NodeEnum::kTaskDeclaration: {
      ExtractTaskDeclaration(node);
      break;
    }
    case NodeEnum::kClassConstructor: {
      ExtractClassConstructor(node);
      break;
    }
    case NodeEnum::kFunctionCall: {
      ExtractFunctionOrTaskCall(node);
      break;
    }
    case NodeEnum::kMethodCallExtension: {
      ExtractMethodCallExtension(node);
      break;
    }
    case NodeEnum::kHierarchyExtension: {
      ExtractMemberExtension(node);
      break;
    }
    case NodeEnum::kClassDeclaration: {
      ExtractClassDeclaration(node);
      break;
    }
    case NodeEnum::kParamDeclaration: {
      ExtractParamDeclaration(node);
      break;
    }
    case NodeEnum::kActualNamedPort: {
      ExtractModuleNamedPort(node);
      break;
    }
    case NodeEnum::kPackageImportItem: {
      ExtractPackageImport(node);
      break;
    }
    case NodeEnum::kQualifiedId: {
      ExtractQualifiedId(node);
      break;
    }
    case NodeEnum::kForInitialization: {
      ExtractForInitialization(node);
      break;
    }
    case NodeEnum::kDataTypeImplicitIdDimensions: {
      ExtractDataTypeImplicitIdDimensions(node);
      break;
    }
    case NodeEnum::kParamByName: {
      ExtractParamByName(node);
      break;
    }
    case NodeEnum::kPreprocessorInclude: {
      ExtractInclude(node);
      break;
    }
    case NodeEnum::kRegisterVariable: {
      ExtractRegisterVariable(node);
      break;
    }
    case NodeEnum::kFunctionPrototype: {
      ExtractPureVirtualFunction(node);
      break;
    }
    case NodeEnum::kTaskPrototype: {
      ExtractPureVirtualTask(node);
      break;
    }
    case NodeEnum::kVariableDeclarationAssignment: {
      ExtractVariableDeclarationAssignment(node);
      break;
    }
    case NodeEnum::kEnumName: {
      ExtractEnumName(node);
      break;
    }
    case NodeEnum::kTypeDeclaration: {
      ExtractTypeDeclaration(node);
      break;
    }
    case NodeEnum::kLoopGenerateConstruct:
    case NodeEnum::kIfClause:
    case NodeEnum::kFinalStatement:
    case NodeEnum::kInitialStatement:
    case NodeEnum::kGenerateElseBody:
    case NodeEnum::kElseClause:
    case NodeEnum::kGenerateIfClause:
    case NodeEnum::kForLoopStatement:
    case NodeEnum::kDoWhileLoopStatement:
    case NodeEnum::kWhileLoopStatement:
    case NodeEnum::kForeachLoopStatement:
    case NodeEnum::kRepeatLoopStatement:
    case NodeEnum::kForeverLoopStatement: {
      ExtractAnonymousScope(node);
      break;
    }
    case NodeEnum::kUnqualifiedId: {
      ExtractUnqualifiedId(node);
      break;
    }
    default: {
      TreeContextVisitor::Visit(node);
    }
  }
  VLOG(3) << "end of " << __FUNCTION__ << ", tag: " << tag;
}

void IndexingFactsTreeExtractor::Visit(const SyntaxTreeLeaf &leaf) {
  switch (leaf.get().token_enum()) {
    case verilog_tokentype::SymbolIdentifier: {
      ExtractSymbolIdentifier(leaf);
      break;
    }
    default: {
      break;
    }
  }
}

void IndexingFactsTreeExtractor::ExtractSymbolIdentifier(
    const SyntaxTreeLeaf &symbol_identifier) {
  facts_tree_context_.top().Children().emplace_back(
      IndexingNodeData(IndexingFactType::kVariableReference,
                       Anchor(symbol_identifier.get(), FileContent())));
}

void IndexingFactsTreeExtractor::ExtractDataDeclaration(
    const SyntaxTreeNode &data_declaration) {
  // For module instantiations
  const std::vector<TreeSearchMatch> gate_instances =
      FindAllGateInstances(data_declaration);
  if (!gate_instances.empty()) {
    ExtractModuleInstantiation(data_declaration, gate_instances);
    return;
  }

  // For bit, int and classes
  const std::vector<TreeSearchMatch> register_variables =
      FindAllRegisterVariables(data_declaration);
  if (!register_variables.empty()) {
    // for classes.
    const std::vector<TreeSearchMatch> class_instances =
        verible::SearchSyntaxTree(data_declaration, NodekClassNew());
    if (!class_instances.empty()) {
      ExtractClassInstances(data_declaration, register_variables);
      return;
    }

    // for struct and union types.
    const SyntaxTreeNode *type_node =
        GetStructOrUnionOrEnumTypeFromDataDeclaration(data_declaration);

    // Ignore if this isn't a struct or union type.
    if (type_node != nullptr &&
        NodeEnum(type_node->Tag().tag) != NodeEnum::kEnumType) {
      ExtractStructUnionDeclaration(*type_node, register_variables);
      return;
    }

    // In case "some_type var1".
    const Symbol *type_identifier =
        GetTypeIdentifierFromDataDeclaration(data_declaration);
    if (type_identifier != nullptr) {
      ExtractTypedVariableDefinition(*type_identifier, register_variables);
      return;
    }

    // Traverse the children to extract inner nodes.
    TreeContextVisitor::Visit(data_declaration);
    return;
  }

  const std::vector<TreeSearchMatch> variable_declaration_assign =
      FindAllVariableDeclarationAssignment(data_declaration);

  if (!variable_declaration_assign.empty()) {
    // for classes.
    const std::vector<TreeSearchMatch> class_instances =
        verible::SearchSyntaxTree(data_declaration, NodekClassNew());
    if (!class_instances.empty()) {
      ExtractClassInstances(data_declaration, variable_declaration_assign);
      return;
    }

    // for struct and union types.
    const SyntaxTreeNode *type_node =
        GetStructOrUnionOrEnumTypeFromDataDeclaration(data_declaration);

    // Ignore if this isn't a struct or union type.
    if (type_node != nullptr &&
        NodeEnum(type_node->Tag().tag) != NodeEnum::kEnumType) {
      ExtractStructUnionDeclaration(*type_node, variable_declaration_assign);
      return;
    }

    // In case "some_type var1".
    const Symbol *type_identifier =
        GetTypeIdentifierFromDataDeclaration(data_declaration);
    if (type_identifier != nullptr) {
      ExtractTypedVariableDefinition(*type_identifier,
                                     variable_declaration_assign);
      return;
    }

    // Traverse the children to extract inner nodes.
    TreeContextVisitor::Visit(data_declaration);
    return;
  }

  // Traverse the children to extract inner nodes.
  TreeContextVisitor::Visit(data_declaration);
}

void IndexingFactsTreeExtractor::ExtractTypedVariableDefinition(
    const Symbol &type_identifier,
    const std::vector<TreeSearchMatch> &variables_matche) {
  IndexingFactNode type_node(
      IndexingNodeData{IndexingFactType::kDataTypeReference});

  type_identifier.Accept(this);
  MoveAndDeleteLastExtractedNode(&type_node);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &type_node);
    for (const TreeSearchMatch &variable : variables_matche) {
      variable.match->Accept(this);
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(type_node));
}

void IndexingFactsTreeExtractor::ExtractModuleOrInterfaceOrProgram(
    const SyntaxTreeNode &declaration_node, IndexingFactType node_type) {
  IndexingFactNode facts_node(IndexingNodeData{node_type});

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &facts_node);
    ExtractModuleOrInterfaceOrProgramHeader(declaration_node);
    ExtractModuleOrInterfaceOrProgramEnd(declaration_node);

    const SyntaxTreeNode *item_list = GetModuleItemList(declaration_node);
    if (item_list) Visit(*item_list);
  }

  facts_tree_context_.top().Children().push_back(std::move(facts_node));
}

void IndexingFactsTreeExtractor::ExtractModuleOrInterfaceOrProgramHeader(
    const SyntaxTreeNode &module_declaration_node) {
  // Extract module name e.g from "module my_module" extracts "my_module".
  const SyntaxTreeLeaf *module_name_leaf =
      GetModuleName(module_declaration_node);
  if (!module_name_leaf) return;
  facts_tree_context_.top().Value().AppendAnchor(
      Anchor(module_name_leaf->get(), FileContent()));

  // Extract parameters if exist.
  const SyntaxTreeNode *param_declaration_list =
      GetParamDeclarationListFromModuleDeclaration(module_declaration_node);
  if (param_declaration_list != nullptr) {
    Visit(*param_declaration_list);
  }

  // Extracting module ports e.g. (input a, input b).
  // Ports are treated as children of the module.
  const SyntaxTreeNode *port_list =
      GetModulePortDeclarationList(module_declaration_node);

  if (port_list == nullptr) {
    return;
  }

  // This boolean is used to distinguish between ANSI and Non-ANSI module
  // ports. e.g in this case: module m(a, b); has_propagated_type will be
  // false as no type has been countered.
  //
  // in case like:
  // module m(a, b, input x, y)
  // for "a", "b" the boolean will be false but for "x", "y" the boolean will
  // be true.
  //
  // The boolean is used to determine whether this the fact for this variable
  // should be a reference or a defintiion.
  bool has_propagated_type = false;
  for (const auto &port : port_list->children()) {
    if (port->Kind() == SymbolKind::kLeaf) continue;

    const SyntaxTreeNode &port_node = SymbolCastToNode(*port);
    const auto tag = static_cast<verilog::NodeEnum>(port_node.Tag().tag);

    if (tag == NodeEnum::kPortDeclaration) {
      has_propagated_type = true;
      ExtractModulePort(port_node, has_propagated_type);
    } else if (tag == NodeEnum::kPort) {
      const SyntaxTreeNode *ref_port = GetPortReferenceFromPort(port_node);
      if (ref_port) ExtractModulePort(*ref_port, has_propagated_type);
    }
  }
}

void IndexingFactsTreeExtractor::ExtractModulePort(
    const SyntaxTreeNode &module_port_node, bool has_propagated_type) {
  const auto tag = static_cast<verilog::NodeEnum>(module_port_node.Tag().tag);

  // For extracting cases like:
  // module m(input a, input b);
  if (tag == NodeEnum::kPortDeclaration) {
    const SyntaxTreeLeaf *leaf =
        GetIdentifierFromPortDeclaration(module_port_node);
    if (!leaf) return;

    facts_tree_context_.top().Children().emplace_back(
        IndexingNodeData(IndexingFactType::kVariableDefinition,
                         Anchor(leaf->get(), FileContent())));
  } else if (tag == NodeEnum::kPortReference) {
    // For extracting Non-ANSI style ports:
    // module m(a, b);
    const SyntaxTreeLeaf *leaf =
        GetIdentifierFromPortReference(module_port_node);
    if (!leaf) return;

    if (has_propagated_type) {
      // Check if the last type was not a primitive type.
      // e.g module (interface_type x, y).
      if (is_leaf(facts_tree_context_.top()) ||
          facts_tree_context_.top()
                  .Children()
                  .back()
                  .Value()
                  .GetIndexingFactType() !=
              IndexingFactType::kDataTypeReference) {
        // Append this as a variable definition.
        facts_tree_context_.top().Children().emplace_back(
            IndexingNodeData(IndexingFactType::kVariableDefinition,
                             Anchor(leaf->get(), FileContent())));
      } else {
        // Append this as a child to previous kDataTypeReference.
        facts_tree_context_.top().Children().back().Children().emplace_back(
            IndexingNodeData(IndexingFactType::kVariableDefinition,
                             Anchor(leaf->get(), FileContent())));
      }
    } else {
      // In case no preceeded data type.
      facts_tree_context_.top().Children().emplace_back(
          IndexingNodeData(IndexingFactType::kVariableReference,
                           Anchor(leaf->get(), FileContent())));
    }
  }

  // Extract unpacked and packed dimensions.
  for (const auto &child : module_port_node.children()) {
    if (child == nullptr || child->Kind() == SymbolKind::kLeaf) {
      continue;
    }
    const auto tag = static_cast<verilog::NodeEnum>(child->Tag().tag);
    if (tag == NodeEnum::kUnqualifiedId) {
      continue;
    }
    if (tag == NodeEnum::kDataType) {
      const SyntaxTreeNode *data_type = GetTypeIdentifierFromDataType(*child);
      // If not null this is a non primitive type and should create
      // kDataTypeReference node for it.
      // This data_type may be some class or interface type.
      if (data_type != nullptr) {
        // Create a node for this data type and append its anchor.
        IndexingFactNode data_type_node(
            IndexingNodeData{IndexingFactType::kDataTypeReference});
        data_type->Accept(this);
        MoveAndDeleteLastExtractedNode(&data_type_node);

        // Make the current port node child of this data type, remove it from
        // the top node and push the kDataTypeRefernce Node.
        data_type_node.Children().push_back(
            std::move(facts_tree_context_.top().Children().back()));
        facts_tree_context_.top().Children().back() = std::move(data_type_node);
        continue;
      }
    }
    child->Accept(this);
  }
}

void IndexingFactsTreeExtractor::ExtractModuleNamedPort(
    const SyntaxTreeNode &actual_named_port) {
  const SyntaxTreeLeaf *named_port = GetActualNamedPortName(actual_named_port);
  if (!named_port) return;
  IndexingFactNode actual_port_node(
      IndexingNodeData(IndexingFactType::kModuleNamedPort,
                       Anchor(named_port->get(), FileContent())));
  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &actual_port_node);
    const Symbol *paren_group = GetActualNamedPortParenGroup(actual_named_port);
    if (paren_group != nullptr) {
      paren_group->Accept(this);
    }
  }

  facts_tree_context_.top().Children().emplace_back(
      std::move(actual_port_node));
}

void IndexingFactsTreeExtractor::ExtractInputOutputDeclaration(
    const SyntaxTreeNode &identifier_unpacked_dimension) {
  const SyntaxTreeLeaf *port_name_leaf =
      GetSymbolIdentifierFromIdentifierUnpackedDimensions(
          identifier_unpacked_dimension);

  if (port_name_leaf) {
    facts_tree_context_.top().Children().emplace_back(
        IndexingNodeData(IndexingFactType::kVariableDefinition,
                         Anchor(port_name_leaf->get(), FileContent())));
  }
}

void IndexingFactsTreeExtractor::ExtractModuleOrInterfaceOrProgramEnd(
    const SyntaxTreeNode &module_declaration_node) {
  const SyntaxTreeLeaf *module_name =
      GetModuleEndLabel(module_declaration_node);

  if (module_name != nullptr) {
    facts_tree_context_.top().Value().AppendAnchor(
        Anchor(module_name->get(), FileContent()));
  }
}

void IndexingFactsTreeExtractor::ExtractModuleInstantiation(
    const SyntaxTreeNode &data_declaration_node,
    const std::vector<TreeSearchMatch> &gate_instances) {
  // Extract module type name.
  const Symbol *type =
      GetTypeIdentifierFromDataDeclaration(data_declaration_node);
  if (type == nullptr) {
    return;
  }

  type->Accept(this);

  // Module instantiations (data declarations) may declare multiple instances
  // sharing the same type in a single statement e.g. bar b1(), b2().

  // Check an edge case first: if there is a data declaration with just type and
  // paren group, it is most likely a function call, not anonymous module /
  // primitive instantiation
  if (gate_instances.size() == 1 &&
      !GetModuleInstanceNameTokenInfoFromGateInstance(
          *gate_instances[0].match)) {
    IndexingFactNode function_node(
        IndexingNodeData{IndexingFactType::kFunctionCall});
    {
      const verible::Symbol *instantiation_base = GetSubtreeAsSymbol(
          data_declaration_node, NodeEnum::kDataDeclaration, 1);
      if (!instantiation_base) return;
      const verible::Symbol *type = GetSubtreeAsSymbol(
          *instantiation_base, NodeEnum::kInstantiationBase, 0);
      const verible::Symbol *reference =
          GetSubtreeAsSymbol(*type, NodeEnum::kInstantiationType, 0);
      if (reference->Tag().tag == (int)NodeEnum::kReference &&
          SymbolCastToNode(*reference).size() > 1) {
        bool is_first = true;
        for (const auto &child : SymbolCastToNode(*reference).children()) {
          if (is_first) {  // skip the first one.
            is_first = false;
            continue;
          }
          if (child->Tag().tag == (int)NodeEnum::kHierarchyExtension) {
            Visit(verible::SymbolCastToNode(*child));
          }
        }
      }
    }
    MoveAndDeleteLastExtractedNode(&function_node);
    const SyntaxTreeNode *paren_group =
        GetParenGroupFromModuleInstantiation(*gate_instances[0].match);
    if (paren_group) {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &function_node);
      Visit(*paren_group);
    }
    facts_tree_context_.top().Children().push_back(std::move(function_node));
    return;
  }
  // Extract module instance type and parameters.
  IndexingFactNode type_node(
      IndexingNodeData{IndexingFactType::kDataTypeReference});
  MoveAndDeleteLastExtractedNode(&type_node);

  // Loop through each instance and associate each declared id with the same
  // type and create its corresponding facts tree node.

  for (const TreeSearchMatch &instance : gate_instances) {
    IndexingFactNode module_instance_node(
        IndexingNodeData{IndexingFactType::kModuleInstance});

    const TokenInfo *variable_name =
        GetModuleInstanceNameTokenInfoFromGateInstance(*instance.match);
    if (variable_name) {
      module_instance_node.Value().AppendAnchor(
          Anchor(*variable_name, FileContent()));
    }

    {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &module_instance_node);
      const SyntaxTreeNode *paren_group =
          GetParenGroupFromModuleInstantiation(*instance.match);
      if (paren_group) Visit(*paren_group);
    }

    type_node.Children().push_back(std::move(module_instance_node));
  }

  facts_tree_context_.top().Children().push_back(std::move(type_node));
}

void IndexingFactsTreeExtractor::ExtractNetDeclaration(
    const SyntaxTreeNode &net_declaration_node) {
  // Nets are treated as children of the enclosing parent.
  // Net declarations may declare multiple instances sharing the same type in
  // a single statement.
  const std::vector<const TokenInfo *> identifiers =
      GetIdentifiersFromNetDeclaration(net_declaration_node);

  // Loop through each instance and associate each declared id with the same
  // type.
  for (const TokenInfo *wire_token_info : identifiers) {
    facts_tree_context_.top().Children().emplace_back(
        IndexingNodeData(IndexingFactType::kVariableDefinition,
                         Anchor(*wire_token_info, FileContent())));
  }
}

void IndexingFactsTreeExtractor::ExtractPackageDeclaration(
    const SyntaxTreeNode &package_declaration_node) {
  IndexingFactNode package_node(IndexingNodeData{IndexingFactType::kPackage});

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &package_node);
    // Extract package name.
    const SyntaxTreeLeaf *pname = GetPackageNameLeaf(package_declaration_node);
    if (pname) {
      facts_tree_context_.top().Value().AppendAnchor(
          Anchor(pname->get(), FileContent()));
    }

    // Extract package name after endpackage if exists.
    const SyntaxTreeLeaf *package_end_name =
        GetPackageNameEndLabel(package_declaration_node);

    if (package_end_name != nullptr) {
      facts_tree_context_.top().Value().AppendAnchor(
          Anchor(package_end_name->get(), FileContent()));
    }

    // Visit package body it exists.
    const Symbol *package_item_list =
        GetPackageItemList(package_declaration_node);
    if (package_item_list != nullptr) {
      package_item_list->Accept(this);
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(package_node));
}

void IndexingFactsTreeExtractor::ExtractMacroDefinition(
    const SyntaxTreeNode &preprocessor_definition) {
  const SyntaxTreeLeaf *macro_name = GetMacroName(preprocessor_definition);
  if (!macro_name) return;
  IndexingFactNode macro_node(IndexingNodeData(
      IndexingFactType::kMacro, Anchor(macro_name->get(), FileContent())));

  // TODO(fangism): access directly, instead of searching.
  const std::vector<TreeSearchMatch> args =
      FindAllMacroDefinitionsArgs(preprocessor_definition);

  for (const TreeSearchMatch &arg : args) {
    const SyntaxTreeLeaf *macro_arg_name = GetMacroArgName(*arg.match);
    if (macro_arg_name) {
      macro_node.Children().emplace_back(
          IndexingNodeData(IndexingFactType::kVariableDefinition,
                           Anchor(macro_arg_name->get(), FileContent())));
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(macro_node));
}

static Anchor GetMacroAnchorFromTokenInfo(const TokenInfo &macro_token_info,
                                          std::string_view file_content) {
  // Strip the prefix "`".
  // e.g.
  // `define TEN 0
  // `TEN --> removes the `
  const std::string_view macro_name =
      absl::StripPrefix(macro_token_info.text(), "`");
  int begin = std::distance(file_content.begin(), macro_name.begin());
  return Anchor(macro_name, begin, macro_name.size());
}

void IndexingFactsTreeExtractor::ExtractMacroCall(
    const SyntaxTreeNode &macro_call) {
  const TokenInfo *macro_call_name_token = GetMacroCallId(macro_call);
  if (!macro_call_name_token) return;
  IndexingFactNode macro_node(IndexingNodeData(
      IndexingFactType::kMacroCall,
      GetMacroAnchorFromTokenInfo(*macro_call_name_token, FileContent())));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &macro_node);

    const SyntaxTreeNode *macro_call_args = GetMacroCallArgs(macro_call);
    if (macro_call_args) Visit(*macro_call_args);
  }

  facts_tree_context_.top().Children().push_back(std::move(macro_node));
}

void IndexingFactsTreeExtractor::ExtractMacroReference(
    const SyntaxTreeLeaf &macro_identifier) {
  facts_tree_context_.top().Children().emplace_back(IndexingNodeData(
      IndexingFactType::kMacroCall,
      GetMacroAnchorFromTokenInfo(macro_identifier.get(), FileContent())));
}

void IndexingFactsTreeExtractor::ExtractClassConstructor(
    const SyntaxTreeNode &class_constructor) {
  const SyntaxTreeLeaf *new_keyword =
      GetNewKeywordFromClassConstructor(class_constructor);
  if (!new_keyword) return;
  IndexingFactNode constructor_node(
      IndexingNodeData(IndexingFactType::kConstructor,
                       Anchor(new_keyword->get(), FileContent())));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &constructor_node);

    // Extract ports.
    ExtractFunctionOrTaskOrConstructorPort(class_constructor);

    // Extract constructor body.
    const SyntaxTreeNode *constructor_body =
        GetClassConstructorStatementList(class_constructor);
    if (constructor_body) Visit(*constructor_body);
  }

  facts_tree_context_.top().Children().push_back(std::move(constructor_node));
}

void IndexingFactsTreeExtractor::ExtractPureVirtualFunction(
    const SyntaxTreeNode &function_prototype) {
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTaskForwardDeclaration});

  // Extract function header.
  const SyntaxTreeNode *function_header =
      GetFunctionPrototypeHeader(function_prototype);
  if (function_header) ExtractFunctionHeader(*function_header, &function_node);

  facts_tree_context_.top().Children().push_back(std::move(function_node));
}

void IndexingFactsTreeExtractor::ExtractPureVirtualTask(
    const SyntaxTreeNode &task_prototype) {
  IndexingFactNode task_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTaskForwardDeclaration});

  // Extract task header.
  const SyntaxTreeNode *task_header = GetTaskPrototypeHeader(task_prototype);
  if (task_header) ExtractTaskHeader(*task_header, &task_node);

  facts_tree_context_.top().Children().push_back(std::move(task_node));
}

void IndexingFactsTreeExtractor::ExtractFunctionDeclaration(
    const SyntaxTreeNode &function_declaration_node) {
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTask});

  // Extract function header.
  const SyntaxTreeNode *function_header =
      GetFunctionHeader(function_declaration_node);
  if (function_header) ExtractFunctionHeader(*function_header, &function_node);

  {
    // Extract function body.
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);
    const SyntaxTreeNode *function_body =
        GetFunctionBlockStatementList(function_declaration_node);
    if (function_body) Visit(*function_body);
  }

  facts_tree_context_.top().Children().push_back(std::move(function_node));
}

void IndexingFactsTreeExtractor::ExtractTaskDeclaration(
    const SyntaxTreeNode &task_declaration_node) {
  IndexingFactNode task_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTask});

  // Extract task header.
  const SyntaxTreeNode *task_header = GetTaskHeader(task_declaration_node);
  if (task_header) ExtractTaskHeader(*task_header, &task_node);

  {
    // Extract task body.
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &task_node);
    const SyntaxTreeNode *task_body =
        GetTaskStatementList(task_declaration_node);
    if (task_body) Visit(*task_body);
  }

  facts_tree_context_.top().Children().push_back(std::move(task_node));
}

void IndexingFactsTreeExtractor::ExtractFunctionHeader(
    const SyntaxTreeNode &function_header, IndexingFactNode *function_node) {
  // Extract function name.
  const Symbol *function_name = GetFunctionHeaderId(function_header);
  if (function_name == nullptr) {
    return;
  }
  function_name->Accept(this);
  MoveAndDeleteLastExtractedNode(function_node);

  {
    IndexingFactNode &function_node_ref = *function_node;
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node_ref);
    // Extract function ports.
    ExtractFunctionOrTaskOrConstructorPort(function_header);
  }
}

void IndexingFactsTreeExtractor::ExtractTaskHeader(
    const SyntaxTreeNode &task_header, IndexingFactNode *task_node) {
  // Extract task name.
  const Symbol *task_name = GetTaskHeaderId(task_header);
  if (task_name == nullptr) {
    return;
  }
  task_name->Accept(this);
  MoveAndDeleteLastExtractedNode(task_node);

  {
    IndexingFactNode &task_node_ref = *task_node;
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &task_node_ref);
    // Extract task ports.
    ExtractFunctionOrTaskOrConstructorPort(task_header);
  }
}

void IndexingFactsTreeExtractor::ExtractFunctionOrTaskOrConstructorPort(
    const SyntaxTreeNode &function_declaration_node) {
  const std::vector<TreeSearchMatch> ports =
      FindAllTaskFunctionPortDeclarations(function_declaration_node);

  for (const TreeSearchMatch &port : ports) {
    const Symbol *port_type = GetTypeOfTaskFunctionPortItem(*port.match);
    if (port_type != nullptr) {
      // port variable name.
      const SyntaxTreeLeaf *port_identifier =
          GetIdentifierFromTaskFunctionPortItem(*port.match);
      if (!port_identifier) continue;

      // variable identifier node.
      IndexingFactNode variable_node(
          IndexingNodeData(IndexingFactType::kVariableDefinition,
                           Anchor(port_identifier->get(), FileContent())));

      // if this port has struct/union/enum data type.
      const SyntaxTreeNode *struct_type =
          GetStructOrUnionOrEnumTypeFromDataType(*port_type);

      if (struct_type != nullptr) {
        // Then this data type is struct/union/enum
        {
          const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                    &variable_node);
          struct_type->Accept(this);
        }

        facts_tree_context_.top().Children().push_back(
            std::move(variable_node));
        continue;
      }

      const SyntaxTreeNode *type_identifier =
          GetTypeIdentifierFromDataType(*port_type);

      if (type_identifier == nullptr) {
        // Then this is a primitive data type.
        // e.g "task f1(int x);"

        {
          const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                    &variable_node);

          const SyntaxTreeNode *packed_dim =
              GetPackedDimensionFromDataType(*port_type);
          if (packed_dim != nullptr) {
            packed_dim->Accept(this);
          }

          const SyntaxTreeNode *unpacked_dimension =
              GetUnpackedDimensionsFromTaskFunctionPortItem(*port.match);
          if (unpacked_dimension) unpacked_dimension->Accept(this);
        }

        facts_tree_context_.top().Children().push_back(
            std::move(variable_node));
        continue;
      }
      // else this is a user defined type.
      // e.g "task f1(some_class var1);".

      IndexingFactNode type_node(
          IndexingNodeData{IndexingFactType::kDataTypeReference});
      type_identifier->Accept(this);
      MoveAndDeleteLastExtractedNode(&type_node);

      type_node.Children().emplace_back(
          IndexingNodeData(IndexingFactType::kVariableDefinition,
                           Anchor(port_identifier->get(), FileContent())));

      {
        const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                  &type_node);

        const SyntaxTreeNode *packed_dim =
            GetPackedDimensionFromDataType(*port_type);
        if (packed_dim != nullptr) {
          packed_dim->Accept(this);
        }

        const SyntaxTreeNode *unpacked_dimension =
            GetUnpackedDimensionsFromTaskFunctionPortItem(*port.match);
        if (unpacked_dimension) unpacked_dimension->Accept(this);
      }

      facts_tree_context_.top().Children().push_back(std::move(type_node));
    }
  }
}

void IndexingFactsTreeExtractor::ExtractFunctionOrTaskCall(
    const SyntaxTreeNode &function_call_node) {
  // check if this node contains an actual call
  if (!function_call_node.empty() &&
      SymbolCastToNode(*function_call_node[0])
          .MatchesTagAnyOf({NodeEnum::kReference, NodeEnum::kMacroCall})) {
    TreeContextVisitor::Visit(function_call_node);
    return;
  }
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionCall});

  // Extract function or task name.
  // It can be single or preceeded with a pkg or class names.
  const SyntaxTreeNode *identifier =
      GetIdentifiersFromFunctionCall(function_call_node);
  if (identifier == nullptr) {
    return;
  }
  Visit(*identifier);

  {
    // These will resolve since `GetIdentifiersFromFunctionCall` would return
    // nullptr if they were not present and that triggers a return above.
    const verible::Symbol *reference_call_base = nullptr;
    reference_call_base =
        GetSubtreeAsSymbol(function_call_node, NodeEnum::kFunctionCall, 0);
    const verible::Symbol *reference = GetSubtreeAsSymbol(
        *reference_call_base, NodeEnum::kReferenceCallBase, 0);
    if (reference->Tag().tag == (int)NodeEnum::kReference) {
      if (SymbolCastToNode(*reference).size() > 1) {
        bool is_first = true;
        for (const auto &child : SymbolCastToNode(*reference).children()) {
          if (is_first) {  // skip the first one
            is_first = false;
            continue;
          }
          if (child->Tag().tag == (int)NodeEnum::kHierarchyExtension) {
            Visit(verible::SymbolCastToNode(*child));
          }
        }
      }
    }
  }
  // Move the data from the last extracted node to the current node and delete
  // that last node.
  MoveAndDeleteLastExtractedNode(&function_node);

  // Terminate if no function name is found.
  // in case of built-in functions: "sin(x)";
  if (function_node.Value().Anchors().empty()) {
    return;
  }

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);
    const SyntaxTreeNode *arguments = GetParenGroupFromCall(function_call_node);
    // Extract function or task parameters.
    if (arguments) Visit(*arguments);
  }

  facts_tree_context_.top().Children().push_back(std::move(function_node));
}

void IndexingFactsTreeExtractor::ExtractMethodCallExtension(
    const SyntaxTreeNode &call_extension_node) {
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionCall});

  // Move the data from the last extracted node to the current node and delete
  // that last node.
  MoveAndDeleteLastExtractedNode(&function_node);

  // Terminate if no function name is found.
  // in case of built-in functions: "q.sort()";
  if (function_node.Value().Anchors().empty()) {
    return;
  }

  {
    const SyntaxTreeLeaf *fun_call =
        GetFunctionCallNameFromCallExtension(call_extension_node);
    if (fun_call) {
      function_node.Value().AppendAnchor(
          Anchor(fun_call->get(), FileContent()));
    }
  }

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);
    const SyntaxTreeNode *arguments =
        GetParenGroupFromCallExtension(call_extension_node);
    // parameters.
    if (arguments) Visit(*arguments);
  }

  facts_tree_context_.top().Children().push_back(std::move(function_node));
}

void IndexingFactsTreeExtractor::ExtractMemberExtension(
    const SyntaxTreeNode &hierarchy_extension_node) {
  IndexingFactNode member_node(
      IndexingNodeData{IndexingFactType::kMemberReference});

  // Move the data from the last extracted node to the current node and delete
  // that last node.
  MoveAndDeleteLastExtractedNode(&member_node);

  {
    const verible::SyntaxTreeLeaf *unqualified =
        GetUnqualifiedIdFromHierarchyExtension(hierarchy_extension_node);
    // member name
    if (unqualified) {
      member_node.Value().AppendAnchor(
          Anchor(unqualified->get(), FileContent()));
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(member_node));
}

void IndexingFactsTreeExtractor::ExtractClassDeclaration(
    const SyntaxTreeNode &class_declaration) {
  IndexingFactNode class_node(IndexingNodeData{IndexingFactType::kClass});

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &class_node);
    // Extract class name.
    const SyntaxTreeLeaf *class_name = GetClassName(class_declaration);
    if (class_name) {
      facts_tree_context_.top().Value().AppendAnchor(
          Anchor(class_name->get(), FileContent()));
    }

    // Extract class name after endclass.
    const SyntaxTreeLeaf *class_end_name = GetClassEndLabel(class_declaration);

    if (class_end_name != nullptr) {
      facts_tree_context_.top().Value().AppendAnchor(
          Anchor(class_end_name->get(), FileContent()));
    }

    const SyntaxTreeNode *param_list =
        GetParamDeclarationListFromClassDeclaration(class_declaration);
    if (param_list != nullptr) {
      Visit(*param_list);
    }

    const SyntaxTreeNode *extended_class = GetExtendedClass(class_declaration);
    if (extended_class != nullptr) {
      IndexingFactNode extends_node(
          IndexingNodeData{IndexingFactType::kExtends});

      // In case of => class X extends Y.
      if (NodeEnum(extended_class->Tag().tag) == NodeEnum::kUnqualifiedId) {
        extends_node.Value().AppendAnchor(Anchor(
            AutoUnwrapIdentifier(*extended_class)->get(), FileContent()));
      } else {
        // In case of => class X extends pkg1::Y.
        ExtractQualifiedId(*extended_class);
        // Construct extends node from the last node which is kMemberReference,
        // remove kMemberReference node and append the new extends node.
        MoveAndDeleteLastExtractedNode(&extends_node);
      }

      // Add the extends node as a child of this class node.
      class_node.Children().push_back(std::move(extends_node));
    }

    // Visit class body.
    const SyntaxTreeNode *class_item_list = GetClassItemList(class_declaration);
    if (class_item_list) Visit(*class_item_list);
  }

  facts_tree_context_.top().Children().push_back(std::move(class_node));
}

void IndexingFactsTreeExtractor::ExtractClassInstances(
    const SyntaxTreeNode &data_declaration_node,
    const std::vector<TreeSearchMatch> &class_instances) {
  IndexingFactNode type_node(
      IndexingNodeData{IndexingFactType::kDataTypeReference});

  const Symbol *type =
      GetTypeIdentifierFromDataDeclaration(data_declaration_node);
  if (type == nullptr) {
    return;
  }

  // Extract class type and parameters.
  type->Accept(this);
  MoveAndDeleteLastExtractedNode(&type_node);

  // Class instances may may appear as multiple instances sharing the same
  // type in a single statement e.g. myClass b1 = new, b2 = new. LRM 8.8 Typed
  // constructor calls
  //
  // Loop through each instance and associate each declared id with the same
  // type and create its corresponding facts tree node.
  for (const TreeSearchMatch &instance : class_instances) {
    IndexingFactNode class_instance_node(
        IndexingNodeData{IndexingFactType::kClassInstance});

    // Re-use the kRegisterVariable and kVariableDeclarationAssignment tag
    // resolver.
    instance.match->Accept(this);
    MoveAndDeleteLastExtractedNode(&class_instance_node);

    type_node.Children().push_back(std::move(class_instance_node));
  }

  facts_tree_context_.top().Children().push_back(std::move(type_node));
}

void IndexingFactsTreeExtractor::ExtractRegisterVariable(
    const SyntaxTreeNode &register_variable) {
  const TokenInfo *instance_name =
      GetInstanceNameTokenInfoFromRegisterVariable(register_variable);
  if (!instance_name) return;

  IndexingFactNode variable_node(
      IndexingNodeData(IndexingFactType::kVariableDefinition,
                       Anchor(*instance_name, FileContent())));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &variable_node);
    const SyntaxTreeNode *unpacked_dimension =
        GetUnpackedDimensionFromRegisterVariable(register_variable);
    if (unpacked_dimension) Visit(*unpacked_dimension);

    const SyntaxTreeNode *expression =
        GetTrailingExpressionFromRegisterVariable(register_variable);
    if (expression != nullptr) {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &variable_node);
      // Visit Trailing Assignment Expression.
      Visit(*expression);
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(variable_node));
}

void IndexingFactsTreeExtractor::ExtractVariableDeclarationAssignment(
    const SyntaxTreeNode &variable_declaration_assignment) {
  const SyntaxTreeLeaf *unqualified_id =
      GetUnqualifiedIdFromVariableDeclarationAssignment(
          variable_declaration_assignment);
  if (!unqualified_id) return;
  IndexingFactNode variable_node(
      IndexingNodeData(IndexingFactType::kVariableDefinition,
                       Anchor(unqualified_id->get(), FileContent())));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &variable_node);
    const SyntaxTreeNode *unpacked_dimension =
        GetUnpackedDimensionFromVariableDeclarationAssign(
            variable_declaration_assignment);
    if (unpacked_dimension) Visit(*unpacked_dimension);

    const SyntaxTreeNode *expression =
        GetTrailingExpressionFromVariableDeclarationAssign(
            variable_declaration_assignment);
    if (expression != nullptr) {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &variable_node);
      // Visit Trailing Assignment Expression.
      Visit(*expression);
    }
  }
  facts_tree_context_.top().Children().push_back(std::move(variable_node));
}

void IndexingFactsTreeExtractor::ExtractUnqualifiedId(
    const SyntaxTreeNode &unqualified_id) {
  const SyntaxTreeLeaf *identifier = AutoUnwrapIdentifier(unqualified_id);
  if (identifier == nullptr) {
    return;
  }

  switch (identifier->get().token_enum()) {
    case verilog_tokentype::MacroIdentifier: {
      ExtractMacroReference(*identifier);
      break;
    }
    case verilog_tokentype::SymbolIdentifier: {
      IndexingFactNode variable_reference(
          IndexingNodeData{IndexingFactType::kVariableReference});
      ExtractSymbolIdentifier(*identifier);
      MoveAndDeleteLastExtractedNode(&variable_reference);

      const SyntaxTreeNode *param_list =
          GetParamListFromUnqualifiedId(unqualified_id);
      if (param_list != nullptr) {
        const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                  &variable_reference);
        param_list->Accept(this);
      }

      facts_tree_context_.top().Children().push_back(
          std::move(variable_reference));
      break;
    }
    default: {
      break;
    }
  }
}

void IndexingFactsTreeExtractor::ExtractParamDeclaration(
    const SyntaxTreeNode &param_declaration) {
  IndexingFactNode param_node(
      IndexingNodeData{IndexingFactType::kParamDeclaration});

  const SyntaxTreeNode *type_assignment =
      GetTypeAssignmentFromParamDeclaration(param_declaration);

  // Parameters can be in two cases:
  // 1st => parameter type x;
  if (type_assignment != nullptr) {
    param_node.Value().AppendAnchor(Anchor(
        ABSL_DIE_IF_NULL(GetIdentifierLeafFromTypeAssignment(*type_assignment))
            ->get(),
        FileContent()));

    const SyntaxTreeNode *expression =
        GetExpressionFromTypeAssignment(*type_assignment);
    if (expression != nullptr) {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &param_node);
      Visit(*expression);
    }
  } else {
    // 2nd => parameter int x;
    // Extract Param name.
    const TokenInfo *parameter_name = GetParameterNameToken(param_declaration);
    if (!parameter_name) return;
    param_node.Value().AppendAnchor(Anchor(*parameter_name, FileContent()));

    {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &param_node);

      const Symbol *assign_expression =
          GetParamAssignExpression(param_declaration);

      if (assign_expression != nullptr &&
          assign_expression->Kind() == SymbolKind::kNode) {
        // Extract trailing expression.
        assign_expression->Accept(this);
      }
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(param_node));
}

void IndexingFactsTreeExtractor::ExtractParamByName(
    const SyntaxTreeNode &param_by_name) {
  const verible::SyntaxTreeLeaf *named_param =
      GetNamedParamFromActualParam(param_by_name);
  if (!named_param) return;
  IndexingFactNode named_param_node(
      IndexingNodeData(IndexingFactType::kNamedParam,
                       Anchor(named_param->get(), FileContent())));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &named_param_node);
    const SyntaxTreeNode *paren_group =
        GetParenGroupFromActualParam(param_by_name);
    if (paren_group != nullptr) {
      Visit(*paren_group);
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(named_param_node));
}

void IndexingFactsTreeExtractor::ExtractPackageImport(
    const SyntaxTreeNode &package_import_item) {
  const SyntaxTreeLeaf *package_name =
      GetImportedPackageName(package_import_item);
  if (!package_name) return;
  IndexingNodeData package_import_data(
      IndexingFactType::kPackageImport,
      Anchor(package_name->get(), FileContent()));

  // Get the name of the imported item (if exists).
  // e.g pkg::var1 ==> return var1.
  // will be nullptr in case of pkg::*.
  const SyntaxTreeLeaf *imported_item =
      GeImportedItemNameFromPackageImportItem(package_import_item);
  if (imported_item != nullptr) {
    package_import_data.AppendAnchor(
        Anchor(imported_item->get(), FileContent()));
  }

  facts_tree_context_.top().Children().emplace_back(
      std::move(package_import_data));
}

// This deep-copy works even if the IndexingNodeData's copy-constructor is
// deleted.
// Defining this here because the following function is the only place in the
// codebase that needs this workaround.
static IndexingNodeData CopyNodeData(const IndexingNodeData &src) {
  IndexingNodeData copy(src.GetIndexingFactType());
  for (const auto &anchor : src.Anchors()) {
    copy.AppendAnchor(Anchor(anchor));  // copy
  }
  return copy;
}

void IndexingFactsTreeExtractor::ExtractQualifiedId(
    const SyntaxTreeNode &qualified_id) {
  IndexingNodeData member_reference_data(IndexingFactType::kMemberReference);

  // Get all the variable names in the qualified id.
  // e.g. split "A#(...)::B#(...)" into components "A#(...)" and "B#(...)"
  for (const auto &child : qualified_id.children()) {
    if (child == nullptr ||
        NodeEnum(child->Tag().tag) != NodeEnum::kUnqualifiedId) {
      continue;
    }
    member_reference_data.AppendAnchor(
        Anchor(AutoUnwrapIdentifier(*child)->get(), FileContent()));

    const SyntaxTreeNode *param_list = GetParamListFromUnqualifiedId(*child);
    if (param_list != nullptr) {
      // Create a copy from the current "member_reference" node to be used for
      // this param reference.
      // Copying inside this for loop costs O(N^2), where N is the
      // depth of a reference (on "A::B::C::D", N=4).
      // Downstream, the lookup for "A" is being done repeatedly.
      // TODO(fangism): rewrite this and its consumer to eliminate the linear
      // copy in a loop and avoid re-lookup.
      IndexingFactNode param_member_reference(
          CopyNodeData(member_reference_data));
      {
        const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                  &param_member_reference);
        Visit(*param_list);
      }

      facts_tree_context_.top().Children().push_back(
          std::move(param_member_reference));
    }
  }

  facts_tree_context_.top().Children().emplace_back(
      std::move(member_reference_data));
}

void IndexingFactsTreeExtractor::ExtractForInitialization(
    const SyntaxTreeNode &for_initialization) {
  // Extracts the variable name from for initialization.
  // e.g from "int i = 0"; ==> extracts "i".
  const SyntaxTreeLeaf *variable_name =
      GetVariableNameFromForInitialization(for_initialization);
  if (variable_name) {
    facts_tree_context_.top().Children().emplace_back(
        IndexingNodeData(IndexingFactType::kVariableDefinition,
                         Anchor(variable_name->get(), FileContent())));
  }

  // Extracts the data the in case it contains packed or unpacked dimension.
  // e.g bit [x : y] var [x : y].
  const SyntaxTreeNode *data_type_node =
      GetDataTypeFromForInitialization(for_initialization);
  if (data_type_node != nullptr) {
    Visit(*data_type_node);
  }

  // Extracts the RHS of the declaration.
  // e.g int i = x; ==> extracts "x".
  const SyntaxTreeNode *expression =
      GetExpressionFromForInitialization(for_initialization);
  if (expression) Visit(*expression);
}

// Returns string_view of `text` with outermost double-quotes removed.
// If `text` is not wrapped in quotes, return it as-is.
static std::string_view StripOuterQuotes(std::string_view text) {
  return absl::StripSuffix(absl::StripPrefix(text, "\""), "\"");
}

void IndexingFactsTreeExtractor::ExtractInclude(
    const SyntaxTreeNode &preprocessor_include) {
  VLOG(1) << __FUNCTION__;
  const SyntaxTreeLeaf *included_filename =
      GetFileFromPreprocessorInclude(preprocessor_include);
  if (included_filename == nullptr) {
    return;
  }

  const std::string_view filename_text = included_filename->get().text();

  // Remove the double quotes from the filesname.
  const std::string_view filename_unquoted = StripOuterQuotes(filename_text);
  VLOG(1) << "got: `include \"" << filename_unquoted << "\"";

  VerilogProject *const project = extraction_state_->project;

  // Open this file (could be first time, or previously opened).
  const auto status_or_file = project->OpenIncludedFile(filename_unquoted);
  if (!status_or_file.ok()) {
    if (errors_ != nullptr) {
      errors_->push_back(status_or_file.status());
    } else {
      LOG(ERROR) << "Failed to open the include file " << filename_unquoted
                 << ": " << status_or_file.status();
    }
    // Skip
    return;
  }

  VerilogSourceFile *const included_file = *status_or_file;
  if (included_file == nullptr) return;
  VLOG(1) << "opened include file: " << included_file->ResolvedPath();

  {
    // Check whether or not this file was already extracted.
    const auto p = extraction_state_->extracted_files.insert(included_file);
    if (!p.second) {
      // If already extracted, skip re-extraction.
      VLOG(1) << "File was previously extracted.";
    } else {
      // Parse included file and extract.
      const auto parse_status = included_file->Parse();
      if (parse_status.ok()) {
        file_list_facts_tree_->Children().push_back(BuildIndexingFactsTree(
            file_list_facts_tree_, *included_file, extraction_state_, errors_));
      } else {
        if (errors_ != nullptr) {
          errors_->push_back(parse_status);
        } else {
          LOG(WARNING) << "Failed to parse the include file "
                       << filename_unquoted << ": " << parse_status;
        }
      }
    }
  }

  // Create a node for include statement with two Anchors:
  // 1st one holds the actual text in the include statement.
  // 2nd one holds the path of the included file relative to the file list.
  facts_tree_context_.top().Children().emplace_back(IndexingNodeData(
      IndexingFactType::kInclude,
      Anchor(filename_text, included_filename->get().left(FileContent()),
             filename_text.size()),
      Anchor(included_file->ResolvedPath())));
}

void IndexingFactsTreeExtractor::ExtractEnumName(
    const SyntaxTreeNode &enum_name) {
  const SyntaxTreeLeaf *symbol_id = GetSymbolIdentifierFromEnumName(enum_name);
  if (!symbol_id) return;
  IndexingFactNode enum_node(IndexingNodeData(
      IndexingFactType::kConstant, Anchor(symbol_id->get(), FileContent())));

  // Iterate over the children and traverse them to extract facts from inner
  // nodes and ignore the leaves.
  // e.g enum {RED[x] = 1, OLD=y} => explores "[x]", "=y".
  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &enum_node);
    for (const auto &child : enum_name.children()) {
      if (child == nullptr || child->Kind() == SymbolKind::kLeaf) {
        continue;
      }
      child->Accept(this);
    }
  }

  facts_tree_context_.top().Children().push_back(std::move(enum_node));
}

void IndexingFactsTreeExtractor::ExtractEnumTypeDeclaration(
    const SyntaxTreeNode &enum_type_declaration) {
  // Extract enum type name.
  const SyntaxTreeLeaf *enum_type_name =
      GetIdentifierFromTypeDeclaration(enum_type_declaration);
  if (!enum_type_name) return;
  facts_tree_context_.top().Children().emplace_back(
      IndexingNodeData(IndexingFactType::kVariableDefinition,
                       Anchor(enum_type_name->get(), FileContent())));

  // Explore the children of this enum type to extract.
  for (const auto &child : enum_type_declaration.children()) {
    if (child == nullptr || child->Kind() == SymbolKind::kLeaf) {
      continue;
    }
    child->Accept(this);
  }
}

void IndexingFactsTreeExtractor::ExtractStructUnionTypeDeclaration(
    const SyntaxTreeNode &type_declaration, const SyntaxTreeNode &struct_type) {
  IndexingFactNode struct_type_node(IndexingNodeData(
      IndexingFactType::kStructOrUnion,
      Anchor(
          ABSL_DIE_IF_NULL(GetIdentifierFromTypeDeclaration(type_declaration))
              ->get(),
          FileContent())));

  // Explore the children of this enum type to extract.
  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &struct_type_node);
    Visit(struct_type);
  }

  facts_tree_context_.top().Children().push_back(std::move(struct_type_node));
}

void IndexingFactsTreeExtractor::ExtractStructUnionDeclaration(
    const SyntaxTreeNode &struct_type,
    const std::vector<TreeSearchMatch> &variables_matched) {
  VLOG(2) << __FUNCTION__;
  // Dummy data type to hold the extracted struct members because there is no
  // data type here.  Its temporary children will be moved out before this
  // returns.
  IndexingFactNode struct_node(
      IndexingNodeData{IndexingFactType::kStructOrUnion});

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &struct_node);
    // Extract struct members.
    Visit(struct_type);
  }

  for (const TreeSearchMatch &variable : variables_matched) {
    // Extract this variable.
    // This can be kRegisterVariable or kVariableDeclarationAssign.
    variable.match->Accept(this);

    CHECK(!facts_tree_context_.top().Children().empty());
    IndexingFactNode &recent(facts_tree_context_.top().Children().back());
    // Append the struct members to be a children of this variable.

    // TODO(fangism): move instead of copying chidren
    // However, std::move-ing each child in the loop crashes,
    // and so does recent.AdoptSubtreesFrom(&struct_node).
    recent.Children().reserve(struct_node.Children().size());
    for (const auto &child : struct_node.Children()) {
      recent.Children().push_back(child);  // copy
    }
  }
  VLOG(2) << "end of " << __FUNCTION__;
}

void IndexingFactsTreeExtractor::ExtractDataTypeImplicitIdDimensions(
    const SyntaxTreeNode &data_type_implicit_id_dimensions) {
  // This node has 2 cases:
  // 1st case:
  // typedef struct {
  //    data_type var_name;
  // } my_struct;
  // In this case this should be a kDataTypeReference with var_name as a
  // child.
  //
  // 2nd case:
  // typedef struct {
  //    struct {int xx;} var_name;
  // } my_struct;
  // In this case var_name should contain "xx" inside it.

  std::pair<const SyntaxTreeLeaf *, int> variable_name =
      GetSymbolIdentifierFromDataTypeImplicitIdDimensions(
          data_type_implicit_id_dimensions);
  if (!variable_name.first) return;

  IndexingFactNode variable_node(
      IndexingNodeData(IndexingFactType::kVariableDefinition,
                       Anchor(variable_name.first->get(), FileContent())));

  if (variable_name.second == 1) {
    const SyntaxTreeLeaf *type_identifier =
        GetNonprimitiveTypeOfDataTypeImplicitDimensions(
            data_type_implicit_id_dimensions);

    if (type_identifier == nullptr) return;

    IndexingFactNode type_node(
        IndexingNodeData(IndexingFactType::kDataTypeReference,
                         Anchor(type_identifier->get(), FileContent())));

    type_node.Children().push_back(std::move(variable_node));
    facts_tree_context_.top().Children().push_back(std::move(type_node));
  } else if (variable_name.second == 2) {
    {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &variable_node);
      for (const auto &child : data_type_implicit_id_dimensions.children()) {
        if (child == nullptr || child->Kind() == SymbolKind::kLeaf) {
          continue;
        }
        child->Accept(this);
      }
    }

    facts_tree_context_.top().Children().push_back(std::move(variable_node));
  }
}

void IndexingFactsTreeExtractor::ExtractTypeDeclaration(
    const SyntaxTreeNode &type_declaration) {
  const SyntaxTreeNode *type =
      GetReferencedTypeOfTypeDeclaration(type_declaration);
  if (type == nullptr) return;

  // Look for enum/struct/union in the referenced type.
  const auto tag = static_cast<NodeEnum>(type->Tag().tag);
  if (tag != NodeEnum::kDataType) return;
  const SyntaxTreeNode *primitive =
      GetStructOrUnionOrEnumTypeFromDataType(*type);
  if (primitive == nullptr) {
    // Then this is a user-defined type.
    // Extract type name.
    const SyntaxTreeLeaf *type_name =
        GetIdentifierFromTypeDeclaration(type_declaration);
    if (type_name == nullptr) return;

    facts_tree_context_.top().Children().emplace_back(
        IndexingNodeData(IndexingFactType::kTypeDeclaration,
                         Anchor(type_name->get(), FileContent())));
    return;
  }

  switch (NodeEnum(primitive->Tag().tag)) {
    case NodeEnum::kEnumType: {
      ExtractEnumTypeDeclaration(type_declaration);
      break;
    }
    case NodeEnum::kStructType: {
      ExtractStructUnionTypeDeclaration(type_declaration, *type);
      break;
    }
    case NodeEnum::kUnionType: {
      ExtractStructUnionTypeDeclaration(type_declaration, *type);
      break;
    }
    default: {
      break;
    }
  }
}

void IndexingFactsTreeExtractor::ExtractAnonymousScope(
    const SyntaxTreeNode &node) {
  IndexingFactNode temp_scope_node(IndexingNodeData(
      IndexingFactType::kAnonymousScope,
      // Generate unique id for this scope.
      Anchor(absl::StrCat("anonymous-scope-", next_anonymous_id++))));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &temp_scope_node);
    TreeContextVisitor::Visit(node);
  }

  facts_tree_context_.top().Children().push_back(std::move(temp_scope_node));
}

void IndexingFactsTreeExtractor::MoveAndDeleteLastExtractedNode(
    IndexingFactNode *new_node) {
  // Terminate if there is no parent or the parent has no children.
  if (facts_tree_context_.empty() || is_leaf(facts_tree_context_.top())) {
    return;
  }

  // Get The last extracted child.
  IndexingFactNode &previous_node = facts_tree_context_.top().Children().back();

  // Fill the anchors of the previous node to the current node.
  new_node->Value().SwapAnchors(&previous_node.Value());

  // Move the children of the previous node to this node.
  AdoptSubtreesFrom(*new_node, &previous_node);

  // Remove the last extracted node.
  facts_tree_context_.top().Children().pop_back();
}

}  // namespace kythe
}  // namespace verilog
