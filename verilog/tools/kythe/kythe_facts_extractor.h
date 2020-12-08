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

#include <iosfwd>
#include <set>

#include "verilog/analysis/verilog_project.h"
#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/kythe_facts.h"

namespace verilog {
namespace kythe {

// Streamable printing class for kythe facts.
// Usage: stream << KytheFactsPrinter(IndexingFactNode);
class KytheFactsPrinter {
 public:
  KytheFactsPrinter(const IndexingFactNode& file_list_facts_tree,
                    const VerilogProject& project, bool debug = false)
      : file_list_facts_tree_(file_list_facts_tree),
        project_(&project),
        debug_(debug) {}

  std::ostream& Print(std::ostream&) const;

 private:
  // The root of the indexing facts tree to extract kythe facts from.
  const IndexingFactNode& file_list_facts_tree_;

  // This project manages the opening and path resolution of referenced files.
  const VerilogProject* const project_;

  // When debugging is enabled, print human-readable un-encoded text.
  const bool debug_;
};

std::ostream& operator<<(std::ostream&, const KytheFactsPrinter&);

// Extracted Kythe indexing facts and edges.
struct KytheIndexingData {
  // Extracted Kythe indexing facts.
  std::set<Fact> facts;

  // Extracted Kythe edges.
  std::set<Edge> edges;
};

// Extract facts across an entire project.
// Extracts node tagged with kFileList where it iterates over every child node
// tagged with kFile from the begining and extracts the facts for each file.
// Currently, the file_list must be dependency-ordered for best results, that
// is, definitions of symbols should be encountered earlier in the file list
// than references to those symbols.
KytheIndexingData ExtractKytheFacts(const IndexingFactNode& file_list,
                                    const VerilogProject& project);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
