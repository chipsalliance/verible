// Copyright 2017-2022 The Verible Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "verilog/analysis/flow_tree.h"

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/util/interval_set.h"
#include "common/util/logging.h"
#include "common/util/status_macros.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

// Adds edges within a conditonal block.
// Such that the first edge represents the condition being true,
// and the second edge represents the condition being false.
absl::Status FlowTree::AddBlockEdges(const ConditionalBlock &block) {
  bool contains_elsif = !block.elsif_locations.empty();
  bool contains_else = block.else_location != source_sequence_.end();

  // Handling `ifdef/ifndef.

  // Assuming the condition is true.
  edges_[block.if_location].push_back(block.if_location + 1);

  // Assuming the condition is false.
  // Checking if there is an `elsif.
  if (contains_elsif) {
    // Add edge to the first `elsif in the block.
    edges_[block.if_location].push_back(block.elsif_locations[0]);
  } else if (contains_else) {
    // Checking if there is an `else.
    edges_[block.if_location].push_back(block.else_location);
  } else {
    // `endif exists.
    edges_[block.if_location].push_back(block.endif_location);
  }

  // Handling `elsif.
  if (contains_elsif) {
    for (auto iter = block.elsif_locations.begin();
         iter != block.elsif_locations.end(); iter++) {
      // Assuming the condition is true.
      edges_[*iter].push_back((*iter) + 1);

      // Assuming the condition is false.
      if (iter + 1 != block.elsif_locations.end()) {
        edges_[*iter].push_back(*(iter + 1));
      } else if (contains_else) {
        edges_[*iter].push_back(block.else_location);
      } else {
        edges_[*iter].push_back(block.endif_location);
      }
    }
  }

  // Handling `else.
  if (contains_else) {
    edges_[block.else_location].push_back(block.else_location + 1);
  }

  // For edges that are generated assuming the conditons are true,
  // We need to add an edge from the end of the condition group of lines to
  // `endif, e.g. `ifdef
  //    <line1>
  //    <line2>
  //    ...
  //    <line_final>
  // `else
  //    <group_of_lines>
  // `endif
  // Edge to be added: from <line_final> to `endif.
  edges_[block.endif_location - 1].push_back(block.endif_location);
  if (contains_elsif) {
    for (auto iter : block.elsif_locations) {
      edges_[iter - 1].push_back(block.endif_location);
    }
  }
  if (contains_else) {
    edges_[block.else_location - 1].push_back(block.endif_location);
  }

  // Connecting `endif to the next token directly (if not EOF).
  auto next_iter = block.endif_location + 1;
  if (next_iter != source_sequence_.end() &&
      next_iter->token_enum() != PP_else &&
      next_iter->token_enum() != PP_elsif &&
      next_iter->token_enum() != PP_endif) {
    edges_[block.endif_location].push_back(next_iter);
  }

  return absl::OkStatus();
}

// Checks if the iterator is pointing to a conditional directive.
bool FlowTree::IsConditional(TokenSequenceConstIterator iterator) {
  auto current_node = iterator->token_enum();
  return current_node == PP_ifndef || current_node == PP_ifdef ||
         current_node == PP_elsif || current_node == PP_else ||
         current_node == PP_endif;
}

// Checks if after the conditional_iterator (`ifdef/`ifndef... ) there exists
// a macro identifier.
absl::Status FlowTree::MacroFollows(
    TokenSequenceConstIterator conditional_iterator) {
  if (conditional_iterator->token_enum() != PP_ifdef &&
      conditional_iterator->token_enum() != PP_ifndef &&
      conditional_iterator->token_enum() != PP_elsif) {
    return absl::InvalidArgumentError("Error macro name can't be extracted.");
  }
  auto macro_iterator = conditional_iterator + 2;
  if (macro_iterator->token_enum() != PP_Identifier) {
    return absl::InvalidArgumentError("Expected identifier for macro name.");
  }
  return absl::OkStatus();
}

// Adds a conditional macro to conditional_macros_ if not added before,
// And gives it a new ID, then saves the ID in conditional_macro_id_ map.
absl::Status FlowTree::AddMacroOfConditional(
    TokenSequenceConstIterator conditional_iterator) {
  auto status = MacroFollows(conditional_iterator);
  if (!status.ok()) {
    return absl::InvalidArgumentError(
        "Error no macro follows the conditional directive.");
  }
  auto macro_iterator = conditional_iterator + 2;
  auto macro_identifier = macro_iterator->text();
  if (conditional_macro_id_.find(macro_identifier) ==
      conditional_macro_id_.end()) {
    conditional_macro_id_[macro_identifier] = conditional_macros_counter_;
    conditional_macros_.push_back(macro_iterator);
    conditional_macros_counter_++;
  }
  return absl::OkStatus();
}

