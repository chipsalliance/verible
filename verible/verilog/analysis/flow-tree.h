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
#include <functional>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "verible/common/text/token-stream-view.h"

namespace verilog {

// FlowTree class builds the control flow graph of a tokenized System-Verilog
// source code. Furthermore, enabling doing the following queries on the graph:
// - Generating all the possible variants (provided via a callback function).
class FlowTree {
 private:
  // 'kMaxDistinctMacros' shows the maximum number of distinct macros that can
  // be considered in conditonal directives.
  static constexpr int kMaxDistinctMacros = 128;

 public:
  using BitSet = std::bitset<kMaxDistinctMacros>;
  using TokenSequenceConstIterator = verible::TokenSequence::const_iterator;

  // "ConditionalBlock" saves locations of conditionals in a "TokenSequence".
  //  All locations should point inside this specific "TokenSequence".
  //  Since it is only used in conjunction with a "TokenSequence",
  //  It should be initialized to last location in this "TokenSequence",
  //  For example in "GenerateControlFlowTree" is initialized to
  //  'source_sequence_.end()'.

  struct Variant {
    // Contains the token sequence of the variant.
    verible::TokenSequence sequence;

    // The i-th bit in "macros_mask" is 1 when the macro (with ID = i) is
    // assumed to be defined, otherwise it is assumed to be undefined.
    BitSet macros_mask;

    // The i-th bit in "visited" is 1 when the macro (with ID = i) is visited or
    // assumed (either defined or not), otherwise it is not visited (its value
    // doesn't affect this variant).
    //
    // e.g.:
    //  `ifdef A
    //    `ifdef B
    //    ...
    //    `endif
    //  `endif
    //
    // Consider the variant in which A is undefined,
    // we notice that B doesn't affect the variant.
    // Then the bit corresponding to B in "visited" is 0.
    BitSet visited;
  };

  // Receive a complete token sequence of one variant.
  // variant_sequence: the generated variant token sequence.
  using VariantReceiver = std::function<bool(const Variant &variant)>;

  explicit FlowTree(verible::TokenSequence source_sequence)
      : source_sequence_(std::move(source_sequence)){};

  // Generates all possible variants.
  absl::Status GenerateVariants(const VariantReceiver &receiver);

  // Returns all the used macros in conditionals, ordered with the same ID as
  // used in BitSets.
  const std::vector<TokenSequenceConstIterator> &GetUsedMacros() {
    return conditional_macros_;
  }

 private:
  struct ConditionalBlock {
    ConditionalBlock(TokenSequenceConstIterator if_location,
                     TokenSequenceConstIterator non_location)
        : if_location(if_location),
          else_location(non_location),
          endif_location(non_location) {}

    // "if_location" points to `ifdef or `ifndef.
    TokenSequenceConstIterator if_location;
    std::vector<TokenSequenceConstIterator> elsif_locations;
    TokenSequenceConstIterator else_location;
    TokenSequenceConstIterator endif_location;
  };

  // Constructs the control flow tree by adding the tree edges in edges_.
  absl::Status GenerateControlFlowTree();

  // Traveses the tree in a depth first manner.
  absl::Status DepthFirstSearch(const VariantReceiver &receiver,
                                TokenSequenceConstIterator current_node);

  // Checks if the iterator points to a conditonal directive (`ifdef/ifndef...).
  static bool IsConditional(TokenSequenceConstIterator iterator);

  // Adds all edges withing a conditional block.
  absl::Status AddBlockEdges(const ConditionalBlock &block);

  // The tree edges which defines the possible next childs of each token in
  // source_sequence_.
  std::map<TokenSequenceConstIterator, std::vector<TokenSequenceConstIterator>>
      edges_;

  // Extracts the conditional macro checked.
  static absl::Status MacroFollows(
      TokenSequenceConstIterator conditional_iterator);

  // Adds macro to conditional_macros_ vector, and save its ID in
  // conditional_macro_id_ map.
  absl::Status AddMacroOfConditional(
      TokenSequenceConstIterator conditional_iterator);

  int GetMacroIDOfConditional(TokenSequenceConstIterator conditional_iterator);

  // Holds all of the conditional blocks.
  std::vector<ConditionalBlock> if_blocks_;

  // The original source code lexed token seqouence.
  const verible::TokenSequence source_sequence_;

  // Current variant being generated by DepthFirstSearch.
  Variant current_variant_;

  // A flag that determines if the VariantReceiver returned 'false'.
  // By default: it assumes VariantReceiver wants more variants.
  bool wants_more_ = true;

  // Mapping each conditional macro to an integer ID,
  // to use it later as a bit offset.
  std::map<std::string_view, int> conditional_macro_id_;

  // A vector containing all the macros used placed by their given ID.
  std::vector<TokenSequenceConstIterator> conditional_macros_;

  // Number of macros appeared in `ifdef/`ifndef/`elsif.
  int conditional_macros_counter_ = 0;
};

}  // namespace verilog

#endif  // VERIBLE_VERILOG_FLOW_TREE_H_
