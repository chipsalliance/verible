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

// This unit provides helper functions that pertain to SystemVerilog
// port declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_PORT_H_
#define VERIBLE_VERILOG_CST_PORT_H_

#include <utility>
#include <vector>

#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {

// Find all individual module port declarations.
std::vector<verible::TreeSearchMatch> FindAllModulePortDeclarations(
    const verible::Symbol&);

// Find all individual port references.
std::vector<verible::TreeSearchMatch> FindAllPortReferences(
    const verible::Symbol&);

// Extract the name of the module port identifier from a port declaration.
const verible::SyntaxTreeLeaf* GetIdentifierFromModulePortDeclaration(
    const verible::Symbol&);

// Extract the name of the module port identifier from a port reference.
// For Non-ANSI style ports e.g module m(a, b);
const verible::SyntaxTreeLeaf* GetIdentifierFromPortReference(
    const verible::Symbol&);

// Find all task/function port declarations.
std::vector<verible::TreeSearchMatch> FindAllTaskFunctionPortDeclarations(
    const verible::Symbol&);

// Syntax tree node builder for tp_port_item nonterminal.
template <typename T0, typename T1, typename T2>
verible::SymbolPtr MakeTaskFunctionPortItem(T0&& direction,
                                            T1&& type_id_dimensions,
                                            T2&& default_value) {
  // TODO(fangism): check assumptions about arguments' node/symbol types
  return verible::MakeTaggedNode(
      NodeEnum::kPortItem, std::forward<T0>(direction),
      std::forward<T1>(type_id_dimensions), std::forward<T2>(default_value));
}

// Extract the kDataType from a single task/function port item.
// The data type could contain only nullptrs (implicit).
const verible::Symbol* GetTypeOfTaskFunctionPortItem(const verible::Symbol&);

// Extract the declared identifier from a task/function port item.
const verible::SyntaxTreeLeaf* GetIdentifierFromTaskFunctionPortItem(
    const verible::Symbol&);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_PORT_H_
