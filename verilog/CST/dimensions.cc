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

#include "verilog/CST/dimensions.h"

#include <vector>

#include "common/analysis/matcher/matcher-builders.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/syntax-tree-search.h"
#include "common/text/concrete-syntax-tree.h"
#include "common/text/symbol.h"
#include "common/text/tree-utils.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

using verible::Symbol;

std::vector<verible::TreeSearchMatch> FindAllPackedDimensions(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekPackedDimensions());
}

std::vector<verible::TreeSearchMatch> FindAllUnpackedDimensions(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekUnpackedDimensions());
}

std::vector<verible::TreeSearchMatch> FindAllDeclarationDimensions(
    const Symbol& root) {
  return SearchSyntaxTree(root, NodekDeclarationDimensions());
}

const Symbol* GetDimensionRangeLeftBound(const Symbol& s) {
  return verible::GetSubtreeAsSymbol(s, NodeEnum::kDimensionRange, 1);
}

const Symbol* GetDimensionRangeRightBound(const verible::Symbol& s) {
  return verible::GetSubtreeAsSymbol(s, NodeEnum::kDimensionRange, 3);
}

}  // namespace verilog
