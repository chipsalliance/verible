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

#include "absl/strings/strip.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/tree_context_visitor.h"
#include "common/text/tree_utils.h"
#include "common/util/file_util.h"
#include "verilog/CST/class.h"
#include "verilog/CST/declaration.h"
#include "verilog/CST/functions.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/macro.h"
#include "verilog/CST/module.h"
#include "verilog/CST/net.h"
#include "verilog/CST/package.h"
#include "verilog/CST/parameters.h"
#include "verilog/CST/port.h"
#include "verilog/CST/statement.h"
#include "verilog/CST/tasks.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/CST/verilog_tree_print.h"
#include "verilog/analysis/verilog_analyzer.h"

namespace verilog {
namespace kythe {

namespace {

using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TreeSearchMatch;

// Given a root to CST this function traverses the tree and extracts and
// constructs the indexing facts tree.
IndexingFactNode BuildIndexingFactsTree(
    const verible::ConcreteSyntaxTree& syntax_tree, absl::string_view base,
    absl::string_view file_name, IndexingFactNode& file_list_facts_tree,
    std::map<std::string, std::string>& extracted_files,
    const std::vector<std::string>& include_dir_paths) {
  IndexingFactsTreeExtractor visitor(base, file_name, file_list_facts_tree,
                                     extracted_files, include_dir_paths);
  if (syntax_tree == nullptr) {
    return visitor.GetRoot();
  }

  const SyntaxTreeNode& root = verible::SymbolCastToNode(*syntax_tree);
  root.Accept(&visitor);
  return visitor.GetRoot();
}

// Given a verilog file returns the extracted indexing facts tree.
IndexingFactNode ExtractOneFile(
    absl::string_view content, absl::string_view filename,
    IndexingFactNode& file_list_facts_tree,
    std::map<std::string, std::string>& extracted_files,
    const std::vector<std::string>& include_dir_paths) {
  verilog::VerilogAnalyzer analyzer(content, filename);
  // Do not parse using AnalyzeAutomaticMode() because index extraction is only
  // expected to work on self-contained files with full syntactic context.
  const auto status = analyzer.Analyze();
  if (!status.ok()) {
    const std::vector<std::string> syntax_error_messages(
        analyzer.LinterTokenErrorMessages());
    for (const auto& message : syntax_error_messages) {
      // logging instead of outputing to std stream because it's used by the
      // extractor.
      LOG(INFO) << message << std::endl;
    }
  }

  const auto& text_structure = analyzer.Data();
  const auto& syntax_tree = text_structure.SyntaxTree();

  return BuildIndexingFactsTree(syntax_tree, analyzer.Data().Contents(),
                                filename, file_list_facts_tree, extracted_files,
                                include_dir_paths);
}

// Tries to read the files in all the given directories.
// Returns the first file it find in case of many files with the same name.
absl::Status SearchForFileAndGetContents(
    std::string& file_path, std::string& content, absl::string_view filename,
    const std::vector<std::string>& directories) {
  for (const auto& dir_path : directories) {
    file_path = verible::file::JoinPath(dir_path, filename);
    if (verible::file::FileExists(file_path).ok()) {
      return verible::file::GetContents(file_path, &content);
    }
  }
  return absl::NotFoundError(absl::StrCat("Couldn't find file: ", filename));
}

}  // namespace

IndexingFactNode ExtractFiles(const std::vector<std::string>& ordered_file_list,
                              absl::string_view file_list_dir,
                              absl::string_view file_list_root,
                              const std::vector<std::string>& include_dir_paths,
                              std::vector<absl::Status>& errors) {
  // Create a node to hold the dirname of the ordered file list and group all
  // the files and acts as a ordered file list of these files.
  IndexingFactNode file_list_facts_tree(IndexingNodeData(
      {Anchor(file_list_dir, 0, 0), Anchor(file_list_root, 0, 0)},
      IndexingFactType::kFileList));
  std::map<std::string, std::string> extracted_files;

  for (const auto& filename : ordered_file_list) {
    // Check if this included file was extracted before.
    if (extracted_files.find(filename) != extracted_files.end()) {
      continue;
    }

    std::string file_path = verible::file::JoinPath(file_list_root, filename);
    std::string content;

    const auto status = verible::file::GetContents(file_path, &content);
    if (!status.ok()) {
      errors.push_back(status);
      LOG(ERROR) << status.message();
      continue;
    }

    file_list_facts_tree.NewChild(
        ExtractOneFile(content, file_path, file_list_facts_tree,
                       extracted_files, include_dir_paths));
  }

  return file_list_facts_tree;
}

void IndexingFactsTreeExtractor::Visit(const SyntaxTreeNode& node) {
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);
  switch (tag) {
    case NodeEnum ::kDescriptionList: {
      // Adds the current root to facts tree context to keep track of the parent
      // node so that it can be used to construct the tree and add children to
      // it.
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &GetRoot());
      TreeContextVisitor::Visit(node);
      break;
    }
    case NodeEnum::kInterfaceDeclaration: {
      ExtractInterface(node);
      break;
    }
    case NodeEnum::kModuleDeclaration: {
      ExtractModule(node);
      break;
    }
    case NodeEnum::kProgramDeclaration: {
      ExtractProgram(node);
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
    case NodeEnum::kSelectVariableDimensionList: {
      ExtractSelectVariableDimension(node);
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
    default: {
      TreeContextVisitor::Visit(node);
    }
  }
}

void IndexingFactsTreeExtractor::Visit(const verible::SyntaxTreeLeaf& leaf) {
  switch (leaf.get().token_enum()) {
    case verilog_tokentype::MacroIdentifier: {
      ExtractMacroReference(leaf);
      break;
    }
    case verilog_tokentype::SymbolIdentifier: {
      ExtractSymbolIdentifier(leaf);
      break;
    }
    default:
      break;
  }
}

void IndexingFactsTreeExtractor::ExtractDataDeclaration(
    const verible::SyntaxTreeNode& data_declaration) {
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
    const SyntaxTreeNode* type_node =
        GetStructOrUnionOrEnumTypeFromDataDeclaration(data_declaration);

    // Ignore if this isn't a struct or union type.
    if (type_node != nullptr &&
        NodeEnum(type_node->Tag().tag) != NodeEnum::kEnumType) {
      ExtractStructUnionDeclaration(*type_node, register_variables);
      return;
    }

    const SyntaxTreeLeaf* type_identifier =
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
    const SyntaxTreeNode* type_node =
        GetStructOrUnionOrEnumTypeFromDataDeclaration(data_declaration);

    // Ignore if this isn't a struct or union type.
    if (type_node != nullptr &&
        NodeEnum(type_node->Tag().tag) != NodeEnum::kEnumType) {
      ExtractStructUnionDeclaration(*type_node, variable_declaration_assign);
      return;
    }

    const SyntaxTreeLeaf* type_identifier =
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
    const SyntaxTreeLeaf& type_identifier,
    const std::vector<TreeSearchMatch>& variables_matche) {
  IndexingFactNode type_node(
      IndexingNodeData{IndexingFactType::kDataTypeReference});

  type_node.Value().AppendAnchor(Anchor(type_identifier.get(), context_.base));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &type_node);
    for (const TreeSearchMatch& variable : variables_matche) {
      Visit(verible::SymbolCastToNode(*variable.match));
    }
  }

