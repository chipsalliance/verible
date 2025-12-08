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

#ifndef VERIBLE_VERILOG_CST_DPI_H_
#define VERIBLE_VERILOG_CST_DPI_H_

// See comment at the top
// verilog/CST/verilog_treebuilder_utils.h that explains use
// of std::forward in Make* helper functions.

#include <cstddef>
#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"  // IWYU pragma: export
#include "verible/common/text/symbol-ptr.h"            // IWYU pragma: export
#include "verible/common/text/symbol.h"                // IWYU pragma: export
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

template <typename T0, typename T1, typename T2, typename T3, typename T4,
          typename T5>
verible::SymbolPtr MakeDPIImport(T0 &&keyword, T1 &&spec, T2 &&property,
                                 T3 &&id, T4 &&equals, T5 &&proto) {
  verible::CheckSymbolAsLeaf(*keyword, verilog_tokentype::TK_import);
  verible::CheckSymbolAsLeaf(*spec, verilog_tokentype::TK_StringLiteral);
  if (id != nullptr) {
    CHECK(IsIdentifierLike(
        verilog_tokentype(verible::SymbolCastToLeaf(*id).get().token_enum())));
  }
  verible::CheckOptionalSymbolAsLeaf(equals, '=');
  CHECK(verible::SymbolCastToNode(*proto).MatchesTagAnyOf(
      {NodeEnum::kFunctionPrototype, NodeEnum::kTaskPrototype}));
  return verible::MakeTaggedNode(
      NodeEnum::kDPIImportItem, std::forward<T0>(keyword),
      std::forward<T1>(spec), std::forward<T2>(property), std::forward<T3>(id),
      std::forward<T4>(equals), std::forward<T5>(proto));
}

// Partial specialization provided as a workaround to passing nullptr
// in positions 3 and 4 (optional symbols).  Compiler is not guaranteed
// to deduce to that some paths are not reachble/applicable.
template <typename T0, typename T1, typename T2, typename T3, typename T4>
verible::SymbolPtr MakeDPIImport(T0 &&keyword, T1 &&spec, T2 &&property,
                                 std::nullptr_t id, std::nullptr_t equals,
                                 T3 &&proto, T4 &&semi) {
  verible::CheckSymbolAsLeaf(*keyword, verilog_tokentype::TK_import);
  verible::CheckSymbolAsLeaf(*spec, verilog_tokentype::TK_StringLiteral);
  CHECK(verible::SymbolCastToNode(*proto).MatchesTagAnyOf(
      {NodeEnum::kFunctionPrototype, NodeEnum::kTaskPrototype}));
  return verible::MakeTaggedNode(
      NodeEnum::kDPIImportItem, std::forward<T0>(keyword),
      std::forward<T1>(spec), std::forward<T2>(property), id, equals,
      std::forward<T3>(proto), std::forward<T4>(semi));
}

// Find all DPI imports.
std::vector<verible::TreeSearchMatch> FindAllDPIImports(
    const verible::Symbol &);

// Returns the function/task prototype.
const verible::SyntaxTreeNode *GetDPIImportPrototype(const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_DPI_H_
