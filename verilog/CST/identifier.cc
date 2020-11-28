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

#include "verilog/CST/identifier.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/parser/verilog_token_classifications.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::down_cast;
using verible::SymbolKind;

std::vector<verible::TreeSearchMatch> FindAllIdentifierUnpackedDimensions(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekIdentifierUnpackedDimensions());
}

std::vector<verible::TreeSearchMatch> FindAllUnqualifiedIds(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekUnqualifiedId());
}

std::vector<verible::TreeSearchMatch> FindAllQualifiedIds(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekQualifiedId());
}

std::vector<verible::TreeSearchMatch> FindAllSymbolIdentifierLeafs(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, SymbolIdentifierLeaf());
}

bool IdIsQualified(const verible::Symbol& symbol) {
  auto t = symbol.Tag();
  if (t.kind != SymbolKind::kNode) return false;
  return NodeEnum(t.tag) == NodeEnum::kQualifiedId;
}

const verible::SyntaxTreeLeaf* GetIdentifier(const verible::Symbol& symbol) {
  auto t = symbol.Tag();
  if (t.kind != SymbolKind::kNode) return nullptr;
  if (NodeEnum(t.tag) != NodeEnum::kUnqualifiedId) return nullptr;
  const auto& node = down_cast<const verible::SyntaxTreeNode&>(symbol);
  const auto* leaf = down_cast<const verible::SyntaxTreeLeaf*>(node[0].get());
  return leaf;
}

const verible::SyntaxTreeLeaf* AutoUnwrapIdentifier(
    const verible::Symbol& symbol) {
  // If it's a leaf, then just return that leaf. Otherwise it is an
  // kUnqualifiedId
  const auto t = symbol.Tag();
  if (t.kind == SymbolKind::kLeaf) {
    if (IsIdentifierLike(verilog_tokentype(t.tag))) {
      return &verible::SymbolCastToLeaf(symbol);
    }
    return nullptr;
  }
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kUnqualifiedId);
  return GetIdentifier(symbol);
}

const verible::SyntaxTreeLeaf*
GetSymbolIdentifierFromIdentifierUnpackedDimensions(
    const verible::Symbol& identifier_unpacked_dimension) {
  const verible::Symbol* child_node =
      GetSubtreeAsSymbol(identifier_unpacked_dimension,
                         NodeEnum::kIdentifierUnpackedDimensions, 0);
  return AutoUnwrapIdentifier(*child_node);
}

}  // namespace verilog
