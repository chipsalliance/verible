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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_

#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"

class IndexingFactsTreeExtractor {
 public:
  // Given a root to CST this function traverses the tree and extracts and
  // constructs the indexing facts tree.
  IndexingFactNode ConstructIndexingFactsTree(
      const verible::SyntaxTreeNode& root,
      absl::string_view base);

 private:
  // Searches the children of current node for a child with the given tag.
  const verible::Symbol* GetChildByTag(const verible::SyntaxTreeNode& root,
                                       verilog::NodeEnum tag);

  // Searches current subtree of CST for a child with the given tag.
  const verible::Symbol* GetFirstChildByTag(const verible::SyntaxTreeNode& root,
                                            verilog::NodeEnum tag);

  // No-Op function as leaf should be resolved by other functions.
  void Extract(const verible::SyntaxTreeLeaf& leaf,
               IndexingFactNode& parent,
               absl::string_view base){};

  // Directs the extraction to the correct function suitable for current node
  // tag.
  void Extract(const verible::SyntaxTreeNode& root,
               IndexingFactNode& parent,
               absl::string_view base);

  // Extracts modules and creates its corresponding fact tree.
  void ExtractModule(const verible::SyntaxTreeNode& node,
                     IndexingFactNode& parent,
                     absl::string_view base);

  // Extracts modules instantiations and creates its corresponding fact tree.
  void ExtractModuleInstantiation(const verible::SyntaxTreeNode& node,
                                  IndexingFactNode& parent,
                                  absl::string_view base);

  // Extracts endmodule and creates its corresponding fact tree.
  void ExtractModuleEnd(const verible::SyntaxTreeNode& node,
                        IndexingFactNode& parent,
                        absl::string_view base);

  // Extracts modules headers and creates its corresponding fact tree.
  void ExtractModuleHeader(const verible::SyntaxTreeNode& node,
                           IndexingFactNode& parent,
                           absl::string_view base);
};

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
