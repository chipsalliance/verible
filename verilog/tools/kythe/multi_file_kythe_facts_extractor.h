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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_MULTI_FILE_KYTHE_FACTS_EXTRACTOR_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_MULTI_FILE_KYTHE_FACTS_EXTRACTOR_H_

#include <vector>

#include "verilog/tools/kythe/indexing_facts_tree.h"
#include "verilog/tools/kythe/scope_resolver.h"

namespace verilog {
namespace kythe {

// Responsible for extracting Kythe facts from multi-files in which they may
// have cross-files references.
// The given trees (files) should be ordered by dependency between files in
// which the first one doesn't depend on other files.
class MultiFileKytheFactsExtractor {
 public:
  // Extracts Kythe facts form the given Indexing facts tree and saves the
  // discovered scopes so that they can be used for definition resolving for the
  // next extracted files.
  void ExtractKytheFacts(const IndexingFactNode& root);

 private:
  // Saves the extracted scopes from every files.
  // This is used to find definition cross-file references while extracting
  // multi-files.
  std::vector<ScopeResolver> files_scope_resolvers_;
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_MULTI_FILE_KYTHE_FACTS_EXTRACTOR_H_
