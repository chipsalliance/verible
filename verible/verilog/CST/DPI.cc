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

#include "verible/verilog/CST/DPI.h"

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

using verible::Symbol;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllDPIImports(const Symbol &root) {
  return SearchSyntaxTree(root, NodekDPIImportItem());
}

const SyntaxTreeNode *GetDPIImportPrototype(const Symbol &symbol) {
  return GetSubtreeAsNode(symbol, NodeEnum::kDPIImportItem, 5);
}

}  // namespace verilog
