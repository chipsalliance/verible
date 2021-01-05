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

#include "verilog/CST/expression.h"

#include <memory>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/type.h"
#include "verilog/CST/verilog_matchers.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::down_cast;
using verible::Symbol;
using verible::SymbolKind;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;
using verible::TreeSearchMatch;

bool IsExpression(const verible::SymbolPtr& symbol_ptr) {
  if (symbol_ptr == nullptr) return false;
  if (symbol_ptr->Kind() != SymbolKind::kNode) return false;
  const auto& node = down_cast<const SyntaxTreeNode&>(*symbol_ptr);
  return node.MatchesTag(NodeEnum::kExpression);
}

bool IsZero(const Symbol& expr) {
  const Symbol* child = verible::DescendThroughSingletons(expr);
  int value;
  if (ConstantIntegerValue(*child, &value)) {
    return value == 0;
  }
  if (child->Kind() != SymbolKind::kLeaf) return false;
  const auto& term = down_cast<const SyntaxTreeLeaf&>(*child);
  auto text = term.get().text();
  if (text == "\'0") return true;
  // TODO(fangism): Could do more sophisticated constant expression evaluation
  // but for now it is fine for this to conservatively return false.
  return false;
}

bool ConstantIntegerValue(const verible::Symbol& expr, int* value) {
  const Symbol* child = verible::DescendThroughSingletons(expr);
  if (child->Kind() != SymbolKind::kLeaf) return false;
  const auto& term = down_cast<const SyntaxTreeLeaf&>(*child);
  // Don't even need to check the leaf token's enumeration type.
  auto text = term.get().text();
  return absl::SimpleAtoi(text, value);
}

const verible::Symbol* UnwrapExpression(const verible::Symbol& expr) {
  if (expr.Kind() == SymbolKind::kLeaf) return &expr;

  const auto& node = verible::SymbolCastToNode(expr);
  const auto tag = static_cast<verilog::NodeEnum>(node.Tag().tag);

  if (tag != NodeEnum::kExpression) return &expr;

  const auto& children = node.children();
  return children.front().get();
}

const verible::Symbol* GetConditionExpressionPredicate(
    const verible::Symbol& condition_expr) {
  return GetSubtreeAsSymbol(condition_expr, NodeEnum::kConditionExpression, 0);
}

const verible::Symbol* GetConditionExpressionTrueCase(
    const verible::Symbol& condition_expr) {
  return GetSubtreeAsSymbol(condition_expr, NodeEnum::kConditionExpression, 2);
}

const verible::Symbol* GetConditionExpressionFalseCase(
    const verible::Symbol& condition_expr) {
  return GetSubtreeAsSymbol(condition_expr, NodeEnum::kConditionExpression, 4);
}

std::vector<TreeSearchMatch> FindAllConditionExpressions(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekConditionExpression());
}

std::vector<TreeSearchMatch> FindAllReferenceFullExpressions(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekReferenceCallBase());
}

static const verible::TokenInfo* ReferenceBaseIsSimple(
    const verible::SyntaxTreeNode& reference_base) {
  const verible::Symbol& bottom(
      *ABSL_DIE_IF_NULL(verible::DescendThroughSingletons(reference_base)));
  const auto tag = bottom.Tag();
  if (tag.kind == verible::SymbolKind::kLeaf) {
    const auto& token(verible::SymbolCastToLeaf(bottom).get());
    return token.token_enum() == SymbolIdentifier ? &token : nullptr;
  }
  // Expect to hit kUnqualifiedId, which has two children.
  // child[0] should be a SymbolIdentifier (or similar) token.
  // child[1] are optional #(parameters), which would imply child[0] is
  // referring to a parameterized type.
  const auto& unqualified_id(
      verible::CheckSymbolAsNode(bottom, NodeEnum::kUnqualifiedId));
  const auto* params = GetParamListFromUnqualifiedId(unqualified_id);
  // If there are parameters, it is not simple reference.
  // It is most likely a class-qualified static reference.
  return params == nullptr
             ? &verible::SymbolCastToLeaf(*unqualified_id.children().front())
                    .get()
             : nullptr;
}

const verible::TokenInfo* ReferenceIsSimpleIdentifier(
    const verible::Symbol& reference) {
  const auto& reference_node(
      verible::CheckSymbolAsNode(reference, NodeEnum::kReferenceCallBase));
  // A simple reference contains one component without hierarchy, indexing, or
  // calls; it looks like just an identifier.
  if (reference_node.children().size() > 1) return nullptr;
  const auto& base_node = verible::SymbolCastToNode(
      *ABSL_DIE_IF_NULL(reference_node.children().front()));
  if (!base_node.MatchesTag(NodeEnum::kReference)) return nullptr;
  return ReferenceBaseIsSimple(base_node);
}

}  // namespace verilog
