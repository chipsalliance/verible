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

#include <vector>

#include "common/analysis/syntax_tree_search.h"
#include "common/text/symbol.h"

namespace verilog {

// Finds all node kDataType declarations. Used for testing the functions below.
std::vector<verible::TreeSearchMatch> FindAllDataTypeDeclarations(
    const verible::Symbol&);

// Finds all nodes tagged with kEnumName.
std::vector<verible::TreeSearchMatch> FindAllEnumNames(
    const verible::Symbol& root);

// Finds all node kDataTypePrimitive declarations. Used for testing the
// functions below.
std::vector<verible::TreeSearchMatch> FindAllDataTypePrimitive(
    const verible::Symbol& root);

// Finds all kTypeDeclaration nodes. Used for testing the functions below.
std::vector<verible::TreeSearchMatch> FindAllTypeDeclarations(
    const verible::Symbol&);

// Finds all node kEnumType declarations. Used for testing if the type
// declaration is an enum.
std::vector<verible::TreeSearchMatch> FindAllEnumTypes(
    const verible::Symbol& root);

// Finds all node kStructType declarations. Used for testing if the type
// declaration is a struct.
std::vector<verible::TreeSearchMatch> FindAllStructTypes(
    const verible::Symbol& root);

// Finds all node kDataTypeImplicitIdDimensions. Used for testing if the type
// declaration is a struct.
std::vector<verible::TreeSearchMatch> FindAllDataTypeImplicitIdDimensions(
    const verible::Symbol& root);

// Finds all node kUnionType declarations. Used for testing if the type
// declaration is a union.
std::vector<verible::TreeSearchMatch> FindAllUnionTypes(
    const verible::Symbol& root);

// Finds all node kInterfaceType declarations. Used for testing if the type
// declaration is an interface.
std::vector<verible::TreeSearchMatch> FindAllInterfaceTypes(
    const verible::Symbol& root);

// Returns true if the node kDataType has declared a storage type.
bool IsStorageTypeOfDataTypeSpecified(const verible::Symbol&);

// Extract the name of the typedef identifier from an enum, struct or union
// declaration.
const verible::SyntaxTreeLeaf* GetIdentifierFromTypeDeclaration(
    const verible::Symbol& symbol);

// Extracts kReferenceCallBase node from nodes tagged with kInstantiationType.
const verible::SyntaxTreeNode& GetReferenceCallBaseFromInstantiationType(
    const verible::Symbol& instantiation_type);

// Extracts kReference node from nodes tagged with kReferenceCallBase.
const verible::SyntaxTreeNode& GetReferenceFromReferenceCallBase(
    const verible::Symbol& reference_call_base);

// Extracts kLocalRoot node from nodes tagged with kReference.
const verible::SyntaxTreeNode& GetLocalRootFromReference(
    const verible::Symbol& reference);

// Extracts kUnqualifiedId node from nodes tagged with kLocalRoot.
const verible::SyntaxTreeNode& GetUnqualifiedIdFromLocalRoot(
    const verible::Symbol& local_root);

// Extracts kUnqualifiedId node from nodes tagged with kReferenceCallBase.
const verible::SyntaxTreeNode& GetUnqualifiedIdFromReferenceCallBase(
    const verible::Symbol& reference_call_base);

// Returns the node tagged with kStructType, kEnumType or kUnionType from node
// tagged with kInstantationType.
const verible::SyntaxTreeNode* GetStructOrUnionOrEnumTypeFromInstantiationType(
    const verible::Symbol& instantiation_type);

// Extracts kPackedDimensions node from node tagged with kDataTypePrimitive.
const verible::SyntaxTreeNode& GetPackedDimensionFromDataType(
    const verible::Symbol& data_type_primitive);

// Extracts kUnqualifiedId node from nodes tagged with kInstantiationType.
const verible::SyntaxTreeNode* GetUnqualifiedIdFromInstantiationType(
    const verible::Symbol& instantiation_type);

// Return the type node of the given type declaration.
const verible::SyntaxTreeNode* GetReferencedTypeOfTypeDeclaration(
    const verible::Symbol& type_declaration);

// Extracts symbol identifier node from node tagged with
// kDataTypeImplicitIdDimension.
// e.g struct {byte xx;} extracts "xx".
// The symbol can be found at index 1 or 2 and each one is different so the
// index is returned to distinguish between them.
// This works around CST structural inconsistency (bug).
std::pair<const verible::SyntaxTreeLeaf*, int>
GetSymbolIdentifierFromDataTypeImplicitIdDimensions(
    const verible::Symbol& struct_union_member);

// For a given node tagged with GetTypeOfDataTypeImplicitIdDimensions returns
// the node spanning the type if it's not primitive type or returns nullptr.
// e.g logic x => returns nullptr.
// e.g from "some_type x" => return "some_type".
const verible::SyntaxTreeLeaf* GetNonprimitiveTypeOfDataTypeImplicitDimensions(
    const verible::Symbol& data_type_implicit_id_dimensions);

// For a given instantiation type node returns the node spanning param
// declaration.
const verible::SyntaxTreeNode* GetParamListFromInstantiationType(
    const verible::Symbol& instantiation_type);

// Extracts symbol identifier node from node tagged with kEnumName.
// e.g from "enum {first}" extracts "first".
const verible::SyntaxTreeLeaf& GetSymbolIdentifierFromEnumName(
    const verible::Symbol& enum_name);

// Returns symbol identifier node for the type name from node tagged with
// kInstantiationType (if exists) or return nullptr.
//- e.g from "some_type x;" return "some_type".
const verible::SyntaxTreeLeaf* GetTypeIdentifierFromInstantiationType(
    const verible::Symbol& instantiation_type);

// Returns symbol identifier node for the type name from node tagged with
// kDataType (if exists) or return nullptr.
//- e.g "Bus x" => extracts "Bus".
const verible::SyntaxTreeLeaf* GetTypeIdentifierFromDataType(
    const verible::Symbol& data_type);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_TYPE_H_