  facts_tree_context_.top().NewChild(type_node);
}

void IndexingFactsTreeExtractor::ExtractModuleOrInterfaceOrProgram(
    const SyntaxTreeNode& declaration_node, IndexingFactNode& facts_node) {
  const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &facts_node);
  ExtractModuleHeader(declaration_node);
  ExtractModuleEnd(declaration_node);

  const SyntaxTreeNode& item_list = GetModuleItemList(declaration_node);
  Visit(item_list);
}

void IndexingFactsTreeExtractor::ExtractModule(
    const SyntaxTreeNode& module_declaration_node) {
  IndexingFactNode module_node(IndexingNodeData{IndexingFactType::kModule});
  ExtractModuleOrInterfaceOrProgram(module_declaration_node, module_node);
  facts_tree_context_.top().NewChild(module_node);
}

void IndexingFactsTreeExtractor::ExtractInterface(
    const SyntaxTreeNode& interface_declaration_node) {
  IndexingFactNode interface_node(
      IndexingNodeData{IndexingFactType::kInterface});
  ExtractModuleOrInterfaceOrProgram(interface_declaration_node, interface_node);
  facts_tree_context_.top().NewChild(interface_node);
}

void IndexingFactsTreeExtractor::ExtractProgram(
    const SyntaxTreeNode& program_declaration_node) {
  IndexingFactNode program_node(IndexingNodeData{IndexingFactType::kProgram});
  ExtractModuleOrInterfaceOrProgram(program_declaration_node, program_node);
  facts_tree_context_.top().NewChild(program_node);
}

void IndexingFactsTreeExtractor::ExtractModuleHeader(
    const SyntaxTreeNode& module_declaration_node) {
  // Extract module name e.g module my_module extracts "my_module".
  const verible::SyntaxTreeLeaf& module_name_leaf =
      GetModuleName(module_declaration_node);
  const Anchor module_name_anchor(module_name_leaf.get(), context_.base);
  facts_tree_context_.top().Value().AppendAnchor(module_name_anchor);

  // Extract parameters if exist.
  const SyntaxTreeNode* param_declaration_list =
      GetParamDeclarationListFromModuleDeclaration(module_declaration_node);
  if (param_declaration_list != nullptr) {
    Visit(*param_declaration_list);
  }

  // Extracting module ports e.g. (input a, input b).
  // Ports are treated as children of the module.
  const SyntaxTreeNode* port_list =
      GetModulePortDeclarationList(module_declaration_node);

  if (port_list == nullptr) {
    return;
  }

  // This boolean is used to distinguish between ANSI and Non-ANSI module ports.
  // e.g in this case:
  // module m(a, b);
  // has_propagated_type will be false as no type has been countered.
  //
  // in case like:
  // module m(a, b, input x, y)
  // for "a", "b" the boolean will be false but for "x", "y" the boolean will be
  // true.
  //
  // The boolean is used to determine whether this the fact for this variable
  // should be a reference or a defintiion.
  bool has_propagated_type = false;
  for (const auto& port : port_list->children()) {
    if (port->Kind() == verible::SymbolKind::kLeaf) continue;

    const SyntaxTreeNode& port_node = verible::SymbolCastToNode(*port);
    const auto tag = static_cast<verilog::NodeEnum>(port_node.Tag().tag);

    if (tag == NodeEnum::kPortDeclaration) {
      has_propagated_type = true;
      ExtractModulePort(port_node, has_propagated_type);
    } else if (tag == NodeEnum::kPort) {
      ExtractModulePort(GetPortReferenceFromPort(port_node),
                        has_propagated_type);
    }
  }
}

