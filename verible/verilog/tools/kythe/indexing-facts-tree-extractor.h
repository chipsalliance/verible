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

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/tools/kythe/indexing-facts-tree.h"

namespace verilog {
namespace kythe {

// Given a SystemVerilog project (set of files), extract and return the
// IndexingFactsTree for the given files.
// The returned tree will have the files as children and they will retain their
// original ordering from the file list.
IndexingFactNode ExtractFiles(std::string_view file_list_path,
                              VerilogProject *project,
                              const std::vector<std::string> &file_names,
                              std::vector<absl::Status> *errors = nullptr);

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_EXTRACTOR_H_
