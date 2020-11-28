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

#include "verilog/tools/kythe/kythe_facts_extractor.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "common/util/logging.h"
#include "verilog/tools/kythe/kythe_schema_constants.h"
#include "verilog/tools/kythe/scope_resolver.h"

namespace verilog {
namespace kythe {

namespace {

// Returns the file path of the file from the given indexing facts tree node
// tagged with kFile.
std::string GetFilePathFromRoot(const IndexingFactNode& root) {
  return root.Value().Anchors()[0].Value();
}

// Create the global signature for the given file.
Signature CreateGlobalSignature(absl::string_view file_path) {
  return Signature(file_path);
}

// From the given list of anchors returns the list of Anchor values.
std::vector<absl::string_view> GetListOfReferencesfromListOfAnchor(
    const std::vector<Anchor>& anchors) {
  std::vector<absl::string_view> references;
  references.reserve(anchors.size());
  for (const auto& anchor : anchors) {
    references.push_back(anchor.Value());
  }
  return references;
}

// Returns the list of references from the given anchors list and appends the
// second Anchor to the end of the list.
std::vector<absl::string_view> ConcatenateReferences(
    const std::vector<Anchor>& anchors, const Anchor& anchor) {
  std::vector<absl::string_view> references(
      GetListOfReferencesfromListOfAnchor(anchors));
  references.push_back(anchor.Value());
  return references;
}

}  // namespace

KytheIndexingData KytheFactsExtractor::ExtractKytheFacts(
    const IndexingFactNode& file_list) {
  // Create a new ScopeResolver and give the ownership to the scope_resolvers
  // vector so that it can outlive KytheFactsExtractor.
  // The ScopeResolver-s are created and linked together as a linked-list
  // structure so that the current ScopeResolver can search for definitions in
  // the previous files' scopes.
  std::vector<std::unique_ptr<ScopeResolver>> scope_resolvers;
  scope_resolvers.push_back(std::unique_ptr<ScopeResolver>(nullptr));

  KytheIndexingData aggregated_indexing_facts;
  for (const IndexingFactNode& root : file_list.Children()) {
    std::string file_path = GetFilePathFromRoot(root);
    scope_resolvers.push_back(absl::make_unique<ScopeResolver>(
        CreateGlobalSignature(file_path), scope_resolvers.back().get()));
    KytheFactsExtractor kythe_extractor(file_path,
                                        scope_resolvers.back().get());

    const auto indexing_data = kythe_extractor.ExtractFile(root);
    aggregated_indexing_facts.facts.insert(indexing_data.facts.begin(),
                                           indexing_data.facts.end());
    aggregated_indexing_facts.edges.insert(indexing_data.edges.begin(),
                                           indexing_data.edges.end());
  }

  return aggregated_indexing_facts;
}

KytheIndexingData KytheFactsExtractor::ExtractFile(
    const IndexingFactNode& root) {
  // For every iteration:
  // saves the current number of extracted facts, do another iteration to
  // extract more facts and if new facts were extracted do another iteration and
  // so on.
  std::size_t number_of_extracted_facts = 0;
  do {
    number_of_extracted_facts = facts_.size();
    IndexingFactNodeTagResolver(root);
  } while (number_of_extracted_facts != facts_.size());

  KytheIndexingData indexing_data;
  for (const Fact& fact : facts_) {
    indexing_data.facts.insert(fact);
  }
  for (const Edge& edge : edges_) {
    indexing_data.edges.insert(edge);
  }

  return indexing_data;
}

void KytheFactsExtractor::IndexingFactNodeTagResolver(
    const IndexingFactNode& node) {
  const auto tag = node.Value().GetIndexingFactType();

  // This VName is used to keep track of the new generated VName and it will be
  // used in scopes, finding variable definitions and creating childof
  // relations.
  VName vname;
  switch (tag) {
    case IndexingFactType::kFile: {
      vname = ExtractFileFact(node);
      break;
    }
    case IndexingFactType::kModule: {
      vname = ExtractModuleFact(node);
      break;
    }
    case IndexingFactType::kInterface: {
      vname = ExtractInterfaceFact(node);
      break;
    }
    case IndexingFactType::kProgram: {
      vname = ExtractProgramFact(node);
      break;
    }
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kClassInstance:
    case IndexingFactType::kVariableDefinition: {
      vname = ExtractVariable(node);
      break;
    }
    case IndexingFactType::kConstant: {
      vname = ExtractConstant(node);
      break;
    }
    case IndexingFactType::kMacro: {
      vname = ExtractMacroDefinition(node);
      break;
    }
    case IndexingFactType::kClass: {
      vname = ExtractClass(node);
      break;
    }
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor: {
      vname = ExtractFunctionOrTask(node);
      break;
    }
    case IndexingFactType::kPackage: {
      vname = ExtractPackageDeclaration(node);
      break;
    }
    case IndexingFactType::kStructOrUnion: {
      vname = ExtractStructOrUnion(node);
      break;
    }
    case IndexingFactType::kAnonymousScope: {
      vname = ExtractAnonymousScope(node);
      break;
    }
    case IndexingFactType::kTypeDeclaration: {
      vname = ExtractTypeDeclaration(node);
      break;
    }
    case IndexingFactType::kDataTypeReference: {
      ExtractDataTypeReference(node);
      break;
    }
    case IndexingFactType::kModuleNamedPort: {
      ExtractModuleNamedPort(node);
      break;
    }
    case IndexingFactType::kNamedParam: {
      ExtractNamedParam(node);
      break;
    }
    case IndexingFactType::kExtends: {
      ExtractExtends(node);
      break;
    }
    case IndexingFactType::kVariableReference: {
      ExtractVariableReference(node);
      break;
    }
    case IndexingFactType::kFunctionCall: {
      ExtractFunctionOrTaskCall(node);
      break;
    }
    case IndexingFactType::kPackageImport: {
      ExtractPackageImport(node);
      break;
    }
    case IndexingFactType::kMacroCall: {
      ExtractMacroCall(node);
      break;
    }
    case IndexingFactType::kMemberReference: {
      ExtractMemberReference(node);
      break;
    }
    case IndexingFactType::kInclude: {
      ExtractInclude(node);
      break;
    }
    default: {
      break;
    }
  }

  AddDefinitionToCurrentScope(tag, vname);
  CreateChildOfEdge(tag, vname);
  Visit(node, vname);
}

void KytheFactsExtractor::AddDefinitionToCurrentScope(IndexingFactType tag,
                                                      const VName& vname) {
  switch (tag) {
    case IndexingFactType::kModule:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kMacro:
    case IndexingFactType::kStructOrUnion:
    case IndexingFactType::kClass:
    case IndexingFactType::kClassInstance:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kPackage:
    case IndexingFactType::kConstant:
    case IndexingFactType::kTypeDeclaration:
    case IndexingFactType::kInterface:
    case IndexingFactType::kProgram: {
      scope_resolver_->AddDefinitionToCurrentScope(vname);
      break;
    }
    default: {
      break;
    }
  }
}

void KytheFactsExtractor::CreateChildOfEdge(IndexingFactType tag,
                                            const VName& vname) {
  // Determines whether to create a child of edge to the parent node or not.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kPackageImport:
    case IndexingFactType::kVariableReference:
    case IndexingFactType::kDataTypeReference:
    case IndexingFactType::kMacroCall:
    case IndexingFactType::kFunctionCall:
    case IndexingFactType::kMacro:
    case IndexingFactType::kModuleNamedPort:
    case IndexingFactType::kMemberReference:
    case IndexingFactType::kInclude:
    case IndexingFactType::kAnonymousScope: {
      break;
    }
    default: {
      if (!vnames_context_.empty()) {
        CreateEdge(vname, kEdgeChildOf, vnames_context_.top());
      }
      break;
    }
  }
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node,
                                const VName& vname) {
  Scope current_scope(vname.signature);
  const auto tag = node.Value().GetIndexingFactType();

  // Determines whether to create a scope for this node or not.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kModule:
    case IndexingFactType::kStructOrUnion:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor:
    case IndexingFactType::kClass:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage:
    case IndexingFactType::kInterface:
    case IndexingFactType::kProgram: {
      // Get the old scope of this node (if it was extracted in a previous
      // iteration).
      const Scope* old_scope = scope_resolver_->SearchForScope(vname.signature);
      if (old_scope != nullptr) {
        current_scope.AppendScope(*old_scope);
      }

      Visit(node, vname, current_scope);
      break;
    }
    case IndexingFactType::kAnonymousScope: {
      Visit(node, vname, current_scope);
      break;
    }
    default: {
      Visit(node);
    }
  }