void IndexingFactsTreeExtractor::ExtractModulePort(
    const SyntaxTreeNode& module_port_node, bool has_propagated_type) {
  const auto tag = static_cast<verilog::NodeEnum>(module_port_node.Tag().tag);

  // For extracting cases like:
  // module m(input a, input b);
  if (tag == NodeEnum::kPortDeclaration) {
    const SyntaxTreeLeaf* leaf =
        GetIdentifierFromModulePortDeclaration(module_port_node);

    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(leaf->get(), context_.base)},
                         IndexingFactType::kVariableDefinition));
  } else if (tag == NodeEnum::kPortReference) {
    // For extracting Non-ANSI style ports:
    // module m(a, b);
    const SyntaxTreeLeaf* leaf =
        GetIdentifierFromPortReference(module_port_node);

    if (has_propagated_type) {
      // Check if the last type was not a primitive type.
      // e.g module (interface_type x, y).
      if (facts_tree_context_.empty() || facts_tree_context_.top().is_leaf() ||
          facts_tree_context_.top()
                  .Children()
                  .back()
                  .Value()
                  .GetIndexingFactType() !=
              IndexingFactType::kDataTypeReference) {
        // Append this as a variable definition.
        facts_tree_context_.top().NewChild(
            IndexingNodeData({Anchor(leaf->get(), context_.base)},
                             IndexingFactType::kVariableDefinition));
      } else {
        // Append this as a child to previous kDataTypeReference.
        facts_tree_context_.top().Children().back().NewChild(
            IndexingNodeData({Anchor(leaf->get(), context_.base)},
                             IndexingFactType::kVariableDefinition));
      }
    } else {
      // In case no preceeded data type.
      facts_tree_context_.top().NewChild(
          IndexingNodeData({Anchor(leaf->get(), context_.base)},
                           IndexingFactType::kVariableReference));
    }
  }

  // Extract unpacked and packed dimensions.
  for (const auto& child : module_port_node.children()) {
    if (child == nullptr || child->Kind() == verible::SymbolKind::kLeaf) {
      continue;
    }
    const auto tag = static_cast<verilog::NodeEnum>(child->Tag().tag);
    if (tag == NodeEnum::kUnqualifiedId) {
      continue;
    }
    if (tag == NodeEnum::kDataType) {
      const SyntaxTreeLeaf* data_type = GetTypeIdentifierFromDataType(*child);
      // If not null this is a non primitive type and should create
      // kDataTypeReference node for it.
      // This data_type may be some class or interface type.
      if (data_type != nullptr) {
        // Create a node for this data type and append its anchor.
        IndexingFactNode data_type_node(
            IndexingNodeData{IndexingFactType::kDataTypeReference});
        data_type_node.Value().AppendAnchor(
            Anchor(data_type->get(), context_.base));

        // TODO(fangism): try to improve this using move semantics, avoid a
        // deep-copy where possible.

        // Make the current port node child of this data type, remove it and
        // push the kDataTypeRefernce Node.
        data_type_node.NewChild(facts_tree_context_.top().Children().back());
        facts_tree_context_.top().Children().pop_back();
        facts_tree_context_.top().NewChild(data_type_node);
        continue;
      }
    }
    Visit(verible::SymbolCastToNode(*child));
  }
}

void IndexingFactsTreeExtractor::ExtractModuleNamedPort(
    const verible::SyntaxTreeNode& actual_named_port) {
  IndexingFactNode actual_port_node(
      IndexingNodeData{IndexingFactType::kModuleNamedPort});

  const SyntaxTreeLeaf& leaf = GetActualNamedPortName(actual_named_port);
  actual_port_node.Value().AppendAnchor(Anchor(leaf.get(), context_.base));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &actual_port_node);
    const verible::Symbol* paren_group =
        GetActualNamedPortParenGroup(actual_named_port);
    if (paren_group != nullptr) {
      Visit(verible::SymbolCastToNode(*paren_group));
    }
  }

  facts_tree_context_.top().NewChild(actual_port_node);
}

void IndexingFactsTreeExtractor::ExtractInputOutputDeclaration(
    const SyntaxTreeNode& identifier_unpacked_dimension) {
  const SyntaxTreeLeaf* port_name_leaf =
      GetSymbolIdentifierFromIdentifierUnpackedDimensions(
          identifier_unpacked_dimension);

  facts_tree_context_.top().NewChild(
      IndexingNodeData({Anchor(port_name_leaf->get(), context_.base)},
                       IndexingFactType::kVariableDefinition));
}

void IndexingFactsTreeExtractor::ExtractModuleEnd(
    const SyntaxTreeNode& module_declaration_node) {
  const verible::SyntaxTreeLeaf* module_name =
      GetModuleEndLabel(module_declaration_node);

  if (module_name != nullptr) {
    facts_tree_context_.top().Value().AppendAnchor(
        Anchor(module_name->get(), context_.base));
  }
}

void IndexingFactsTreeExtractor::ExtractModuleInstantiation(
    const SyntaxTreeNode& data_declaration_node,
    const std::vector<TreeSearchMatch>& gate_instances) {
  IndexingFactNode type_node(
      IndexingNodeData{IndexingFactType::kDataTypeReference});

  // Extract module type name.
  const SyntaxTreeLeaf* type =
      GetTypeIdentifierFromDataDeclaration(data_declaration_node);
  if (type == nullptr) {
    return;
  }
  type_node.Value().AppendAnchor(Anchor(type->get(), context_.base));

  // Extract parameter list
  const SyntaxTreeNode* param_list =
      GetParamListFromDataDeclaration(data_declaration_node);
  if (param_list != nullptr) {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &type_node);
    Visit(*param_list);
  }

  // Module instantiations (data declarations) may declare multiple instances
  // sharing the same type in a single statement e.g. bar b1(), b2().
  //
  // Loop through each instance and associate each declared id with the same
  // type and create its corresponding facts tree node.
  for (const TreeSearchMatch& instance : gate_instances) {
    IndexingFactNode module_instance_node(
        IndexingNodeData{IndexingFactType::kModuleInstance});

    const verible::TokenInfo& variable_name =
        GetModuleInstanceNameTokenInfoFromGateInstance(*instance.match);
    const Anchor variable_name_anchor(variable_name, context_.base);
    module_instance_node.Value().AppendAnchor(variable_name_anchor);

    {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &module_instance_node);
      const SyntaxTreeNode& paren_group =
          GetParenGroupFromModuleInstantiation(*instance.match);
      Visit(paren_group);
    }

    type_node.NewChild(module_instance_node);
  }

  facts_tree_context_.top().NewChild(type_node);
}

