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

#ifndef VERIBLE_VERILOG_CST_PARAMETERS_H_
#define VERIBLE_VERILOG_CST_PARAMETERS_H_

// See comment at the top
// verilog/CST/verilog_treebuilder_utils.h that explains use
// of std::forward in Make* helper functions.

#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

// Creates a node tagged kParamType.
// From "parameter type [dim] id [dim] = value;",
// this node spans "type [dim] id [dim]."
template <typename T0, typename T1, typename T2, typename T3>
verible::SymbolPtr MakeParamTypeDeclaration(T0 &&type_info,
                                            T1 &&packed_dimensions,
                                            T2 &&identifier,
                                            T3 &&unpacked_dimensions) {
  CHECK(verible::SymbolCastToNode(*ABSL_DIE_IF_NULL(type_info))
            .MatchesTag(NodeEnum::kTypeInfo));
  return verible::MakeTaggedNode(
      NodeEnum::kParamType, std::forward<T0>(type_info),
      std::forward<T1>(packed_dimensions), std::forward<T2>(identifier),
      std::forward<T3>(unpacked_dimensions));
}

// Creates a node tagged kTypeInfo, which holds the parameter type information.
template <typename T0, typename T1, typename T2>
verible::SymbolPtr MakeTypeInfoNode(T0 &&primitive_type, T1 &&signed_unsigned,
                                    T2 &&user_defined_type) {
  return verible::MakeTaggedNode(
      NodeEnum::kTypeInfo, std::forward<T0>(primitive_type),
      std::forward<T1>(signed_unsigned), std::forward<T2>(user_defined_type));
}

// Finds all parameter/localparam declarations.
std::vector<verible::TreeSearchMatch> FindAllParamDeclarations(
    const verible::Symbol &);

// Finds all nodes tagged with kParamByName.
std::vector<verible::TreeSearchMatch> FindAllNamedParams(
    const verible::Symbol &);

// Returns the token_enum of the parameter keyword from the node
// kParamDeclaration (either TK_parameter or TK_localparam).
verilog_tokentype GetParamKeyword(const verible::Symbol &);

// Returns the token for kParamDeclaration symbol
const verible::TokenInfo *GetParameterToken(const verible::Symbol &symbol);

// Returns a pointer to either TK_type or kParamType node, which holds the param
// type, id, and dimensions info for that parameter.
const verible::Symbol *GetParamTypeSymbol(const verible::Symbol &);

// Returns a pointer to the symbol holding the node kTypeInfo under the node
// kParamDeclaration.
const verible::Symbol *GetParamTypeInfoSymbol(const verible::Symbol &);

// Get right-hand side of a parameter assignment expression.
const verible::Symbol *GetParamAssignExpression(const verible::Symbol &symbol);

// Returns the token of the declared parameter.
const verible::TokenInfo *GetParameterNameToken(const verible::Symbol &);

// Returns all tokens for a parameter declaration.
std::vector<const verible::TokenInfo *> GetAllParameterNameTokens(
    const verible::Symbol &);

// Get the token info for a given kParameterAssign node symbol
const verible::TokenInfo *GetAssignedParameterNameToken(
    const verible::Symbol &symbol);

// Get the symbols for all kParameterAssign nodes
std::vector<const verible::Symbol *> GetAllAssignedParameterSymbols(
    const verible::Symbol &root);

// Returns the token of the SymbolIdentifier from the node kParamDeclaration.
// Used specifically for 'parameter type' declarations.
const verible::TokenInfo *GetSymbolIdentifierFromParamDeclaration(
    const verible::Symbol &);

// Returns true if the parameter is a parameter type declaration from the node
// kParamDeclaration.
bool IsParamTypeDeclaration(const verible::Symbol &);

// Returns a pointer to the symbol holding the node kTypeAssignment under the
// node kParamDeclaration.
const verible::SyntaxTreeNode *GetTypeAssignmentFromParamDeclaration(
    const verible::Symbol &);

// Returns a pointer to the identifier leaf holding the SymbolIdentifier under
// the node kTypeAssignment.
const verible::SyntaxTreeLeaf *GetIdentifierLeafFromTypeAssignment(
    const verible::Symbol &);

// Returns a pointer to the expression node holding under the node
// kTypeAssignment.
// e.g from "class m(type x = y)" returns the node spanning "y".
const verible::SyntaxTreeNode *GetExpressionFromTypeAssignment(
    const verible::Symbol &);

// Returns true if the node kTypeInfo is empty (all children are nullptr).
bool IsTypeInfoEmpty(const verible::Symbol &);

// Return the node spanning param name from a node tagged with kParamByName.
// e.g from "module_type #(.N(x))" return the leaf spanning "N".
const verible::SyntaxTreeLeaf *GetNamedParamFromActualParam(
    const verible::Symbol &);

// Return the node spanning the paren group from a node tagged with
// kParamByName.
// e.g from "module_type #(.N(x))" return the leaf spanning "(x)".
const verible::SyntaxTreeNode *GetParenGroupFromActualParam(
    const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_PARAMETERS_H_
