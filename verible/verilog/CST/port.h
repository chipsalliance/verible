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

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

// Find all individual port declarations.
std::vector<verible::TreeSearchMatch> FindAllPortDeclarations(
    const verible::Symbol &);

// Find all nodes tagged with kPort.
std::vector<verible::TreeSearchMatch> FindAllPortReferences(
    const verible::Symbol &);

// Find all nodes tagged with kActualNamedPort.
std::vector<verible::TreeSearchMatch> FindAllActualNamedPort(
    const verible::Symbol &);

// Extract the name of the port identifier from a port declaration.
const verible::SyntaxTreeLeaf *GetIdentifierFromPortDeclaration(
    const verible::Symbol &);

// Extract the direction from a port declaration.
// Can return nullptr if the direction is not explicitly specified.
const verible::SyntaxTreeLeaf *GetDirectionFromPortDeclaration(
    const verible::Symbol &);

// Find all individual module port declarations.
std::vector<verible::TreeSearchMatch> FindAllModulePortDeclarations(
    const verible::Symbol &);

// Extract the name of the module port identifier from a port declaration.
const verible::SyntaxTreeLeaf *GetIdentifierFromModulePortDeclaration(
    const verible::Symbol &);

// Extract the direction from a module port declaration.
const verible::SyntaxTreeLeaf *GetDirectionFromModulePortDeclaration(
    const verible::Symbol &);

// Extract the name of the module port identifier from a port reference.
// For Non-ANSI style ports e.g module m(a, b);
const verible::SyntaxTreeLeaf *GetIdentifierFromPortReference(
    const verible::Symbol &);

// Extracts the node tagged with kPortReference from a node tagged with kPort.
const verible::SyntaxTreeNode *GetPortReferenceFromPort(
    const verible::Symbol &);

// Find all task/function port declarations.
std::vector<verible::TreeSearchMatch> FindAllTaskFunctionPortDeclarations(
    const verible::Symbol &);

// Syntax tree node builder for tp_port_item nonterminal.
template <typename T0, typename T1, typename T2>
verible::SymbolPtr MakeTaskFunctionPortItem(T0 &&direction,
                                            T1 &&type_id_dimensions,
                                            T2 &&default_value) {
  // TODO(fangism): check assumptions about arguments' node/symbol types
  return verible::MakeTaggedNode(
      NodeEnum::kPortItem, std::forward<T0>(direction),
      std::forward<T1>(type_id_dimensions), std::forward<T2>(default_value));
}

// Extract the kDataType from a single task/function port item.
// The data type could contain only nullptrs (implicit).
const verible::Symbol *GetTypeOfTaskFunctionPortItem(const verible::Symbol &);

// Extract the declared identifier from a task/function port item.
const verible::SyntaxTreeLeaf *GetIdentifierFromTaskFunctionPortItem(
    const verible::Symbol &);

// Extract the unpacked dimensions from a task/function port item.
const verible::SyntaxTreeNode *GetUnpackedDimensionsFromTaskFunctionPortItem(
    const verible::Symbol &);

// Returns the leaf node containing the name of the actual named port.
// example: from ".x(y)" this returns the leaf spanning "x".
// Returns nullptr if it doesn't exist.
const verible::SyntaxTreeLeaf *GetActualNamedPortName(const verible::Symbol &);

// Returns the node containing the paren group of the actual named port (if
// exists).
// e.g. from ".x(y)" returns the node spanning (y), from ".z" return
// nullptr.
const verible::Symbol *GetActualNamedPortParenGroup(const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_PORT_H_