void IndexingFactsTreeExtractor::ExtractSelectVariableDimension(
    const verible::SyntaxTreeNode& variable_dimension) {
  // Terminate if there is no parent or the parent has no children.
  if (facts_tree_context_.empty() || facts_tree_context_.top().is_leaf()) {
    return;
  }

  // Make the previous node the parent of this node.
  // e.g x[i] ==> make node of "x" the parent of the current variable dimension
  // "[i]".
  const IndexingFactsTreeContext::AutoPop p(
      &facts_tree_context_, &facts_tree_context_.top().Children().back());

  // Visit the children of this node.
  TreeContextVisitor::Visit(variable_dimension);
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

void IndexingFactsTreeExtractor::ExtractPackageDeclaration(
    const SyntaxTreeNode& package_declaration_node) {
  IndexingFactNode package_node(IndexingNodeData{IndexingFactType::kPackage});

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &package_node);
    // Extract package name.
    const SyntaxTreeLeaf& package_name_leaf =
        GetPackageNameLeaf(package_declaration_node);
    const Anchor class_name_anchor(package_name_leaf.get(), context_.base);
    facts_tree_context_.top().Value().AppendAnchor(class_name_anchor);

    // Extract package name after endpackage if exists.
    const SyntaxTreeLeaf* package_end_name =
        GetPackageNameEndLabel(package_declaration_node);

    if (package_end_name != nullptr) {
      const Anchor package_end_anchor(package_end_name->get(), context_.base);
      facts_tree_context_.top().Value().AppendAnchor(package_end_anchor);
    }

    // Visit package body it exists.
    const verible::Symbol* package_item_list =
        GetPackageItemList(package_declaration_node);
    if (package_item_list != nullptr) {
      Visit(verible::SymbolCastToNode(*package_item_list));
    }
  }

  facts_tree_context_.top().NewChild(package_node);
}

void IndexingFactsTreeExtractor::ExtractMacroDefinition(
    const verible::SyntaxTreeNode& preprocessor_definition) {
  const verible::SyntaxTreeLeaf& macro_name =
      GetMacroName(preprocessor_definition);

  IndexingFactNode macro_node(IndexingNodeData(
      {Anchor(macro_name.get(), context_.base)}, IndexingFactType::kMacro));

  const std::vector<verible::TreeSearchMatch> args =
      FindAllMacroDefinitionsArgs(preprocessor_definition);

  for (const verible::TreeSearchMatch& arg : args) {
    const verible::SyntaxTreeLeaf& leaf = GetMacroArgName(*arg.match);

    macro_node.NewChild(
        IndexingNodeData({Anchor(leaf.get(), context_.base)},
                         IndexingFactType::kVariableDefinition));
  }

  facts_tree_context_.top().NewChild(macro_node);
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

void IndexingFactsTreeExtractor::ExtractClassConstructor(
    const verible::SyntaxTreeNode& class_constructor) {
  IndexingFactNode constructor_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTask});

  const SyntaxTreeLeaf& new_keyword =
      GetNewKeywordFromClassConstructor(class_constructor);
  constructor_node.Value().AppendAnchor(
      Anchor(new_keyword.get(), context_.base));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &constructor_node);

    // Extract ports.
    ExtractFunctionTaskConstructorPort(class_constructor);

    // Extract constructor body.
    const SyntaxTreeNode& constructor_body =
        GetClassConstructorStatementList(class_constructor);
    Visit(constructor_body);
  }

  facts_tree_context_.top().NewChild(constructor_node);
}

void IndexingFactsTreeExtractor::ExtractFunctionDeclaration(
    const SyntaxTreeNode& function_declaration_node) {
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTask});

  // Extract function name.
  const verible::Symbol* function_name =
      GetFunctionId(function_declaration_node);
  if (function_name == nullptr) {
    return;
  }
  Visit(verible::SymbolCastToNode(*function_name));
  MoveAndDeleteLastSibling(function_node);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);
    // Extract function ports.
    ExtractFunctionTaskConstructorPort(function_declaration_node);

    // Extract function body.
    const SyntaxTreeNode& function_body =
        GetFunctionBlockStatementList(function_declaration_node);
    Visit(function_body);
  }

  facts_tree_context_.top().NewChild(function_node);
}

void IndexingFactsTreeExtractor::ExtractTaskDeclaration(
    const SyntaxTreeNode& task_declaration_node) {
  IndexingFactNode task_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTask});

  // Extract task name.
  const auto* task_name = GetTaskId(task_declaration_node);
  if (task_name == nullptr) {
    return;
  }
  Visit(verible::SymbolCastToNode(*task_name));
  MoveAndDeleteLastSibling(task_node);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &task_node);

    // Extract task ports.
    ExtractFunctionTaskConstructorPort(task_declaration_node);

    // Extract task body.
    const SyntaxTreeNode& task_body =
        GetTaskStatementList(task_declaration_node);
    Visit(task_body);
  }

  facts_tree_context_.top().NewChild(task_node);
}

void IndexingFactsTreeExtractor::ExtractFunctionTaskConstructorPort(
    const SyntaxTreeNode& function_declaration_node) {
  const std::vector<TreeSearchMatch> ports =
      FindAllTaskFunctionPortDeclarations(function_declaration_node);

  for (const TreeSearchMatch& port : ports) {
    const SyntaxTreeLeaf* leaf =
        GetIdentifierFromTaskFunctionPortItem(*port.match);

    // TODO(minatoma): Consider using kPorts or kParam for ports and params
    // instead of variables (same goes for modules).
    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(leaf->get(), context_.base)},
                         IndexingFactType::kVariableDefinition));
  }
}

