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

#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/tools/kythe/indexing-facts-tree.h"
#include "verible/verilog/tools/kythe/kythe-facts.h"

namespace verilog {
namespace kythe {

// Streamable printing class for kythe facts.
// Usage: stream << KytheFactsPrinter(IndexingFactNode);
class KytheFactsPrinter {
 public:
  KytheFactsPrinter(const IndexingFactNode &file_list_facts_tree,
                    const VerilogProject &project, bool debug = false)
      : file_list_facts_tree_(file_list_facts_tree),
        project_(&project),
        debug_(debug) {}

  // Print Kythe facts as a stream of JSON entries (one per line). Note: single
  // facts are well formatted JSON, but the overall output isn't!
  std::ostream &PrintJsonStream(std::ostream &) const;

  // Print Kythe facts as a single, well formatted & human readable JSON.
  std::ostream &PrintJson(std::ostream &) const;

  friend std::ostream &operator<<(std::ostream &stream,
                                  const KytheFactsPrinter &kythe_facts_printer);

 private:
  // The root of the indexing facts tree to extract kythe facts from.
  const IndexingFactNode &file_list_facts_tree_;

  // This project manages the opening and path resolution of referenced files.
  const VerilogProject *const project_;

  // When debugging is enabled, print human-readable un-encoded text.
  const bool debug_;
};

std::ostream &operator<<(std::ostream &, const KytheFactsPrinter &);

// Output sink interface for producing the Kythe output.
class KytheOutput {
 public:
  // Output all Kythe facts from the indexing data.
  virtual void Emit(const Fact &fact) = 0;
  virtual void Emit(const Edge &edge) = 0;
  virtual ~KytheOutput() = default;
};

// Extract facts across an entire project.
// Extracts node tagged with kFileList where it iterates over every child node
// tagged with kFile from the begining and extracts the facts for each file.
// Currently, the file_list must be dependency-ordered for best results, that
// is, definitions of symbols should be encountered earlier in the file list
// than references to those symbols.
void StreamKytheFactsEntries(KytheOutput *kythe_output,
                             const IndexingFactNode &file_list_facts_tree,
                             const VerilogProject &project);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
