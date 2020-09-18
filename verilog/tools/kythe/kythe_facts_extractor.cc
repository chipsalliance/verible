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

namespace verilog {
namespace kythe {

std::string CreateSignature(absl::string_view name) {
  return absl::StrCat(name, "#");
}

void KytheFactsExtractor::ExtractKytheFacts(const IndexingFactNode& root) {
  CreatePackageScopes(root);
  IndexingFactNodeTagResolver(root);
}

void KytheFactsExtractor::CreatePackageScopes(const IndexingFactNode& root) {
  // Searches for packages as a first pass and saves their scope to be used for
  // imports.
  for (const IndexingFactNode& child : root.Children()) {
    if (child.Value().GetIndexingFactType() != IndexingFactType::kPackage) {
      continue;
    }

    VName package_vname = ExtractPackageDeclaration(child);
    std::vector<VName> current_scope;
    Visit(child, package_vname, current_scope);

    // Save the scope and the members of each package.
    scope_context_[package_vname.signature] = current_scope;
  }
}

void KytheFactsExtractor::IndexingFactNodeTagResolver(
    const IndexingFactNode& node) {
  const auto tag = node.Value().GetIndexingFactType();

  VName vname("");

  // TODO(minatoma): Refactor this switch and move
  // scope_context_.top().push_back(vname);
  // to another function to make the code more readable.
  //
  // Directs flow to the appropriate function suitable to extract kythe facts
  // for this node.
  switch (tag) {
    case IndexingFactType::kFile: {
      vname = ExtractFileFact(node);
      break;
    }
    case IndexingFactType::kModule: {
      vname = ExtractModuleFact(node);
      vertical_scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kDataTypeReference: {
      vname = ExtractDataTypeReference(node);
      break;
    }
    case IndexingFactType::kModuleInstance: {
      vname = ExtractModuleInstanceFact(node);
      vertical_scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kModuleNamedPort: {
      vname = ExtractModuleNamedPort(node);
      break;
    }
    case IndexingFactType::kVariableDefinition: {
      vname = ExtractVariableDefinitionFact(node);
      vertical_scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kMacro: {
      vname = ExtractMacroDefinition(node);
      vertical_scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kVariableReference: {
      vname = ExtractVariableReferenceFact(node);
      break;
    }
    case IndexingFactType::kClass: {
      vname = ExtractClassFact(node);
      vertical_scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kClassInstance: {
      vname = ExtractClassInstances(node);
      vertical_scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kFunctionOrTask: {
      vname = ExtractFunctionOrTask(node);
      vertical_scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kFunctionCall: {
      vname = ExtractFunctionOrTaskCall(node);
      break;
    }
    case IndexingFactType::kPackageImport: {
      vname = ExtractPackageImport(node);
      break;
    }
    case IndexingFactType::kMacroCall: {
      vname = ExtractMacroCall(node);
      break;
    }
    case IndexingFactType::kMemberReference: {
      vname = ExtractMemberReference(node);
      break;
    }
    case IndexingFactType::kPackage: {
      // vname = ExtractPackageDeclaration(node);
      // vertical_scope_context_.top().push_back(vname);
      return;
    }
    default: {
      break;
    }
  }

  // TODO(minatoma): move to a function to make the code more readable.
  // Determines whether or not to create the childof relation with the parent.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kPackageImport:
    case IndexingFactType::kVariableReference:
    case IndexingFactType::kDataTypeReference:
    case IndexingFactType::kMacroCall:
    case IndexingFactType::kFunctionCall:
    case IndexingFactType::kMacro:
    case IndexingFactType::kModuleNamedPort: {
      break;
    }
    default: {
      if (!vnames_context_.empty()) {
        GenerateEdgeString(vname, kEdgeChildOf, vnames_context_.top());
      }
      break;
    }
  }

  std::vector<VName> current_scope;

  // TODO(minatoma): move to a function to make the code more readable
  // Determines whether or not to add the current node as a scope in vnames
  // context.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kModule:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kClass:
    case IndexingFactType::kMacro: {
      Visit(node, vname, current_scope);
      scope_context_[vname.signature] = current_scope;
      break;
    }
    default: {
      Visit(node);
    }
  }

  // TODO(minatoma): move to a function to make the code more readable
  // Determines whether or not to add the current scope to the scope context.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kModule:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kClass:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage: {
      scope_context_[vname.signature] = current_scope;
      break;
    }
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kClassInstance: {
      // TODO(minatoma): Refactor this to get rid of this search.
      const VName* found_vname = vertical_scope_context_.SearchForDefinition(
          CreateSignature(node.Parent()->Value().Anchors()[0].Value()));

      scope_context_[vname.signature] = scope_context_[found_vname->signature];
    }
    default: {
      break;
    }
  }
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node,
                                const VName& vname,
                                std::vector<VName>& current_scope) {
  const VNameContext::AutoPop vnames_auto_pop(&vnames_context_, &vname);
  const ScopeContext::AutoPop scope_auto_pop(&vertical_scope_context_,
                                             &current_scope);
  LOG(INFO) << "START " << vname.signature;
  Visit(node);
  for (auto x : current_scope) {
    LOG(INFO) << x.signature;
  }
  LOG(INFO) << "End " << vname.signature;
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node) {
  for (const IndexingFactNode& child : node.Children()) {
    IndexingFactNodeTagResolver(child);
  }
}

VName KytheFactsExtractor::ExtractFileFact(
    const IndexingFactNode& file_fact_node) {
  const VName file_vname(file_path_, "", "", "");
  const std::string& code_text = file_fact_node.Value().Anchors()[1].Value();

  GenerateFactString(file_vname, kFactNodeKind, kNodeFile);
  GenerateFactString(file_vname, kFactText, code_text);

  return file_vname;
}

VName KytheFactsExtractor::ExtractModuleFact(
    const IndexingFactNode& module_fact_node) {
  const auto& anchors = module_fact_node.Value().Anchors();
  const Anchor& module_name = anchors[0];
  const Anchor& module_end_label = anchors[1];

  const VName module_vname(file_path_,
                           CreateScopeRelativeSignature(module_name.Value()));
  const VName module_name_anchor = PrintAnchorVName(module_name);

  GenerateFactString(module_vname, kFactNodeKind, kNodeRecord);
  GenerateFactString(module_vname, kFactSubkind, kSubkindModule);
  GenerateFactString(module_vname, kFactComplete, kCompleteDefinition);
  GenerateEdgeString(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (anchors.size() > 1) {
    const VName module_end_label_anchor = PrintAnchorVName(module_end_label);
    GenerateEdgeString(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

VName KytheFactsExtractor::ExtractDataTypeReference(
    const IndexingFactNode& data_type_reference) {
  const auto& anchors = data_type_reference.Value().Anchors();
  const Anchor& type = anchors[0];

  const VName* type_vname = vertical_scope_context_.SearchForDefinition(
      CreateSignature(type.Value()));

  if (type_vname == nullptr) {
    return VName("");
  }

  const VName type_anchor = PrintAnchorVName(type);
  GenerateEdgeString(type_anchor, kEdgeRef, *type_vname);

  return *type_vname;
}

VName KytheFactsExtractor::ExtractModuleInstanceFact(
    const IndexingFactNode& module_instance_fact_node) {
  const auto& anchors = module_instance_fact_node.Value().Anchors();
  const Anchor& instance_name = anchors[0];

  const VName module_instance_vname(
      file_path_, CreateScopeRelativeSignature(instance_name.Value()));
  const VName module_instance_anchor = PrintAnchorVName(instance_name);

  GenerateFactString(module_instance_vname, kFactNodeKind, kNodeVariable);
  GenerateFactString(module_instance_vname, kFactComplete, kCompleteDefinition);
  GenerateEdgeString(module_instance_anchor, kEdgeDefinesBinding,
                     module_instance_vname);

  // TODO(minatoma): Consider changing to children so that they can be extracted
  // using ExtractVariableReference.
  for (const auto& anchor :
       verible::make_range(anchors.begin() + 1, anchors.end())) {
    const VName* port_vname_definition =
        vertical_scope_context_.SearchForDefinition(
            CreateSignature(anchor.Value()));

    if (port_vname_definition == nullptr) {
      continue;
    }

    const VName port_vname_anchor = PrintAnchorVName(anchor);
    GenerateEdgeString(port_vname_anchor, kEdgeRef, *port_vname_definition);
  }

  return module_instance_vname;
}

VName KytheFactsExtractor::ExtractModuleNamedPort(
    const IndexingFactNode& named_port_node) {
  const auto& port_name = named_port_node.Value().Anchors()[0];

  // TODO(minatoma): Change this to use the general scope that will be created.
  //
  // Parent Node must be kModuleInstance and the grand parent node must be
  // kDataTypeReference.
  const Anchor& module_type =
      named_port_node.Parent()->Parent()->Value().Anchors()[0];
  const VName* named_port_module_vname =
      vertical_scope_context_.SearchForDefinition(
          CreateSignature(module_type.Value()));

  if (named_port_module_vname == nullptr) {
    return VName("");
  }

  const VName actual_port_vname(
      file_path_, CreateScopeRelativeSignature(
                      port_name.Value(), named_port_module_vname->signature));
  const VName port_vname_anchor = PrintAnchorVName(port_name);
  GenerateEdgeString(port_vname_anchor, kEdgeRef, actual_port_vname);

  if (named_port_node.Children().empty()) {
    const VName* definition_vname = vertical_scope_context_.SearchForDefinition(
        CreateSignature(port_name.Value()));

    if (definition_vname == nullptr) {
      return VName("");
    }

    GenerateEdgeString(port_vname_anchor, kEdgeRef, *definition_vname);
  }

  return actual_port_vname;
}

VName KytheFactsExtractor::ExtractVariableDefinitionFact(
    const IndexingFactNode& variable_definition_fact_node) {
  const auto& anchor = variable_definition_fact_node.Value().Anchors()[0];
  const VName variable_vname(file_path_,
                             CreateScopeRelativeSignature(anchor.Value()));
  const VName variable_vname_anchor = PrintAnchorVName(anchor);

  GenerateFactString(variable_vname, kFactNodeKind, kNodeVariable);
  GenerateFactString(variable_vname, kFactComplete, kCompleteDefinition);
  GenerateEdgeString(variable_vname_anchor, kEdgeDefinesBinding,
                     variable_vname);

  return variable_vname;
}

VName KytheFactsExtractor::ExtractVariableReferenceFact(
    const IndexingFactNode& variable_reference_fact_node) {
  const auto& anchor = variable_reference_fact_node.Value().Anchors()[0];
  const VName variable_vname_anchor = PrintAnchorVName(anchor);

  const VName* variable_definition_vname =
      vertical_scope_context_.SearchForDefinition(
          CreateSignature(anchor.Value()));
  if (variable_definition_vname != nullptr) {
    GenerateEdgeString(variable_vname_anchor, kEdgeRef,
                       *variable_definition_vname);

    return *variable_definition_vname;
  } else {
    const VName variable_vname(file_path_,
                               CreateScopeRelativeSignature(anchor.Value()));
    GenerateEdgeString(variable_vname_anchor, kEdgeRef, variable_vname);

    return variable_vname;
  }
}

VName KytheFactsExtractor::ExtractPackageDeclaration(
    const IndexingFactNode& package_declaration_node) {
  const auto& anchors = package_declaration_node.Value().Anchors();
  const Anchor& package_name = anchors[0];

  const VName package_vname(file_path_,
                            CreateScopeRelativeSignature(package_name.Value()));
  const VName package_name_anchor = PrintAnchorVName(package_name);

  GenerateFactString(package_vname, kFactNodeKind, kNodePackage);
  GenerateEdgeString(package_name_anchor, kEdgeDefinesBinding, package_vname);

  if (anchors.size() > 1) {
    const Anchor& package_end_label = anchors[1];
    const VName package_end_label_anchor = PrintAnchorVName(package_end_label);
    GenerateEdgeString(package_end_label_anchor, kEdgeRef, package_vname);
  }

  return package_vname;
}

VName KytheFactsExtractor::ExtractMacroDefinition(
    const IndexingFactNode& macro_definition_node) {
  const Anchor& macro_name = macro_definition_node.Value().Anchors()[0];

  const VName macro_vname(file_path_, CreateSignature(macro_name.Value()));
  const VName module_name_anchor = PrintAnchorVName(macro_name);

  GenerateFactString(macro_vname, kFactNodeKind, kNodeMacro);
  GenerateEdgeString(module_name_anchor, kEdgeDefinesBinding, macro_vname);

  return macro_vname;
}

VName KytheFactsExtractor::ExtractMacroCall(
    const IndexingFactNode& macro_call_node) {
  const Anchor& macro_name = macro_call_node.Value().Anchors()[0];
  const VName macro_vname_anchor = PrintAnchorVName(macro_name);

  // We pass a substring to ignore the ` before macro name.
  // e.g.
  // `define TEN 0
  // `TEN --> removes the `
  const VName variable_definition_vname(
      file_path_, CreateSignature(macro_name.Value().substr(1)));

  GenerateEdgeString(macro_vname_anchor, kEdgeRefExpands,
                     variable_definition_vname);

  return variable_definition_vname;
}

VName KytheFactsExtractor::ExtractFunctionOrTask(
    const IndexingFactNode& function_fact_node) {
  const auto& function_name = function_fact_node.Value().Anchors()[0];

  const VName function_vname(
      file_path_, CreateScopeRelativeSignature(function_name.Value()));

  const VName function_vname_anchor = PrintAnchorVName(function_name);

  GenerateFactString(function_vname, kFactNodeKind, kNodeFunction);
  GenerateFactString(function_vname, kFactComplete, kCompleteDefinition);
  GenerateEdgeString(function_vname_anchor, kEdgeDefinesBinding,
                     function_vname);

  return function_vname;
}

VName KytheFactsExtractor::ExtractFunctionOrTaskCall(
    const IndexingFactNode& function_call_fact_node) {
  const auto& function_name = function_call_fact_node.Value().Anchors()[0];

  const VName* function_vname = vertical_scope_context_.SearchForDefinition(
      CreateSignature(function_name.Value()));

  if (function_vname == nullptr) {
    return VName("");
  }

  const VName function_vname_anchor = PrintAnchorVName(function_name);

  GenerateEdgeString(function_vname_anchor, kEdgeRef, *function_vname);
  GenerateEdgeString(function_vname_anchor, kEdgeRefCall, *function_vname);

  return function_vname_anchor;
}

VName KytheFactsExtractor::ExtractClassFact(
    const IndexingFactNode& class_fact_node) {
  const auto& anchors = class_fact_node.Value().Anchors();
  const Anchor& class_name = anchors[0];
  const Anchor& class_end_label = anchors[1];

  const VName class_vname(file_path_,
                          CreateScopeRelativeSignature(class_name.Value()));
  const VName class_name_anchor = PrintAnchorVName(class_name);

  GenerateFactString(class_vname, kFactNodeKind, kNodeRecord);
  GenerateFactString(class_vname, kFactComplete, kCompleteDefinition);
  GenerateEdgeString(class_name_anchor, kEdgeDefinesBinding, class_vname);

  if (anchors.size() > 1) {
    const VName class_end_label_anchor = PrintAnchorVName(class_end_label);
    GenerateEdgeString(class_end_label_anchor, kEdgeRef, class_vname);
  }

  return class_vname;
}

VName KytheFactsExtractor::ExtractClassInstances(
    const IndexingFactNode& class_instance_fact_node) {
  const auto& anchors = class_instance_fact_node.Value().Anchors();
  const Anchor& instance_name = anchors[0];

  const VName class_instance_vname(
      file_path_, CreateScopeRelativeSignature(instance_name.Value()));
  const VName class_instance_anchor = PrintAnchorVName(instance_name);

  GenerateFactString(class_instance_vname, kFactNodeKind, kNodeVariable);
  GenerateFactString(class_instance_vname, kFactComplete, kCompleteDefinition);
  GenerateEdgeString(class_instance_anchor, kEdgeDefinesBinding,
                     class_instance_vname);

  return class_instance_vname;
}

VName KytheFactsExtractor::ExtractPackageImport(
    const IndexingFactNode& import_fact_node) {
  const auto& anchors = import_fact_node.Value().Anchors();
  const Anchor& package_name = anchors[0];

  const VName package_vname(file_path_, CreateSignature(package_name.Value()));
  const VName package_anchor = PrintAnchorVName(package_name);

  GenerateEdgeString(package_anchor, kEdgeRefImports, package_vname);

  // case of import pkg::my_variable.
  if (anchors.size() > 1) {
    const Anchor& imported_item_name = anchors[1];
    const VName* defintion_vname = SearchForDefinitionVNameInScopeContext(
        CreateSignature(package_name.Value()),
        CreateSignature(imported_item_name.Value()));

    if (defintion_vname == nullptr) {
      return VName("");
    }

    const VName imported_item_anchor = PrintAnchorVName(imported_item_name);
    GenerateEdgeString(imported_item_anchor, kEdgeRef, *defintion_vname);

    // Add the found definition to the current scope as if it was declared in
    // our scope so that it can be captured without "::".
    vertical_scope_context_.top().push_back(*defintion_vname);
  } else {
    // case of import pkg::*.
    // Add all the definitions in that package to the current scope as if it was
    // declared in our scope so that it can be captured without "::".
    const auto current_package_scope =
        scope_context_.find(CreateSignature(package_name.Value()));

    if (current_package_scope == scope_context_.end()) {
      return VName("");
    }

    for (const VName& vname : current_package_scope->second) {
      vertical_scope_context_.top().push_back(vname);
    }
  }

  return package_vname;
}

VName KytheFactsExtractor::ExtractMemberReference(
    const IndexingFactNode& member_reference_node) {
  const auto& anchors = member_reference_node.Value().Anchors();
  const Anchor& containing_block_name = anchors[0];
  const Anchor& member_name = anchors[1];

  // Searches for the member in the packages.
  const VName* containing_block_vname = SearchForDefinitionVNameInScopeContext(
      CreateSignature(containing_block_name.Value()),
      CreateSignature(member_name.Value()));

  std::string definition_signature = "";

  const VName package_vname(file_path_,
                            CreateSignature(containing_block_name.Value()));
  const VName package_anchor = PrintAnchorVName(containing_block_name);
  GenerateEdgeString(package_anchor, kEdgeRef, package_vname);

  definition_signature = package_vname.signature;

  // Generate reference edge for all the members.
  // e.g pkg::my_class::my_inner_class::static_var.
  for (const auto& anchor :
       verible::make_range(anchors.begin() + 1, anchors.end())) {
    const VName* definition_vname = SearchForDefinitionVNameInScopeContext(
        definition_signature, CreateSignature(anchor.Value()));

    const VName reference_anchor = PrintAnchorVName(anchor);
    GenerateEdgeString(reference_anchor, kEdgeRef, *definition_vname);

    definition_signature = definition_vname->signature;
  }

  return *containing_block_vname;
}

const VName* KytheFactsExtractor::SearchForDefinitionVNameInScopeContext(
    absl::string_view package_name, absl::string_view reference_name) const {
  LOG(INFO) << "================================";
  LOG(INFO) << "FIND " << package_name << " ++> " << reference_name;
  for (auto x : scope_context_) {
    LOG(INFO) << "NAME " << x.first;
    for (auto y : x.second) {
      LOG(INFO) << y.signature;
    }
    LOG(INFO) << "END\n";
  }
  LOG(INFO) << "================================";

  const auto package_scope = scope_context_.find(std::string(package_name));
  if (package_scope == scope_context_.end()) {
    return nullptr;
  }

  for (const VName& vname : package_scope->second) {
    if (absl::StartsWith(vname.signature, reference_name)) {
      return &vname;
    }
  }

  return nullptr;
}

VName KytheFactsExtractor::PrintAnchorVName(const Anchor& anchor) {
  const VName anchor_vname(file_path_,
                           absl::Substitute(R"(@$0:$1)", anchor.StartLocation(),
                                            anchor.EndLocation()));

  GenerateFactString(anchor_vname, kFactNodeKind, kNodeAnchor);
  GenerateFactString(anchor_vname, kFactAnchorStart,
                     absl::Substitute(R"($0)", anchor.StartLocation()));
  GenerateFactString(anchor_vname, kFactAnchorEnd,
                     absl::Substitute(R"($0)", anchor.EndLocation()));

  return anchor_vname;
}

std::string KytheFactsExtractor::CreateScopeRelativeSignature(
    absl::string_view signature, absl::string_view parent_signature) const {
  return absl::StrCat(CreateSignature(signature), parent_signature);
}

std::string KytheFactsExtractor::CreateScopeRelativeSignature(
    absl::string_view signature) const {
  return vnames_context_.empty()
             ? CreateSignature(signature)
             : CreateScopeRelativeSignature(signature,
                                            vnames_context_.top().signature);
}

void KytheFactsExtractor::GenerateFactString(
    const VName& vname, absl::string_view fact_name,
    absl::string_view fact_value) const {
  *stream_ << absl::Substitute(
      R"({"source": $0,"fact_name": "$1","fact_value": "$2"})",
      vname.ToString(), fact_name, absl::Base64Escape(fact_value));
}

void KytheFactsExtractor::GenerateEdgeString(const VName& source_node,
                                             absl::string_view edge_name,
                                             const VName& target_node) const {
  *stream_ << absl::Substitute(
      R"({"source": $0,"edge_kind": "$1","target": $2,"fact_name": "/"})",
      source_node.ToString(), edge_name, target_node.ToString());
}

std::string GetFilePathFromRoot(const IndexingFactNode& root) {
  return root.Value().Anchors()[0].Value();
}

std::ostream& KytheFactsPrinter::Print(std::ostream& stream) const {
  KytheFactsExtractor kythe_extractor(GetFilePathFromRoot(root_), &stream);
  kythe_extractor.ExtractKytheFacts(root_);
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const KytheFactsPrinter& kythe_facts_printer) {
  kythe_facts_printer.Print(stream);
  return stream;
}

}  // namespace kythe
}  // namespace verilog