void IndexingFactsTreeExtractor::ExtractFunctionOrTaskCall(
    const SyntaxTreeNode& function_call_node) {
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionCall});

  // Extract function or task name.
  // It can be single or preceeded with a pkg or class names.
  const SyntaxTreeNode& identifiers =
      GetIdentifiersFromFunctionCall(function_call_node);
  Visit(identifiers);

  // Move the data from the last sibling to the current node and delete that
  // sibling.
  MoveAndDeleteLastSibling(function_node);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);
    const SyntaxTreeNode& arguments = GetParenGroupFromCall(function_call_node);
    // Extract function or task parameters.
    Visit(arguments);
  }

  facts_tree_context_.top().NewChild(function_node);
}

void IndexingFactsTreeExtractor::ExtractMethodCallExtension(
    const verible::SyntaxTreeNode& call_extension_node) {
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionCall});

  // Move the data from the last sibling to the current node and delete that
  // sibling
  MoveAndDeleteLastSibling(function_node);

  const SyntaxTreeLeaf& function_name =
      GetFunctionCallNameFromCallExtension(call_extension_node);
  function_node.Value().AppendAnchor(
      Anchor(function_name.get(), context_.base));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &function_node);
    const SyntaxTreeNode& arguments =
        GetParenGroupFromCallExtension(call_extension_node);
    // parameters.
    Visit(arguments);
  }

  facts_tree_context_.top().NewChild(function_node);
}

void IndexingFactsTreeExtractor::ExtractMemberExtension(
    const verible::SyntaxTreeNode& hierarchy_extension_node) {
  IndexingFactNode member_node(
      IndexingNodeData{IndexingFactType::kMemberReference});

  // Move the data from the last sibling to the current node and delete that
  // sibling
  MoveAndDeleteLastSibling(member_node);

  // Append the member name to the current anchors.
  const SyntaxTreeLeaf& member_name =
      GetUnqualifiedIdFromHierarchyExtension(hierarchy_extension_node);
  member_node.Value().AppendAnchor(Anchor(member_name.get(), context_.base));

  facts_tree_context_.top().NewChild(member_node);
}

void IndexingFactsTreeExtractor::ExtractClassDeclaration(
    const SyntaxTreeNode& class_declaration) {
  IndexingFactNode class_node(IndexingNodeData{IndexingFactType::kClass});

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &class_node);
    // Extract class name.
    const SyntaxTreeLeaf& class_name_leaf = GetClassName(class_declaration);
    const Anchor class_name_anchor(class_name_leaf.get(), context_.base);
    facts_tree_context_.top().Value().AppendAnchor(class_name_anchor);

    // Extract class name after endclass.
    const SyntaxTreeLeaf* class_end_name = GetClassEndLabel(class_declaration);

    if (class_end_name != nullptr) {
      const Anchor class_end_anchor(class_end_name->get(), context_.base);
      facts_tree_context_.top().Value().AppendAnchor(class_end_anchor);
    }

    const SyntaxTreeNode* param_list =
        GetParamDeclarationListFromClassDeclaration(class_declaration);
    if (param_list != nullptr) {
      Visit(*param_list);
    }

    const SyntaxTreeNode* extended_class = GetExtendedClass(class_declaration);
    if (extended_class != nullptr) {
      // In case of => class X extends Y.
      if (NodeEnum(extended_class->Tag().tag) == NodeEnum::kUnqualifiedId) {
        class_node.NewChild(IndexingNodeData(
            {Anchor(AutoUnwrapIdentifier(*extended_class)->get(),
                    context_.base)},
            IndexingFactType::kExtends));
      } else {
        // In case of => class X extends pkg1::Y.
        ExtractQualifiedId(*extended_class);

        // Construct extends node from the last node which is kMemberReference,
        // remove kMemberReference node and append the new extends node.
        IndexingFactNode extends_node(
            IndexingNodeData(class_node.Children().back().Value().Anchors(),
                             IndexingFactType::kExtends));
        class_node.Children().pop_back();
        class_node.NewChild(extends_node);
      }
    }

    // Visit class body.
    const SyntaxTreeNode& class_item_list = GetClassItemList(class_declaration);
    Visit(class_item_list);
  }

  facts_tree_context_.top().NewChild(class_node);
}

void IndexingFactsTreeExtractor::ExtractClassInstances(
    const SyntaxTreeNode& data_declaration_node,
    const std::vector<TreeSearchMatch>& class_instances) {
  // TODO(minatoma): refactor this function and git rid of the logic in the for
  // loop because kRegisterVariable and kVariableDeclarationAssignment are
  // extracted in function.
  IndexingFactNode type_node(
      IndexingNodeData{IndexingFactType::kDataTypeReference});

  const verible::SyntaxTreeLeaf* type =
      GetTypeIdentifierFromDataDeclaration(data_declaration_node);
  if (type == nullptr) {
    return;
  }
  const Anchor type_anchor(type->get(), context_.base);
  type_node.Value().AppendAnchor(type_anchor);

  // Extract parameter list
  const SyntaxTreeNode* param_list =
      GetParamListFromDataDeclaration(data_declaration_node);
  if (param_list != nullptr) {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &type_node);
    Visit(*param_list);
  }

  // Class instances may may appear as multiple instances sharing the same type
  // in a single statement e.g. myClass b1 = new, b2 = new.
  // LRM 8.8 Typed constructor calls
  //
  // Loop through each instance and associate each declared id with the same
  // type and create its corresponding facts tree node.
  for (const TreeSearchMatch& instance : class_instances) {
    IndexingFactNode class_instance_node(
        IndexingNodeData{IndexingFactType::kClassInstance});
    const auto tag = static_cast<verilog::NodeEnum>(instance.match->Tag().tag);

    const SyntaxTreeNode* trailing_expression = nullptr;
    if (tag == NodeEnum::kRegisterVariable) {
      const verible::TokenInfo& instance_name =
          GetInstanceNameTokenInfoFromRegisterVariable(*instance.match);

      class_instance_node.Value().AppendAnchor(
          Anchor(instance_name, context_.base));

      trailing_expression =
          GetTrailingExpressionFromRegisterVariable(*instance.match);
    } else if (tag == NodeEnum::kVariableDeclarationAssignment) {
      const SyntaxTreeLeaf& leaf =
          GetUnqualifiedIdFromVariableDeclarationAssignment(*instance.match);
      class_instance_node.Value().AppendAnchor(
          Anchor(leaf.get(), context_.base));

      trailing_expression =
          GetTrailingExpressionFromVariableDeclarationAssign(*instance.match);
    }

    if (trailing_expression != nullptr) {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &class_instance_node);
      // Visit Trailing Assignment Expression.
      Visit(*trailing_expression);
    }

    type_node.NewChild(class_instance_node);
  }

  facts_tree_context_.top().NewChild(type_node);
}