// Gets the conditonal macro ID from the conditional_macro_id_.
// Note: conditional_iterator is pointing to the conditional.
int FlowTree::GetMacroIDOfConditional(
    TokenSequenceConstIterator conditional_iterator) {
  auto status = MacroFollows(conditional_iterator);
  if (!status.ok()) {
    // TODO(karimtera): add a better error handling.
    return -1;
  }
  auto macro_iterator = conditional_iterator + 2;
  auto macro_identifier = macro_iterator->text();
  // It is always assumed that the macro already exists in the map.
  return conditional_macro_id_[macro_identifier];
}

// An API that provides a callback function to receive variants.
absl::Status FlowTree::GenerateVariants(const VariantReceiver &receiver) {
  auto status = GenerateControlFlowTree();
  if (!status.ok()) {
    return status;
  }
  return DepthFirstSearch(receiver, source_sequence_.begin());
}

absl::StatusOr<FlowTree::DefineVariants> FlowTree::MinCoverDefineVariants() {
  auto status = GenerateControlFlowTree();
  if (!status.ok()) return status;
  verible::IntervalSet<int64_t> covered;       // Tokens covered by
                                               // MinCoverDefineVariants.
  verible::IntervalSet<int64_t> last_covered;  // Tokens covered
                                               // by the previous iterations.
  DefineVariants define_variants;  // The result – all define variants that
                                   // should cover the entire source
  DefineSet visited;  // Visited defines are ones that are assumed to be defined
                      // or undefined (decided in a previous iteration)
  const int64_t tok_count = static_cast<int64_t>(source_sequence_.size());
  while (!covered.Contains({0, tok_count})) {
    DefineSet defines;  // Define sets are moved into the define variants list,
                        // so we make a new one each iteration
    visited.clear();    // We keep the visited set to avoid unnecessary
                        // allocations, but clear it each iteration
    TokenSequenceConstIterator tok_it = source_sequence_.begin();
    while (tok_it < source_sequence_.end()) {
      covered.Add(std::distance(source_sequence_.begin(), tok_it));
      if (tok_it->token_enum() == PP_ifdef ||
          tok_it->token_enum() == PP_ifndef ||
          tok_it->token_enum() == PP_elsif) {
        const auto macro_id_it = tok_it + 2;
        auto macro_text = macro_id_it->text();
        bool negated = tok_it->token_enum() == PP_ifndef;
        // If this macro was already visited (either defined/undefined), we
        // to stick to the same branch. TODO: handle `defines
        if (visited.contains(macro_text)) {
          bool assume_condition_is_true =
              (negated ^ defines.contains(macro_text));
          tok_it = edges_[tok_it][assume_condition_is_true ? 0 : 1];
        } else {
          // First time we see this macro; mark as visited
          visited.insert(macro_text);
          const auto if_it = edges_[tok_it][0];
          const auto if_idx = std::distance(source_sequence_.begin(), if_it);
          const auto else_it = edges_[tok_it][1];
          const auto else_idx =
              std::distance(source_sequence_.begin(), else_it);
          if (!covered.Contains({if_idx, else_idx})) {
            // If the `ifdef is not covered, we assume the condition is true
            if (!negated) defines.insert(macro_text);
            tok_it = if_it;
          } else {
            // Else we assume the condition is false
            if (negated) defines.insert(macro_text);
            tok_it = else_it;
          }
        }
      } else {
        const auto it = edges_.find(tok_it);
        if (it == edges_.end() || it->second.empty()) {
          // If there's no outgoing edge, just move to the next token.
          tok_it++;
        } else {
          // Else jump
          tok_it = edges_[tok_it][0];
        }
      }
    }
    define_variants.push_back(std::move(defines));
    // To prevent an infinite loop, if nothing new was covered, break.
    if (last_covered == covered) {
      // TODO: If there are nested `ifdefs that contradict each other early in
      // the source, this will prevent us from traversing the rest of the flow
      // tree. It would be better to detect this case, assume that the
      // contradicting part is covered, and continue the analysis.
      VLOG(4) << "Giving up on finding all define variants";
      break;  // Perhaps we should error?
    }
    last_covered = covered;
  }
  VLOG(4) << "Done generating define variants. Coverage: " << covered;
  return define_variants;
}

