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

#ifndef VERIBLE_VERILOG_CST_DECLARATION_H_
#define VERIBLE_VERILOG_CST_DECLARATION_H_

// See comment at the top
// verilog/CST/verilog_treebuilder_utils.h that explains use
// of std::forward in Make* helper functions.

#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol-ptr.h"  // IWYU pragma: export
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

// Interface for consistently building a type-id-dimensions tuple.
template <typename T1, typename T2, typename T3>
verible::SymbolPtr MakeTypeIdDimensionsTuple(T1 &&type, T2 &&id,
                                             T3 &&unpacked_dimensions) {
  verible::CheckSymbolAsNode(*type.get(), NodeEnum::kDataType);
  // id can be qualified or unqualified
  verible::CheckOptionalSymbolAsNode(unpacked_dimensions,
                                     NodeEnum::kUnpackedDimensions);
  return verible::MakeTaggedNode(NodeEnum::kDataTypeImplicitBasicIdDimensions,
                                 std::forward<T1>(type), std::forward<T2>(id),
                                 std::forward<T3>(unpacked_dimensions));
}

// Interface for consistently building a type-id tuple (no unpacked dimensions).
// TODO(fangism): combine this with MakeTypeIdDimensionsTuple above?
//   That would be one fewer auxiliary CST node type.
template <typename T1, typename T2>
verible::SymbolPtr MakeTypeIdTuple(T1 &&type, T2 &&id) {
  verible::CheckSymbolAsNode(*type.get(), NodeEnum::kDataType);
  verible::CheckSymbolAsNode(*id.get(), NodeEnum::kUnqualifiedId);
  return verible::MakeTaggedNode(NodeEnum::kTypeIdentifierId,
                                 std::forward<T1>(type), std::forward<T2>(id));
}

// Repacks output of MakeTypeIdDimensionsTuple into a type-id pair.
verible::SymbolPtr RepackReturnTypeId(verible::SymbolPtr type_id_tuple);

// Maps lexical token enum to corresponding syntax tree node.
// Useful for syntax tree construction.
NodeEnum DeclarationKeywordToNodeEnum(const verible::Symbol &);

template <typename T1, typename T2>
verible::SymbolPtr MakeInstantiationBase(T1 &&type, T2 &&decl_list) {
  verible::CheckSymbolAsNode(*type.get(), NodeEnum::kInstantiationType);
  // decl_list could contain either instantiations or variable declarations
  return verible::MakeTaggedNode(NodeEnum::kInstantiationBase,
                                 std::forward<T1>(type),
                                 std::forward<T2>(decl_list));
}

// Interface for consistently building a data declaration.
template <typename T1, typename T2, typename T3>
verible::SymbolPtr MakeDataDeclaration(T1 &&qualifiers, T2 &&inst_base,
                                       T3 &&semicolon) {
  verible::CheckOptionalSymbolAsNode(qualifiers, NodeEnum::kQualifierList);
  if (inst_base.get()->Tag().tag == (int)NodeEnum::kFunctionCall) {
    return verible::ExtendNode(std::forward<T2>(inst_base),
                               std::forward<T3>(semicolon));
  }
  verible::CheckSymbolAsNode(*inst_base.get(), NodeEnum::kInstantiationBase);
  verible::CheckSymbolAsLeaf(*semicolon.get(), ';');
  return verible::MakeTaggedNode(
      NodeEnum::kDataDeclaration, std::forward<T1>(qualifiers),
      std::forward<T2>(inst_base), std::forward<T3>(semicolon));
}

// Find all data declarations.
std::vector<verible::TreeSearchMatch> FindAllDataDeclarations(
    const verible::Symbol &);
std::vector<verible::TreeSearchMatch> FindAllNetVariables(
    const verible::Symbol &);
std::vector<verible::TreeSearchMatch> FindAllRegisterVariables(
    const verible::Symbol &);
std::vector<verible::TreeSearchMatch> FindAllGateInstances(
    const verible::Symbol &);
std::vector<verible::TreeSearchMatch> FindAllVariableDeclarationAssignment(
    const verible::Symbol &);

