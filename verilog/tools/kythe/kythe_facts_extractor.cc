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

#include "absl/strings/escaping.h"
#include "absl/strings/substitute.h"
#include "verilog/tools/kythe/kythe_schema_constants.h"
#include "verilog/tools/kythe/scope_resolver.h"

namespace verilog {
namespace kythe {

void KytheFactsExtractor::ExtractKytheFacts(const IndexingFactNode& root) {
  // For every iteration:
  // saves the current number of extracted facts, do another iteration to
  // extract more facts and if new facts were extracted do another iteration and
  // so on.
  std::size_t number_of_extracted_facts = 0;
  do {
    number_of_extracted_facts = facts_.size();
    IndexingFactNodeTagResolver(root);
  } while (number_of_extracted_facts != facts_.size());
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
    case IndexingFactType::kModuleInstance: {
      vname = ExtractModuleInstance(node);
      break;
    }
    case IndexingFactType::kVariableDefinition: {
      vname = ExtractVariableDefinition(node);
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
    case IndexingFactType::kParamDeclaration: {
      vname = ExtractParamDeclaration(node);
      break;
    }
    case IndexingFactType::kClassInstance: {
      vname = ExtractClassInstances(node);
      break;
    }
    case IndexingFactType::kFunctionOrTask: {
      vname = ExtractFunctionOrTask(node);
      break;
    }
    case IndexingFactType::kPackage: {
      vname = ExtractPackageDeclaration(node);
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
      ExtractMemberReference(node, false);
      break;
    }
    default: {
      break;
    }
  }

  AddVNameToVerticalScope(tag, vname);
  CreateChildOfEdge(tag, vname);
  Visit(node, vname);
}

void KytheFactsExtractor::AddVNameToVerticalScope(IndexingFactType tag,
                                                  const VName& vname) {
  switch (tag) {
    case IndexingFactType::kModule:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kMacro:
    case IndexingFactType::kClass:
    case IndexingFactType::kClassInstance:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kParamDeclaration:
    case IndexingFactType::kPackage: {
      scope_resolver_->AddDefinitionToScopeContext(vname);
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
    case IndexingFactType::kMemberReference: {
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
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kClass:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage: {
      // Get the old scope of this node (if it was extracted in a previous
      // iteration).
      const Scope* old_scope = scope_resolver_->SearchForScope(vname.signature);
      if (old_scope != nullptr) {
        current_scope.AppendScope(*old_scope);
      }

      Visit(node, vname, current_scope);
      break;
    }
    default: {
      Visit(node);
    }
  }

  ConstructFlattenedScope(node, vname, current_scope);
}

void KytheFactsExtractor::ConstructFlattenedScope(const IndexingFactNode& node,
                                                  const VName& vname,
                                                  const Scope& current_scope) {
  const auto tag = node.Value().GetIndexingFactType();

  // Determines whether to add the current scope to the scope context or not.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kModule:
    case IndexingFactType::kClass:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage: {
      scope_resolver_->MapSignatureToScope(vname.signature, current_scope);
      break;
    }
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kClassInstance: {
      // TODO(minatoma): fix this in case the name was kQualified id.
      const std::vector<const VName*> found_vnames =
          scope_resolver_->SearchForDefinitions(
              {node.Parent()->Value().Anchors()[0].Value()});

      if (found_vnames.empty()) {
        break;
      }

      scope_resolver_->MapSignatureToScopeOfSignature(
          vname.signature, found_vnames[0]->signature);

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
  const VName file_vname(file_path_, Signature(""), "", "");
  const std::string& code_text = file_fact_node.Value().Anchors()[1].Value();

  CreateFact(file_vname, kFactNodeKind, kNodeFile);
  CreateFact(file_vname, kFactText, code_text);

  return file_vname;
}

VName KytheFactsExtractor::ExtractModuleFact(
    const IndexingFactNode& module_fact_node) {
  const auto& anchors = module_fact_node.Value().Anchors();
  const Anchor& module_name = anchors[0];
  const Anchor& module_end_label = anchors[1];

  const VName module_vname(file_path_,
                           CreateScopeRelativeSignature(module_name.Value()));
  const VName module_name_anchor = CreateAnchor(module_name);

  CreateFact(module_vname, kFactNodeKind, kNodeRecord);
  CreateFact(module_vname, kFactSubkind, kSubkindModule);
  CreateFact(module_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (anchors.size() > 1) {
    const VName module_end_label_anchor = CreateAnchor(module_end_label);
    CreateEdge(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

void KytheFactsExtractor::ExtractDataTypeReference(
    const IndexingFactNode& data_type_reference) {
  const auto& anchors = data_type_reference.Value().Anchors();
  const Anchor& type = anchors[0];

  const std::vector<const VName*> type_vnames =
      scope_resolver_->SearchForDefinitions({type.Value()});

  if (type_vnames.empty()) {
    return;
  }

  const VName type_anchor = CreateAnchor(type);
  CreateEdge(type_anchor, kEdgeRef, *type_vnames[0]);
}

VName KytheFactsExtractor::ExtractModuleInstance(
    const IndexingFactNode& module_instance_node) {
  const auto& anchors = module_instance_node.Value().Anchors();
  const Anchor& instance_name = anchors[0];

  const VName module_instance_vname(
      file_path_, CreateScopeRelativeSignature(instance_name.Value()));

  const VName module_instance_anchor = CreateAnchor(instance_name);
  CreateFact(module_instance_vname, kFactNodeKind, kNodeVariable);
  CreateFact(module_instance_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(module_instance_anchor, kEdgeDefinesBinding,
             module_instance_vname);

  return module_instance_vname;
}

void KytheFactsExtractor::ExtractNamedParam(
    const IndexingFactNode& named_param_node) {
  // Get the anchors.
  const auto& param_name = named_param_node.Value().Anchors()[0];

  // Search for the module or class that contains this parameter.
  // Parent Node must be kDataTypeReference.
  const Anchor& parent_data_type =
      named_param_node.Parent()->Value().Anchors()[0];

  // Search inside the found module or class for the referenced parameter.
  const std::vector<const VName*> param_vnames =
      scope_resolver_->SearchForDefinitions(
          {parent_data_type.Value(), param_name.Value()});

  if (param_vnames.size() != 2) {
    return;
  }

  // Create the facts for this parameter reference.
  const VName param_vname_anchor = CreateAnchor(param_name);
  CreateEdge(param_vname_anchor, kEdgeRef, *param_vnames[1]);
}

void KytheFactsExtractor::ExtractModuleNamedPort(
    const IndexingFactNode& named_port_node) {
  const auto& port_name = named_port_node.Value().Anchors()[0];

  // Parent Node must be kModuleInstance and the grand parent node must be
  // kDataTypeReference.
  const Anchor& module_type =
      named_port_node.Parent()->Parent()->Value().Anchors()[0];

  const std::vector<const VName*> actual_port_vnames =
      scope_resolver_->SearchForDefinitions(
          {module_type.Value(), port_name.Value()});

  if (actual_port_vnames.size() != 2) {
    return;
  }

  const VName port_vname_anchor = CreateAnchor(port_name);
  CreateEdge(port_vname_anchor, kEdgeRef, *actual_port_vnames[1]);

  if (named_port_node.is_leaf()) {
    const std::vector<const VName*> definition_vnames =
        scope_resolver_->SearchForDefinitions({port_name.Value()});

    if (!definition_vnames.empty()) {
      CreateEdge(port_vname_anchor, kEdgeRef, *definition_vnames[0]);
    }
  }
}

VName KytheFactsExtractor::ExtractVariableDefinition(
    const IndexingFactNode& variable_definition_node) {
  const auto& anchor = variable_definition_node.Value().Anchors()[0];
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
  const auto& anchor = variable_reference_node.Value().Anchors()[0];

  const std::vector<const VName*> variable_definition_vnames =
      scope_resolver_->SearchForDefinitions({anchor.Value()});
  if (variable_definition_vnames.empty()) {
    return;
  }

  const VName variable_vname_anchor = CreateAnchor(anchor);
  CreateEdge(variable_vname_anchor, kEdgeRef, *variable_definition_vnames[0]);
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

  // We pass a substring to ignore the ` before macro name.
  // e.g.
  // `define TEN 0
  // `TEN --> removes the `
  const VName variable_definition_vname(
      file_path_, Signature(macro_name.Value().substr(1)));

  CreateEdge(macro_vname_anchor, kEdgeRefExpands, variable_definition_vname);
}

VName KytheFactsExtractor::ExtractFunctionOrTask(
    const IndexingFactNode& function_fact_node) {
  const auto& function_name = function_fact_node.Value().Anchors()[0];

  const VName function_vname(
      file_path_, CreateScopeRelativeSignature(function_name.Value()));

  const VName function_vname_anchor = CreateAnchor(function_name);

  CreateFact(function_vname, kFactNodeKind, kNodeFunction);
  CreateFact(function_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(function_vname_anchor, kEdgeDefinesBinding, function_vname);

  return function_vname;
}

void KytheFactsExtractor::ExtractFunctionOrTaskCall(
    const IndexingFactNode& function_call_fact_node) {
  const auto& anchors = function_call_fact_node.Value().Anchors();

  // In case function_name();
  if (anchors.size() == 1) {
    const auto& function_name = anchors[0];

    const std::vector<const VName*> function_vnames =
        scope_resolver_->SearchForDefinitions({function_name.Value()});

    if (function_vnames.empty()) {
      return;
    }

    const VName function_vname_anchor = CreateAnchor(function_name);

    CreateEdge(function_vname_anchor, kEdgeRef, *function_vnames[0]);
    CreateEdge(function_vname_anchor, kEdgeRefCall, *function_vnames[0]);
  } else {
    // In case pkg::class1::function_name().
    IndexingNodeData member_reference_data(IndexingFactType::kMemberReference);
    for (const Anchor& anchor : anchors) {
      member_reference_data.AppendAnchor(
          Anchor(anchor.Value(), anchor.StartLocation(), anchor.EndLocation()));
    }
    ExtractMemberReference(IndexingFactNode(member_reference_data), true);
  }
}

VName KytheFactsExtractor::ExtractClass(
    const IndexingFactNode& class_fact_node) {
  const auto& anchors = class_fact_node.Value().Anchors();
  const Anchor& class_name = anchors[0];
  const Anchor& class_end_label = anchors[1];

  const VName class_vname(file_path_,
                          CreateScopeRelativeSignature(class_name.Value()));
  const VName class_name_anchor = CreateAnchor(class_name);

  CreateFact(class_vname, kFactNodeKind, kNodeRecord);
  CreateFact(class_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(class_name_anchor, kEdgeDefinesBinding, class_vname);

  if (anchors.size() > 1) {
    const VName class_end_label_anchor = CreateAnchor(class_end_label);
    CreateEdge(class_end_label_anchor, kEdgeRef, class_vname);
  }

  return class_vname;
}

VName KytheFactsExtractor::ExtractClassInstances(
    const IndexingFactNode& class_instance_fact_node) {
  const auto& anchors = class_instance_fact_node.Value().Anchors();
  const Anchor& instance_name = anchors[0];

  const VName class_instance_vname(
      file_path_, CreateScopeRelativeSignature(instance_name.Value()));
  const VName class_instance_anchor = CreateAnchor(instance_name);

  CreateFact(class_instance_vname, kFactNodeKind, kNodeVariable);
  CreateFact(class_instance_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(class_instance_anchor, kEdgeDefinesBinding, class_instance_vname);

  return class_instance_vname;
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
    const std::vector<const VName*> definition_vnames =
        scope_resolver_->SearchForDefinitions(
            {package_name_anchor.Value(), imported_item_name.Value()});

    // Loop over the found definitions and create kythe facts.
    for (size_t i = 0; i < definition_vnames.size(); i++) {
      const VName current_anchor = CreateAnchor(anchors[i]);
      if (i == 0) {
        CreateEdge(current_anchor, kEdgeRefImports, *definition_vnames[i]);
      } else {
        CreateEdge(current_anchor, kEdgeRef, *definition_vnames[i]);
      }
    }

    if (definition_vnames.size() != 2) {
      return;
    }

    // Add the found definition to the current scope as if it was declared in
    // our scope so that it can be captured without "::".
    scope_resolver_->AddDefinitionToScopeContext(*definition_vnames[1]);
  } else {
    // case of import pkg::*.
    // Add all the definitions in that package to the current scope as if it was
    // declared in our scope so that it can be captured without "::".

    // Search for member hierarchy in the scopes.
    const std::vector<const VName*> definition_vnames =
        scope_resolver_->SearchForDefinitions({package_name_anchor.Value()});
    if (definition_vnames.empty()) {
      return;
    }

    const VName current_anchor = CreateAnchor(package_name_anchor);
    CreateEdge(current_anchor, kEdgeRefImports, *definition_vnames[0]);

    const Scope* current_package_scope =
        scope_resolver_->SearchForScope(definition_vnames[0]->signature);
    if (current_package_scope == nullptr) {
      return;
    }

    scope_resolver_->AddDefinitionToScopeContext(*definition_vnames[0]);
    scope_resolver_->AppendScopeToScopeContext(*current_package_scope);
  }
}

void KytheFactsExtractor::ExtractMemberReference(
    const IndexingFactNode& member_reference_node, bool is_function_call) {
  const auto& anchors = member_reference_node.Value().Anchors();

  // Extract the list of reference_names.
  std::vector<std::string> references_names;
  references_names.reserve(anchors.size());
  for (const Anchor& anchor : anchors) {
    references_names.push_back(anchor.Value());
  }

  // Search for member hierarchy in the scopes.
  const std::vector<const VName*> definitions =
      scope_resolver_->SearchForDefinitions(references_names);

  // Loop over the found definitions and create kythe facts.
  for (size_t i = 0; i < definitions.size(); i++) {
    const VName current_anchor_vname = CreateAnchor(anchors[i]);
    CreateEdge(current_anchor_vname, kEdgeRef, *definitions[i]);
  }

  // Checking if we found all the member heirarchy by ensuring the size of the
  // found definitions is equal to the size of the given anchors and then
  // creating ref/call edge if it was a function call.
  if (definitions.size() == anchors.size() && is_function_call) {
    const VName current_anchor_vname = CreateAnchor(anchors.back());
    CreateEdge(current_anchor_vname, kEdgeRefCall, *definitions.back());
  }
}

VName KytheFactsExtractor::ExtractParamDeclaration(
    const IndexingFactNode& param_declaration_node) {
  // Get the anchors and the parameter name.
  const auto& anchors = param_declaration_node.Value().Anchors();
  const Anchor& param_name = anchors[0];

  // Create the VName for this variable relative to the current scope.
  const VName param_vname(file_path_,
                          CreateScopeRelativeSignature(param_name.Value()));

  // Create the facts for the parameter.
  const VName param_name_anchor = CreateAnchor(param_name);
  CreateFact(param_vname, kFactNodeKind, kNodeVariable);
  CreateFact(param_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(param_name_anchor, kEdgeDefinesBinding, param_vname);

  return param_vname;
}

VName KytheFactsExtractor::CreateAnchor(const Anchor& anchor) {
  const VName anchor_vname(file_path_, Signature(absl::Substitute(
                                           R"(@$0:$1)", anchor.StartLocation(),
                                           anchor.EndLocation())));

  CreateFact(anchor_vname, kFactNodeKind, kNodeAnchor);
  CreateFact(anchor_vname, kFactAnchorStart,
             absl::Substitute(R"($0)", anchor.StartLocation()));
  CreateFact(anchor_vname, kFactAnchorEnd,
             absl::Substitute(R"($0)", anchor.EndLocation()));

  return anchor_vname;
}

Signature KytheFactsExtractor::CreateScopeRelativeSignature(
    absl::string_view signature, const Signature& parent_signature) const {
  return Signature(parent_signature, signature);
}

Signature KytheFactsExtractor::CreateScopeRelativeSignature(
    absl::string_view signature) const {
  return vnames_context_.empty()
             ? Signature(signature)
             : CreateScopeRelativeSignature(signature,
                                            vnames_context_.top().signature);
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

const std::set<Fact>& KytheFactsExtractor::GetExtractedFacts() const {
  return facts_;
}

const std::set<Edge>& KytheFactsExtractor::GetExtractedEdges() const {
  return edges_;
}

std::string GetFilePathFromRoot(const IndexingFactNode& root) {
  return root.Value().Anchors()[0].Value();
}

std::ostream& KytheFactsPrinter::Print(std::ostream& stream) const {
  std::vector<ScopeResolver> scope_resolvers;

  for (const IndexingFactNode& root : trees_) {
    // Create a new ScopeResolver and give the ownership to the scope_resolvers
    // vector so that it can outlive KytheFactsExtractor.
    // The ScopeResolver-s are created and linked together as a linked-list
    // structure so that the current ScopeResolver can search for definitions in
    // the previous files' scopes.
    if (!scope_resolvers.empty()) {
      scope_resolvers.push_back(ScopeResolver(scope_resolvers.back()));
    } else {
      scope_resolvers.push_back(ScopeResolver(nullptr));
    }

    KytheFactsExtractor kythe_extractor(GetFilePathFromRoot(root),
                                        &scope_resolvers.back());
    kythe_extractor.ExtractKytheFacts(root);
    for (const Fact& fact : kythe_extractor.GetExtractedFacts()) {
      stream << fact;
    }
    for (const Edge& edge : kythe_extractor.GetExtractedEdges()) {
      stream << edge;
    }
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
