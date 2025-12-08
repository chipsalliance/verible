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
// type declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_TYPE_H_
#define VERIBLE_VERILOG_CST_TYPE_H_

#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"  // IWYU pragma: export
#include "verible/common/text/concrete-syntax-tree.h"  // IWYU pragma: export
#include "verible/common/text/symbol-ptr.h"            // IWYU pragma: export
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

template <typename T1, typename T2, typename T3, typename T4>
verible::SymbolPtr MakeDataType(T1 &&qualifiers, T2 &&type,
                                T3 &&delay_or_drive_strength,
                                T4 &&packed_dimensions) {
  verible::CheckOptionalSymbolAsNode(packed_dimensions,
                                     NodeEnum::kPackedDimensions);
  return verible::MakeTaggedNode(
      NodeEnum::kDataType, std::forward<T1>(qualifiers), std::forward<T2>(type),
      std::forward<T3>(delay_or_drive_strength),
      std::forward<T4>(packed_dimensions));
}

// 3-argument form assumes no delay/drive-strength modifiers
template <typename T1, typename T2, typename T3>
verible::SymbolPtr MakeDataType(T1 &&qualifiers, T2 &&type,
                                T3 &&packed_dimensions) {
  return MakeDataType(std::forward<T1>(qualifiers), std::forward<T2>(type),
                      nullptr, std::forward<T3>(packed_dimensions));
}

// 2-argument form assumes null qualifiers
template <typename T1, typename T2>
verible::SymbolPtr MakeDataType(T1 &&type, T2 &&packed_dimensions) {
  return MakeDataType(nullptr, std::forward<T1>(type),
                      std::forward<T2>(packed_dimensions));
}

// 1-argument form assumes no packed dimensions, no qualifiers
template <typename T1>
verible::SymbolPtr MakeDataType(T1 &&type) {
  return MakeDataType(std::forward<T1>(type), nullptr);
}

template <typename T1, typename T2, typename T3, typename T4, typename T5>
verible::SymbolPtr MakeTypeDeclaration(T1 &&keyword, T2 &&referenced_type,
                                       T3 &&id, T4 &&dimensions, T5 &&semi) {
  verible::CheckSymbolAsLeaf(*ABSL_DIE_IF_NULL(keyword), TK_typedef);
  /* id should be one of several identifier types, usually SymbolIdentifier */
  verible::CheckSymbolAsLeaf(*ABSL_DIE_IF_NULL(semi), ';');
  return MakeTaggedNode(NodeEnum::kTypeDeclaration,                           //
                        std::forward<T1>(keyword),                            //
                        std::forward<T2>(ABSL_DIE_IF_NULL(referenced_type)),  //
                        std::forward<T3>(ABSL_DIE_IF_NULL(id)),               //
                        std::forward<T4>(dimensions),                         //
                        std::forward<T5>(semi));
}

// 4-argument form assumes null dimensions
template <typename T1, typename T2, typename T3, typename T4>
verible::SymbolPtr MakeTypeDeclaration(T1 &&keyword, T2 &&referenced_type,
                                       T3 &&id, T4 &&semi) {
  return MakeTypeDeclaration(keyword, referenced_type, id, nullptr, semi);
}

// From a type like "foo::bar_t[3:0]", returns the node spanning "foo::bar_t",
// removing any qualifiers or dimensions.
const verible::Symbol *GetBaseTypeFromDataType(
    const verible::Symbol &data_type);

// Re-structures and re-tags subtree to look like a data-type with packed
// dimensions.  This is needed as a consequence of re-using a slice of the
// grammar for multiple purposes, which was a necessary defense against LR
// grammar conflicts.
// The original reference_call_base pointer is consumed in the process.
verible::SymbolPtr ReinterpretReferenceAsDataTypePackedDimensions(
    verible::SymbolPtr &reference_call_base);  // NOLINT

// Finds all node kDataType declarations. Used for testing the functions below.
std::vector<verible::TreeSearchMatch> FindAllDataTypeDeclarations(
    const verible::Symbol &);

// Finds all nodes tagged with kEnumName.
std::vector<verible::TreeSearchMatch> FindAllEnumNames(
    const verible::Symbol &root);

// Finds all node kDataTypePrimitive declarations. Used for testing the
// functions below.
std::vector<verible::TreeSearchMatch> FindAllDataTypePrimitive(
    const verible::Symbol &root);

// Finds all kTypeDeclaration nodes. Used for testing the functions below.
std::vector<verible::TreeSearchMatch> FindAllTypeDeclarations(
    const verible::Symbol &);

// Finds all node kEnumType declarations. Used for testing if the type
// declaration is an enum.
std::vector<verible::TreeSearchMatch> FindAllEnumTypes(
    const verible::Symbol &root);

// Finds all node kStructType declarations. Used for testing if the type
// declaration is a struct.
std::vector<verible::TreeSearchMatch> FindAllStructTypes(
    const verible::Symbol &root);

// Finds all node kDataTypeImplicitIdDimensions. Used for testing if the type
// declaration is a struct.
std::vector<verible::TreeSearchMatch> FindAllDataTypeImplicitIdDimensions(
    const verible::Symbol &root);

// Finds all node kUnionType declarations. Used for testing if the type
// declaration is a union.
std::vector<verible::TreeSearchMatch> FindAllUnionTypes(
    const verible::Symbol &root);

