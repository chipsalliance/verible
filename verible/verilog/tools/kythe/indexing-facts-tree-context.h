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

#ifndef VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_CONTEXT_H_
#define VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_CONTEXT_H_

#include "verible/common/util/auto-pop-stack.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/tools/kythe/indexing-facts-tree.h"

namespace verilog {
namespace kythe {

// Container with a stack of IndexingFactNode to hold context of
// IndexingFactNodes during traversal of an IndexingFactsTree.
class IndexingFactsTreeContext
    : public verible::AutoPopStack<IndexingFactNode *> {
 public:
  using base_type = verible::AutoPopStack<IndexingFactNode *>;

  // member class to handle push and pop of stack safely
  using AutoPop = base_type::AutoPop;

 public:
  // returns the top IndexingFactsNode of the stack.
  IndexingFactNode &top() { return *ABSL_DIE_IF_NULL(base_type::top()); }
};

}  // namespace kythe
}  // namespace verilog

#endif  // VERIBLE_VERILOG_TOOLS_KYTHE_INDEXING_FACTS_TREE_CONTEXT_H_
