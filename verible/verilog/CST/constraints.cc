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

#include "verible/verilog/CST/constraints.h"

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllConstraintDeclarations(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekConstraintDeclaration());
}

bool IsOutOfLineConstraintDefinition(const verible::Symbol &symbol) {
  const auto *identifier_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kConstraintDeclaration, 2);

  return IdIsQualified(*identifier_symbol);
}

const verible::TokenInfo *GetSymbolIdentifierFromConstraintDeclaration(
    const verible::Symbol &symbol) {
  const auto *identifier_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kConstraintDeclaration, 2);
  if (!identifier_symbol) return nullptr;
  return &AutoUnwrapIdentifier(*identifier_symbol)->get();
}

}  // namespace verilog
