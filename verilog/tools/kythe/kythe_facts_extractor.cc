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

void KytheFactsExtractor::Visit(const IndexingFactNode& node) {
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
    case IndexingFactType::kMacroCall: {
      vname = ExtractMacroCall(node);
      break;
    }
  }

  if (tag != IndexingFactType::kFile) {
    GenerateEdgeString(vname, kEdgeChildOf, vnames_context_.top());
  }

  std::vector<VName> current_scope;
  const ScopeContext::AutoPop scope_auto_pop(&scope_context_, &current_scope);

  // Determines whether or not to add the current node as a scope in vnames
  // context.
  switch (tag) {
    case IndexingFactType::kFile:
    case IndexingFactType::kModule:
    case IndexingFactType::kModuleInstance:
    case IndexingFactType::kVariableDefinition:
    case IndexingFactType::kFunctionOrTask: {
      const VNameContext::AutoPop vnames_auto_pop(&vnames_context_, &vname);
      for (const verible::VectorTree<IndexingNodeData>& child :
           node.Children()) {
        Visit(child);
      }
      break;
    }
    default: {
      for (const verible::VectorTree<IndexingNodeData>& child :
           node.Children()) {
        Visit(child);
      }
    }
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

  const VName module_vname(
      file_path_,
      CreateScopeRelativeSignature(CreateModuleSignature(module_name.Value())));
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
      file_path_, CreateScopeRelativeSignature(
                      CreateVariableSignature(instance_name.Value())));
  const VName module_instance_anchor = PrintAnchorVName(instance_name);

  const VName module_type_vname =
      *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(
          CreateModuleSignature(module_type.Value())));

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
        file_path_,
        CreateScopeRelativeSignature(CreateVariableSignature(anchor.Value())));
    const VName port_vname_definition =
        *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(
            CreateVariableSignature(anchor.Value())));

    const VName port_vname_anchor = PrintAnchorVName(anchor);

    GenerateEdgeString(port_vname_anchor, kEdgeRef, port_vname_definition);
  }

  return module_instance_vname;
}

VName KytheFactsExtractor::ExtractVariableDefinitionFact(
    const IndexingFactNode& variable_definition_fact_node) {
  const auto& anchor = variable_definition_fact_node.Value().Anchors()[0];
  const VName variable_vname(
      file_path_,
      CreateScopeRelativeSignature(CreateVariableSignature(anchor.Value())));
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

  const VName* variable_definition_vname = scope_context_.SearchForDefinition(
      CreateVariableSignature(anchor.Value()));
  if (variable_definition_vname != nullptr) {
    GenerateEdgeString(variable_vname_anchor, kEdgeRef,
                       *variable_definition_vname);

    return *variable_definition_vname;
  } else {
    const VName variable_vname(
        file_path_,
        CreateScopeRelativeSignature(CreateVariableSignature(anchor.Value())));
    GenerateEdgeString(variable_vname_anchor, kEdgeRef, variable_vname);

    return variable_vname;
  }
}

VName KytheFactsExtractor::ExtractMacroDefinition(
    const IndexingFactNode& macro_definition_node) {
  const Anchor& macro_name = macro_definition_node.Value().Anchors()[0];

  const VName macro_vname(file_path_, CreateMacroSignature(macro_name.Value()));
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
      file_path_, CreateMacroSignature(macro_name.Value().substr(1)));

  GenerateEdgeString(macro_vname_anchor, kEdgeRefExpands,
                     variable_definition_vname);

  return variable_definition_vname;
}

VName KytheFactsExtractor::ExtractFunctionOrTask(
    const IndexingFactNode& function_fact_node) {
  const auto& function_name = function_fact_node.Value().Anchors()[0];

  const VName function_vname(
      file_path_, CreateScopeRelativeSignature(
                      CreateFunctionOrTaskSignature(function_name.Value())));

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

  const VName function_vname =
      *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(
          CreateFunctionOrTaskSignature(function_name.Value())));

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

  const VName class_vname(
      file_path_,
      CreateScopeRelativeSignature(CreateClassSignature(class_name.Value())));
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
      file_path_, CreateScopeRelativeSignature(
                      CreateVariableSignature(instance_name.Value())));
  const VName class_instance_anchor = PrintAnchorVName(instance_name);

  const VName class_type_vname =
      *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(
          CreateClassSignature(class_type.Value())));

  const VName class_type_anchor = PrintAnchorVName(class_type);

  GenerateFactString(class_instance_vname, kFactNodeKind, kNodeVariable);
  GenerateFactString(class_instance_vname, kFactComplete, kCompleteDefinition);

  GenerateEdgeString(class_type_anchor, kEdgeRef, class_type_vname);
  GenerateEdgeString(class_instance_vname, kEdgeTyped, class_type_vname);
  GenerateEdgeString(class_instance_anchor, kEdgeDefinesBinding,
                     class_instance_vname);

  return class_instance_vname;
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
    absl::string_view signature) const {
  return absl::StrCat(signature, "#", vnames_context_.top().signature);
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

std::string CreateModuleSignature(absl::string_view module_name) {
  return absl::StrCat(module_name, "#module");
}

std::string CreateClassSignature(absl::string_view class_name) {
  return absl::StrCat(class_name, "#class");
}

std::string CreateVariableSignature(absl::string_view variable_name) {
  return absl::StrCat(variable_name, "#variable");
}

std::string CreateMacroSignature(absl::string_view macro_name) {
  return absl::StrCat(macro_name, "#macro");
}

std::string CreateFunctionOrTaskSignature(absl::string_view function_name) {
  return absl::StrCat(function_name, "#function");
}

std::string GetFilePathFromRoot(const IndexingFactNode& root) {
  return root.Value().Anchors()[0].Value();
}

std::ostream& KytheFactsPrinter::Print(std::ostream& stream) const {
  KytheFactsExtractor kythe_extractor(GetFilePathFromRoot(root_), &stream);
  kythe_extractor.Visit(root_);
  return stream;
}

std::ostream& operator<<(std::ostream& stream,
                         const KytheFactsPrinter& kythe_facts_printer) {
  kythe_facts_printer.Print(stream);
  return stream;
}

}  // namespace kythe
}  // namespace verilog
