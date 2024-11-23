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

#include "verible/verilog/CST/parameters.h"

#include <initializer_list>
#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/token-info.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"
#include "verible/verilog/parser/verilog-token-enum.h"

namespace verilog {

using verible::down_cast;
using verible::SyntaxTreeLeaf;

std::vector<verible::TreeSearchMatch> FindAllParamDeclarations(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekParamDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllNamedParams(
    const verible::Symbol &root) {
  return verible::SearchSyntaxTree(root, NodekParamByName());
}

verilog_tokentype GetParamKeyword(const verible::Symbol &symbol) {
  // Currently the LRM is vague on what to do if no parameter/localparam is
  // declared, see example below. As such, if it's not declared, we will treat
  // it as a parameter.
  //
  // module foo #(int Bar = 1); endmodule
  //
  const auto *param_keyword_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kParamDeclaration, 0);
  if (param_keyword_symbol == nullptr) return TK_parameter;
  const auto *leaf = down_cast<const SyntaxTreeLeaf *>(param_keyword_symbol);
  return static_cast<verilog_tokentype>(leaf->get().token_enum());
}

const verible::TokenInfo *GetParameterToken(const verible::Symbol &symbol) {
  const auto *param_keyword_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kParamDeclaration, 0);
  if (param_keyword_symbol == nullptr) return nullptr;
  const auto *leaf = down_cast<const SyntaxTreeLeaf *>(param_keyword_symbol);
  return &leaf->get();
}

const verible::Symbol *GetParamTypeSymbol(const verible::Symbol &symbol) {
  return verible::GetSubtreeAsSymbol(symbol, NodeEnum::kParamDeclaration, 1);
}

const verible::TokenInfo *GetParameterNameToken(const verible::Symbol &symbol) {
  const auto *param_type_symbol = GetParamTypeSymbol(symbol);
  if (!param_type_symbol) return nullptr;
  // Check for implicit type declaration, in which case [2] will be a leaf.
  const auto *identifier_symbol =
      verible::GetSubtreeAsSymbol(*param_type_symbol, NodeEnum::kParamType, 2);
  if (!identifier_symbol) return nullptr;
  auto t = identifier_symbol->Tag();
  const SyntaxTreeLeaf *identifier_leaf = nullptr;
  if (t.kind == verible::SymbolKind::kNode) {
    identifier_leaf = GetIdentifier(*identifier_symbol);
  } else {
    identifier_leaf = down_cast<const SyntaxTreeLeaf *>(identifier_symbol);
  }
  if (!identifier_leaf) return nullptr;
  return &identifier_leaf->get();
}

std::vector<const verible::TokenInfo *> GetAllParameterNameTokens(
    const verible::Symbol &symbol) {
  std::vector<const verible::TokenInfo *> identifiers;
  identifiers.push_back(GetParameterNameToken(symbol));

  for (const auto *s : GetAllAssignedParameterSymbols(symbol)) {
    identifiers.push_back(GetAssignedParameterNameToken(*s));
  }

  return identifiers;
}

const verible::TokenInfo *GetAssignedParameterNameToken(
    const verible::Symbol &symbol) {
  const auto *identifier = SymbolCastToNode(symbol)[0].get();
  if (!identifier) return nullptr;

  return &AutoUnwrapIdentifier(*identifier)->get();
}

std::vector<const verible::Symbol *> GetAllAssignedParameterSymbols(
    const verible::Symbol &root) {
  std::vector<const verible::Symbol *> symbols;

  for (const auto &id : SearchSyntaxTree(root, NodekParameterAssign())) {
    symbols.push_back(id.match);
  }

  return symbols;
}

const verible::TokenInfo *GetSymbolIdentifierFromParamDeclaration(
    const verible::Symbol &symbol) {
  // Assert that symbol is a 'parameter type' declaration.
  if (!IsParamTypeDeclaration(symbol)) return nullptr;

  const auto *type_symbol = GetTypeAssignmentFromParamDeclaration(symbol);
  if (!type_symbol) return nullptr;
  const auto *symbol_identifier_leaf =
      GetIdentifierLeafFromTypeAssignment(*type_symbol);
  if (!symbol_identifier_leaf) return nullptr;
  return &symbol_identifier_leaf->get();
}

