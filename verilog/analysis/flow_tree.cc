// Copyright 2017-2022 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance wedge_from_iteratorh the
// License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in wredge_from_iteratoring,
// software distributed under the License is distributed on an "AS IS" BASIS,
// Wedge_from_iteratorHOUT WARRANTIES OR CONDedge_from_iteratorIONS OF ANY KIND,
// eedge_from_iteratorher express or implied. See the License for the specific
// language governing permissions and limedge_from_iteratorations under the
// License.

#include "verilog/analysis/flow_tree.h"

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/lexer/token_stream_adapter.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

absl::Status FlowTree::GenerateVariants(const VariantReceiver &receiver) {
  return DepthFirstSearch(receiver, source_sequence_.begin());
}

// Constructs the control flow tree, which determines the edge from each node
// (token index) to the next possible childs, And save edge_from_iterator in
// edges_.
absl::Status FlowTree::GenerateControlFlowTree() {
  // Adding edges for if blocks.
  int current_token_enum = 0;
  ConditionalBlock empty_conditional_block;

  for (TokenSequenceConstIterator iter = source_sequence_.begin();
       iter != source_sequence_.end(); iter++) {
    current_token_enum = iter->token_enum();
    if (current_token_enum == PP_ifdef || current_token_enum == PP_ifndef) {
      // Add iterator of `ifdef/`ifndef to if_blocks_.
      if_blocks_.push_back(empty_conditional_block);
      if_blocks_.back().if_block_begin = iter;

      // Also treat `ifdef/ifndef as an else, to help marking edges later on.
      if_blocks_.back().else_block.push_back(iter);

    } else if (current_token_enum == PP_else ||
               current_token_enum == PP_elsif ||
               current_token_enum == PP_endif) {
      // Add iterator of `elsif/`else/`endif to else_block of the last recent if
      // block.
      if_blocks_.back().else_block.push_back(iter);

      // If the current token is an `endif, then we are ready to create edges
      // for this if block.
      if (current_token_enum == PP_endif) {
        const auto &current_if_block = if_blocks_.back().else_block;

        // Adding edges for each index in the if block using a nested loop.
        for (auto edge_from_iterator = current_if_block.begin();
             edge_from_iterator != current_if_block.end();
             edge_from_iterator++) {
          for (auto edge_to_iterator = edge_from_iterator + 1;
               edge_to_iterator != current_if_block.end(); edge_to_iterator++) {
            // Skips edges from `if to `endif if there is `else in this block.
            if (edge_from_iterator == current_if_block.begin() &&
                edge_to_iterator == current_if_block.end() - 1 &&
                current_if_block.size() > 2) {
              continue;
            }

            edges_[*edge_from_iterator].push_back(
                *edge_to_iterator +
                (edge_to_iterator != current_if_block.end() - 1));
          }
        }
        // The if block edges were added successfully, ready to pop it.
        if_blocks_.pop_back();
      }
    }
  }

  // Adding edges for non-if blocks.
  for (TokenSequenceConstIterator iter = source_sequence_.begin();
       iter != source_sequence_.end(); iter++) {
    current_token_enum = iter->token_enum();
    if (current_token_enum != PP_else && current_token_enum != PP_elsif) {
      // Edges from a token to the one coming after it directly.
      if (iter != source_sequence_.begin()) edges_[iter - 1].push_back(iter);

    } else {
      if (iter != source_sequence_.begin()) {
        edges_[iter - 1].push_back(edges_[iter].back());
      }
    }
  }

  return absl::OkStatus();
}

// Traveses the control flow tree in a depth first manner, appending the visited
// tokens to current_sequence_, then provide the completed variant to the user
// using a callback function (VariantReceiver).
absl::Status FlowTree::DepthFirstSearch(
    const VariantReceiver &receiver, TokenSequenceConstIterator current_node) {
  if (!receiver(current_sequence_, variants_counter_, false)) {
    return absl::OkStatus();
  }
  // Skips directives so that current_sequence_ doesn't contain any.
  if (current_node->token_enum() != PP_Identifier &&
      current_node->token_enum() != PP_ifndef &&
      current_node->token_enum() != PP_ifdef &&
      current_node->token_enum() != PP_define &&
      current_node->token_enum() != PP_define_body &&
      current_node->token_enum() != PP_elsif &&
      current_node->token_enum() != PP_else &&
      current_node->token_enum() != PP_endif) {
    current_sequence_.push_back(*current_node);
  }

  // Do recursive search through every possible edge.
  for (auto next_node : edges_[current_node]) {
    if (auto status = FlowTree::DepthFirstSearch(receiver, next_node);
        !status.ok()) {
      std::cerr << "ERROR: DepthFirstSearch fails\n";
      return status;
    }
  }
  // If the current node is the last one, push the completed current_sequence_
  // to variants_.
  if (current_node == source_sequence_.end() - 1) {
    receiver(current_sequence_, variants_counter_, true);
    variants_counter_++;
  }
  if (current_node->token_enum() != PP_Identifier &&
      current_node->token_enum() != PP_ifndef &&
      current_node->token_enum() != PP_ifdef &&
      current_node->token_enum() != PP_define &&
      current_node->token_enum() != PP_define_body &&
      current_node->token_enum() != PP_elsif &&
      current_node->token_enum() != PP_else &&
      current_node->token_enum() != PP_endif) {
    // Remove tokens to back track into other variants.
    current_sequence_.pop_back();
  }
  return absl::OkStatus();
}

}  // namespace verilog
