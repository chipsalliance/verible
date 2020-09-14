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
    package_scope_context_[package_vname.signature] = current_scope;
  }
}

void KytheFactsExtractor::IndexingFactNodeTagResolver(
    const IndexingFactNode& node) {
  const auto tag = node.Value().GetIndexingFactType();

  VName vname("");

  // Directs flow to the appropriate function suitable to extract kythe facts
  // for this node.
  switch (tag) {
    case IndexingFactType::kFile: {
      vname = ExtractFileFact(node);
      break;
    }
    case IndexingFactType::kModule: {
      vname = ExtractModuleFact(node);
      scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kModuleInstance: {
      vname = ExtractModuleInstanceFact(node);
      scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kVariableDefinition: {
      vname = ExtractVariableDefinitionFact(node);
      scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kMacro: {
      vname = ExtractMacroDefinition(node);
      scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kVariableReference: {
      vname = ExtractVariableReferenceFact(node);
      break;
    }
    case IndexingFactType::kClass: {
      vname = ExtractClassFact(node);
      scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kClassInstance: {
      vname = ExtractClassInstances(node);
      break;
    }
    case IndexingFactType::kFunctionOrTask: {
      vname = ExtractFunctionOrTask(node);
      scope_context_.top().push_back(vname);
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
      // Return as packages should be extracted at the first pass.
      return;
    }
    default: {
      break;
    }
  }

  // Determines whether or not to create the childof relation with the parent.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kPackageImport: {
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

  // Determines whether or not to add the current node as a scope in vnames
  // context.
  switch (tag) {
    case IndexingFactType::kModule:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kFunctionOrTask: {
      Visit(node, vname, current_scope);
      break;
    }
    default: {
      Visit(node, current_scope);
    }
  }
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node,
                                const VName& vname,
                                std::vector<VName>& current_scope) {
  const VNameContext::AutoPop vnames_auto_pop(&vnames_context_, &vname);
  Visit(node, current_scope);
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node,
                                std::vector<VName>& current_scope) {
  const ScopeContext::AutoPop scope_auto_pop(&scope_context_, &current_scope);
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

VName KytheFactsExtractor::ExtractModuleInstanceFact(
    const IndexingFactNode& module_instance_fact_node) {
  const auto& anchors = module_instance_fact_node.Value().Anchors();
  const Anchor& module_type = anchors[0];
  const Anchor& instance_name = anchors[1];

  const VName module_instance_vname(
      file_path_, CreateScopeRelativeSignature(instance_name.Value()));
  const VName module_instance_anchor = PrintAnchorVName(instance_name);

  const VName module_type_vname = *ABSL_DIE_IF_NULL(
      scope_context_.SearchForDefinition(module_type.Value()));

  const VName module_type_anchor = PrintAnchorVName(module_type);

  GenerateFactString(module_instance_vname, kFactNodeKind, kNodeVariable);
  GenerateFactString(module_instance_vname, kFactComplete, kCompleteDefinition);

  GenerateEdgeString(module_type_anchor, kEdgeRef, module_type_vname);
  GenerateEdgeString(module_instance_vname, kEdgeTyped, module_type_vname);
  GenerateEdgeString(module_instance_anchor, kEdgeDefinesBinding,
                     module_instance_vname);

  for (const auto& anchor :
       verible::make_range(anchors.begin() + 2, anchors.end())) {
    const VName port_vname_reference(
        file_path_, CreateScopeRelativeSignature(anchor.Value()));
    const VName port_vname_definition =
        *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(anchor.Value()));

    const VName port_vname_anchor = PrintAnchorVName(anchor);

    GenerateEdgeString(port_vname_anchor, kEdgeRef, port_vname_definition);
  }

  return module_instance_vname;
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
      scope_context_.SearchForDefinition(anchor.Value());
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

  const VName macro_vname(file_path_, macro_name.Value());
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
  const VName variable_definition_vname(file_path_,
                                        macro_name.Value().substr(1));

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

  const VName function_vname = *ABSL_DIE_IF_NULL(
      scope_context_.SearchForDefinition(function_name.Value()));

  const VName function_vname_anchor = PrintAnchorVName(function_name);

  GenerateEdgeString(function_vname_anchor, kEdgeRef, function_vname);
  GenerateEdgeString(function_vname_anchor, kEdgeRefCall, function_vname);

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
  const Anchor& class_type = anchors[0];
  const Anchor& instance_name = anchors[1];

  const VName class_instance_vname(
      file_path_, CreateScopeRelativeSignature(instance_name.Value()));
  const VName class_instance_anchor = PrintAnchorVName(instance_name);

  const VName class_type_vname =
      *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(class_type.Value()));

  const VName class_type_anchor = PrintAnchorVName(class_type);

  GenerateFactString(class_instance_vname, kFactNodeKind, kNodeVariable);
  GenerateFactString(class_instance_vname, kFactComplete, kCompleteDefinition);

  GenerateEdgeString(class_type_anchor, kEdgeRef, class_type_vname);
  GenerateEdgeString(class_instance_vname, kEdgeTyped, class_type_vname);
  GenerateEdgeString(class_instance_anchor, kEdgeDefinesBinding,
                     class_instance_vname);

  return class_instance_vname;
}

VName KytheFactsExtractor::ExtractPackageImport(
    const IndexingFactNode& import_fact_node) {
  const auto& anchors = import_fact_node.Value().Anchors();
  const Anchor& package_name = anchors[0];

  const VName package_vname(file_path_, package_name.Value());
  const VName package_anchor = PrintAnchorVName(package_name);

  GenerateEdgeString(package_anchor, kEdgeRefImports, package_vname);

  // case of import pkg::my_variable.
  if (anchors.size() > 1) {
    const Anchor& imported_item_name = anchors[1];
    const VName& defintion_vname =
        *ABSL_DIE_IF_NULL(SearchForDefinitionVNameInPackage(
            package_name.Value(), imported_item_name.Value()));

    const VName imported_item_anchor = PrintAnchorVName(imported_item_name);
    GenerateEdgeString(imported_item_anchor, kEdgeRef, defintion_vname);

    // Add the found definition to the current scope as if it was declared in
    // our scope so that it can be captured without "::".
    scope_context_.top().push_back(defintion_vname);
  } else {
    // case of import pkg::*.
    // Add all the definitions in that package to the current scope as if it was
    // declared in our scope so that it can be captured without "::".
    const auto current_package_scope =
        package_scope_context_.find(package_name.Value());
    for (const VName& vname : current_package_scope->second) {
      scope_context_.top().push_back(vname);
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
  const VName* containing_block_vname = SearchForDefinitionVNameInPackage(
      containing_block_name.Value(), member_name.Value());

  std::string definition_signature = "";

  // In case it is a package member.
  if (containing_block_vname != nullptr) {
    const VName package_vname(file_path_, containing_block_name.Value());
    const VName package_anchor = PrintAnchorVName(containing_block_name);
    GenerateEdgeString(package_anchor, kEdgeRef, package_vname);

    definition_signature = package_vname.signature;
  } else {
    // In case the member is a class member not a package member.
    containing_block_vname = ABSL_DIE_IF_NULL(
        scope_context_.SearchForDefinition(containing_block_name.Value()));

    const VName class_anchor = PrintAnchorVName(containing_block_name);
    GenerateEdgeString(class_anchor, kEdgeRef, *containing_block_vname);

    definition_signature = containing_block_vname->signature;
  }

  // Generate reference edge for all the members.
  // e.g pkg::my_class::my_inner_class::static_var.
  for (const auto& anchor :
       verible::make_range(anchors.begin() + 1, anchors.end())) {
    definition_signature =
        CreateScopeRelativeSignature(anchor.Value(), definition_signature);

    const VName definition_vname(file_path_, definition_signature);
    const VName reference_anchor = PrintAnchorVName(anchor);
    GenerateEdgeString(reference_anchor, kEdgeRef, definition_vname);
  }

  return *containing_block_vname;
}

const VName* KytheFactsExtractor::SearchForDefinitionVNameInPackage(
    absl::string_view package_name, absl::string_view reference_name) const {
  const auto package_scope =
      package_scope_context_.find(std::string(package_name));
  if (package_scope == package_scope_context_.end()) {
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
  return absl::StrCat(signature, "#", parent_signature);
}

std::string KytheFactsExtractor::CreateScopeRelativeSignature(
    absl::string_view signature) const {
  return vnames_context_.empty()
             ? std::string(signature)
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
