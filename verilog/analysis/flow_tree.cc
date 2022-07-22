// Copyright 2017-2020 The Verible Authors.
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

// Constructs the control flow tree, which determines the edge from each node
// (token index) to the next possible childs, And save edge_from_iterator in
// edges_.
absl::Status FlowTree::GenerateControlFlowTree() {
  // Adding edges for if blocks.
  int token_index = 0;
  int current_token_enum = 0;
  for (auto current_token : source_sequence_) {
    current_token_enum = current_token.token_enum();

    if (current_token_enum == PP_ifdef || current_token_enum == PP_ifndef) {
      ifs_.push_back(token_index);  // add index of `ifdef and `ifndef to ifs_.
      elses_[ifs_.back()].push_back(
          token_index);  // also add edge_from_iterator to elses_ as if will
                         // help marking edges later on.

    } else if (current_token_enum == PP_else ||
               current_token_enum == PP_elsif ||
               current_token_enum == PP_endif) {
      elses_[ifs_.back()].push_back(
          token_index);  // add index of `elsif, `else and `endif to else_ of
                         // the last recent if block.
      if (current_token_enum ==
          PP_endif) {  // if the current token is an `endif, then we are ready
                       // to create edges for this if block.
        auto& current_if_block =
            elses_[ifs_.back()];  // current_if_block contains all indexes of
                                  // ifs and elses in the latest block.

        // Adding edges for each index in the if block using a nested loop.
        for (auto edge_from_iterator = current_if_block.begin();
             edge_from_iterator != current_if_block.end();
             edge_from_iterator++) {
          for (auto edge_to_iterator = edge_from_iterator + 1;
               edge_to_iterator != current_if_block.end(); edge_to_iterator++) {
            if (edge_from_iterator == current_if_block.begin() &&
                edge_to_iterator == current_if_block.end() - 1 &&
                current_if_block.size() > 2)
              continue;  // skip edges from `if to `endif if there is an else in
                         // this bloc wheneven there is an else in this block.
            edges_[*edge_from_iterator].push_back(
                *edge_to_iterator +
                (edge_to_iterator !=
                 current_if_block.end() - 1));  // add the possible edge.
          }
        }
        ifs_.pop_back();  // the if block edges were added, ready to pop it.
      }
    }
    token_index++;  // increment the token index.
  }

  // Adding edges for non-if blocks.
  token_index = 0;
  for (auto current_token : source_sequence_) {
    current_token_enum = current_token.token_enum();
    if (current_token_enum != PP_else && current_token_enum != PP_elsif) {
      if (token_index > 0)
        edges_[token_index - 1].push_back(
            token_index);  // edges from a token to the one coming after it
                           // directly.
    } else {
      if (token_index > 0)
        edges_[token_index - 1].push_back(
            edges_[token_index]
                .back());  // edges from the last token in `ifdef/`ifndef body
                           // to `endif from the same if block.
    }
    token_index++;  // increment the token index.
  }

  return absl::OkStatus();
}

// Traveses the control flow tree in a depth first manner, appending the visited
// tokens to current_sequence_, then adding current_sequence_ to variants_ upon
// completing.
absl::Status FlowTree::DepthFirstSearch(int current_node_index) {
  // skips preprocessor directives so that current_sequence_ doesn't contain
  // any.
  const auto& current_token = source_sequence_[current_node_index];
  if (current_token.token_enum() != PP_Identifier &&
      current_token.token_enum() != PP_ifndef &&
      current_token.token_enum() != PP_ifdef &&
      current_token.token_enum() != PP_define &&
      current_token.token_enum() != PP_define_body &&
      current_token.token_enum() != PP_elsif &&
      current_token.token_enum() != PP_else &&
      current_token.token_enum() != PP_endif)
    current_sequence_.push_back(current_token);

  // do recursive search through every possible edge.
  for (auto next_node : edges_[current_node_index]) {
    if (auto status = FlowTree::DepthFirstSearch(next_node); !status.ok()) {
      std::cerr << "ERROR: DepthFirstSearch fails\n";
      return status;
    }
  }
  if (current_node_index ==
      int(source_sequence_.size()) -
          1) {  // if the current node is the last one, push the completed
                // current_sequence_ to variants_.
    variants_.push_back(current_sequence_);
  }
  if (current_token.token_enum() != PP_Identifier &&
      current_token.token_enum() != PP_ifndef &&
      current_token.token_enum() != PP_ifdef &&
      current_token.token_enum() != PP_define &&
      current_token.token_enum() != PP_define_body &&
      current_token.token_enum() != PP_elsif &&
      current_token.token_enum() != PP_else &&
      current_token.token_enum() != PP_endif)
    current_sequence_
        .pop_back();  // remove tokens to back track into other variants.
  return absl::OkStatus();
}

}  // namespace verilog
