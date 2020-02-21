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

#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllConstraintDeclarations(
    const verible::Symbol& root);

bool IsOutOfLineConstraintDefinition(const verible::Symbol& symbol);

const verible::TokenInfo& GetSymbolIdentifierFromConstraintDeclaration(
    const verible::Symbol& symbol);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_CONSTRAINTS_H_
