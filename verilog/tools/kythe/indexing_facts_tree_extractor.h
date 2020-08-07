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

namespace verilog {
namespace kythe {

// Given a verilog file returns the extracted indexing facts tree.
IndexingFactNode ExtractOneFile(absl::string_view content,
                                absl::string_view filename,
                                int& exit_status,
                                bool& parse_ok);

// Given a root to CST this function traverses the tree and extracts and
// constructs the indexing facts tree.
IndexingFactNode BuildIndexingFactsTree(const verible::SyntaxTreeNode& root,
                                        absl::string_view base);

// No-Op function as leaf should be resolved by other functions.
void Extract(const verible::SyntaxTreeLeaf& leaf,
             IndexingFactNode& parent,
             absl::string_view base);

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
                      IndexingNodeData& parent,
                      absl::string_view base);

// Extracts modules headers and creates its corresponding fact tree.
void ExtractModuleHeader(const verible::SyntaxTreeNode& node,
                         IndexingNodeData& parent,
                         absl::string_view base);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