bool IsParamTypeDeclaration(const verible::Symbol &symbol) {
  // Assert that symbol is a parameter declaration.
  auto t = symbol.Tag();
  CHECK_EQ(t.kind, verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kParamDeclaration);

  const auto *param_type_symbol = GetParamTypeSymbol(symbol);
  if (param_type_symbol->Kind() == verible::SymbolKind::kLeaf) {
    // Check that its token_enum is TK_type.
    const auto *tk_type_leaf =
        down_cast<const SyntaxTreeLeaf *>(param_type_symbol);
    CHECK_EQ(tk_type_leaf->get().token_enum(), TK_type);
    return true;
  }
  return false;
}

const verible::SyntaxTreeNode *GetTypeAssignmentFromParamDeclaration(
    const verible::Symbol &symbol) {
  // Get the Type AssignmentList or kTypeAssignment symbol.
  const auto *assignment_symbol =
      verible::GetSubtreeAsSymbol(symbol, NodeEnum::kParamDeclaration, 2);
  if (assignment_symbol == nullptr) {
    return nullptr;
  }

  const auto assignment_tag = assignment_symbol->Tag();

  // TODO(fangism): restructure CST for consistency and simplify this logic
  // Check which type of node it is.
  if (NodeEnum(assignment_tag.tag) == NodeEnum::kTypeAssignment) {
    return &verible::SymbolCastToNode(*assignment_symbol);
  }
  if (NodeEnum(assignment_tag.tag) == NodeEnum::kTypeAssignmentList) {
    const auto &type_symbol = verible::GetSubtreeAsNode(
        *assignment_symbol, NodeEnum::kTypeAssignmentList, 0,
        NodeEnum::kTypeAssignment);
    return type_symbol;
  }

  return nullptr;
}

const verible::SyntaxTreeLeaf *GetIdentifierLeafFromTypeAssignment(
    const verible::Symbol &symbol) {
  return verible::GetSubtreeAsLeaf(symbol, NodeEnum::kTypeAssignment, 0);
}

const verible::SyntaxTreeNode *GetExpressionFromTypeAssignment(
    const verible::Symbol &type_assignment) {
  const verible::Symbol *expression = verible::GetSubtreeAsSymbol(
      type_assignment, NodeEnum::kTypeAssignment, 2);
  if (expression == nullptr ||
      NodeEnum(expression->Tag().tag) != NodeEnum::kExpression) {
    return nullptr;
  }
  return &verible::SymbolCastToNode(*expression);
}

const verible::Symbol *GetParamTypeInfoSymbol(const verible::Symbol &symbol) {
  const auto *param_type_symbol = GetParamTypeSymbol(symbol);
  return verible::GetSubtreeAsSymbol(*param_type_symbol, NodeEnum::kParamType,
                                     0);
}

namespace {
// TODO(hzeller): provide something like this in tree_utils.h ?
struct EnumTokenIndex {
  NodeEnum expected_type;
  int next_index;
};
const verible::Symbol *TryDescentPath(
    const verible::Symbol &symbol, std::initializer_list<EnumTokenIndex> path) {
  const verible::Symbol *value = &symbol;
  for (auto p : path) {
    if (NodeEnum(value->Tag().tag) != p.expected_type) return nullptr;
    value = GetSubtreeAsSymbol(*value, p.expected_type, p.next_index);
    if (value == nullptr) return nullptr;
  }
  return value;
}
}  // namespace

const verible::Symbol *GetParamAssignExpression(const verible::Symbol &symbol) {
  return TryDescentPath(symbol, {{NodeEnum::kParamDeclaration, 2},
                                 {NodeEnum::kTrailingAssign, 1},
                                 {NodeEnum::kExpression, 0}});
}

bool IsTypeInfoEmpty(const verible::Symbol &symbol) {
  // Assert that symbol is NodekTypeInfo
  CHECK_EQ(symbol.Kind(), verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kTypeInfo);

  const auto &type_info_node = verible::SymbolCastToNode(symbol);

  return (type_info_node[0] == nullptr && type_info_node[1] == nullptr &&
          type_info_node[2] == nullptr);
}

const verible::SyntaxTreeLeaf *GetNamedParamFromActualParam(
    const verible::Symbol &param_by_name) {
  const verible::SyntaxTreeLeaf *param_name =
      verible::GetSubtreeAsLeaf(param_by_name, NodeEnum::kParamByName, 1);
  if (!param_name) return nullptr;
  return AutoUnwrapIdentifier(*param_name);
}

const verible::SyntaxTreeNode *GetParenGroupFromActualParam(
    const verible::Symbol &param_by_name) {
  return verible::CheckOptionalSymbolAsNode(
      verible::GetSubtreeAsSymbol(param_by_name, NodeEnum::kParamByName, 2));
}

}  // namespace verilog
