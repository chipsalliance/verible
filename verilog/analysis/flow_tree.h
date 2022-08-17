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

#include <bitset>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "common/lexer/token_stream_adapter.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {
using TokenSequenceConstIterator = verible::TokenSequence::const_iterator;

// Receive a complete token sequence of one variant.
// 1st argument: generated variant.
// 2nd argument: number of generated variants (to decide if more is needed).
// 3rd argument: checks if the variant is ready to be sent.
using VariantReceiver =
    std::function<bool(const verible::TokenSequence &, const int, const bool)>;

struct ConditionalBlock {
  TokenSequenceConstIterator ifdef_iterator;
  TokenSequenceConstIterator ifndef_iterator;
  std::vector<TokenSequenceConstIterator> elsif_iterators;
  TokenSequenceConstIterator else_iterator;
  TokenSequenceConstIterator endif_iterator;
};

// FlowTree class builds the control flow graph of a tokenized System-Verilog
// source code. Furthermore, enabling doing the following queries on the graph:
// 1) Generating all the possible variants (provided via a callback function).
// 2) TODO(karimtera): uniquely identify a variant with a bitset.
class FlowTree {
 public:
  explicit FlowTree(verible::TokenSequence source_sequence)
      : source_sequence_(std::move(source_sequence)){};

  // Constructs the control flow tree by adding the tree edges in edges_.
  absl::Status GenerateControlFlowTree();

  // Generates all possible variants.
  absl::Status GenerateVariants(const VariantReceiver &receiver);

 private:
  // Traveses the tree in a depth first manner.
  absl::Status DepthFirstSearch(const VariantReceiver &receiver,
                                TokenSequenceConstIterator current_node,
                                std::bitset<128> assumed);

  // Checks if the iterator points to a conditonal directive (`ifdef/ifndef...).
  static bool IsConditional(TokenSequenceConstIterator iterator);

  //
  absl::Status AddBlockEdges(const ConditionalBlock &block);

  // The tree edges which defines the possible next childs of each token in
  // source_sequence_.
  std::map<TokenSequenceConstIterator, std::vector<TokenSequenceConstIterator>>
      edges_;

  // Extracts the conditional macro checked.
  static absl::Status MacroFollows(
      TokenSequenceConstIterator conditional_iterator);

  // Adds macro to conditional_macro_id_ map.
  absl::Status AddMacroOfConditionalToMap(
      TokenSequenceConstIterator conditional_iterator);

  int GetMacroIDOfConditional(TokenSequenceConstIterator conditional_iterator);

  // Holds all of the conditional blocks.
  std::vector<ConditionalBlock> if_blocks_;

  // The original source code lexed token seqouence.
  const verible::TokenSequence source_sequence_;

  // The variant's token sequence currrently being built by DepthFirstSearch.
  verible::TokenSequence current_sequence_;

  // Macros assumed in the variant that is currently begin built.
  // TODO(karimtera): Is it better to use dymanic_bitset?
  std::bitset<128> current_macros_;

  // Mapping each conditional macro to an integer ID,
  // to use it later as a bit offset.
  std::map<absl::string_view, int> conditional_macro_id_;

  // Number of macros appeared in `ifdef/`ifndef/`elsif.
  int conditional_macros_counter = 0;

  // Number of vairants generated so far.
  int variants_counter_ = 0;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_FLOW_TREE_H_
