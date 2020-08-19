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

#include <iostream>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/substitute.h"
#include "kythe_facts_extractor.h"
#include "kythe_schema_constants.h"

namespace verilog {
namespace kythe {

std::string GetFilePathFromRoot(const IndexingFactNode& root) {
  return root.Value().Anchors()[0].Value();
}

void ExtractKytheFacts(const IndexingFactNode& node) {
  KytheFactsExtractor kythe_extractor(GetFilePathFromRoot(node));
  kythe_extractor.Visit(node);
}

void KytheFactsExtractor::Visit(const IndexingFactNode& node) {
  const auto tag =
      static_cast<IndexingFactType>(node.Value().GetIndexingFactType());

  VName vname(file_path_);

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
      vname = ExtractModuleInstanceFact(node);
      break;
    }
    case IndexingFactType::kVariableDefinition: {
      vname = ExtractVariableDefinition(node);
      break;
    }
    case IndexingFactType::kVariableReference: {
      vname = ExtractVariableReference(node);
      break;
    }
  }

  if (tag != IndexingFactType::kFile) {
    std::cout << Edge(vname, kEdgeChildOf, vnames_context_.top());
  }

  const VNameContextAutoPop vnames_auto_pop(&vnames_context_, &vname);
  for (const verible::VectorTree<IndexingNodeData>& child : node.Children()) {
    Visit(child);
  }
}

VName KytheFactsExtractor::ExtractFileFact(const IndexingFactNode& node) {
  const VName file_vname(file_path_, "", "", "");

  std::cout << Fact(file_vname, kFactNodeKind, kNodeFile);
  std::cout << Fact(file_vname, kFactText, node.Value().Anchors()[1].Value());

  return file_vname;
}

VName KytheFactsExtractor::ExtractModuleFact(const IndexingFactNode& node) {
  const auto& anchors = node.Value().Anchors();
  const VName module_vname(file_path_,
                           CreateModuleSignature(anchors[0].Value()));

  std::cout << Fact(module_vname, kFactNodeKind, kNodeRecord);
  std::cout << Fact(module_vname, kFactSubkind, kSubkindModule);
  std::cout << Fact(module_vname, kFactComplete, kCompleteDefinition);

  const VName module_name_anchor = PrintAnchorVName(anchors[0], file_path_);

  std::cout << Edge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (node.Value().Anchors().size() > 1) {
    const VName module_end_label_anchor =
        PrintAnchorVName(anchors[1], file_path_);
    std::cout << Edge(module_end_label_anchor, kEdgeRef, module_vname);
  }

  return module_vname;
}

VName KytheFactsExtractor::ExtractModuleInstanceFact(
    const IndexingFactNode& node) {
  const auto& anchors = node.Value().Anchors();
  const VName module_instance_vname(
      file_path_,
      CreateVariableSignature(anchors[1].Value(), vnames_context_.top()));
  const VName module_instance_anchor = PrintAnchorVName(anchors[1], file_path_);
  const VName module_type_vname(file_path_,
                                CreateModuleSignature(anchors[0].Value()));
  const VName module_type_anchor = PrintAnchorVName(anchors[0], file_path_);

  std::cout << Fact(module_instance_vname, kFactNodeKind, kNodeVariable);
  std::cout << Fact(module_instance_vname, kFactComplete, kCompleteDefinition);

  std::cout << Edge(module_type_anchor, kEdgeRef, module_type_vname);
  std::cout << Edge(module_instance_vname, kEdgeTyped, module_type_vname);
  std::cout << Edge(module_instance_anchor, kEdgeDefinesBinding,
                    module_instance_vname);

  for (size_t i = 1; i < anchors.size(); i++) {
    const VName port_vname_reference(
        file_path_,
        CreateVariableSignature(anchors[i].Value(), vnames_context_.top()));
    const VName port_vname_definition(
        file_path_,
        CreateVariableSignature(anchors[i].Value(), vnames_context_.top()));
    const VName port_vname_anchor = PrintAnchorVName(anchors[i], file_path_);

    std::cout << Edge(port_vname_anchor, kEdgeRef, port_vname_definition);
  }

  return module_instance_vname;
}

VName KytheFactsExtractor::ExtractVariableDefinition(
    const IndexingFactNode& node) {
  const auto& anchor = node.Value().Anchors()[0];
  const VName variable_vname(
      file_path_,
      CreateVariableSignature(anchor.Value(), vnames_context_.top()));
  const VName variable_vname_anchor = PrintAnchorVName(anchor, file_path_);

  std::cout << Fact(variable_vname, kFactNodeKind, kNodeVariable);
  std::cout << Fact(variable_vname, kFactComplete, kCompleteDefinition);

  std::cout << Edge(variable_vname_anchor, kEdgeDefinesBinding, variable_vname);

  return variable_vname;
}

VName KytheFactsExtractor::ExtractVariableReference(
    const IndexingFactNode& node) {
  const auto& anchor = node.Value().Anchors()[0];
  const VName variable_vname(
      file_path_,
      CreateVariableSignature(anchor.Value(), vnames_context_.top()));
  const VName variable_vname_anchor = PrintAnchorVName(anchor, file_path_);

  std::cout << Edge(variable_vname_anchor, kEdgeRef, variable_vname);

  return variable_vname;
}

VName PrintAnchorVName(const Anchor& anchor, absl::string_view file_path) {
  const VName anchor_vname(file_path,
                           absl::Substitute(R"(@$0:$1)", anchor.StartLocation(),
                                            anchor.EndLocation()));

  std::cout << Fact(anchor_vname, kFactNodeKind, kNodeAnchor);
  std::cout << Fact(anchor_vname, kFactAnchorStart,
                    absl::Substitute(R"($0)", anchor.StartLocation()));
  std::cout << Fact(anchor_vname, kFactAnchorEnd,
                    absl::Substitute(R"($0)", anchor.EndLocation()));

  return anchor_vname;
}

std::string CreateModuleSignature(absl::string_view module_name) {
  return absl::StrCat(module_name, "#module");
}

std::string CreateVariableSignature(absl::string_view instance_name,
                                    const VName& parent_vname) {
  return absl::StrCat(instance_name, "#variable", parent_vname.signature);
}

}  // namespace kythe
}  // namespace verilog
