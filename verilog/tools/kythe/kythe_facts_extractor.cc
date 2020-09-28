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
  std::size_t number_of_extracted_facts = 0;
  do {
    number_of_extracted_facts = facts_.size();
    is_new_facts_extracted_ = false;
    IndexingFactNodeTagResolver(root);
  } while (number_of_extracted_facts != facts_.size());

  for (const Fact& fact : facts_) {
    *stream_ << fact;
  }
  for (const Edge& edge : edges_) {
    *stream_ << edge;
  }
}

void KytheFactsExtractor::IndexingFactNodeTagResolver(
    const IndexingFactNode& node) {
  const auto tag = node.Value().GetIndexingFactType();

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
    case IndexingFactType::kFunctionOrTask: {
      vertical_scope_resolver_.top().AddMemberItem(vname);
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
    case IndexingFactType::kModule:
    case IndexingFactType::kFunctionOrTask:
    case IndexingFactType::kClass:
    case IndexingFactType::kMacro:
    case IndexingFactType::kPackage:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kClassInstance: {
      // Get the old scope of this node (if it was extracted in a previous
      // iteration).
      Scope* old_scope =
          flattened_scope_resolver_.SearchForScope(vname.signature);
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
      flattened_scope_resolver_.MapSignatureToScope(vname.signature,
                                                    current_scope);
      break;
    }
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kClassInstance: {
      // TODO(minatoma): fix this in case the name was kQualified id.
      const VName* found_vname = vertical_scope_resolver_.SearchForDefinition(
          node.Parent()->Value().Anchors()[0].Value());

      if (found_vname == nullptr) {
        break;
      }

      flattened_scope_resolver_.MapSignatureToScopeOfSignature(
          vname.signature, found_vname->signature);

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
  const VerticalScopeResolver::AutoPop scope_auto_pop(&vertical_scope_resolver_,
                                                      &current_scope);
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
  const VName module_name_anchor = PrintAnchorVName(module_name);

  CreateFact(module_vname, kFactNodeKind, kNodeRecord);
  CreateFact(module_vname, kFactSubkind, kSubkindModule);
  CreateFact(module_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (anchors.size() > 1) {
    const VName module_end_label_anchor = PrintAnchorVName(module_end_label);
    CreateEdge(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

void KytheFactsExtractor::ExtractDataTypeReference(
    const IndexingFactNode& data_type_reference) {
  const auto& anchors = data_type_reference.Value().Anchors();
  const Anchor& type = anchors[0];

  const VName* type_vname =
      vertical_scope_resolver_.SearchForDefinition(type.Value());

  if (type_vname == nullptr) {
    return;
  }

  const VName type_anchor = PrintAnchorVName(type);
  CreateEdge(type_anchor, kEdgeRef, *type_vname);
}

VName KytheFactsExtractor::ExtractModuleInstance(
    const IndexingFactNode& module_instance_node) {
  const auto& anchors = module_instance_node.Value().Anchors();
  const Anchor& instance_name = anchors[0];

  const VName module_instance_vname(
      file_path_, CreateScopeRelativeSignature(instance_name.Value()));

  const VName module_instance_anchor = PrintAnchorVName(instance_name);
  CreateFact(module_instance_vname, kFactNodeKind, kNodeVariable);
  CreateFact(module_instance_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(module_instance_anchor, kEdgeDefinesBinding,
             module_instance_vname);

  return module_instance_vname;
}

void KytheFactsExtractor::ExtractModuleNamedPort(
    const IndexingFactNode& named_port_node) {
  const auto& port_name = named_port_node.Value().Anchors()[0];

  // Parent Node must be kModuleInstance and the grand parent node must be
  // kDataTypeReference.
  const Anchor& module_type =
      named_port_node.Parent()->Parent()->Value().Anchors()[0];
  const VName* named_port_module_vname =
      vertical_scope_resolver_.SearchForDefinition(module_type.Value());

  if (named_port_module_vname == nullptr) {
    return;
  }

  const VName* actual_port_vname =
      flattened_scope_resolver_.SearchForVNameInScope(
          named_port_module_vname->signature, port_name.Value());

  if (actual_port_vname == nullptr) {
    return;
  }

  const VName port_vname_anchor = PrintAnchorVName(port_name);
  CreateEdge(port_vname_anchor, kEdgeRef, *actual_port_vname);

  if (named_port_node.Children().empty()) {
    const VName* definition_vname =
        vertical_scope_resolver_.SearchForDefinition(port_name.Value());

    if (definition_vname != nullptr) {
      CreateEdge(port_vname_anchor, kEdgeRef, *definition_vname);
    }
  }
}

VName KytheFactsExtractor::ExtractVariableDefinition(
    const IndexingFactNode& variable_definition_node) {
  const auto& anchor = variable_definition_node.Value().Anchors()[0];
  const VName variable_vname(file_path_,
                             CreateScopeRelativeSignature(anchor.Value()));
  const VName variable_vname_anchor = PrintAnchorVName(anchor);

  CreateFact(variable_vname, kFactNodeKind, kNodeVariable);
  CreateFact(variable_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(variable_vname_anchor, kEdgeDefinesBinding, variable_vname);

  return variable_vname;
}

void KytheFactsExtractor::ExtractVariableReference(
    const IndexingFactNode& variable_reference_node) {
  const auto& anchor = variable_reference_node.Value().Anchors()[0];

  const VName* variable_definition_vname =
      vertical_scope_resolver_.SearchForDefinition(anchor.Value());
  if (variable_definition_vname == nullptr) {
    return;
  }

  const VName variable_vname_anchor = PrintAnchorVName(anchor);
  CreateEdge(variable_vname_anchor, kEdgeRef, *variable_definition_vname);
}

VName KytheFactsExtractor::ExtractPackageDeclaration(
    const IndexingFactNode& package_declaration_node) {
  const auto& anchors = package_declaration_node.Value().Anchors();
  const Anchor& package_name = anchors[0];

  const VName package_vname(file_path_,
                            CreateScopeRelativeSignature(package_name.Value()));
  const VName package_name_anchor = PrintAnchorVName(package_name);

  CreateFact(package_vname, kFactNodeKind, kNodePackage);
  CreateEdge(package_name_anchor, kEdgeDefinesBinding, package_vname);

  if (anchors.size() > 1) {
    const Anchor& package_end_label = anchors[1];
    const VName package_end_label_anchor = PrintAnchorVName(package_end_label);
    CreateEdge(package_end_label_anchor, kEdgeRef, package_vname);
  }

  return package_vname;
}

VName KytheFactsExtractor::ExtractMacroDefinition(
    const IndexingFactNode& macro_definition_node) {
  const Anchor& macro_name = macro_definition_node.Value().Anchors()[0];

  const VName macro_vname(file_path_, Signature(macro_name.Value()));
  const VName module_name_anchor = PrintAnchorVName(macro_name);

  CreateFact(macro_vname, kFactNodeKind, kNodeMacro);
  CreateEdge(module_name_anchor, kEdgeDefinesBinding, macro_vname);

  return macro_vname;
}

void KytheFactsExtractor::ExtractMacroCall(
    const IndexingFactNode& macro_call_node) {
  const Anchor& macro_name = macro_call_node.Value().Anchors()[0];
  const VName macro_vname_anchor = PrintAnchorVName(macro_name);

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

  const VName function_vname_anchor = PrintAnchorVName(function_name);

  CreateFact(function_vname, kFactNodeKind, kNodeFunction);
  CreateFact(function_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(function_vname_anchor, kEdgeDefinesBinding, function_vname);

  return function_vname;
}

void KytheFactsExtractor::ExtractFunctionOrTaskCall(
    const IndexingFactNode& function_call_node) {
  const auto& anchors = function_call_node.Value().Anchors();

  // In case function_name();
  if (anchors.size() == 1) {
    const auto& function_name = anchors[0];

    const VName* function_vname =
        vertical_scope_resolver_.SearchForDefinition(function_name.Value());

    if (function_vname == nullptr) {
      return;
    }

    const VName function_vname_anchor = PrintAnchorVName(function_name);

    CreateEdge(function_vname_anchor, kEdgeRef, *function_vname);
    CreateEdge(function_vname_anchor, kEdgeRefCall, *function_vname);
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
  const VName class_name_anchor = PrintAnchorVName(class_name);

  CreateFact(class_vname, kFactNodeKind, kNodeRecord);
  CreateFact(class_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(class_name_anchor, kEdgeDefinesBinding, class_vname);

  if (anchors.size() > 1) {
    const VName class_end_label_anchor = PrintAnchorVName(class_end_label);
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
  const VName class_instance_anchor = PrintAnchorVName(instance_name);

  CreateFact(class_instance_vname, kFactNodeKind, kNodeVariable);
  CreateFact(class_instance_vname, kFactComplete, kCompleteDefinition);
  CreateEdge(class_instance_anchor, kEdgeDefinesBinding, class_instance_vname);

  return class_instance_vname;
}

void KytheFactsExtractor::ExtractPackageImport(
    const IndexingFactNode& import_fact_node) {
  const auto& anchors = import_fact_node.Value().Anchors();
  const Anchor& package_name = anchors[0];

  const VName package_vname(file_path_, Signature(package_name.Value()));
  const VName package_anchor = PrintAnchorVName(package_name);

  CreateEdge(package_anchor, kEdgeRefImports, package_vname);

  // case of import pkg::my_variable.
  if (anchors.size() > 1) {
    const Anchor& imported_item_name = anchors[1];
    const VName* defintion_vname =
        flattened_scope_resolver_.SearchForVNameInScope(
            Signature(package_name.Value()), imported_item_name.Value());

    if (defintion_vname == nullptr) {
      return;
    }

    const VName imported_item_anchor = PrintAnchorVName(imported_item_name);
    CreateEdge(imported_item_anchor, kEdgeRef, *defintion_vname);

    // Add the found definition to the current scope as if it was declared in
    // our scope so that it can be captured without "::".
    vertical_scope_resolver_.top().AddMemberItem(*defintion_vname);
  } else {
    // case of import pkg::*.
    // Add all the definitions in that package to the current scope as if it was
    // declared in our scope so that it can be captured without "::".
    const Scope* current_package_scope =
        flattened_scope_resolver_.SearchForScope(package_vname.signature);

    if (current_package_scope == nullptr) {
      return;
    }

    vertical_scope_resolver_.top().AddMemberItem(package_vname);
    vertical_scope_resolver_.top().AppendScope(*current_package_scope);
  }
}

void KytheFactsExtractor::ExtractMemberReference(
    const IndexingFactNode& member_reference_node, bool is_function_call) {
  const auto& anchors = member_reference_node.Value().Anchors();
  const Anchor& containing_block_name = anchors[0];

  // Searches for the member in the packages.
  const Scope* containing_block_scope =
      flattened_scope_resolver_.SearchForScope(
          Signature(containing_block_name.Value()));

  Signature definition_signature;

  // In case it is a package member e.g pkg::var.
  if (containing_block_scope != nullptr) {
    const VName package_vname(file_path_,
                              containing_block_scope->GetSignature());
    const VName package_anchor = PrintAnchorVName(containing_block_name);
    CreateEdge(package_anchor, kEdgeRef, package_vname);

    definition_signature = package_vname.signature;
  } else {
    // TODO(minatoma): this can be removed in case the search inside flattened
    // scope is modified to search for something that starts with the given
    // signature.
    //
    // In case the member is a class member not a package member.
    const VName* containing_block_vname =
        vertical_scope_resolver_.SearchForDefinition(
            containing_block_name.Value());

    if (containing_block_vname == nullptr) {
      return;
    }

    const VName class_anchor = PrintAnchorVName(containing_block_name);
    CreateEdge(class_anchor, kEdgeRef, *containing_block_vname);

    definition_signature = containing_block_vname->signature;
  }

  // Generate reference edge for all the members.
  // e.g pkg::my_class::my_inner_class::static_var.
  const VName* definition_vname;
  VName reference_anchor;
  for (const auto& anchor :
       verible::make_range(anchors.begin() + 1, anchors.end())) {
    definition_vname = flattened_scope_resolver_.SearchForVNameInScope(
        definition_signature, anchor.Value());

    if (definition_vname == nullptr) {
      continue;
    }

    reference_anchor = PrintAnchorVName(anchor);
    CreateEdge(reference_anchor, kEdgeRef, *definition_vname);

    definition_signature = definition_vname->signature;
  }

  if (is_function_call && definition_vname != nullptr) {
    CreateEdge(reference_anchor, kEdgeRefCall, *definition_vname);
  }
}

VName KytheFactsExtractor::PrintAnchorVName(const Anchor& anchor) {
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
