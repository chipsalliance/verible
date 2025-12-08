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

#ifndef VERIBLE_VERILOG_CST_CONSTRAINTS_H_
#define VERIBLE_VERILOG_CST_CONSTRAINTS_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllConstraintDeclarations(
    const verible::Symbol &root);

bool IsOutOfLineConstraintDefinition(const verible::Symbol &symbol);

const verible::TokenInfo *GetSymbolIdentifierFromConstraintDeclaration(
    const verible::Symbol &symbol);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_CONSTRAINTS_H_
