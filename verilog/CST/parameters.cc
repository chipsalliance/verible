// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/CST/parameters.h"

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::down_cast;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllParamDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekParamDeclaration());
}

int GetParamKeyword(const verible::Symbol& symbol) {
  // Assert that symbol is a parameter declaration.
  auto t = symbol.Tag();
  CHECK_EQ(t.kind, verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kParamDeclaration);
  const auto& node = down_cast<const SyntaxTreeNode&>(symbol);

  // Currently the LRM is vague on what to do if no parameter/localparam is
  // declared, see example below. As such, if it's not declared, we will treat
  // it as a parameter.
  //
  // module foo #(int Bar = 1); endmodule
  //
  const int kParamKeywordIdx = 0;
  if (node[kParamKeywordIdx] == nullptr) return TK_parameter;
  const auto* param_keyword_symbol = node[kParamKeywordIdx].get();
  const auto* leaf = down_cast<const verible::SyntaxTreeLeaf*>(
      ABSL_DIE_IF_NULL(param_keyword_symbol));
  return leaf->get().token_enum;
}

const verible::Symbol* GetParamTypeSymbol(const verible::Symbol& symbol) {
  // Assert that symbol is a parameter declaration.
  auto t = symbol.Tag();
  CHECK_EQ(t.kind, verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kParamDeclaration);

  const auto& node = down_cast<const SyntaxTreeNode&>(symbol);
  constexpr int kParamTypeIdx = 1;
  return node[kParamTypeIdx].get();
}

const verible::TokenInfo& GetParameterNameToken(const verible::Symbol& symbol) {
  const auto* param_type_symbol = GetParamTypeSymbol(symbol);
  const auto& param_type_node =
      down_cast<const SyntaxTreeNode&>(*param_type_symbol);

  // Check for implicit type declaration, in which case [2] will be a leaf.
  const auto* identifier_symbol = param_type_node[2].get();
  auto t = ABSL_DIE_IF_NULL(identifier_symbol)->Tag();
  const SyntaxTreeLeaf* identifier_leaf = nullptr;
  if (t.kind == verible::SymbolKind::kNode)
    identifier_leaf = GetIdentifier(*identifier_symbol);
  else
    identifier_leaf = down_cast<const SyntaxTreeLeaf*>(identifier_symbol);

  return ABSL_DIE_IF_NULL(identifier_leaf)->get();
}

const verible::TokenInfo& GetSymbolIdentifierFromParamDeclaration(
    const verible::Symbol& symbol) {
  // Assert that symbol is a 'parameter type' declaration.
  CHECK(IsParamTypeDeclaration(symbol));

  const auto* type_symbol = GetTypeAssignmentFromParamDeclaration(symbol);
  const auto* symbol_identifier_leaf =
      GetIdentifierLeafFromTypeAssignment(*ABSL_DIE_IF_NULL(type_symbol));
  return ABSL_DIE_IF_NULL(symbol_identifier_leaf)->get();
}

bool IsParamTypeDeclaration(const verible::Symbol& symbol) {
  // Assert that symbol is a parameter declaration.
  auto t = symbol.Tag();
  CHECK_EQ(t.kind, verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kParamDeclaration);

  const auto* param_type_symbol = GetParamTypeSymbol(symbol);
  if (param_type_symbol->Kind() == verible::SymbolKind::kLeaf) {
    // Check that its token_enum is TK_type.
    const auto* tk_type_leaf =
        down_cast<const SyntaxTreeLeaf*>(param_type_symbol);
    CHECK_EQ(tk_type_leaf->get().token_enum, TK_type);
    return true;
  }
  return false;
}

const verible::Symbol* GetTypeAssignmentFromParamDeclaration(
    const verible::Symbol& symbol) {
  // Assert that symbol is NodekParamDeclaration
  CHECK_EQ(symbol.Kind(), verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kParamDeclaration);

  // Get the Type AssignmentList or kTypeAssignment symbol.
  const auto& node = down_cast<const SyntaxTreeNode&>(symbol);
  constexpr int kTypeAssignmentIdx = 2;
  const auto* assignment_symbol = node[kTypeAssignmentIdx].get();
  CHECK_EQ(ABSL_DIE_IF_NULL(assignment_symbol)->Kind(),
           verible::SymbolKind::kNode);
  const auto assignment_tag = assignment_symbol->Tag();

  // Check which type of node it is.
  if (NodeEnum(assignment_tag.tag) == NodeEnum::kTypeAssignment) {
    CHECK_EQ(NodeEnum(assignment_symbol->Tag().tag), NodeEnum::kTypeAssignment);
    return assignment_symbol;
  } else {
    CHECK_EQ(NodeEnum(assignment_symbol->Tag().tag),
             NodeEnum::kTypeAssignmentList);

    const auto& assignment_node =
        down_cast<const SyntaxTreeNode&>(*assignment_symbol);
    const auto* type_symbol = assignment_node[0].get();

    // Make sure we're returning a NodekTypeAssignment.
    CHECK_EQ(type_symbol->Kind(), verible::SymbolKind::kNode);
    CHECK_EQ(NodeEnum(type_symbol->Tag().tag), NodeEnum::kTypeAssignment);
    return type_symbol;
  }
}

const verible::SyntaxTreeLeaf* GetIdentifierLeafFromTypeAssignment(
    const verible::Symbol& symbol) {
  // Assert that symbol is a kTypeAssignment node.
  CHECK_EQ(symbol.Kind(), verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kTypeAssignment);

  const auto& type_assignment_node = down_cast<const SyntaxTreeNode&>(symbol);
  // Get the SymbolIdentifier.
  const auto* identifier_symbol = type_assignment_node[0].get();
  return down_cast<const SyntaxTreeLeaf*>(ABSL_DIE_IF_NULL(identifier_symbol));
}

const verible::Symbol* GetParamTypeInfoSymbol(const verible::Symbol& symbol) {
  // Assert that symbol is NodekParamDeclaration
  CHECK_EQ(symbol.Kind(), verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kParamDeclaration);

  const auto* param_type_symbol = GetParamTypeSymbol(symbol);
  const auto& node = verible::SymbolCastToNode(*param_type_symbol);
  constexpr int kTypeInfoIdx = 0;
  return node[kTypeInfoIdx].get();
}

bool IsTypeInfoEmpty(const verible::Symbol& symbol) {
  // Assert that symbol is NodekTypeInfo
  CHECK_EQ(symbol.Kind(), verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kTypeInfo);

  const auto& type_info_node = verible::SymbolCastToNode(symbol);

  return (type_info_node[0] == nullptr && type_info_node[1] == nullptr &&
          type_info_node[2] == nullptr);
}

}  // namespace verilog
