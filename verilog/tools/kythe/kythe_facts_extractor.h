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

// Node vector name for kythe facts.
struct VName {
  explicit VName(absl::string_view path, absl::string_view signature = "",
                 absl::string_view root = "",
                 absl::string_view language = "verilog",
                 absl::string_view corpus = "https://github.com/google/verible")
      : signature(absl::Base64Escape(signature)),
        path(path),
        language(language),
        corpus(corpus),
        root(root) {}

  std::string ToString() const;

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
  Fact(const VName& vname, absl::string_view name, absl::string_view value)
      : node_vname(vname),
        fact_name(name),
        fact_value(absl::Base64Escape(value)) {}

  // The vname of the node this fact is about.
  const VName& node_vname;

  // The name identifying this fact.
  std::string fact_name;

  // The given value to this fact.
  std::string fact_value;
};

// Edges for kythe.
struct Edge {
  Edge(const VName& source, absl::string_view name, const VName& target)
      : source_node(source), edge_name(name), target_node(target) {}

  // The vname of the source node of this edge.
  const VName& source_node;

  // The edge name which identifies the edge kind.
  std::string edge_name;

  // The vname of the target node of this edge.
  const VName& target_node;
};

// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
class KytheFactsExtractor {
 public:
  // Type that is used to keep track of the path to the root of indexing facts
  // tree.
  using IndexingFactsTreeContext = std::vector<const IndexingFactNode*>;

  // Type that is used to keep track of the vnames of ancestors
  using VNamesContext = std::vector<const VName*>;

  using FactTreeContextAutoPop =
      AutoPopBack<IndexingFactsTreeContext, const IndexingFactNode*>;

  using VNameContextAutoPop = AutoPopBack<VNamesContext, const VName*>;

  explicit KytheFactsExtractor(absl::string_view file_path)
      : file_path_(file_path) {}

  void Visit(const IndexingFactNode&);

 private:
  // Extracts kythe facts from file node.
  VName ExtractFileFact(const IndexingFactNode&);

  // Extracts kythe facts from module instance node.
  VName ExtractModuleInstanceFact(const IndexingFactNode&);

  // Extracts kythe facts from module node.
  VName ExtractModuleFact(const IndexingFactNode&);

  // Extracts kythe facts from module port node.
  VName ExtractModulePort(const IndexingFactNode&);

  // The verilog file name which the facts are extracted from.
  std::string file_path_;

  // Keeps track of facts tree ancestors as the visitor traverses the facts
  // tree.
  IndexingFactsTreeContext facts_tree_context_;

  // Keeps track of VNames of ancestors as the visitor traverses the facts
  // tree.
  VNamesContext vnames_context_;
};

// Creates the signature for module names.
std::string CreateModuleSignature(absl::string_view);

// Creates the signature for module instantiations.
std::string CreateVariableSignature(absl::string_view, const VName&);

// Extracts file path from indexing facts tree root.
std::string GetFilePathFromRoot(const IndexingFactNode&);

// Extracts Kythe facts from IndexingFactTree.
void ExtractKytheFacts(const IndexingFactNode&);

// Generates an anchor VName for kythe.
VName PrintAnchorVName(const Anchor&, absl::string_view);

std::ostream& operator<<(std::ostream&, const VName&);

std::ostream& operator<<(std::ostream&, const Fact&);

std::ostream& operator<<(std::ostream&, const Edge&);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
