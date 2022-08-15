// Copyright 2017-2022 The Verible Authors.
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
#include <stack>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "common/lexer/token_stream_adapter.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
using TokenSequenceConstIterator = verible::TokenSequence::const_iterator;

struct ConditionalBlock {
  // Start of a ifdef/ifndef block
  TokenSequenceConstIterator if_block_begin;

  // Subsequent else/elsif blocks
  std::vector<TokenSequenceConstIterator> else_block;
};

class FlowTree {
 public:
  explicit FlowTree(verible::TokenSequence source_sequence)
      : source_sequence_(std::move(source_sequence)){};

  // Constructs the control flow tree by adding the tree edges in edges_.
  absl::Status GenerateControlFlowTree();

  // Generates all possible variants.
  absl::Status GenerateVariants();

  // A memory for all variants generated.
  std::vector<verible::TokenSequence> variants_;

 private:
  // Traveses the tree in a depth first manner.
  absl::Status DepthFirstSearch(TokenSequenceConstIterator current_node);

  // The tree edges which defines the possible next childs of each token in
  // source_sequence_.
  std::map<TokenSequenceConstIterator, std::vector<TokenSequenceConstIterator>>
      edges_;

  // Holds all of the conditional blocks.
  std::vector<ConditionalBlock> if_blocks_;

  // The original source code lexed token seqouence.
  const verible::TokenSequence source_sequence_;

  // The variant's token sequence currrently being built by DepthFirstSearch.
  verible::TokenSequence current_sequence_;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_FLOW_TREE_H_
