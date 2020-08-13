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

#include <string>
#include <utility>

#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"

namespace verilog {
namespace kythe {

namespace {
// Type that is used to keep track of the path to the root of indexing facts
// tree.
using IndexingFactsTreeContext2 = std::vector<const IndexingFactNode*>;

using AutoPop2 =
    AutoPopBack<IndexingFactsTreeContext2, const IndexingFactNode*>;

}  // namespace

// Node vector name for kythe facts.
struct VName {
  explicit VName(absl::string_view p, absl::string_view s = "",
                 absl::string_view r = "", absl::string_view l = "verilog",
                 absl::string_view c = "https://github.com/google/verible")
      : signature(absl::Base64Escape(s)),
        path(p),
        language(l),
        corpus(c),
        root(r) {}

  std::string ToString() const {
    return absl::Substitute(
        R"({"signature": "$0","path": "$1","language": "$2","root": "$3","corpus": "$4"})",
        signature, path, language, root, corpus);
  }

  // Unique identifier for this VName.
  std::string signature;

  // Path for the file the name is extracted from.
  std::string path;

  // The language this name belongs to.
  std::string language;

  // The corpus of source code this VName belongs to
  std::string corpus;

  // A directory path or project identifier inside the Corpus.
  std::string root;
};

// Facts for kythe.
struct Fact {
  Fact(VName vname, absl::string_view name, absl::string_view value)
      : node_vname(std::move(vname)),
        fact_name(name),
        fact_value(absl::Base64Escape(value)) {}

  // The vname of the node this fact is about.
  VName node_vname;

  // The name identifying this fact.
  std::string fact_name;

  // The given value to this fact.
  std::string fact_value;
};

// Edges for kythe.
struct Edge {
  Edge(VName source, absl::string_view name, VName target)
      : source_node(std::move(source)),
        edge_name(name),
        target_node(std::move(target)) {}

  // The vname of the source node of this edge.
  VName source_node;

  // The edge name which identifies the edge kind.
  std::string edge_name;

  // The vname of the target node of this edge.
  VName target_node;
};

// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
class KytheFactsExtractor {
 public:
  // Type that is used to keep track of the path to the root of indexing facts
  // tree.
  using IndexingFactsTreeContext = std::vector<const IndexingFactNode*>;

  explicit KytheFactsExtractor(absl::string_view file_path)
      : file_path_(file_path) {}

  void Visit(const IndexingFactNode&);

 private:
  // Extracts kythe facts from file node.
  void ExtractFileFact(const IndexingFactNode&);

  // Extracts kythe facts from module instance node.
  void ExtractModuleInstanceFact(const IndexingFactNode& tree);

  // Extracts kythe facts from module node.
  void ExtractModuleFact(const IndexingFactNode& tree);

  // The verilog file name which the facts are extracted from.
  std::string file_path_;

  // Keeps track of facts tree ancestors as the visitor traverses the facts
  // tree.
  IndexingFactsTreeContext facts_tree_context_;
};

// Creates the signature for module names.
std::string CreateModuleSignature(absl::string_view);

// Creates the signature for module instantiations.
std::string CreateModuleInstantiationSignature(absl::string_view);

// Extracts file path from indexing facts tree root.
std::string GetFilePathFromRoot(const IndexingFactNode&);

// Extracts Kythe facts from IndexingFactTree.
void ExtractKytheFacts(const IndexingFactNode&);

// Generates an anchor VName for kythe.
VName PrintAnchorVname(const Anchor&, absl::string_view);

std::ostream& operator<<(std::ostream&, const Fact&);

std::ostream& operator<<(std::ostream&, const Edge&);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
