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
// package declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_PACKAGE_H_
#define VERIBLE_VERILOG_CST_PACKAGE_H_

#include <vector>

#include "common/analysis/syntax_tree_search.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"

namespace verilog {

// Find all package declarations.
std::vector<verible::TreeSearchMatch> FindAllPackageDeclarations(
    const verible::Symbol&);

// Extract the subnode of a package declaration that is the package name.
const verible::TokenInfo& GetPackageNameToken(const verible::Symbol&);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_PACKAGE_H_
