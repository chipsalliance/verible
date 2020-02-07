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

#include "verilog/CST/module.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllModuleDeclarations(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekModuleDeclaration());
}

const verible::SyntaxTreeNode& GetModuleHeader(
    const verible::Symbol& module_symbol) {
  return verible::GetSubtreeAsNode(module_symbol, NodeEnum::kModuleDeclaration,
                                   0, NodeEnum::kModuleHeader);
}

const verible::SyntaxTreeNode& GetInterfaceHeader(
    const verible::Symbol& module_symbol) {
  return verible::GetSubtreeAsNode(module_symbol, NodeEnum::kInterfaceDeclaration,
                                   0, NodeEnum::kModuleHeader);
}

const verible::TokenInfo& GetModuleNameToken(const verible::Symbol& s) {
  const auto& header_node = GetModuleHeader(s);
  const auto& name_leaf =
      verible::GetSubtreeAsLeaf(header_node, NodeEnum::kModuleHeader, 2);
  return name_leaf.get();
}

const verible::TokenInfo& GetInterfaceNameToken(const verible::Symbol& s) {
  const auto& header_node = GetInterfaceHeader(s);
  const auto& name_leaf =
      verible::GetSubtreeAsLeaf(header_node, NodeEnum::kModuleHeader, 2);
  return name_leaf.get();
}

}  // namespace verilog
