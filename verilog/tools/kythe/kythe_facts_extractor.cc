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

void ExtractKytheFacts(IndexingFactNode& node) {
  KytheFactsExtractor kythe_extractor(node);
  kythe_extractor.Visit(node);
}

void KytheFactsExtractor::Visit(IndexingFactNode& node) {
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
  for (auto& child : node.Children()) {
    Visit(child);
  }
}

void KytheFactsExtractor::ExtractFileFact(IndexingFactNode& node) {
  VName file_vname(file_path_, "", "");

  PrintFact(file_vname, kFactNodeKind, kNodeFile);
  PrintFact(file_vname, kFactText, node.Value().Anchors()[1].Value());
}

void KytheFactsExtractor::ExtractModuleFact(IndexingFactNode& node) {
  VName module_vname(file_path_,
                     CreateModuleSignature(node.Value().Anchors()[0].Value()));

  PrintFact(module_vname, kFactNodeKind, kNodeRecord);
  PrintFact(module_vname, kFactSubkind, kSubkindModule);
  PrintFact(module_vname, kFactComplete, kCompleteDefinition);

  VName module_name_anchor =
      PrintAnchorVname(node.Value().Anchors()[0], file_path_);

  PrintEdge(module_name_anchor, kEdgeDefinesBinding, module_vname);

  if (node.Value().Anchors().size() > 1) {
    VName module_end_label_anchor =
        PrintAnchorVname(node.Value().Anchors()[1], file_path_);
    PrintEdge(module_end_label_anchor, kEdgeRef, module_vname);
  }
}

void KytheFactsExtractor::ExtractModuleInstanceFact(IndexingFactNode& node) {
  VName module_instance_vname(
      file_path_,
      CreateModuleInstantiationSignature(node.Value().Anchors()[1].Value()));
  VName module_instance_anchor =
      PrintAnchorVname(node.Value().Anchors()[1], file_path_);
  VName module_type_vname(
      file_path_, CreateModuleSignature(node.Value().Anchors()[0].Value()));
  VName module_type_anchor =
      PrintAnchorVname(node.Value().Anchors()[0], file_path_);

  PrintFact(module_instance_vname, kFactNodeKind, kNodeVariable);
  PrintFact(module_instance_vname, kFactComplete, kCompleteDefinition);

  PrintEdge(module_type_anchor, kEdgeRef, module_type_vname);
  PrintEdge(module_instance_vname, kEdgeTyped, module_type_vname);
  PrintEdge(module_instance_anchor, kEdgeDefinesBinding, module_instance_vname);
}

std::string PrintVName(const VName& vname) {
  return absl::Substitute(
      R"({"signature": "$0","path": "$1","language": "$2","root": "$3","corpus": "$4"})",
      vname.signature, vname.path, vname.language, vname.root, vname.corpus);
}

void PrintFact(const VName& node_vname, absl::string_view fact_name,
               absl::string_view fact_val) {
  std::cout << absl::Substitute(
                   R"({"source": $0,"fact_name": "$1","fact_value": "$2"})",
                   PrintVName(node_vname), fact_name,
                   absl::Base64Escape(fact_val))
            << '\n';
}

void PrintEdge(const VName& source, absl::string_view edge_name,
               const VName& target) {
  std::cout
      << absl::Substitute(
             R"({"source": $0,"edge_kind": "$1","target": $2,"fact_name": "/"})",
             PrintVName(source), edge_name, PrintVName(target))
      << '\n';
}

VName PrintAnchorVname(const Anchor& anchor, absl::string_view file_path) {
  VName anchor_vname(file_path,
                     absl::Substitute(R"(@$0:$1)", anchor.StartLocation(),
                                      anchor.EndLocation()));

  PrintFact(anchor_vname, kFactNodeKind, kNodeAnchor);
  PrintFact(anchor_vname, kFactAnchorStart,
            absl::Substitute(R"($0)", anchor.StartLocation()));
  PrintFact(anchor_vname, kFactAnchorEnd,
            absl::Substitute(R"($0)", anchor.EndLocation()));

  return anchor_vname;
}

std::string CreateModuleSignature(const std::string& module_name) {
  return module_name + "#module";
}

std::string CreateModuleInstantiationSignature(
    const std::string& instance_name) {
  return instance_name + "#variable#module";
}

}  // namespace kythe
}  // namespace verilog
