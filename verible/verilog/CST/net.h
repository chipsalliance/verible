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
// net declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_NET_H_
#define VERIBLE_VERILOG_CST_NET_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"

namespace verilog {

// Find all net declarations.  In the grammar, net_declaration only falls under
// package_or_generate_item, so this excludes nets declared as ports.
// See port.h for port declarations.
// TODO(b/132652866): handle data declarations like 'logic' and 'reg'.
std::vector<verible::TreeSearchMatch> FindAllNetDeclarations(
    const verible::Symbol &);

// Returns tokens that correspond to declared names in net declarations
std::vector<const verible::TokenInfo *> GetIdentifiersFromNetDeclaration(
    const verible::Symbol &symbol);

// Returns the declared identifier from a kNetVariable or nullptr if invalid.
const verible::SyntaxTreeLeaf *GetNameLeafOfNetVariable(
    const verible::Symbol &net_variable);

// Returns the declared identifier from a kRegisterVariable.
const verible::SyntaxTreeLeaf *GetNameLeafOfRegisterVariable(
    const verible::Symbol &register_variable);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_NET_H_
