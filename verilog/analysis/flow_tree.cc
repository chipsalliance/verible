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

absl::Status FlowTree::GenerateControlFlowTree() {
  int idx = 0;
  int current_enum = 0;
  for (auto u : source_sequence_) {
    current_enum = u.token_enum();

    if (current_enum == PP_ifdef || current_enum == PP_ifndef) {
      ifs_.push_back(idx);
      elses_[ifs_.back()].push_back(idx);

    } else if (current_enum == PP_else || current_enum == PP_elsif ||
               current_enum == PP_endif) {
      elses_[ifs_.back()].push_back(idx);
      if (current_enum == PP_endif) {
        auto& myelses = elses_[ifs_.back()];
        for (auto it = myelses.begin(); it != myelses.end(); it++) {
          for (auto it2 = it + 1; it2 != myelses.end(); it2++) {
            if (it == myelses.begin() && it2 == myelses.end() - 1) continue;
            edges_[*it].push_back(*it2 + (it2 != myelses.end() - 1));
          }
        }
        ifs_.pop_back();
      }
    }
    idx++;
  }
  idx = 0;
  for (auto u : source_sequence_) {
    current_enum = u.token_enum();
    if (current_enum != PP_else && current_enum != PP_elsif) {
      if (idx > 0) edges_[idx - 1].push_back(idx);
    } else {
      if (idx > 0) edges_[idx - 1].push_back(edges_[idx].back());
    }
    idx++;
  }

  return absl::OkStatus();
}

absl::Status FlowTree::DepthFirstSearch(int index) {
  const auto& curr = source_sequence_[index];
  if (curr.token_enum() != PP_Identifier && curr.token_enum() != PP_ifndef &&
      curr.token_enum() != PP_ifdef && curr.token_enum() != PP_define &&
      curr.token_enum() != PP_define_body && curr.token_enum() != PP_elsif &&
      curr.token_enum() != PP_else && curr.token_enum() != PP_endif)
    current_sequence_.push_back(curr);
  for (auto u : edges_[index]) {
    auto status = FlowTree::DepthFirstSearch(u);  // handle errors
  }
  if (index == int(source_sequence_.size()) - 1) {
    variants_.push_back(current_sequence_);
  }
  if (curr.token_enum() != PP_Identifier && curr.token_enum() != PP_ifndef &&
      curr.token_enum() != PP_ifdef && curr.token_enum() != PP_define &&
      curr.token_enum() != PP_define_body && curr.token_enum() != PP_elsif &&
      curr.token_enum() != PP_else && curr.token_enum() != PP_endif)
    current_sequence_.pop_back();
  return absl::OkStatus();
}

}  // namespace verilog
