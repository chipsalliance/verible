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
      vname = ExtractVariableDefinition(node);
      scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kVariableReference: {
      vname = ExtractVariableReference(node);
      break;
    }
    case IndexingFactType::kFunctionOrTask: {
      vname = ExtractFunctionOrTask(node);
      scope_context_.top().push_back(vname);
      break;
    }
    case IndexingFactType::kFunctionCall: {
      vname = ExtractFucntionOrTaskCall(node);
      break;
    }
  }

  if (tag != IndexingFactType::kFile) {
    *stream_ << Edge(vname, kEdgeChildOf, vnames_context_.top());
  }

  std::vector<VName> current_scope;
  const ScopeContext::AutoPop scope_auto_pop(&scope_context_, &current_scope);
  const VNameContext::AutoPop vnames_auto_pop(&vnames_context_, &vname);
  for (const verible::VectorTree<IndexingNodeData>& child : node.Children()) {
    Visit(child);
  }
}

VName KytheFactsExtractor::ExtractFileFact(const IndexingFactNode& node) {
  const VName file_vname(file_path_, "", "", "");
  const std::string& code_text = node.Value().Anchors()[1].Value();

  *stream_ << Fact(file_vname, kFactNodeKind, kNodeFile);
  *stream_ << Fact(file_vname, kFactText, code_text);

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

  *stream_ << Fact(module_vname, kFactNodeKind, kNodeRecord);
  *stream_ << Fact(module_vname, kFactSubkind, kSubkindModule);
  *stream_ << Fact(module_vname, kFactComplete, kCompleteDefinition);
  *stream_ << Edge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (anchors.size() > 1) {
    const VName module_end_label_anchor = PrintAnchorVName(module_end_label);
    *stream_ << Edge(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

VName KytheFactsExtractor::ExtractModuleInstanceFact(
    const IndexingFactNode& node) {
  const auto& anchors = node.Value().Anchors();
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

  *stream_ << Fact(module_instance_vname, kFactNodeKind, kNodeVariable);
  *stream_ << Fact(module_instance_vname, kFactComplete, kCompleteDefinition);

  *stream_ << Edge(module_type_anchor, kEdgeRef, module_type_vname);
  *stream_ << Edge(module_instance_vname, kEdgeTyped, module_type_vname);
  *stream_ << Edge(module_instance_anchor, kEdgeDefinesBinding,
                   module_instance_vname);

  for (const auto& anchor :
       verible::make_range(anchors.begin() + 2, anchors.end())) {
    const VName port_vname_reference(
        file_path_,
        CreateScopeRelativeSignature(CreateVariableSignature(anchor.Value())));
    const VName port_vname_definition(
        file_path_,
        CreateScopeRelativeSignature(CreateVariableSignature(anchor.Value())));
    const VName port_vname_anchor = PrintAnchorVName(anchor);

    *stream_ << Edge(port_vname_anchor, kEdgeRef, port_vname_definition);
  }

  return module_instance_vname;
}

VName KytheFactsExtractor::ExtractVariableDefinition(
    const IndexingFactNode& node) {
  const auto& anchor = node.Value().Anchors()[0];
  const VName variable_vname(
      file_path_,
      CreateScopeRelativeSignature(CreateVariableSignature(anchor.Value())));
  const VName variable_vname_anchor = PrintAnchorVName(anchor);

  *stream_ << Fact(variable_vname, kFactNodeKind, kNodeVariable);
  *stream_ << Fact(variable_vname, kFactComplete, kCompleteDefinition);

  *stream_ << Edge(variable_vname_anchor, kEdgeDefinesBinding, variable_vname);

  return variable_vname;
}

VName KytheFactsExtractor::ExtractVariableReference(
    const IndexingFactNode& node) {
  const auto& anchor = node.Value().Anchors()[0];
  const VName variable_vname_anchor = PrintAnchorVName(anchor);

  const VName* variable_definition_vname =
      scope_context_.SearchForDefinition(CreateModuleSignature(anchor.Value()));
  if (variable_definition_vname != nullptr) {
    *stream_ << Edge(variable_vname_anchor, kEdgeRef,
                     *variable_definition_vname);

    return *variable_definition_vname;
  } else {
    const VName variable_vname(
        file_path_,
        CreateScopeRelativeSignature(CreateModuleSignature(anchor.Value())));
    *stream_ << Edge(variable_vname_anchor, kEdgeRef, variable_vname);

    return variable_vname;
  }
}

VName KytheFactsExtractor::ExtractFunctionOrTask(
    const IndexingFactNode& function_fact_node) {
  const auto& function_name = function_fact_node.Value().Anchors()[0];

  const VName function_vname(
      file_path_, CreateScopeRelativeSignature(
                      CreateFunctionOrTaskSignature(function_name.Value())));

  const VName function_vname_anchor = PrintAnchorVName(function_name);

  *stream_ << Fact(function_vname, kFactNodeKind, kNodeFunction);
  *stream_ << Fact(function_vname, kFactComplete, kCompleteDefinition);
  *stream_ << Edge(function_vname_anchor, kEdgeDefinesBinding, function_vname);

  return function_vname;
}

VName KytheFactsExtractor::ExtractFucntionOrTaskCall(
    const IndexingFactNode& function_call_fact_node) {
  const auto& function_name = function_call_fact_node.Value().Anchors()[0];

  const VName function_vname =
      *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(
          CreateFunctionOrTaskSignature(function_name.Value())));

  const VName function_vname_anchor = PrintAnchorVName(function_name);

  *stream_ << Edge(function_vname_anchor, kEdgeRef, function_vname);
  *stream_ << Edge(function_vname_anchor, kEdgeRefCall, function_vname);

  return function_vname;
}

VName KytheFactsExtractor::PrintAnchorVName(const Anchor& anchor) {
  const VName anchor_vname(file_path_,
                           absl::Substitute(R"(@$0:$1)", anchor.StartLocation(),
                                            anchor.EndLocation()));

  *stream_ << Fact(anchor_vname, kFactNodeKind, kNodeAnchor);
  *stream_ << Fact(anchor_vname, kFactAnchorStart,
                   absl::Substitute(R"($0)", anchor.StartLocation()));
  *stream_ << Fact(anchor_vname, kFactAnchorEnd,
                   absl::Substitute(R"($0)", anchor.EndLocation()));

  return anchor_vname;
}

std::string KytheFactsExtractor::CreateScopeRelativeSignature(
    absl::string_view signature) {
  return absl::StrCat(signature, "#", vnames_context_.top().signature);
}

std::string CreateModuleSignature(absl::string_view module_name) {
  return absl::StrCat(module_name, "#module");
}

std::string CreateVariableSignature(absl::string_view variable_name) {
  return absl::StrCat(variable_name, "#variable");
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
