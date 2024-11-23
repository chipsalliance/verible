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

#ifndef VERIBLE_VERILOG_CST_FUNCTIONS_H_
#define VERIBLE_VERILOG_CST_FUNCTIONS_H_

// See comment at the top
// verilog/CST/verilog_treebuilder_utils.h that explains use
// of std::forward in Make* helper functions.

#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/CST/verilog-treebuilder-utils.h"

namespace verilog {

// Construct a function header CST node, without the trailing ';'.
template <typename T0, typename T1, typename T2, typename T3, typename T4>
verible::SymbolPtr MakeFunctionHeader(T0 &&qualifiers, T1 &&function_start,
                                      T2 &&lifetime, T3 &&return_type_id,
                                      T4 &&ports) {
  verible::CheckOptionalSymbolAsNode(qualifiers, NodeEnum::kQualifierList);
  ExpectString(function_start, "function");
  verible::CheckOptionalSymbolAsNode(ports, NodeEnum::kParenGroup);
  return verible::MakeTaggedNode(
      NodeEnum::kFunctionHeader, std::forward<T0>(qualifiers),
      std::forward<T1>(function_start), std::forward<T2>(lifetime),
      std::forward<T3>(
          return_type_id) /* flattens to separate type and id nodes */,
      std::forward<T4>(ports));
}

// Construct a function header CST node, with the trailing ';'.
template <typename T0, typename T1, typename T2, typename T3, typename T4,
          typename T5>
verible::SymbolPtr MakeFunctionHeader(T0 &&qualifiers, T1 &&function_start,
                                      T2 &&lifetime, T3 &&return_type_id,
                                      T4 &&ports, T5 &&semicolon) {
  ExpectString(semicolon, ";");
  return ExtendNode(
      MakeFunctionHeader(
          std::forward<T0>(qualifiers), std::forward<T1>(function_start),
          std::forward<T2>(lifetime),
          std::forward<T3>(
              return_type_id) /* flattens to separate type and id nodes */,
          std::forward<T4>(ports)),
      std::forward<T5>(semicolon));
}

template <typename T0, typename T1, typename T2, typename T3, typename T4,
          typename T5, typename T6, typename T7, typename T8, typename T9>
verible::SymbolPtr MakeFunctionDeclaration(T0 &&qualifiers, T1 &&function_start,
                                           T2 &&lifetime, T3 &&return_type_id,
                                           T4 &&ports, T5 &&semicolon,
                                           T6 &&function_items, T7 &&body,
                                           T8 &&function_end, T9 &&label) {
  ExpectString(function_end, "endfunction");
  return verible::MakeTaggedNode(
      NodeEnum::kFunctionDeclaration,
      MakeFunctionHeader(
          std::forward<T0>(qualifiers), std::forward<T1>(function_start),
          std::forward<T2>(lifetime),
          std::forward<T3>(
              return_type_id) /* flattens to separate type and id nodes */,
          std::forward<T4>(ports), std::forward<T5>(semicolon)),
      std::forward<T6>(function_items), std::forward<T7>(body),
      std::forward<T8>(function_end), std::forward<T9>(label));
}

// Find all function declarations, including class method declarations.
std::vector<verible::TreeSearchMatch> FindAllFunctionDeclarations(
    const verible::Symbol &);

// Find all function headers (in declarations and prototypes).
std::vector<verible::TreeSearchMatch> FindAllFunctionHeaders(
    const verible::Symbol &);

// Find all function prototypes (extern, pure virtual).
std::vector<verible::TreeSearchMatch> FindAllFunctionPrototypes(
    const verible::Symbol &);

// Find all function (or Task) calls.
std::vector<verible::TreeSearchMatch> FindAllFunctionOrTaskCalls(
    const verible::Symbol &);

// Find all function (or Task) calls extension e.g class_name.function_call().
std::vector<verible::TreeSearchMatch> FindAllFunctionOrTaskCallsExtension(
    const verible::Symbol &);

// Find all constructor prototypes.
std::vector<verible::TreeSearchMatch> FindAllConstructorPrototypes(
    const verible::Symbol &);

// Returns the function declaration header (return type, id, ports)
const verible::SyntaxTreeNode *GetFunctionHeader(
    const verible::Symbol &function_decl);

// Returns the function prototype header (return type, id, ports)
const verible::SyntaxTreeNode *GetFunctionPrototypeHeader(
    const verible::Symbol &function_decl);

// FunctionHeader accessors

// Returns the function lifetime of the function header.
const verible::Symbol *GetFunctionHeaderLifetime(
    const verible::Symbol &function_header);

// Returns the parenthesis group containing the formal ports list, or nullptr.
const verible::Symbol *GetFunctionHeaderFormalPortsGroup(
    const verible::Symbol &function_header);

// Returns the return type of the function header.
const verible::Symbol *GetFunctionHeaderReturnType(
    const verible::Symbol &function_header);

// Returns the id of the function header.
const verible::Symbol *GetFunctionHeaderId(
    const verible::Symbol &function_header);

// FunctionDeclaration acccessors

// Returns the function lifetime of the node.
const verible::Symbol *GetFunctionLifetime(
    const verible::Symbol &function_decl);

// Returns the parenthesis group containing the formal ports list, or nullptr.
const verible::Symbol *GetFunctionFormalPortsGroup(
    const verible::Symbol &function_decl);

// Returns the return type of the function declaration.
const verible::Symbol *GetFunctionReturnType(
    const verible::Symbol &function_decl);

// Returns the id of the function declaration.
const verible::Symbol *GetFunctionId(const verible::Symbol &function_decl);

// Returns leaf node for function name.
// e.g. function my_fun(); return leaf node for "my_fun".
const verible::SyntaxTreeLeaf *GetFunctionName(const verible::Symbol &);

// Returns local root node from node tagged with kFunctionCall.
const verible::SyntaxTreeNode *GetLocalRootFromFunctionCall(
    const verible::Symbol &);

// Return the node spanning identifier for the function call node.
// e.g from "pkg::get()" returns the node spanning "pkg::get".
const verible::SyntaxTreeNode *GetIdentifiersFromFunctionCall(
    const verible::Symbol &function_call);

// Returns leaf node for function name in function call.
// e.g my_function(); return leaf node for "my_function".
const verible::SyntaxTreeLeaf *GetFunctionCallName(const verible::Symbol &);

// Returns leaf node for function name in function call extension.
// e.g class_name.my_function(); return leaf node for "my_function".
const verible::SyntaxTreeLeaf *GetFunctionCallNameFromCallExtension(
    const verible::Symbol &);

// Returns the function declaration body.
const verible::SyntaxTreeNode *GetFunctionBlockStatementList(
    const verible::Symbol &);

// Return the node spanning the paren group of function call.
// e.g my_function(a, b, c) return the node spanning (a, b, c).
const verible::SyntaxTreeNode *GetParenGroupFromCall(const verible::Symbol &);

// Return the node spanning the paren group of function call extension.
// e.g my_class.my_function(a, b, c) return the node spanning (a, b, c).
const verible::SyntaxTreeNode *GetParenGroupFromCallExtension(
    const verible::Symbol &);

// Returns leaf node for the "new" keyword of a constructor prototype.
const verible::SyntaxTreeLeaf *GetConstructorPrototypeNewKeyword(
    const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_FUNCTIONS_H_