// For a given data declaration (includes module instantiation), returns the
// subtree containing qualifiers.  e.g. from "const foo bar, baz;",
// this returns the subtree spanning "const".  Returns nullptr if there
// are no qualifiers.
const verible::SyntaxTreeNode *GetQualifiersOfDataDeclaration(
    const verible::Symbol &data_declaration);

// For a given data declaration (includes module instantiation), returns the
// subtree containing the type.  e.g. from "foo #(...) bar..., baz...;",
// this returns the subtree spanning "foo #(...)".
// It is possible for type to be implicit, in which case, the node
// will be an empty subtree.
const verible::SyntaxTreeNode *GetInstantiationTypeOfDataDeclaration(
    const verible::Symbol &data_declaration);

// For a given data declaration returns the node spanning param declaration
// list. e.g "module_type #(N)" return the node for "#(N)".
const verible::SyntaxTreeNode *GetParamListFromDataDeclaration(
    const verible::Symbol &);

// For a given data declaration (includes module instantiation), returns the
// subtree containing instances.  e.g. from "foo bar..., baz...;",
// this returns the subtree spanning "bar..., baz..."
const verible::SyntaxTreeNode *GetInstanceListFromDataDeclaration(
    const verible::Symbol &data_declaration);

// For a given module gate instance return the node spanning the paren group.
// e.g "module_type instance(a, b, c)" return the node spanning "(a, b, c)".
const verible::SyntaxTreeNode *GetParenGroupFromModuleInstantiation(
    const verible::Symbol &gate_instance);

// For a given gate instance subtree returns the TokenInfo of the module name.
// e.g. "bar b1();" returns TokenInfo for "b1".
const verible::TokenInfo *GetModuleInstanceNameTokenInfoFromGateInstance(
    const verible::Symbol &);

// For a given register variable subtree returns the TokenInfo of the instance
// name. e.g. "int b1;" returns TokenInfo for "b1".
const verible::TokenInfo *GetInstanceNameTokenInfoFromRegisterVariable(
    const verible::Symbol &);

// For a given node tagged with kVariableDeclarationAssign return the
// unqualified id inside that node.
// e.g from "int x" or "logic x" or "bit x" return the leaf spanning "x".
const verible::SyntaxTreeLeaf *
GetUnqualifiedIdFromVariableDeclarationAssignment(
    const verible::Symbol &variable_declaration_assign);

// Extracts kExpression node from nodes tagged with kVariableDeclarationAssign
// (if exists).
const verible::SyntaxTreeNode *
GetTrailingExpressionFromVariableDeclarationAssign(
    const verible::Symbol &variable_declaration_assign);

// Extracts kExpression node from nodes tagged with kRegisterVariable (if
// exists).
const verible::SyntaxTreeNode *GetTrailingExpressionFromRegisterVariable(
    const verible::Symbol &register_variable);

// Extracts kPackedDimensions node from nodes tagged with kDataDeclaration.
const verible::SyntaxTreeNode *GetPackedDimensionFromDataDeclaration(
    const verible::Symbol &data_declaration);

// Extracts kUnpackedDimensions node from nodes tagged with kRegisterVariable.
const verible::SyntaxTreeNode *GetUnpackedDimensionFromRegisterVariable(
    const verible::Symbol &register_variable);

// Extracts kUnpackedDimensions node from nodes tagged with
// kVariableDeclarationAssign.
const verible::SyntaxTreeNode *
GetUnpackedDimensionFromVariableDeclarationAssign(
    const verible::Symbol &variable_declaration_assign);

// Returns the type name from data declaration (if exists).
// The type can be inside a node tagged with kDataType or kLocalRoot.
// e.g module_type m();  return "module_type".
// e.g some_type m; return "some_type".
const verible::Symbol *GetTypeIdentifierFromDataDeclaration(
    const verible::Symbol &);

// Returns the node tagged with kStructType, kEnumType or kUnionType from node
// tagged with kDataDeclaration.
const verible::SyntaxTreeNode *GetStructOrUnionOrEnumTypeFromDataDeclaration(
    const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_DECLARATION_H_
