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
  Visit(root);
}

void KytheFactsExtractor::CreatePackageScopes(const IndexingFactNode& root) {
  for (const verible::VectorTree<IndexingNodeData>& child : root.Children()) {
    if (child.Value().GetIndexingFactType() != IndexingFactType::kPackage) {
      continue;
    }

    VName package_vname = ExtractPackageDeclaration(child);

    std::vector<VName> current_scope;
    const ScopeContext::AutoPop scope_auto_pop(&scope_context_, &current_scope);
    const VNameContext::AutoPop vnames_auto_pop(&vnames_context_,
                                                &package_vname);

    for (const verible::VectorTree<IndexingNodeData>& grand_child :
         child.Children()) {
      Visit(grand_child);
    }

    package_scope_context[package_vname.signature] = current_scope;
  }

  for (auto x : package_scope_context) {
    LOG(INFO) << x.first;
    for (auto y : x.second) {
      LOG(INFO) << y.signature;
    }
    LOG(INFO) << "";
  }
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node) {
  const auto tag = node.Value().GetIndexingFactType();

  VName vname(file_path_);

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
    default: {
      break;
    }
  }

  if (tag != IndexingFactType::kFile) {
    //*stream_ << Edge(vname, kEdgeChildOf, vnames_context_.top());
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
  const VName module_name_anchor = PrintAnchorVName(module_name, file_path_);

  *stream_ << Fact(module_vname, kFactNodeKind, kNodeRecord);
  *stream_ << Fact(module_vname, kFactSubkind, kSubkindModule);
  *stream_ << Fact(module_vname, kFactComplete, kCompleteDefinition);
  *stream_ << Edge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (anchors.size() > 1) {
    const VName module_end_label_anchor =
        PrintAnchorVName(module_end_label, file_path_);
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
  const VName module_instance_anchor =
      PrintAnchorVName(instance_name, file_path_);

  const VName module_type_vname =
      *ABSL_DIE_IF_NULL(scope_context_.SearchForDefinition(
          CreateModuleSignature(module_type.Value())));

  const VName module_type_anchor = PrintAnchorVName(module_type, file_path_);

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
    const VName port_vname_anchor = PrintAnchorVName(anchor, file_path_);

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
  const VName variable_vname_anchor = PrintAnchorVName(anchor, file_path_);

  *stream_ << Fact(variable_vname, kFactNodeKind, kNodeVariable);
  *stream_ << Fact(variable_vname, kFactComplete, kCompleteDefinition);

  *stream_ << Edge(variable_vname_anchor, kEdgeDefinesBinding, variable_vname);

  return variable_vname;
}

VName KytheFactsExtractor::ExtractVariableReference(
    const IndexingFactNode& node) {
  const auto& anchor = node.Value().Anchors()[0];
  const VName variable_vname_anchor = PrintAnchorVName(anchor, file_path_);

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

VName KytheFactsExtractor::ExtractPackageDeclaration(
    const IndexingFactNode& package_declaration_node) {
  const auto& anchors = package_declaration_node.Value().Anchors();
  const Anchor& package_name = anchors[0];
  const Anchor& package_end_label = anchors[1];

  const VName package_vname(file_path_,
                            CreateScopeRelativeSignature(
                                CreatePackageSignature(package_name.Value())));
  const VName package_name_anchor = PrintAnchorVName(package_name, file_path_);

  *stream_ << Fact(package_vname, kFactNodeKind, kNodePackage);
  *stream_ << Edge(package_name_anchor, kEdgeDefinesBinding, package_vname);

  if (anchors.size() > 1) {
    const VName package_end_label_anchor =
        PrintAnchorVName(package_end_label, file_path_);
    *stream_ << Edge(package_end_label_anchor, kEdgeRef, package_vname);
  }

  return package_vname;
}

VName KytheFactsExtractor::PrintAnchorVName(const Anchor& anchor,
                                            absl::string_view file_path) {
  const VName anchor_vname(file_path,
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
  return vnames_context_.empty()
             ? std::string(signature)
             : absl::StrCat(signature, "#", vnames_context_.top().signature);
}

std::string CreatePackageSignature(absl::string_view package_name) {
  return absl::StrCat(package_name, "#package");
}

std::string CreateModuleSignature(absl::string_view module_name) {
  return absl::StrCat(module_name, "#module");
}

std::string CreateVariableSignature(absl::string_view variable_name) {
  return absl::StrCat(variable_name, "#variable");
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
