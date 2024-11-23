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

#include "verible/verilog/CST/expression.h"

#include <memory>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

using verible::down_cast;
using verible::Symbol;
using verible::SymbolKind;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TreeSearchMatch;

bool IsExpression(const verible::SymbolPtr &symbol_ptr) {
  if (symbol_ptr == nullptr) return false;
  if (symbol_ptr->Kind() != SymbolKind::kNode) return false;
  const auto &node = down_cast<const SyntaxTreeNode &>(*symbol_ptr);
  return node.MatchesTag(NodeEnum::kExpression);
}

bool IsZero(const Symbol &expr) {
  const Symbol *child = verible::DescendThroughSingletons(expr);
  int value;
  if (ConstantIntegerValue(*child, &value)) {
    return value == 0;
  }
  if (child->Kind() != SymbolKind::kLeaf) return false;
  const auto &term = down_cast<const SyntaxTreeLeaf &>(*child);
  auto text = term.get().text();
  // TODO(fangism): Could do more sophisticated constant expression evaluation
  // but for now this is a good first implementation.
  return (text == "\'0");
}

bool ConstantIntegerValue(const verible::Symbol &expr, int *value) {
  const Symbol *child = verible::DescendThroughSingletons(expr);
  if (child->Kind() != SymbolKind::kLeaf) return false;
  const auto &term = down_cast<const SyntaxTreeLeaf &>(*child);
  // Don't even need to check the leaf token's enumeration type.
  auto text = term.get().text();
  return absl::SimpleAtoi(text, value);
}

const verible::Symbol *UnwrapExpression(const verible::Symbol &expr) {
  if (expr.Kind() == SymbolKind::kLeaf) return &expr;

  const auto &node = verible::SymbolCastToNode(expr);
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);

  if (tag != NodeEnum::kExpression) return &expr;

  return node.front().get();
}

const verible::Symbol *GetConditionExpressionPredicate(
    const verible::Symbol &condition_expr) {
  return GetSubtreeAsSymbol(condition_expr, NodeEnum::kConditionExpression, 0);
}

const verible::Symbol *GetConditionExpressionTrueCase(
    const verible::Symbol &condition_expr) {
  return GetSubtreeAsSymbol(condition_expr, NodeEnum::kConditionExpression, 2);
}

const verible::Symbol *GetConditionExpressionFalseCase(
    const verible::Symbol &condition_expr) {
  return GetSubtreeAsSymbol(condition_expr, NodeEnum::kConditionExpression, 4);
}

const verible::TokenInfo *GetUnaryPrefixOperator(
    const verible::Symbol &symbol) {
  const SyntaxTreeNode *node = symbol.Kind() == SymbolKind::kNode
                                   ? &verible::SymbolCastToNode(symbol)
                                   : nullptr;
  if (!node || !MatchNodeEnumOrNull(*node, NodeEnum::kUnaryPrefixExpression)) {
    return nullptr;
  }
  const verible::Symbol *leaf_symbol = node->front().get();
  return &verible::down_cast<const verible::SyntaxTreeLeaf *>(leaf_symbol)
              ->get();
}

const verible::Symbol *GetUnaryPrefixOperand(const verible::Symbol &symbol) {
  const SyntaxTreeNode *node = symbol.Kind() == SymbolKind::kNode
                                   ? &verible::SymbolCastToNode(symbol)
                                   : nullptr;
  if (!node || !MatchNodeEnumOrNull(*node, NodeEnum::kUnaryPrefixExpression)) {
    return nullptr;
  }
  return node->back().get();
}

std::vector<verible::TreeSearchMatch> FindAllBinaryOperations(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekBinaryExpression());
}

std::vector<TreeSearchMatch> FindAllConditionExpressions(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekConditionExpression());
}

std::vector<TreeSearchMatch> FindAllReferenceFullExpressions(
    const verible::Symbol &root) {
  auto references = verible::SearchSyntaxTree(root, NodekReference());
  auto reference_calls =
      verible::SearchSyntaxTree(root, NodekReferenceCallBase());
  for (auto &reference : references) {
    if (!(reference.context.DirectParentIs(NodeEnum::kReferenceCallBase))) {
      reference_calls.emplace_back(reference);
    }
  }
  return reference_calls;
}

static const verible::TokenInfo *ReferenceBaseIsSimple(
    const verible::SyntaxTreeNode &reference_base) {
  const Symbol *bottom = verible::DescendThroughSingletons(reference_base);
  if (!bottom) return nullptr;

  const auto tag = bottom->Tag();
  if (tag.kind == verible::SymbolKind::kLeaf) {
    const auto &token(verible::SymbolCastToLeaf(*bottom).get());
    return token.token_enum() == SymbolIdentifier ? &token : nullptr;
  }
  // Expect to hit kUnqualifiedId, which has two children.
  // child[0] should be a SymbolIdentifier (or similar) token.
  // child[1] are optional #(parameters), which would imply child[0] is
  // referring to a parameterized type.
  const auto &unqualified_id(
      verible::CheckSymbolAsNode(*bottom, NodeEnum::kUnqualifiedId));
  const auto *params = GetParamListFromUnqualifiedId(unqualified_id);
  // If there are parameters, it is not simple reference.
  // It is most likely a class-qualified static reference.
  return params == nullptr
             ? &verible::SymbolCastToLeaf(*unqualified_id.front()).get()
             : nullptr;
}

const verible::TokenInfo *ReferenceIsSimpleIdentifier(
    const verible::Symbol &reference) {
  // remove calls since they are not simple - but a ReferenceCallBase can be
  // just a reference, depending on where it is placed in the code
  if (reference.Tag().tag == (int)NodeEnum::kReferenceCallBase) return nullptr;
  const auto &reference_node(
      verible::CheckSymbolAsNode(reference, NodeEnum::kReference));
  // A simple reference contains one component without hierarchy, indexing, or
  // calls; it looks like just an identifier.
  if (reference_node.size() > 1) return nullptr;
  const auto &base_symbol = reference_node.front();
  if (!base_symbol) return nullptr;
  const auto &base_node = verible::SymbolCastToNode(*base_symbol);
  if (!base_node.MatchesTag(NodeEnum::kLocalRoot)) return nullptr;
  return ReferenceBaseIsSimple(base_node);
}

const verible::SyntaxTreeLeaf *GetIncrementDecrementOperator(
    const verible::Symbol &expr) {
  if (expr.Kind() != SymbolKind::kNode) return nullptr;

  const SyntaxTreeNode &node = verible::SymbolCastToNode(expr);

  if (!node.MatchesTag(NodeEnum::kIncrementDecrementExpression)) return nullptr;

  // Structure changes depending on the type of IncrementDecrement
  bool is_post = node.front().get()->Kind() == SymbolKind::kNode;

  return verible::GetSubtreeAsLeaf(
      expr, NodeEnum::kIncrementDecrementExpression, is_post ? 1 : 0);
}

const verible::SyntaxTreeNode *GetIncrementDecrementOperand(
    const verible::Symbol &expr) {
  if (expr.Kind() != SymbolKind::kNode) return nullptr;

  const SyntaxTreeNode &node = verible::SymbolCastToNode(expr);

  if (!node.MatchesTag(NodeEnum::kIncrementDecrementExpression)) return nullptr;

  // Structure changes depending on the type of IncrementDecrement
  bool is_post = node.front().get()->Kind() == SymbolKind::kNode;
  return verible::GetSubtreeAsNode(
      expr, NodeEnum::kIncrementDecrementExpression, is_post ? 0 : 1);
}

}  // namespace verilog