  ConstructScope(node, vname, current_scope);
}

void KytheFactsExtractor::ConstructScope(const IndexingFactNode& node,
                                         const VName& vname,
                                         Scope& current_scope) {
  const auto tag = node.Value().GetIndexingFactType();

  // Determines whether to add the current scope to the scope context or not.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kModule:
    case IndexingFactType::kStructOrUnion:
    case IndexingFactType::kClass:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage:
    case IndexingFactType::kFunctionOrTaskForwardDeclaration:
    case IndexingFactType::kConstructor:
    case IndexingFactType::kInterface:
    case IndexingFactType::kProgram: {
      scope_resolver_->MapSignatureToScope(vname.signature, current_scope);
      break;
    }
    case IndexingFactType::kVariableDefinition: {
      // Break if this variable has no type.
      if (node.Parent() == nullptr ||
          node.Parent()->Value().GetIndexingFactType() !=
              IndexingFactType::kDataTypeReference) {
        scope_resolver_->MapSignatureToScope(vname.signature, current_scope);
        break;
      }

      // TODO(minatoma): refactor this and the below case into function.
      // TODO(minatoma): move this case to below and make variable definitions
      // scope-less.
      // TODO(minatoma): use kAnonymousType and kAnonymousTypeReference to git
      // rid of this case (if possible).
      // TODO(minatoma): consider getting rid of kModuleInstance and
      // kClassInstance and use kVariableDefinition if they don't provide
      // anything new.
      const auto& parent_anchors = node.Parent()->Value().Anchors();
      const std::vector<std::pair<const VName*, const Scope*>> definitions =
          scope_resolver_->SearchForDefinitions(
              GetListOfReferencesfromListOfAnchor(parent_anchors));

      if (!definitions.empty() && definitions.size() == parent_anchors.size() &&
          definitions.back().second != nullptr) {
        current_scope.AppendScope(*definitions.back().second);
      }

      scope_resolver_->MapSignatureToScope(vname.signature, current_scope);

      break;
    }
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kClassInstance: {
      if (node.Parent() == nullptr ||
          node.Parent()->Value().GetIndexingFactType() !=
              IndexingFactType::kDataTypeReference) {
        break;
      }

      // Find the scope of the parent data type and append the members of it to
      // the scope of the current instance.
      const auto& parent_anchors = node.Parent()->Value().Anchors();
      const std::vector<std::pair<const VName*, const Scope*>> definitions =
          scope_resolver_->SearchForDefinitions(
              GetListOfReferencesfromListOfAnchor(parent_anchors));

      if (definitions.empty() || definitions.size() != parent_anchors.size() ||
          definitions.back().second == nullptr) {
        break;
      }

      scope_resolver_->MapSignatureToScope(vname.signature,
                                           *definitions.back().second);
      break;
    }
    default: {
      break;
    }
  }
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node,
                                const VName& vname, Scope& current_scope) {
  const VNameContext::AutoPop vnames_auto_pop(&vnames_context_, &vname);
  const ScopeContext::AutoPop scope_auto_pop(
      &scope_resolver_->GetMutableScopeContext(), &current_scope);
  Visit(node);
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node) {
  for (const IndexingFactNode& child : node.Children()) {
    IndexingFactNodeTagResolver(child);
  }
}

