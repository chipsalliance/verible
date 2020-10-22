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
    std::set<std::string>& extracted_files) {
  IndexingFactsTreeExtractor visitor(base, file_name, file_list_facts_tree,
                                     extracted_files);
  if (syntax_tree == nullptr) {
    return visitor.GetRoot();
  }

  const SyntaxTreeNode& root = verible::SymbolCastToNode(*syntax_tree);
  root.Accept(&visitor);
  return visitor.GetRoot();
}

// Given a verilog file returns the extracted indexing facts tree.
IndexingFactNode ExtractOneFile(absl::string_view content,
                                absl::string_view filename,
                                IndexingFactNode& file_list_facts_tree,
                                std::set<std::string>& extracted_files) {
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
                                filename, file_list_facts_tree,
                                extracted_files);
}

// Returns the path of the file relative to the file list.
std::string GetPathRelativeToFileList(absl::string_view file_list_dir,
                                      absl::string_view filename) {
  return verible::file::JoinPath(file_list_dir, filename);
}

}  // namespace

IndexingFactNode ExtractFiles(const std::vector<std::string>& ordered_file_list,
                              int& exit_status,
                              absl::string_view file_list_dir) {
  // Create a node to hold the dirname of the ordered file list and group all
  // the files and acts as a ordered file list of these files.
  IndexingFactNode file_list_facts_tree(IndexingNodeData(
      {Anchor(file_list_dir, 0, 0)}, IndexingFactType::kFileList));
  std::set<std::string> extracted_files;

  for (const auto& filename : ordered_file_list) {
    std::string file_path = GetPathRelativeToFileList(file_list_dir, filename);

    // Check if this included file was extracted before.
    if (extracted_files.find(file_path) != extracted_files.end()) {
      continue;
    }
    extracted_files.insert(file_path);

    std::string content;
    if (!verible::file::GetContents(file_path, &content).ok()) {
      LOG(ERROR) << "Error while reading file: " << file_path;
      exit_status = 1;
      continue;
    }

    file_list_facts_tree.NewChild(ExtractOneFile(
        content, file_path, file_list_facts_tree, extracted_files));
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
    case NodeEnum::kSelectVariableDimension: {
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

    // Traverse the children to extract inner nodes.
    TreeContextVisitor::Visit(data_declaration);
    return;
  }

  // Traverse the children to extract inner nodes.
  TreeContextVisitor::Visit(data_declaration);
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
  const verible::TokenInfo& type =
      GetTypeTokenInfoFromDataDeclaration(data_declaration_node);
  const Anchor type_anchor(type, context_.base);
  type_node.Value().AppendAnchor(type_anchor);

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

void IndexingFactsTreeExtractor::ExtractFunctionDeclaration(
    const SyntaxTreeNode& function_declaration_node) {
  IndexingFactNode function_node(
      IndexingNodeData{IndexingFactType::kFunctionOrTask});

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
  const auto* task_name_leaf = GetTaskName(task_declaration_node);
  const Anchor task_name_anchor(task_name_leaf->get(), context_.base);
  task_node.Value().AppendAnchor(task_name_anchor);

  {
    const IndexingFactsTreeContext::AutoPop p(&facts_tree_context_, &task_node);

    // Extract task ports.
    ExtractFunctionTaskPort(task_declaration_node);

    // Extract task body.
    const SyntaxTreeNode& task_body =
        GetTaskStatementList(task_declaration_node);
    Visit(task_body);
  }

  facts_tree_context_.top().NewChild(task_node);
}

void IndexingFactsTreeExtractor::ExtractFunctionTaskPort(
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
  const SyntaxTreeNode& local_root =
      GetLocalRootFromFunctionCall(function_call_node);
  const std::vector<TreeSearchMatch> unqualified_ids =
      FindAllUnqualifiedIds(local_root);
  for (const TreeSearchMatch& unqualified_id : unqualified_ids) {
    function_node.Value().AppendAnchor(Anchor(
        AutoUnwrapIdentifier(*unqualified_id.match)->get(), context_.base));
  }

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

  // Terminate if there is no parent or the parent has no children.
  if (facts_tree_context_.empty() || facts_tree_context_.top().is_leaf()) {
    return;
  }

  const IndexingFactNode& previous_node =
      facts_tree_context_.top().Children().back();

  // Fill the anchors of the previous node to the current node.
  for (const Anchor& anchor : previous_node.Value().Anchors()) {
    function_node.Value().AppendAnchor(anchor);
  }

  // Move the children of the previous node to this node.
  for (const IndexingFactNode& child : previous_node.Children()) {
    function_node.NewChild(child);
  }

  // The node is removed so that it can be treated as a function call.
  facts_tree_context_.top().Children().pop_back();

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

  // Terminate if there is no parent or the parent has no children.
  if (facts_tree_context_.empty() || facts_tree_context_.top().is_leaf()) {
    return;
  }

  const IndexingFactNode& previous_node =
      facts_tree_context_.top().Children().back();

  // Fill the anchors of the previous node to the current node.
  // Previous node should be a kMemberReference or kVariableReference.
  for (const Anchor& anchor : previous_node.Value().Anchors()) {
    member_node.Value().AppendAnchor(anchor);
  }

  // Move the children of the previous node to this node.
  for (const IndexingFactNode& child : previous_node.Children()) {
    member_node.NewChild(child);
  }

  // The node is removed so that it can be treated as a member.
  facts_tree_context_.top().Children().pop_back();

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
  IndexingFactNode class_node(
      IndexingNodeData{IndexingFactType::kDataTypeReference});

  const verible::TokenInfo& type =
      GetTypeTokenInfoFromDataDeclaration(data_declaration_node);
  const Anchor type_anchor(type, context_.base);

  class_node.Value().AppendAnchor(type_anchor);

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

    class_node.NewChild(class_instance_node);
  }

  facts_tree_context_.top().NewChild(class_node);
}

void IndexingFactsTreeExtractor::ExtractRegisterVariable(
    const verible::SyntaxTreeNode& register_variable) {
  IndexingFactNode variable_node(
      IndexingNodeData{IndexingFactType::kVariableDefinition});

  const verible::TokenInfo& variable_name_token_info =
      GetInstanceNameTokenInfoFromRegisterVariable(register_variable);
  variable_node.Value().AppendAnchor(
      Anchor(variable_name_token_info, context_.base));

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
  const std::vector<TreeSearchMatch> unqualified_ids =
      FindAllUnqualifiedIds(qualified_id);

  for (const TreeSearchMatch& unqualified_id : unqualified_ids) {
    member_reference_data.AppendAnchor(Anchor(
        AutoUnwrapIdentifier(*unqualified_id.match)->get(), context_.base));
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

  std::string file_list_dirname =
      file_list_facts_tree_.Value().Anchors()[0].Value();

  absl::string_view filename_text = included_filename.get().text();

  // Remove the double quotes from the filesname.
  absl::string_view filename = StripOuterQuotes(filename_text);
  int startLocation = included_filename.get().left(context_.base);
  int endLocation = included_filename.get().right(context_.base);

  std::string file_path =
      GetPathRelativeToFileList(file_list_dirname, filename);

  // Create a node for include statement with two Anchors:
  // 1st one holds the actual text in the include statement.
  // 2nd one holds the path of the included file relative to the file list.
  facts_tree_context_.top().NewChild(
      IndexingNodeData({Anchor(filename_text, startLocation, endLocation),
                        Anchor(file_path, 0, 0)},
                       IndexingFactType::kInclude));

  // Check if this included file was extracted before.
  if (extracted_files_.find(file_path) != extracted_files_.end()) {
    return;
  }
  extracted_files_.insert(file_path);

  std::string content;
  if (!verible::file::GetContents(file_path, &content).ok()) {
    LOG(ERROR) << "Error while reading file: " << file_path;
    return;
  }

  file_list_facts_tree_.NewChild(ExtractOneFile(
      content, file_path, file_list_facts_tree_, extracted_files_));
}

void IndexingFactsTreeExtractor::ExtractEnumName(
    const verible::SyntaxTreeNode& enum_name) {
  IndexingFactNode enum_node(IndexingNodeData{IndexingFactType::kConstant});

  const SyntaxTreeLeaf& name = GetSymbolIdentifierFromEnumName(enum_name);
  enum_node.Value().AppendAnchor(Anchor{name.get(), context_.base});

  // Iterate over the children and traverse them to extract facts form inner
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

void IndexingFactsTreeExtractor::ExtractTypeDeclaration(
    const verible::SyntaxTreeNode& type_declaration) {
  // Determine if this type declaration is a enum type.
  const std::vector<TreeSearchMatch> enum_types =
      FindAllEnumTypes(type_declaration);
  if (!enum_types.empty()) {
    ExtractEnumTypeDeclaration(type_declaration);
    return;
  }

  // Determine if this type declaration is a struct type.
  const std::vector<TreeSearchMatch> struct_types =
      FindAllStructTypes(type_declaration);
  if (!struct_types.empty()) {
    // TODO(minatoma): add extraction for structs here.
    return;
  }
}

}  // namespace kythe
}  // namespace verilog
