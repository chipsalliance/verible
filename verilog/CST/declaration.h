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

#ifndef VERIBLE_VERILOG_CST_DECLARATION_H_
#define VERIBLE_VERILOG_CST_DECLARATION_H_

// See comment at the top
// verilog/CST/verilog_treebuilder_utils.h that explains use
// of std::forward in Make* helper functions.

#include <utility>

#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {

// Interface for consistently building a type-id-dimensions tuple.
template <typename T1, typename T2, typename T3>
verible::SymbolPtr MakeTypeIdDimensionsTuple(T1&& type, T2&& id,
                                             T3&& unpacked_dimensions) {
  return verible::MakeTaggedNode(NodeEnum::kDataTypeImplicitBasicIdDimensions,
                                 std::forward<T1>(type), std::forward<T2>(id),
                                 std::forward<T3>(unpacked_dimensions));
}

// Repacks output of MakeTypeIdDimensionsTuple into a type-id pair.
verible::SymbolPtr RepackReturnTypeId(verible::SymbolPtr type_id_tuple);

// Maps lexical token enum to corresponding syntax tree node.
// Useful for syntax tree construction.
NodeEnum DeclarationKeywordToNodeEnum(const verible::Symbol&);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_DECLARATION_H_
