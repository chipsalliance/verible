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

#ifndef VERIBLE_VERILOG_FLOW_TREE_H_
#define VERIBLE_VERILOG_FLOW_TREE_H_

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "common/lexer/token_stream_adapter.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

class FlowTree {
 public:
  explicit FlowTree(verible::TokenSequence source_sequence)
      : source_sequence_(std::move(source_sequence)){};

  absl::Status GenerateControlFlowTree();  // constructs the control flow tree.
  absl::Status DepthFirstSearch(
      int index);  // travese the tree in a depth first manner.
  std::vector<verible::TokenSequence>
      variants_;  // a memory for all variants generated.

 private:
  std::vector<int>
      ifs_;  // vector of all `ifdef/`ifndef indexes in source_sequence_.
  std::map<int, std::vector<int>> elses_;  // indexes of `elsif/`else indexes in
                                           // source_sequence_ of each if block.
  std::map<int, std::vector<int>>
      edges_;  // the tree edges which defines the possible next childs of each
               // token in source_sequence_.
  verible::TokenSequence
      source_sequence_;  // the original source code lexed token seqouence.
  verible::TokenSequence
      current_sequence_;  // the variant's token sequence currrently being built
                          // by DepthFirstSearch.
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_FLOW_TREE_H_