// Constructs the control flow tree, which determines the edge from each node
// (token index) to the next possible childs, And save edge_from_iterator in
// edges_.
absl::Status FlowTree::GenerateControlFlowTree() {
  // Adding edges for if blocks.
  const TokenSequenceConstIterator non_location = source_sequence_.end();

  for (TokenSequenceConstIterator iter = source_sequence_.begin();
       iter != source_sequence_.end(); iter++) {
    int current_token_enum = iter->token_enum();

    if (IsConditional(iter)) {
      switch (current_token_enum) {
        case PP_ifdef:
        case PP_ifndef: {
          if_blocks_.emplace_back(iter, non_location);
          auto status = AddMacroOfConditional(iter);
          if (!status.ok()) {
            return absl::InvalidArgumentError(
                "ERROR: couldn't give a macro an ID.");
          }
          break;
        }
        case PP_elsif: {
          if (if_blocks_.empty()) {
            return absl::InvalidArgumentError("ERROR: Unmatched `elsif.");
          }
          if_blocks_.back().elsif_locations.push_back(iter);
          auto status = AddMacroOfConditional(iter);
          if (!status.ok()) {
            return absl::InvalidArgumentError(
                "ERROR: couldn't give a macro an ID.");
          }
          break;
        }
        case PP_else: {
          if (if_blocks_.empty()) {
            return absl::InvalidArgumentError("ERROR: Unmatched `else.");
          }
          if_blocks_.back().else_location = iter;
          break;
        }
        case PP_endif: {
          if (if_blocks_.empty()) {
            return absl::InvalidArgumentError("ERROR: Unmatched `endif.");
          }
          if_blocks_.back().endif_location = iter;
          auto status = AddBlockEdges(if_blocks_.back());
          if (!status.ok()) return status;
          // TODO(karimtera): add an error message.
          if_blocks_.pop_back();
          break;
        }
      }

    } else {
      // Only add normal edges if the next token is not `else/`elsif/`endif.
      auto next_iter = iter + 1;
      if (next_iter != source_sequence_.end() &&
          next_iter->token_enum() != PP_else &&
          next_iter->token_enum() != PP_elsif &&
          next_iter->token_enum() != PP_endif) {
        edges_[iter].push_back(next_iter);
      }
    }
  }

  // Checks for uncompleted conditionals.
  if (!if_blocks_.empty()) {
    return absl::InvalidArgumentError(
        "ERROR: Uncompleted conditional is found.");
  }
  return absl::OkStatus();
}

// Traveses the control flow tree in a depth first manner, appending the visited
// tokens to current_variant_, then provide the completed variant to the user
// using a callback function (VariantReceiver).
absl::Status FlowTree::DepthFirstSearch(
    const VariantReceiver &receiver, TokenSequenceConstIterator current_node) {
  if (!wants_more_) return absl::OkStatus();

  // Skips directives so that current_variant_ doesn't contain any.
  if (current_node->token_enum() != PP_Identifier &&
      current_node->token_enum() != PP_ifndef &&
      current_node->token_enum() != PP_ifdef &&
      current_node->token_enum() != PP_define &&
      current_node->token_enum() != PP_define_body &&
      current_node->token_enum() != PP_elsif &&
      current_node->token_enum() != PP_else &&
      current_node->token_enum() != PP_endif) {
    current_variant_.sequence.push_back(*current_node);
  }

  // Checks if the current token is a `ifdef/`ifndef/`elsif.
  if (current_node->token_enum() == PP_ifdef ||
      current_node->token_enum() == PP_ifndef ||
      current_node->token_enum() == PP_elsif) {
    int macro_id = GetMacroIDOfConditional(current_node);
    bool negated = (current_node->token_enum() == PP_ifndef);
    // Checks if this macro is already visited (either defined/undefined).
    if (current_variant_.visited.test(macro_id)) {
      bool assume_condition_is_true =
          (negated ^ current_variant_.macros_mask.test(macro_id));
      if (auto status = DepthFirstSearch(
              receiver, edges_[current_node][!assume_condition_is_true]);
          !status.ok()) {
        LOG(ERROR) << "ERROR: DepthFirstSearch fails. " << status;
        return status;
      }
    } else {
      current_variant_.visited.flip(macro_id);
      // This macro wans't visited before, then we can check both edges.
      // Assume the condition is true.
      if (negated) {
        current_variant_.macros_mask.reset(macro_id);
      } else {
        current_variant_.macros_mask.set(macro_id);
      }
      if (auto status = DepthFirstSearch(receiver, edges_[current_node][0]);
          !status.ok()) {
        LOG(ERROR) << "ERROR: DepthFirstSearch fails. " << status;
        return status;
      }

      // Assume the condition is false.
      if (!negated) {
        current_variant_.macros_mask.reset(macro_id);
      } else {
        current_variant_.macros_mask.set(macro_id);
      }
      if (auto status = DepthFirstSearch(receiver, edges_[current_node][1]);
          !status.ok()) {
        LOG(ERROR) << "ERROR: DepthFirstSearch fails. " << status;
        return status;
      }
      // Undo the change to allow for backtracking.
      current_variant_.visited.flip(macro_id);
    }
  } else {
    // Do recursive search through every possible edge.
    // Expected to be only one edge in this case.
    for (auto next_node : edges_[current_node]) {
      if (auto status = FlowTree::DepthFirstSearch(receiver, next_node);
          !status.ok()) {
        LOG(ERROR) << "ERROR: DepthFirstSearch fails. " << status;
        return status;
      }
    }
  }
  // If the current node is the last one, push the completed current_variant_
  // then it is ready to be sent.
  if (current_node == source_sequence_.end() - 1) {
    wants_more_ &= receiver(current_variant_);
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
    current_variant_.sequence.pop_back();
  }
  return absl::OkStatus();
}

}  // namespace verilog