VName KytheFactsExtractor::ExtractFileFact(
    const IndexingFactNode& file_fact_node) {
  VName file_vname(file_path_, Signature(""), "", "");
  const std::string& code_text = file_fact_node.Value().Anchors()[1].Value();

  CreateFact(file_vname, kFactNodeKind, kNodeFile);
  CreateFact(file_vname, kFactText, code_text);

  // Update the signature of the file to be the global signature.
  // Used in scopes and makes signatures unique.
  file_vname.signature = CreateGlobalSignature(file_path_);
  return file_vname;
}

VName KytheFactsExtractor::ExtractModuleFact(
    const IndexingFactNode& module_fact_node) {
  const auto& anchors = module_fact_node.Value().Anchors();
  const Anchor& module_name = anchors[0];

  const VName module_vname(file_path_,
                           CreateScopeRelativeSignature(module_name.Value()));
  const VName module_name_anchor = CreateAnchor(module_name);

  CreateFact(module_vname, kFactNodeKind, kNodeRecord);
  CreateFact(module_vname, kFactSubkind, kSubkindModule);
  CreateFact(module_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (anchors.size() > 1) {
    const Anchor& module_end_label = anchors[1];
    const VName module_end_label_anchor = CreateAnchor(module_end_label);
    CreateEdge(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

VName KytheFactsExtractor::ExtractProgramFact(
    const IndexingFactNode& program_fact_node) {
  const auto& anchors = program_fact_node.Value().Anchors();
  const Anchor& program_name = anchors[0];

  const VName program_vname(file_path_,
                            CreateScopeRelativeSignature(program_name.Value()));
  const VName program_name_anchor = CreateAnchor(program_name);

  CreateFact(program_vname, kFactNodeKind, kNodeRecord);
  CreateFact(program_vname, kFactSubkind, kSubkindProgram);
  CreateEdge(program_name_anchor, kEdgeDefinesBinding, program_vname);

  if (anchors.size() > 1) {
    const Anchor& program_end_label = anchors[1];
    const VName program_end_label_anchor = CreateAnchor(program_end_label);
    CreateEdge(program_end_label_anchor, kEdgeRef, program_vname);
  }

  return program_vname;
}

VName KytheFactsExtractor::ExtractInterfaceFact(
    const IndexingFactNode& interface_fact_node) {
  const auto& anchors = interface_fact_node.Value().Anchors();
  const Anchor& interface_name = anchors[0];

  const VName interface_vname(
      file_path_, CreateScopeRelativeSignature(interface_name.Value()));
  const VName interface_name_anchor = CreateAnchor(interface_name);

  CreateFact(interface_vname, kFactNodeKind, kNodeInterface);
  CreateEdge(interface_name_anchor, kEdgeDefinesBinding, interface_vname);

  if (anchors.size() > 1) {
    const Anchor& interface_end_label = anchors[1];
    const VName interface_end_label_anchor = CreateAnchor(interface_end_label);
    CreateEdge(interface_end_label_anchor, kEdgeRef, interface_vname);
  }

  return interface_vname;
}

void KytheFactsExtractor::ExtractDataTypeReference(
    const IndexingFactNode& data_type_reference) {
  const auto& anchors = data_type_reference.Value().Anchors();

  const std::vector<std::pair<const VName*, const Scope*>> type_vnames =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, type_vnames);
}

VName KytheFactsExtractor::ExtractTypeDeclaration(
    const IndexingFactNode& type_declaration) {
  const auto& anchor = type_declaration.Value().Anchors()[0];
  const VName type_vname(file_path_,
                         CreateScopeRelativeSignature(anchor.Value()));
  const VName type_vname_anchor = CreateAnchor(anchor);

  CreateFact(type_vname, kFactNodeKind, kNodeTAlias);
  CreateEdge(type_vname_anchor, kEdgeDefinesBinding, type_vname);

  return type_vname;
}

void KytheFactsExtractor::ExtractNamedParam(
    const IndexingFactNode& named_param_node) {
  // Get the anchors.
  const auto& param_name = named_param_node.Value().Anchors()[0];

  // Search for the module or class that contains this parameter.
  // Parent Node must be kDataTypeReference or kMemberReference or kExtends.
  const std::vector<Anchor>& parent_data_type =
      named_param_node.Parent()->Value().Anchors();

  // Search inside the found module or class for the referenced parameter.
  const std::vector<std::pair<const VName*, const Scope*>> param_vnames =
      scope_resolver_->SearchForDefinitions(
          ConcatenateReferences(parent_data_type, param_name));

  // Check if all the references are found.
  if (param_vnames.size() != parent_data_type.size() + 1) {
    return;
  }

  // Create the facts for this parameter reference.
  const VName param_vname_anchor = CreateAnchor(param_name);
  CreateEdge(param_vname_anchor, kEdgeRef, *param_vnames.back().first);
}

void KytheFactsExtractor::ExtractModuleNamedPort(
    const IndexingFactNode& named_port_node) {
  const auto& port_name = named_port_node.Value().Anchors()[0];

  // Parent Node must be kModuleInstance and the grand parent node must be
  // kDataTypeReference.
  const std::vector<Anchor>& module_type =
      named_port_node.Parent()->Parent()->Value().Anchors();

  const std::vector<std::pair<const VName*, const Scope*>> actual_port_vnames =
      scope_resolver_->SearchForDefinitions(
          ConcatenateReferences(module_type, port_name));

  // Check if all the references are found.
  if (actual_port_vnames.size() != module_type.size() + 1) {
    return;
  }

  const VName port_vname_anchor = CreateAnchor(port_name);
  CreateEdge(port_vname_anchor, kEdgeRef, *actual_port_vnames.back().first);

  if (named_port_node.is_leaf()) {
    const std::vector<std::pair<const VName*, const Scope*>> definition_vnames =
        scope_resolver_->SearchForDefinitions({port_name.Value()});

    if (!definition_vnames.empty()) {
      CreateEdge(port_vname_anchor, kEdgeRef, *definition_vnames[0].first);
    }
  }
}

VName KytheFactsExtractor::ExtractVariable(
    const IndexingFactNode& variable_definition_node) {
  const auto& anchors = variable_definition_node.Value().Anchors();
  CHECK(!anchors.empty());
  const Anchor& anchor = anchors[0];
  const VName variable_vname(file_path_,
                             CreateScopeRelativeSignature(anchor.Value()));
  const VName variable_vname_anchor = CreateAnchor(anchor);

  CreateFact(variable_vname, kFactNodeKind, kNodeVariable);
  CreateFact(variable_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(variable_vname_anchor, kEdgeDefinesBinding, variable_vname);

  return variable_vname;
}

void KytheFactsExtractor::ExtractVariableReference(
    const IndexingFactNode& variable_reference_node) {
  const auto& anchors = variable_reference_node.Value().Anchors();

  const std::vector<std::pair<const VName*, const Scope*>>
      variable_definition_vnames = scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, variable_definition_vnames);
}

VName KytheFactsExtractor::ExtractPackageDeclaration(
    const IndexingFactNode& package_declaration_node) {
  const auto& anchors = package_declaration_node.Value().Anchors();
  const Anchor& package_name = anchors[0];

  const VName package_vname(file_path_,
                            CreateScopeRelativeSignature(package_name.Value()));
  const VName package_name_anchor = CreateAnchor(package_name);

  CreateFact(package_vname, kFactNodeKind, kNodePackage);
  CreateEdge(package_name_anchor, kEdgeDefinesBinding, package_vname);

  if (anchors.size() > 1) {
    const Anchor& package_end_label = anchors[1];
    const VName package_end_label_anchor = CreateAnchor(package_end_label);
    CreateEdge(package_end_label_anchor, kEdgeRef, package_vname);
  }

  return package_vname;
}

VName KytheFactsExtractor::ExtractMacroDefinition(
    const IndexingFactNode& macro_definition_node) {
  const Anchor& macro_name = macro_definition_node.Value().Anchors()[0];

  // The signature is relative to the global scope so no relative signature
  // created here.
  const VName macro_vname(file_path_, Signature(macro_name.Value()));
  const VName module_name_anchor = CreateAnchor(macro_name);

  CreateFact(macro_vname, kFactNodeKind, kNodeMacro);
  CreateEdge(module_name_anchor, kEdgeDefinesBinding, macro_vname);

  return macro_vname;
}

void KytheFactsExtractor::ExtractMacroCall(
    const IndexingFactNode& macro_call_node) {
  const Anchor& macro_name = macro_call_node.Value().Anchors()[0];
  const VName macro_vname_anchor = CreateAnchor(macro_name);

  // The signature is relative to the global scope so no relative signature
  // created here.
  const VName variable_definition_vname(file_path_,
                                        Signature(macro_name.Value()));

  CreateEdge(macro_vname_anchor, kEdgeRefExpands, variable_definition_vname);
}

VName KytheFactsExtractor::ExtractFunctionOrTask(
    const IndexingFactNode& function_fact_node) {
  const auto& function_name = function_fact_node.Value().Anchors()[0];

  const VName function_vname(
      file_path_, CreateScopeRelativeSignature(function_name.Value()));

  const VName function_vname_anchor = CreateAnchor(function_name);

  CreateFact(function_vname, kFactNodeKind, kNodeFunction);
  CreateEdge(function_vname_anchor, kEdgeDefinesBinding, function_vname);

  auto tag = function_fact_node.Value().GetIndexingFactType();
  switch (tag) {
    case IndexingFactType::kFunctionOrTask: {
      CreateFact(function_vname, kFactComplete, kCompleteDefinition);
      break;
    }
    case IndexingFactType::kFunctionOrTaskForwardDeclaration: {
      CreateFact(function_vname, kFactComplete, kInComplete);
      break;
    }
    case IndexingFactType::kConstructor: {
      CreateFact(function_vname, kFactSubkind, kSubkindConstructor);
      break;
    }
    default: {
      break;
    }
  }

  // Check if there is a function with the same name in the current scope and if
  // exists output "overrides" edge.
  const VName* overridden_function_vname =
      scope_resolver_->SearchForDefinitionInCurrentScope(function_name.Value());

  // TODO(minatoma): add a check to output this edge only if the parent is class
  // or interface.
  // TODO(minatoma): add a function like SyntaxTreeNode::MatchesTagAnyOf to
  // IndexingFactsTree.
  if (overridden_function_vname != nullptr) {
    CreateEdge(function_vname, kEdgeOverrides, *overridden_function_vname);

    // Delete the overriden base class function from the current scope so that
    // any reference would reference the current function and not the function
    // in the base class.
    scope_resolver_->RemoveDefinitionFromCurrentScope(
        *overridden_function_vname);
  }

  return function_vname;
}

void KytheFactsExtractor::ExtractFunctionOrTaskCall(
    const IndexingFactNode& function_call_fact_node) {
  const auto& anchors = function_call_fact_node.Value().Anchors();

  // Search for member hierarchy in the scopes.
  const std::vector<std::pair<const VName*, const Scope*>> definitions =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, definitions);

  // creating ref/call edge.
  // If the sizes aren't equal that means we couldn't find the function
  // defintion.
  if (!definitions.empty() && definitions.size() == anchors.size()) {
    const VName current_anchor_vname = CreateAnchor(anchors.back());
    CreateEdge(current_anchor_vname, kEdgeRefCall, *definitions.back().first);
  }
}

VName KytheFactsExtractor::ExtractClass(
    const IndexingFactNode& class_fact_node) {
  const auto& anchors = class_fact_node.Value().Anchors();
  const Anchor& class_name = anchors[0];

  const VName class_vname(file_path_,
                          CreateScopeRelativeSignature(class_name.Value()));
  const VName class_name_anchor = CreateAnchor(class_name);

  CreateFact(class_vname, kFactNodeKind, kNodeRecord);
  CreateFact(class_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(class_name_anchor, kEdgeDefinesBinding, class_vname);

  if (anchors.size() > 1) {
    const Anchor& class_end_label = anchors[1];
    const VName class_end_label_anchor = CreateAnchor(class_end_label);
    CreateEdge(class_end_label_anchor, kEdgeRef, class_vname);
  }

  return class_vname;
}

void KytheFactsExtractor::ExtractExtends(const IndexingFactNode& extends_node) {
  const auto& anchors = extends_node.Value().Anchors();

  // Search for member hierarchy in the scopes.
  const std::vector<std::pair<const VName*, const Scope*>> definitions =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, definitions);

  // Check if all the definitions were found.
  if (definitions.size() != anchors.size() || definitions.empty()) {
    return;
  }

  // Create kythe facts for extends.
  const VName& derived_class_vname = vnames_context_.top();
  CreateEdge(derived_class_vname, kEdgeExtends, *definitions.back().first);

  // Append the members of the parent class as members of the current class's
  // scope.
  scope_resolver_->AppendScopeToCurrentScope(*definitions.back().second);
}

void KytheFactsExtractor::ExtractPackageImport(
    const IndexingFactNode& import_fact_node) {
  // TODO(minatoma): remove the imported vnames before exporting the scope as
  // imports aren't intended to be accessible from outside the enclosing parent.
  // Alternatively, maintain separate sets: exported, non-exported, or provide
  // an attribute to distinguish.
  const auto& anchors = import_fact_node.Value().Anchors();
  const Anchor& package_name_anchor = anchors[0];

  // case of import pkg::my_variable.
  if (anchors.size() > 1) {
    const Anchor& imported_item_name = anchors[1];

    // Search for member hierarchy in the scopes.
    const std::vector<std::pair<const VName*, const Scope*>> definition_vnames =
        scope_resolver_->SearchForDefinitions(
            {package_name_anchor.Value(), imported_item_name.Value()});

    // Loop over the found definitions and create kythe facts.
    for (size_t i = 0; i < definition_vnames.size(); i++) {
      const VName current_anchor = CreateAnchor(anchors[i]);
      if (i == 0) {
        CreateEdge(current_anchor, kEdgeRefImports,
                   *definition_vnames[i].first);
      } else {
        CreateEdge(current_anchor, kEdgeRef, *definition_vnames[i].first);
      }
    }

    if (definition_vnames.size() != 2) {
      return;
    }

    // Add the found definition to the current scope as if it was declared in
    // our scope so that it can be captured without "::".
    scope_resolver_->AddDefinitionToCurrentScope(*definition_vnames[1].first);
  } else {
    // case of import pkg::*.
    // Add all the definitions in that package to the current scope as if it was
    // declared in our scope so that it can be captured without "::".

    // Search for member hierarchy in the scopes.
    const std::vector<std::pair<const VName*, const Scope*>> definition_vnames =
        scope_resolver_->SearchForDefinitions({package_name_anchor.Value()});
    if (definition_vnames.empty()) {
      return;
    }

    const VName current_anchor = CreateAnchor(package_name_anchor);
    CreateEdge(current_anchor, kEdgeRefImports, *definition_vnames[0].first);

    scope_resolver_->AddDefinitionToCurrentScope(*definition_vnames[0].first);
    scope_resolver_->AppendScopeToCurrentScope(*definition_vnames[0].second);
  }
}

void KytheFactsExtractor::ExtractMemberReference(
    const IndexingFactNode& member_reference_node) {
  // TODO(fangism): [algorithm] For member references like "A::B::C::D",
  // we currently construct member reference chains "A", "A,B", "A,B,C"...
  // which is O(N^2), so "A" is being looked-up repeatedly, the result of
  // previous lookups is not being re-used.  Re-structure and fix this.

  const auto& anchors = member_reference_node.Value().Anchors();

  // Search for member hierarchy in the scopes.
  const std::vector<std::pair<const VName*, const Scope*>> definitions =
      scope_resolver_->SearchForDefinitions(
          GetListOfReferencesfromListOfAnchor(anchors));

  CreateAnchorReferences(anchors, definitions);
}

void KytheFactsExtractor::ExtractInclude(const IndexingFactNode& include_node) {
  const auto& anchors = include_node.Value().Anchors();
  const Anchor& file_name = anchors[0];
  const Anchor& file_path = anchors[1];

  const VName file_vname(file_path.Value(), Signature(""), "", "");
  const VName file_anchor = CreateAnchor(file_name);

  CreateEdge(file_anchor, kEdgeRefIncludes, file_vname);

  const Scope* included_file_scope =
      scope_resolver_->SearchForScope(Signature(file_path.Value()));
  if (included_file_scope == nullptr) {
    LOG(INFO) << "File Scope Not Found For file: " << file_path.Value();
    return;
  }

  // Create child of edge between the parent and the member of the included
  // file.
  for (const ScopeMemberItem& member : included_file_scope->Members()) {
    CreateEdge(member.vname, kEdgeChildOf, vnames_context_.top());
  }

  // Append the scope of the included file to the current scope.
  scope_resolver_->AppendScopeToCurrentScope(*included_file_scope);
}

VName KytheFactsExtractor::ExtractAnonymousScope(
    const IndexingFactNode& temp_scope) {
  const auto& scope_id = temp_scope.Value().Anchors()[0];
  return VName(file_path_, CreateScopeRelativeSignature(scope_id.Value()));
}

VName KytheFactsExtractor::ExtractConstant(const IndexingFactNode& constant) {
  const auto& anchor = constant.Value().Anchors()[0];
  const VName constant_vname(file_path_,
                             CreateScopeRelativeSignature(anchor.Value()));
  const VName variable_vname_anchor = CreateAnchor(anchor);

  CreateFact(constant_vname, kFactNodeKind, kNodeConstant);
  CreateEdge(variable_vname_anchor, kEdgeDefinesBinding, constant_vname);

  return constant_vname;
}

VName KytheFactsExtractor::ExtractStructOrUnion(
    const IndexingFactNode& struct_node) {
  const auto& anchors = struct_node.Value().Anchors();
  const Anchor& struct_name = anchors[0];

  const VName struct_vname(file_path_,
                           CreateScopeRelativeSignature(struct_name.Value()));
  const VName struct_name_anchor = CreateAnchor(struct_name);

  CreateFact(struct_vname, kFactNodeKind, kNodeRecord);
  CreateEdge(struct_name_anchor, kEdgeDefinesBinding, struct_vname);

  return struct_vname;
}

void KytheFactsExtractor::CreateAnchorReferences(
    const std::vector<Anchor>& anchors,
    const std::vector<std::pair<const VName*, const Scope*>>& definitions) {
  // Loop over the definitions and create kythe facts.
  for (size_t i = 0; i < definitions.size(); i++) {
    const VName current_anchor_vname = CreateAnchor(anchors[i]);
    CreateEdge(current_anchor_vname, kEdgeRef, *definitions[i].first);
  }
}

VName KytheFactsExtractor::CreateAnchor(const Anchor& anchor) {
  const VName anchor_vname(
      file_path_, Signature(absl::StrCat("@", anchor.StartLocation(), ":",
                                         anchor.EndLocation())));

  CreateFact(anchor_vname, kFactNodeKind, kNodeAnchor);
  CreateFact(anchor_vname, kFactAnchorStart,
             absl::StrCat(anchor.StartLocation()));
  CreateFact(anchor_vname, kFactAnchorEnd, absl::StrCat(anchor.EndLocation()));

  return anchor_vname;
}

Signature KytheFactsExtractor::CreateScopeRelativeSignature(
    absl::string_view signature) const {
  // Append the given signature to the signature of the parent.
  return Signature(vnames_context_.top().signature, signature);
}

void KytheFactsExtractor::CreateFact(const VName& vname,
                                     absl::string_view fact_name,
                                     absl::string_view fact_value) {
  facts_.insert(Fact(vname, fact_name, fact_value));
}

void KytheFactsExtractor::CreateEdge(const VName& source_node,
                                     absl::string_view edge_name,
                                     const VName& target_node) {
  edges_.insert(Edge(source_node, edge_name, target_node));
}

std::ostream& KytheFactsPrinter::Print(std::ostream& stream) const {
  const auto indexing_data =
      KytheFactsExtractor::ExtractKytheFacts(file_list_facts_tree_);

  for (const Fact& fact : indexing_data.facts) {
    fact.FormatJSON(stream, debug_) << std::endl;
  }
  for (const Edge& edge : indexing_data.edges) {
    edge.FormatJSON(stream, debug_) << std::endl;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const KytheFactsPrinter& kythe_facts_printer) {
  kythe_facts_printer.Print(stream);
  return stream;
}

}  // namespace kythe
}  // namespace verilog
