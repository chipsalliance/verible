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

#ifndef VERIBLE_VERILOG_CST_EXPRESSION_H_
#define VERIBLE_VERILOG_CST_EXPRESSION_H_

// See comment at the top
// verilog/CST/verilog_treebuilder_utils.h that explains use
// of std::forward in Make* helper functions.

#include <utility>

#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {

// Example usage: $$ = MakeBinaryExpression($1, $2, $3);
template <typename T1, typename T2, typename T3>
verible::SymbolPtr MakeBinaryExpression(T1&& lhs, T2&& op, T3&& rhs) {
  return verible::MakeTaggedNode(NodeEnum::kBinaryExpression,
                                 std::forward<T1>(lhs), std::forward<T2>(op),
                                 std::forward<T3>(rhs));
}

// Returns true if symbol is a kNode tagged with kExpression
// Does not match other Expression tags
bool IsExpression(const verible::SymbolPtr&);

// Returns true if expression is a literal 0.
// Does not evaluate constant expressions for equivalence to 0.
bool IsZero(const verible::Symbol&);

// Returns true if integer value is successfully interpreted.
bool ConstantIntegerValue(const verible::Symbol&, int*);

// Returns the Symbol directly underneath a `kExpression` node, otherwise
// returns itself.
const verible::Symbol* UnwrapExpression(const verible::Symbol&);

// Returns the predicate expression of a kConditionExpression.
const verible::Symbol* GetConditionExpressionPredicate(const verible::Symbol&);

// Returns the true-case expression of a kConditionExpression.
const verible::Symbol* GetConditionExpressionTrueCase(const verible::Symbol&);

// Returns the false-case expression of a kConditionExpression.
const verible::Symbol* GetConditionExpressionFalseCase(const verible::Symbol&);

// From a statement like "assign foo = condition_a ? a : b;", returns condition
// expressions "condition_a ? a : b".
std::vector<verible::TreeSearchMatch> FindAllConditionExpressions(
    const verible::Symbol&);

// TODO(fangism): evaluate constant expressions

// From a statement like "a[i].j[k] = p[q].r(s);", returns full references
// expressions "a[i].j[k]" and "p[q].r(s)".
// References include any indexing [], hierarchy ".x" or call "(...)"
// extensions.
std::vector<verible::TreeSearchMatch> FindAllReferenceFullExpressions(
    const verible::Symbol&);

// Returns true if reference expression is a plain variable reference with no
// hierarchy, no indexing, no calls.
const verible::TokenInfo* ReferenceIsSimpleIdentifier(
    const verible::Symbol& reference);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_EXPRESSION_H_