// Finds all node kInterfaceType declarations. Used for testing if the type
// declaration is an interface.
std::vector<verible::TreeSearchMatch> FindAllInterfaceTypes(
    const verible::Symbol &root);

// Returns true if the node kDataType has declared a storage type.
bool IsStorageTypeOfDataTypeSpecified(const verible::Symbol &);

// Extract the name of the typedef identifier from an enum, struct or union
// declaration.
const verible::SyntaxTreeLeaf *GetIdentifierFromTypeDeclaration(
    const verible::Symbol &symbol);

// Extracts kUnqualifiedId or kQualifiedId node from nodes tagged with
// kLocalRoot.
// e.g from "pkg::some_type var1" return "pkg::some_type".
const verible::Symbol *GetIdentifiersFromLocalRoot(
    const verible::Symbol &local_root);

// Extracts kUnqualifiedId or kQualifiedId node from nodes tagged with
// kDataType.
// e.g from "pkg::some_type var1" return "pkg::some_type".
const verible::Symbol *GetIdentifiersFromDataType(
    const verible::Symbol &data_type);

// Extracts kUnqualifiedId node from nodes tagged with kReferenceCallBase.
const verible::SyntaxTreeNode *GetUnqualifiedIdFromReferenceCallBase(
    const verible::Symbol &reference_call_base);

// Returns the node tagged with kStructType, kEnumType or kUnionType from node
// tagged with kInstantationType.
const verible::SyntaxTreeNode *GetStructOrUnionOrEnumTypeFromInstantiationType(
    const verible::Symbol &instantiation_type);

// Returns the node tagged with kStructType, kEnumType or kUnionType from node
// tagged with kDataType.
const verible::SyntaxTreeNode *GetStructOrUnionOrEnumTypeFromDataType(
    const verible::Symbol &data_type);

// Extracts kPackedDimensions node from node tagged with kDataTypePrimitive.
const verible::SyntaxTreeNode *GetPackedDimensionFromDataType(
    const verible::Symbol &data_type_primitive);

// Extracts a type node (without dimensions) from nodes tagged with
// kInstantiationType.
const verible::Symbol *GetBaseTypeFromInstantiationType(
    const verible::Symbol &instantiation_type);

// For a given unqualified id node returns the node spanning param
// declaration.
// e.g from "class_name#(x, y)" returns returns the node spanning "#(x, y)".
const verible::SyntaxTreeNode *GetParamListFromUnqualifiedId(
    const verible::Symbol &unqualified_id);
const verible::SyntaxTreeNode *GetParamListFromBaseType(
    const verible::Symbol &base_type);

// Return the type node of the given type declaration.
const verible::SyntaxTreeNode *GetReferencedTypeOfTypeDeclaration(
    const verible::Symbol &type_declaration);

// Extracts symbol identifier node from node tagged with
// kDataTypeImplicitIdDimension.
// e.g struct {byte xx;} extracts "xx".
// The symbol can be found at index 1 or 2 and each one is different so the
// index is returned to distinguish between them.
// This works around CST structural inconsistency (bug).
std::pair<const verible::SyntaxTreeLeaf *, int>
GetSymbolIdentifierFromDataTypeImplicitIdDimensions(
    const verible::Symbol &struct_union_member);

// For a given node tagged with GetTypeOfDataTypeImplicitIdDimensions returns
// the node spanning the type if it's not primitive type or returns nullptr.
// e.g "logic x" => returns nullptr.
// e.g from "some_type x" => return "some_type".
const verible::SyntaxTreeLeaf *GetNonprimitiveTypeOfDataTypeImplicitDimensions(
    const verible::Symbol &data_type_implicit_id_dimensions);

// For a given instantiation type node returns the node spanning param
// declaration.
const verible::SyntaxTreeNode *GetParamListFromInstantiationType(
    const verible::Symbol &instantiation_type);

// Extracts symbol identifier node from node tagged with kEnumName or
// nullptr if it doesn't exist.
// e.g from "enum {first}" extracts "first".
const verible::SyntaxTreeLeaf *GetSymbolIdentifierFromEnumName(
    const verible::Symbol &enum_name);

// Returns symbol identifier node for the type name from node tagged with
// kInstantiationType (if exists) or return nullptr.
//- e.g from "some_type x;" return "some_type".
const verible::Symbol *GetTypeIdentifierFromInstantiationType(
    const verible::Symbol &instantiation_type);

// Returns symbol identifier node for the type name from node tagged with
// kDataType (if exists) or return nullptr if the base type is not a named
// user-defined type.
//- e.g "Bus [x:y]" => extracts "Bus".
const verible::SyntaxTreeNode *GetTypeIdentifierFromDataType(
    const verible::Symbol &data_type);

// Returns symbol identifier node for the type name from node tagged with
// kDataType (if exists) or return nullptr if the base type is not a named
// user-defined type.
//- e.g "Bus" (as a type) return "Bus" (leaf token).
const verible::SyntaxTreeNode *GetTypeIdentifierFromBaseType(
    const verible::Symbol &base_type);

const verible::SyntaxTreeNode *GetLocalRootFromReference(
    const verible::Symbol &reference);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_TYPE_H_
