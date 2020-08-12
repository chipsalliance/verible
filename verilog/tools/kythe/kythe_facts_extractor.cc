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

#include <string>

#include "absl/strings/substitute.h"
#include "kythe_facts_extractor.h"

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
  }

  const AutoPop p(&facts_tree_context_, &node);
  for (auto& child : node.Children()) {
    Visit(child);
  }
}

void KytheFactsExtractor::ExtractFileFact(IndexingFactNode& node) {

}

void KytheFactsExtractor::ExtractModuleFact(IndexingFactNode& node) {}

void KytheFactsExtractor::ExtractModuleInstanceFact(IndexingFactNode& node) {}

std::string VName(const std::string& signature, const std::string& path,
                  const std::string& language, const std::string& root,
                  const std::string& corpus) {
  return absl::Substitute(
      R"({
                     "signature": $0,
                     "path": $1,
                     "language": $2,
                     "root": $3,
                     "corpus": $4
                 })",
      signature, path, language, root, corpus);
}

std::string fact(const std::string& node_vname, const std::string& fact_name,
                 const std::string& fact_val) {
  return absl::Substitute(
      R"({
                     "source": $0,
                     "fact_name": /kythe/$1,
                     "fact_value": $2
                 })",
      node_vname, fact_name, fact_val);
}

std::string edge(const std::string& source, const std::string& edge_name,
                 const std::string& target) {
  return absl::Substitute(
      R"({
                     "source": $0,
                     "edge_kind": /kythe/edge/$1,
                     "target": $2,
                     "fact_name": "/"
                })",
      source, edge_name, target);
}

std::string anchorVName(const std::string& file_vname, const std::string& begin,
                        const std::string& end) {
  return vname("@" + begin + ":" + end, file_vname.path, "ex", kRoot, kCorpus);
}

std::string anchor(const std::string& file_vname, const std::string& begin,
                   const std::string& end) {
  z var anchor_vname = anchorVName(file_vname, begin, end);
  return [
    fact(anchor_vname, "node/kind", "anchor"),
    fact(anchor_vname, "loc/start", begin.toString()),
    fact(anchor_vname, "loc/end", end.toString()),
  ];
}

}  // namespace kythe
}  // namespace verilog
