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

#include "verilog/CST/dimensions.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

using verible::down_cast;
using verible::Symbol;
using verible::SymbolKind;
using verible::SymbolPtr;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllPackedDimensions(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekPackedDimensions());
}

std::vector<verible::TreeSearchMatch> FindAllUnpackedDimensions(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekUnpackedDimensions());
}

std::vector<verible::TreeSearchMatch> FindAllDeclarationDimensions(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekDeclarationDimensions());
}

const verible::SymbolPtr& GetDimensionRangeLeftBound(const verible::Symbol& s) {
  auto t = s.Tag();
  CHECK_EQ(t.kind, SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kDimensionRange);
  return down_cast<const SyntaxTreeNode&>(s).children()[1];
}

const verible::SymbolPtr& GetDimensionRangeRightBound(
    const verible::Symbol& s) {
  auto t = s.Tag();
  CHECK_EQ(t.kind, SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kDimensionRange);
  return down_cast<const SyntaxTreeNode&>(s).children()[3];
}

}  // namespace verilog
