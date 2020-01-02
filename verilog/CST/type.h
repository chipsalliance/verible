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

// Finds all kTypeDeclaration nodes. Used for testing the functions below.
std::vector<verible::TreeSearchMatch> FindAllTypeDeclarations(
    const verible::Symbol&);

// Finds all node kStructDataType declarations. Used for testing if the type
// declaration is a struct.
std::vector<verible::TreeSearchMatch> FindAllStructDataTypeDeclarations(
    const verible::Symbol& root);

// Finds all node kUnionDataType declarations. Used for testing if the type
// declaration is a union.
std::vector<verible::TreeSearchMatch> FindAllUnionDataTypeDeclarations(
    const verible::Symbol& root);

// Returns true if the node kDataType has declared a storage type.
bool IsStorageTypeOfDataTypeSpecified(const verible::Symbol&);

// Extract the name of the typedef identifier from an enum, struct or union
// declaration.
const verible::SyntaxTreeLeaf* GetIdentifierFromTypeDeclaration(
    const verible::Symbol& symbol);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_TYPE_H_
