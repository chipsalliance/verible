// Copyright 2017-2023 The Verible Authors.
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

// See comment at the top of
// verilog/CST/verilog_treebuilder_utils.h that explains use
// of std::forward in Make* helper functions.

#include <utility>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-classifications.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

// Example usage: $$ = MakeBinaryExpression($1, $2, $3);
template <typename T1, typename T2, typename T3>
verible::SymbolPtr MakeBinaryExpression(T1 &&lhs, T2 &&op, T3 &&rhs) {
  const verible::SyntaxTreeLeaf &this_op(SymbolCastToLeaf(*op));
  const auto this_tag = verilog_tokentype(this_op.Tag().tag);
  // If parent (lhs) operator is associative and matches this one,
  // then flatten them into the same set of siblings.
  // This prevents excessive tree depth for long associative expressions.
  if (IsAssociativeOperator(this_tag) &&
      lhs->Kind() == verible::SymbolKind::kNode) {
    const auto &lhs_node =
        verible::down_cast<const verible::SyntaxTreeNode &>(*lhs);
    if (lhs_node.MatchesTag(NodeEnum::kBinaryExpression)) {
      const verible::SyntaxTreeLeaf &lhs_op =
          verible::SymbolCastToLeaf(*lhs_node[1]);
      const auto lhs_tag = verilog_tokentype(lhs_op.Tag().tag);
      if (lhs_tag == this_tag) {
        return verible::ExtendNode(std::forward<T1>(lhs), std::forward<T2>(op),
                                   std::forward<T3>(rhs));
      }
    }
  }
  return verible::MakeTaggedNode(NodeEnum::kBinaryExpression,
                                 std::forward<T1>(lhs), std::forward<T2>(op),
                                 std::forward<T3>(rhs));
}

// Returns true if symbol is a kNode tagged with kExpression
// Does not match other Expression tags
bool IsExpression(const verible::SymbolPtr &);

// Returns true if expression is a literal 0.
// Does not evaluate constant expressions for equivalence to 0.
bool IsZero(const verible::Symbol &);

// Returns true if integer value is successfully interpreted.
bool ConstantIntegerValue(const verible::Symbol &, int *);

// Returns the Symbol directly underneath a `kExpression` node, otherwise
// returns itself.
const verible::Symbol *UnwrapExpression(const verible::Symbol &);

// Returns the predicate expression of a kConditionExpression.
const verible::Symbol *GetConditionExpressionPredicate(const verible::Symbol &);

// Returns the true-case expression of a kConditionExpression.
const verible::Symbol *GetConditionExpressionTrueCase(const verible::Symbol &);

// Returns the false-case expression of a kConditionExpression.
const verible::Symbol *GetConditionExpressionFalseCase(const verible::Symbol &);

// Returns the operator of a kUnaryPrefixExpression
const verible::TokenInfo *GetUnaryPrefixOperator(const verible::Symbol &);

// Returns the operand of a kUnaryPrefixExpression
const verible::Symbol *GetUnaryPrefixOperand(const verible::Symbol &);

// From binary expression operations, e.g. "a + b".
// Associative binary operators may span more than two operands, e.g. "a+b+c".
std::vector<verible::TreeSearchMatch> FindAllBinaryOperations(
    const verible::Symbol &);

// TODO(fangism): evaluate constant expressions
// From a statement like "assign foo = condition_a ? a : b;", returns condition
// expressions "condition_a ? a : b".
std::vector<verible::TreeSearchMatch> FindAllConditionExpressions(
    const verible::Symbol &);

// TODO(fangism): evaluate constant expressions

// From a statement like "a[i].j[k] = p[q].r(s);", returns full references
// expressions "a[i].j[k]" and "p[q].r(s)".
// References include any indexing [], hierarchy ".x" or call "(...)"
// extensions.
std::vector<verible::TreeSearchMatch> FindAllReferenceFullExpressions(
    const verible::Symbol &);

// Returns true if reference expression is a plain variable reference with no
// hierarchy, no indexing, no calls.
const verible::TokenInfo *ReferenceIsSimpleIdentifier(
    const verible::Symbol &reference);

// Return the operator for a kIncrementDecrementExpression
// This function would extract the leaf containing '++' from expression '++a'
const verible::SyntaxTreeLeaf *GetIncrementDecrementOperator(
    const verible::Symbol &expr);

// Return the operator for a kIncrementDecrementExpression
// This function would extract the leaf containing 'a' from expression '++a'
const verible::SyntaxTreeNode *GetIncrementDecrementOperand(
    const verible::Symbol &expr);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_EXPRESSION_H_
