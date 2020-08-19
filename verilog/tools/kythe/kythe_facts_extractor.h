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
#include "verilog/tools/kythe/indexing_facts_tree_context.h"
#include "verilog/tools/kythe/indexing_facts_tree_extractor.h"
#include "verilog/tools/kythe/kythe_facts.h"
#include "verilog/tools/kythe/vname_context.h"

namespace verilog {
namespace kythe {

// Responsible for traversing IndexingFactsTree and processing its different
// nodes to produce kythe indexing facts.
class KytheFactsExtractor {
 public:
  using VNameContextAutoPop = VNameContext::AutoPop;

  explicit KytheFactsExtractor(absl::string_view file_path)
      : file_path_(file_path) {}

  void Visit(const IndexingFactNode&);

 private:
  // Extracts kythe facts from file node and returns it VName.
  VName ExtractFileFact(const IndexingFactNode&);

  // Extracts kythe facts from module instance node and returns it VName.
  VName ExtractModuleInstanceFact(const IndexingFactNode&);

  // Extracts kythe facts from module node and returns it VName.
  VName ExtractModuleFact(const IndexingFactNode&);

  // Extracts kythe facts from module port node and returns its VName.
  VName ExtractVariableDefinition(const IndexingFactNode& node);

  // Extracts kythe facts from a module port reference node and returns its
  // VName.
  VName ExtractVariableReference(const IndexingFactNode& node);

  // The verilog file name which the facts are extracted from.
  std::string file_path_;

  // Keeps track of VNames of ancestors as the visitor traverses the facts
  // tree.
  VNameContext vnames_context_;
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

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_KYTHE_FACTS_EXTRACTOR_H_