void IndexingFactsTreeExtractor::ExtractRegisterVariable(
    const verible::SyntaxTreeNode& register_variable) {
  IndexingFactNode variable_node(
      IndexingNodeData{IndexingFactType::kVariableDefinition});

  const verible::TokenInfo& variable_name_token_info =
      GetInstanceNameTokenInfoFromRegisterVariable(register_variable);
  variable_node.Value().AppendAnchor(
      Anchor(variable_name_token_info, context_.base));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &variable_node);
    const SyntaxTreeNode& unpacked_dimension =
        GetUnpackedDimensionFromRegisterVariable(register_variable);
    Visit(unpacked_dimension);

    const SyntaxTreeNode* expression =
        GetTrailingExpressionFromRegisterVariable(register_variable);
    if (expression != nullptr) {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &variable_node);
      // Visit Trailing Assignment Expression.
      Visit(*expression);
    }
  }

  facts_tree_context_.top().NewChild(variable_node);
}

void IndexingFactsTreeExtractor::ExtractVariableDeclarationAssignment(
    const verible::SyntaxTreeNode& variable_declaration_assignment) {
  IndexingFactNode variable_node(
      IndexingNodeData{IndexingFactType::kVariableDefinition});

  const SyntaxTreeLeaf& leaf =
      GetUnqualifiedIdFromVariableDeclarationAssignment(
          variable_declaration_assignment);
  variable_node.Value().AppendAnchor(Anchor(leaf.get(), context_.base));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &variable_node);
    const SyntaxTreeNode& unpacked_dimension =
        GetUnpackedDimensionFromVariableDeclarationAssign(
            variable_declaration_assignment);
    Visit(unpacked_dimension);

    const SyntaxTreeNode* expression =
        GetTrailingExpressionFromVariableDeclarationAssign(
            variable_declaration_assignment);
    if (expression != nullptr) {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &variable_node);
      // Visit Trailing Assignment Expression.
      Visit(*expression);
    }
  }
  facts_tree_context_.top().NewChild(variable_node);
}

void IndexingFactsTreeExtractor::ExtractSymbolIdentifier(
    const SyntaxTreeLeaf& unqualified_id) {
  // Get the symbol name.
  const SyntaxTreeLeaf* leaf = AutoUnwrapIdentifier(unqualified_id);
  facts_tree_context_.top().NewChild(
      IndexingNodeData({Anchor(leaf->get(), context_.base)},
                       IndexingFactType::kVariableReference));
}

void IndexingFactsTreeExtractor::ExtractParamDeclaration(
    const verible::SyntaxTreeNode& param_declaration) {
  IndexingFactNode param_node(
      IndexingNodeData{IndexingFactType::kParamDeclaration});

  const SyntaxTreeNode* type_assignment =
      GetTypeAssignmentFromParamDeclaration(param_declaration);

  // Parameters can be in two cases:
  // 1st => parameter type x;
  if (type_assignment != nullptr) {
    const SyntaxTreeLeaf* identifier =
        GetIdentifierLeafFromTypeAssignment(*type_assignment);
    param_node.Value().AppendAnchor(Anchor(identifier->get(), context_.base));

    const SyntaxTreeNode* expression =
        GetExpressionFromTypeAssignment(*type_assignment);
    if (expression != nullptr) {
      {
        const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                  &param_node);
        Visit(*expression);
      }
    }
  } else {
    // 2nd => parameter int x;
    // Extract Param name.
    const verible::TokenInfo& param_name =
        GetParameterNameToken(param_declaration);
    param_node.Value().AppendAnchor(Anchor(param_name, context_.base));

    {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &param_node);

      const verible::Symbol* assign_expression =
          GetParamAssignExpression(param_declaration);

      if (assign_expression != nullptr &&
          assign_expression->Kind() == verible::SymbolKind::kNode) {
        // Extract trailing expression.
        Visit(verible::SymbolCastToNode(*assign_expression));
      }
    }
  }

  facts_tree_context_.top().NewChild(param_node);
}

void IndexingFactsTreeExtractor::ExtractParamByName(
    const verible::SyntaxTreeNode& param_by_name) {
  IndexingFactNode named_param_node(
      IndexingNodeData{IndexingFactType::kNamedParam});

  const SyntaxTreeLeaf& leaf = GetNamedParamFromActualParam(param_by_name);
  named_param_node.Value().AppendAnchor(Anchor(leaf.get(), context_.base));

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &named_param_node);
    const verible::SyntaxTreeNode* paren_group =
        GetParenGroupFromActualParam(param_by_name);
    if (paren_group != nullptr) {
      Visit(*paren_group);
    }
  }

  facts_tree_context_.top().NewChild(named_param_node);
}

void IndexingFactsTreeExtractor::ExtractPackageImport(
    const SyntaxTreeNode& package_import_item) {
  IndexingNodeData package_import_data(IndexingFactType::kPackageImport);

  // Extracts the name of the imported package.
  const SyntaxTreeLeaf& package_name =
      GetImportedPackageName(package_import_item);

  package_import_data.AppendAnchor(Anchor(package_name.get(), context_.base));

  // Get the name of the imported item (if exists).
  // e.g pkg::var1 ==> return var1.
  // will be nullptr in case of pkg::*.
  const SyntaxTreeLeaf* imported_item =
      GeImportedItemNameFromPackageImportItem(package_import_item);
  if (imported_item != nullptr) {
    package_import_data.AppendAnchor(
        Anchor(imported_item->get(), context_.base));
  }

  facts_tree_context_.top().NewChild(package_import_data);
}

