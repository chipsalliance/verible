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

#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"

namespace verilog {

// Find all macro calls.
std::vector<verible::TreeSearchMatch> FindAllMacroCalls(const verible::Symbol&);

// Find all macro calls that are whole item-level constructs.
// Compared to FindAllMacroCalls, this excludes macro call expressions.
std::vector<verible::TreeSearchMatch> FindAllMacroGenericItems(
    const verible::Symbol&);

// Returns the leaf containing the macro call name.
const verible::TokenInfo& GetMacroCallId(const verible::Symbol&);

// Returns the leaf containing the macro (as generic item) name.
const verible::TokenInfo& GetMacroGenericItemId(const verible::Symbol&);

// Returns the node containing the macro call arguments (without parentheses).
const verible::SyntaxTreeNode& GetMacroCallArgs(const verible::Symbol&);

// Returns true if there are no macro call args, e.g. `foo().
bool MacroCallArgsIsEmpty(const verible::SyntaxTreeNode&);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_MACRO_H_
