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
// module declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_MACRO_H_
#define VERIBLE_VERILOG_CST_MACRO_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"  // IWYU pragma: export
#include "verible/common/text/symbol.h"                // IWYU pragma: export
#include "verible/common/text/token-info.h"
#include "verible/common/text/visitors.h"

namespace verilog {

// Find all nodes tagged with kPreprocessorDefine.
std::vector<verible::TreeSearchMatch> FindAllMacroDefinitions(
    const verible::Symbol &);

// Find all macro calls.
std::vector<verible::TreeSearchMatch> FindAllMacroCalls(
    const verible::Symbol &);

// Find all preprocessor includes.
std::vector<verible::TreeSearchMatch> FindAllPreprocessorInclude(
    const verible::Symbol &root);

// Find all macro calls that are whole item-level constructs.
// Compared to FindAllMacroCalls, this excludes macro call expressions.
std::vector<verible::TreeSearchMatch> FindAllMacroGenericItems(
    const verible::Symbol &);

// Finds all macro definition args e.g `PRINT(str1, str2) returns the nodes
// spanning str1 and str2.
std::vector<verible::TreeSearchMatch> FindAllMacroDefinitionsArgs(
    const verible::Symbol &);

// Returns the leaf containing the macro call name.
const verible::TokenInfo *GetMacroCallId(const verible::Symbol &);

// Returns the leaf containing the macro (as generic item) name.
const verible::TokenInfo *GetMacroGenericItemId(const verible::Symbol &);

// Returns the node containing the macro call paren group
const verible::SyntaxTreeNode *GetMacroCallParenGroup(const verible::Symbol &s);

// Returns the node containing the macro call arguments (without parentheses).
const verible::SyntaxTreeNode *GetMacroCallArgs(const verible::Symbol &);

// Returns true if there are no macro call args, e.g. `foo().
bool MacroCallArgsIsEmpty(const verible::SyntaxTreeNode &);

// Returns the leaf node containing the macro name from node tagged with
// kPreprocessorDefine or nullptr if it doesn't exist.
const verible::SyntaxTreeLeaf *GetMacroName(const verible::Symbol &);

// Returns the leaf node containing the macro arg name from node tagged with
// kMacroFormalArg or nullptr if it doesn't exist.
const verible::SyntaxTreeLeaf *GetMacroArgName(const verible::Symbol &);

// Returns the leaf node containing the filename from the node tagged with
// kPreprocessorInclude or nullptr if the argument is not a simple
// string-literal.
const verible::SyntaxTreeLeaf *GetFileFromPreprocessorInclude(
    const verible::Symbol &);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_MACRO_H_
