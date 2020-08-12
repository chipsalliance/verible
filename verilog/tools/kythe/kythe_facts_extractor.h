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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_

#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"

namespace verilog {
namespace kythe {

// Node vector name for kythe facts.
struct VName {
  explicit VName(absl::string_view p, absl::string_view s = "",
                 absl::string_view l = "verilog")
      : signature(absl::Base64Escape(s)), path(p), language(l) {}

  // Unique identifier for this VName.
  std::string signature = "";

  // Path for the file the name is extracted from.
  std::string path = "";

  // The language this name belongs to.
  std::string language = "";

  // The corpus of source code this VName belongs to
  const std::string corpus = "https://github.com/google/verible";

  // A directory path or project identifier inside the Corpus.
  const std::string root = "";
};

// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
class KytheFactsExtractor {
 public:
  explicit KytheFactsExtractor(const IndexingFactNode& root)
      : file_path_(root.Value().Anchors()[0].Value()) {}

  void Visit(IndexingFactNode&);

 private:
  // Extracts kythe facts from file node.
  void ExtractFileFact(IndexingFactNode&);

  // Extracts kythe facts from module instance node.
  void ExtractModuleInstanceFact(IndexingFactNode& tree);

  // Extracts kythe facts from module node.
  void ExtractModuleFact(IndexingFactNode& tree);

  // The verilog file name which the facts are extracted from.
  std::string file_path_;

  // Keeps track of facts tree ancestors as the visitor traverses the facts
  // tree.
  IndexingFactsTreeContext facts_tree_context_;
};

// Creates the signature for module names.
std::string CreateModuleSignature(const std::string&);

// Creates the signature for module instantiations.
std::string CreateModuleInstantiationSignature(const std::string&);

// Extracts Kythe facts from IndexingFactTree.
void ExtractKytheFacts(IndexingFactNode&);

// Generates the vnames used for kythe facts.
std::string PrintVName(const VName&);

// Generates a kythe indexing fact for the given vname, fact type and value.
void PrintFact(const VName& node_vname, absl::string_view fact_name,
               absl::string_view fact_val);

// Generates a kythe edge from some node to another in kythe graph.
void PrintEdge(const VName& source, absl::string_view edge_name,
               const VName& target);

// Generates an anchor VName for kythe.
VName PrintAnchorVname(const Anchor&, absl::string_view);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
