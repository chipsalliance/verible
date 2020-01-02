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

#include "verilog/CST/functions.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllFunctionDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekFunctionDeclaration());
}

const verible::SyntaxTreeNode& GetFunctionHeader(
    const verible::Symbol& symbol) {
  return verible::GetSubtreeAsNode(symbol, NodeEnum::kFunctionDeclaration, 0,
                                   NodeEnum::kFunctionHeader);
}

const verible::Symbol* GetFunctionLifetime(const verible::Symbol& symbol) {
  const auto& header = GetFunctionHeader(symbol);
  return verible::GetSubtreeAsSymbol(header, NodeEnum::kFunctionHeader, 2);
}

const verible::Symbol* GetFunctionId(const verible::Symbol& symbol) {
  const auto& header = GetFunctionHeader(symbol);
  return verible::GetSubtreeAsSymbol(header, NodeEnum::kFunctionHeader, 4);
}

}  // namespace verilog