void IndexingFactsTreeExtractor::ExtractQualifiedId(
    const verible::SyntaxTreeNode& qualified_id) {
  IndexingNodeData member_reference_data(IndexingFactType::kMemberReference);

  // Get all the variable names in the qualified id.
  for (const auto& child : qualified_id.children()) {
    if (child == nullptr) continue;
    if (NodeEnum(child->Tag().tag) != NodeEnum::kUnqualifiedId) {
      continue;
    }
    member_reference_data.AppendAnchor(
        Anchor(AutoUnwrapIdentifier(*child)->get(), context_.base));

    const SyntaxTreeNode* param_list = GetParamListFromUnqualifiedId(*child);
    if (param_list != nullptr) {
      IndexingFactNode param_member_reference(member_reference_data);

      {
        const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                  &param_member_reference);
        Visit(*param_list);
      }

      facts_tree_context_.top().NewChild(param_member_reference);
    }
  }

  facts_tree_context_.top().NewChild(member_reference_data);
}

void IndexingFactsTreeExtractor::ExtractForInitialization(
    const verible::SyntaxTreeNode& for_initialization) {
  // Extracts the variable name from for initialization.
  // e.g int i = 0; ==> extracts "i".
  const SyntaxTreeLeaf& variable_name =
      GetVariableNameFromForInitialization(for_initialization);
  facts_tree_context_.top().NewChild(
      IndexingNodeData({Anchor(variable_name.get(), context_.base)},
                       IndexingFactType::kVariableDefinition));

  // Extracts the data the in case it contains packed or unpacked dimension.
  // e.g bit [x : y] var [x : y].
  const SyntaxTreeNode* data_type_node =
      GetDataTypeFromForInitialization(for_initialization);
  if (data_type_node != nullptr) {
    Visit(*data_type_node);
  }

  // Extracts the RHS of the declaration.
  // e.g int i = x; ==> extracts "x".
  const SyntaxTreeNode& expression =
      GetExpressionFromForInitialization(for_initialization);
  Visit(expression);
}

// Returns string_view of `text` with outermost double-quotes removed.
// If `text` is not wrapped in quotes, return it as-is.
absl::string_view StripOuterQuotes(absl::string_view text) {
  return absl::StripSuffix(absl::StripPrefix(text, "\""), "\"");
}

void IndexingFactsTreeExtractor::ExtractInclude(
    const verible::SyntaxTreeNode& preprocessor_include) {
  const SyntaxTreeLeaf& included_filename =
      GetFileFromPreprocessorInclude(preprocessor_include);

  absl::string_view filename_text = included_filename.get().text();

  // Remove the double quotes from the filesname.
  const absl::string_view filename_unquoted = StripOuterQuotes(filename_text);
  const std::string filename(filename_unquoted.begin(),
                             filename_unquoted.end());
  int startLocation = included_filename.get().left(context_.base);
  int endLocation = included_filename.get().right(context_.base);

  std::string file_path = "";

  // Check if this included file was extracted before.
  const auto filename_itr = extracted_files_.find(filename);
  if (filename_itr != extracted_files_.end()) {
    file_path = filename_itr->second;

    // Create a node for include statement with two Anchors:
    // 1st one holds the actual text in the include statement.
    // 2nd one holds the path of the included file relative to the file list.
    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(filename_text, startLocation, endLocation),
                          Anchor(file_path, 0, 0)},
                         IndexingFactType::kInclude));
  } else {
    std::string content;

    auto status = SearchForFileAndGetContents(file_path, content, filename,
                                              include_dir_paths_);
    if (!status.ok()) {
      // Couldn't find the included file in any of include directories.
      LOG(ERROR) << "Error while reading file: " << filename;
      return;
    }

    extracted_files_[filename] = file_path;

    // Create a node for include statement with two Anchors:
    // 1st one holds the actual text in the include statement.
    // 2nd one holds the path of the included file relative to the file
    // list.
    facts_tree_context_.top().NewChild(
        IndexingNodeData({Anchor(filename_text, startLocation, endLocation),
                          Anchor(file_path, 0, 0)},
                         IndexingFactType::kInclude));

    file_list_facts_tree_.NewChild(
        ExtractOneFile(content, file_path, file_list_facts_tree_,
                       extracted_files_, include_dir_paths_));
  }
}

void IndexingFactsTreeExtractor::ExtractEnumName(
    const verible::SyntaxTreeNode& enum_name) {
  IndexingFactNode enum_node(IndexingNodeData{IndexingFactType::kConstant});

  const SyntaxTreeLeaf& name = GetSymbolIdentifierFromEnumName(enum_name);
  enum_node.Value().AppendAnchor(Anchor{name.get(), context_.base});

  // Iterate over the children and traverse them to extract facts from inner
  // nodes and ignore the leaves.
  // e.g enum {RED[x] = 1, OLD=y} => explores "[x]", "=y".
  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &enum_node);
    for (const auto& child : enum_name.children()) {
      if (child == nullptr || child->Kind() == verible::SymbolKind::kLeaf) {
        continue;
      }
      Visit(verible::SymbolCastToNode(*child));
    }
  }

  facts_tree_context_.top().NewChild(enum_node);
}

