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

#include <vector>
#include <map>
#include <string>
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/lexer/token_stream_adapter.h"
#include "verilog/parser/verilog_token_enum.h"
#include "verilog/analysis/flow_tree.h"

namespace verilog {

absl::Status FlowTree::GenerateControlFlowTree(){
  int idx=0;
  int current_enum=0;
  for(auto u:source_sequence_){
    current_enum=u.token_enum();

    if(current_enum == PP_ifdef || current_enum == PP_ifndef){
      ifs_.push_back(idx);
      elses_[ifs_.back()].push_back(idx);

    }else if(current_enum == PP_else || current_enum == PP_elsif || current_enum == PP_endif){
      elses_[ifs_.back()].push_back(idx);
      if(current_enum == PP_endif){
        auto & myelses= elses_[ifs_.back()];
        for(int i=0;i<myelses.size();i++){
          for(int j=i+1;j<myelses.size();j++){
            if(!i&&j==myelses.size()-1) continue;
            edges_[myelses[i]].push_back(myelses[j]+1);
          }
        }
        ifs_.pop_back();
      }
    }
    idx++;
  }
  idx=0;
  int prv_enum=0;
  for(auto u:source_sequence_){
    current_enum=u.token_enum();
    if(current_enum != PP_else && current_enum != PP_elsif){
      if(idx>0) edges_[idx-1].push_back(idx);
    }else{
      if(idx>0) edges_[idx-1].push_back(edges_[idx].back());
    }
    prv_enum = current_enum;
    idx++;
  }

  return absl::OkStatus();
}

absl::Status FlowTree::DepthFirstSearch(int index){
  const auto & curr=source_sequence_[index];
  if(curr.token_enum()!=PP_Identifier && curr.token_enum() != PP_ifndef && curr.token_enum()!=PP_ifdef
    && curr.token_enum()!=PP_define && curr.token_enum()!=PP_define_body
    && curr.token_enum()!=PP_elsif && curr.token_enum()!=PP_else && curr.token_enum()!=PP_endif) current_sequence_.push_back(curr);
  for(auto u:edges_[index]){
    auto status = FlowTree::DepthFirstSearch(u); // handle errors
  }
  if(index==source_sequence_.size()-1){
    variants_.push_back(current_sequence_);
  }
  if(curr.token_enum()!=PP_Identifier && curr.token_enum() != PP_ifndef && curr.token_enum()!=PP_ifdef
    && curr.token_enum()!=PP_define && curr.token_enum()!=PP_define_body
    && curr.token_enum()!=PP_elsif && curr.token_enum()!=PP_else && curr.token_enum()!=PP_endif) current_sequence_.pop_back();
  return absl::OkStatus();
}

} // namespace verilog

