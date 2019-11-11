// Copyright 2017-2019 The Verible Authors.
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
#include "common/util/logging.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

using verible::SymbolKind;

std::vector<verible::TreeSearchMatch> FindAllModuleDeclarations(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekModuleDeclaration());
}

const verible::SyntaxTreeNode& GetModuleHeader(
    const verible::Symbol& module_symbol) {
  const auto t = module_symbol.Tag();
  CHECK_EQ(t.kind, SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kModuleDeclaration);
  const auto& module_node = SymbolCastToNode(module_symbol);
  // first child is the whole header
  return verible::SymbolCastToNode(*ABSL_DIE_IF_NULL(module_node[0]));
}

const verible::TokenInfo& GetModuleNameToken(const verible::Symbol& s) {
  const auto& header_node = GetModuleHeader(s);
  const auto& name_leaf = SymbolCastToLeaf(*ABSL_DIE_IF_NULL(header_node[2]));
  return name_leaf.get();
}

}  // namespace verilog
