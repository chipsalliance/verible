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

namespace {

using AutoPop = AutoPopBack<KytheFactsExtractor::IndexingFactsTreeContext,
                            const IndexingFactNode*>;

}

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
  switch (tag) {
    case IndexingFactType::kFile: {
      ExtractFileFact(node);
      break;
    }
    case IndexingFactType::kModule: {
      ExtractModuleFact(node);
      break;
    }
    case IndexingFactType::kModuleInstance: {
      ExtractModuleInstanceFact(node);
      break;
    }
    case IndexingFactType::kModulePortInput:
      break;
    case IndexingFactType::kModulePortOutput:
      break;
  }

  const AutoPop p(&facts_tree_context_, &node);
  for (const verible::VectorTree<IndexingNodeData>& child : node.Children()) {
    Visit(child);
  }
}

void KytheFactsExtractor::ExtractFileFact(const IndexingFactNode& node) {
  const VName file_vname(file_path_, "", "", "");

  std::cout << Fact(file_vname, kFactNodeKind, kNodeFile);
  std::cout << Fact(file_vname, kFactText, node.Value().Anchors()[1].Value());
}

void KytheFactsExtractor::ExtractModuleFact(const IndexingFactNode& node) {
  const auto& anchors = node.Value().Anchors();
  const VName module_vname(file_path_,
                           CreateModuleSignature(anchors[0].Value()));

  std::cout << Fact(module_vname, kFactNodeKind, kNodeRecord);
  std::cout << Fact(module_vname, kFactSubkind, kSubkindModule);
  std::cout << Fact(module_vname, kFactComplete, kCompleteDefinition);

  const VName module_name_anchor = PrintAnchorVname(anchors[0], file_path_);

  std::cout << Edge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (node.Value().Anchors().size() > 1) {
    const VName module_end_label_anchor =
        PrintAnchorVname(anchors[1], file_path_);
    std::cout << Edge(module_end_label_anchor, kEdgeRef, module_vname);
  }
}

void KytheFactsExtractor::ExtractModuleInstanceFact(
    const IndexingFactNode& node) {
  const auto& anchors = node.Value().Anchors();
  const VName module_instance_vname(
      file_path_, CreateModuleInstantiationSignature(anchors[1].Value()));
  const VName module_instance_anchor = PrintAnchorVname(anchors[1], file_path_);
  const VName module_type_vname(file_path_,
                                CreateModuleSignature(anchors[0].Value()));
  const VName module_type_anchor = PrintAnchorVname(anchors[0], file_path_);

  std::cout << Fact(module_instance_vname, kFactNodeKind, kNodeVariable);
  std::cout << Fact(module_instance_vname, kFactComplete, kCompleteDefinition);

  std::cout << Edge(module_type_anchor, kEdgeRef, module_type_vname);
  std::cout << Edge(module_instance_vname, kEdgeTyped, module_type_vname);
  std::cout << Edge(module_instance_anchor, kEdgeDefinesBinding,
                    module_instance_vname);
}

VName PrintAnchorVname(const Anchor& anchor, absl::string_view file_path) {
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

std::string CreateModuleSignature(const absl::string_view module_name) {
  return absl::StrCat(module_name, "#module");
}

std::string CreateModuleInstantiationSignature(
    const absl::string_view instance_name) {
  return absl::StrCat(instance_name, "#variable#module");
}

std::ostream& operator<<(std::ostream& stream, const Fact& fact) {
  stream << absl::Substitute(
      R"({"source": $0,"fact_name": "$1","fact_value": "$2"})",
      fact.node_vname.ToString(), fact.fact_name, fact.fact_value);
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Edge& edge) {
  stream << absl::Substitute(
      R"({"source": $0,"edge_kind": "$1","target": $2,"fact_name": "/"})",
      edge.source_node.ToString(), edge.edge_name, edge.target_node.ToString());
  return stream;
}

}  // namespace kythe
}  // namespace verilog