void IndexingFactsTreeExtractor::ExtractEnumTypeDeclaration(
    const verible::SyntaxTreeNode& enum_type_declaration) {
  // Extract enum type name.
  const SyntaxTreeLeaf* enum_type_name =
      GetIdentifierFromTypeDeclaration(enum_type_declaration);
  facts_tree_context_.top().NewChild(
      IndexingNodeData({Anchor(enum_type_name->get(), context_.base)},
                       IndexingFactType::kVariableDefinition));

  // Explore the children of this enum type to extract.
  for (const auto& child : enum_type_declaration.children()) {
    if (child == nullptr || child->Kind() == verible::SymbolKind::kLeaf) {
      continue;
    }
    Visit(verible::SymbolCastToNode(*child));
  }
}

void IndexingFactsTreeExtractor::ExtractStructUnionTypeDeclaration(
    const SyntaxTreeNode& type_declaration, const SyntaxTreeNode& struct_type) {
  IndexingFactNode struct_type_node(
      IndexingNodeData{IndexingFactType::kStructOrUnion});

  // Extract struct type name.
  const SyntaxTreeLeaf* struct_type_name =
      GetIdentifierFromTypeDeclaration(type_declaration);
  struct_type_node.Value().AppendAnchor(
      Anchor(struct_type_name->get(), context_.base));

  // Explore the children of this enum type to extract.
  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &struct_type_node);
    Visit(struct_type);
  }

  facts_tree_context_.top().NewChild(struct_type_node);
}

void IndexingFactsTreeExtractor::ExtractStructUnionDeclaration(
    const SyntaxTreeNode& struct_type,
    const std::vector<TreeSearchMatch>& variables_matched) {
  // Dummy data type to hold the extracted struct members because there is no
  // data type here.
  IndexingFactNode struct_node(
      IndexingNodeData{IndexingFactType::kStructOrUnion});

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &struct_node);
    // Extract struct members.
    Visit(struct_type);
  }

  for (const TreeSearchMatch& variable : variables_matched) {
    // Extract this variable.
    // This can be kRegisterVariable or kVariableDeclarationAssign.
    Visit(verible::SymbolCastToNode(*variable.match));

    // Append the struct members to be a children of this variable.
    for (const auto& child : struct_node.Children()) {
      facts_tree_context_.top().Children().back().NewChild(child);
    }
  }
}

void IndexingFactsTreeExtractor::ExtractDataTypeImplicitIdDimensions(
    const SyntaxTreeNode& data_type_implicit_id_dimensions) {
  // This node has 2 cases:
  // 1st case:
  // typedef struct {
  //    data_type var_name;
  // } my_struct;
  // In this case this should be a kDataTypeReference with var_name as a child.
  //
  // 2nd case:
  // typedef struct {
  //    struct {int xx;} var_name;
  // } my_struct;
  // In this case var_name should contain "xx" inside it.

  std::pair<const verible::SyntaxTreeLeaf*, int> variable_name =
      GetSymbolIdentifierFromDataTypeImplicitIdDimensions(
          data_type_implicit_id_dimensions);

  IndexingFactNode variable_node(
      IndexingNodeData{IndexingFactType::kVariableDefinition});
  variable_node.Value().AppendAnchor(
      Anchor(variable_name.first->get(), context_.base));

  if (variable_name.second == 1) {
    const SyntaxTreeLeaf* type_identifier =
        GetNonprimitiveTypeOfDataTypeImplicitDimensions(
            data_type_implicit_id_dimensions);

    if (type_identifier == nullptr) {
      return;
    }

    IndexingFactNode type_node(
        IndexingNodeData{IndexingFactType::kDataTypeReference});
    type_node.Value().AppendAnchor(
        Anchor(type_identifier->get(), context_.base));

    type_node.NewChild(variable_node);
    facts_tree_context_.top().NewChild(type_node);
  } else if (variable_name.second == 2) {
    {
      const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                                &variable_node);
      for (const auto& child : data_type_implicit_id_dimensions.children()) {
        if (child == nullptr || child->Kind() == verible::SymbolKind::kLeaf) {
          continue;
        }
        Visit(verible::SymbolCastToNode(*child));
      }
    }

    facts_tree_context_.top().NewChild(variable_node);
  }
}

void IndexingFactsTreeExtractor::ExtractTypeDeclaration(
    const verible::SyntaxTreeNode& type_declaration) {
  VLOG(1) << __FUNCTION__;
  const SyntaxTreeNode* type =
      GetReferencedTypeOfTypeDeclaration(type_declaration);
  if (type == nullptr) return;

  // Look for enum/struct/union in the referenced type.
  const auto tag = static_cast<NodeEnum>(type->Tag().tag);
  if (tag != NodeEnum::kDataType) return;
  const SyntaxTreeNode* primitive =
      GetStructOrUnionOrEnumTypeFromDataType(*type);
  if (primitive == nullptr) return;

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
    const verible::SyntaxTreeNode& node) {
  IndexingFactNode temp_scope_node(
      IndexingNodeData{IndexingFactType::kAnonymousScope});

  // Generate unique id for this scope.
  temp_scope_node.Value().AppendAnchor(
      Anchor(absl::StrCat("anonymous-scope-", next_anonymous_id++), 0, 0));
  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_,
                                              &temp_scope_node);
    TreeContextVisitor::Visit(node);
  }

  facts_tree_context_.top().NewChild(temp_scope_node);
}

void IndexingFactsTreeExtractor::MoveAndDeleteLastSibling(
    IndexingFactNode& new_node) {
  // Terminate if there is no parent or the parent has no children.
  if (facts_tree_context_.empty() || facts_tree_context_.top().is_leaf()) {
    return;
  }

  // Get The last sibling.
  IndexingFactNode& previous_node = facts_tree_context_.top().Children().back();

  // Fill the anchors of the previous node to the current node.
  new_node.Value().SwapAnchors(&previous_node.Value());

  // Move the children of the previous node to this node.
  new_node.AdoptSubtreesFrom(&previous_node);

  // The node is removed so that it can be treated as a function call.
  facts_tree_context_.top().Children().pop_back();
}

}  // namespace kythe
}  // namespace verilog
