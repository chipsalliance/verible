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

#include "verilog/tools/kythe/multi_file_kythe_facts_extractor.h"

#include <iostream>

#include "verilog/tools/kythe/kythe_facts_extractor.h"

namespace verilog {
namespace kythe {

void MultiFileKytheFactsExtractor::ExtractKytheFacts(
    const IndexingFactNode& root) {
  if (!files_scope_resolvers_.empty()) {
    files_scope_resolvers_.push_back(
        ScopeResolver(files_scope_resolvers_.back()));
  } else {
    files_scope_resolvers_.push_back(ScopeResolver(nullptr));
  }

  KytheFactsExtractor kythe_extractor(GetFilePathFromRoot(root), &std::cout,
                                      &files_scope_resolvers_.back());
  kythe_extractor.ExtractKytheFacts(root);
}

}  // namespace kythe
}  // namespace verilog
