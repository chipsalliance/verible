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

#include "verilog/CST/statement.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

using verible::Symbol;
using verible::SymbolCastToNode;
using verible::SyntaxTreeNode;

static const SyntaxTreeNode& GetGenericStatementBody(
    const SyntaxTreeNode& node) {
  // In most controlled constructs, the controlled statement body is
  // in tail position.  Exceptions include: DoWhile.
  return SymbolCastToNode(*node.children().back());
}

const SyntaxTreeNode& GetIfClauseGenerateBody(const Symbol& if_clause) {
  const auto& body_node = GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(if_clause), NodeEnum::kGenerateIfClause));
  return GetSubtreeAsNode(body_node, NodeEnum::kGenerateIfBody, 0);
}

const SyntaxTreeNode& GetElseClauseGenerateBody(const Symbol& else_clause) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(else_clause), NodeEnum::kGenerateElseClause));
  return GetSubtreeAsNode(body_node, NodeEnum::kGenerateElseBody, 0);
}

const SyntaxTreeNode& GetLoopGenerateBody(const Symbol& loop) {
  return GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(loop), NodeEnum::kLoopGenerateConstruct));
}

const SyntaxTreeNode& GetConditionalGenerateIfClause(
    const Symbol& conditional) {
  return GetSubtreeAsNode(conditional, NodeEnum::kConditionalGenerateConstruct,
                          0, NodeEnum::kGenerateIfClause);
}

const SyntaxTreeNode* GetConditionalGenerateElseClause(
    const Symbol& conditional) {
  const auto& node = CheckNodeEnum(SymbolCastToNode(conditional),
                                   NodeEnum::kConditionalGenerateConstruct);
  if (node.children().size() < 2) return nullptr;
  const Symbol* else_ptr = node.children().back().get();
  if (else_ptr == nullptr) return nullptr;
  return &CheckNodeEnum(SymbolCastToNode(*else_ptr),
                        NodeEnum::kGenerateElseClause);
}

const SyntaxTreeNode& GetIfClauseStatementBody(const Symbol& if_clause) {
  const auto& body_node = GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(if_clause), NodeEnum::kIfClause));
  return GetSubtreeAsNode(body_node, NodeEnum::kIfBody, 0);
}

const SyntaxTreeNode& GetElseClauseStatementBody(const Symbol& else_clause) {
  const auto& body_node = GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(else_clause), NodeEnum::kElseClause));
  return GetSubtreeAsNode(body_node, NodeEnum::kElseBody, 0);
}

const SyntaxTreeNode& GetConditionalStatementIfClause(
    const Symbol& conditional) {
  return GetSubtreeAsNode(conditional, NodeEnum::kConditionalStatement, 0,
                          NodeEnum::kIfClause);
}

const SyntaxTreeNode* GetConditionalStatementElseClause(
    const Symbol& conditional) {
  const auto& node = CheckNodeEnum(SymbolCastToNode(conditional),
                                   NodeEnum::kConditionalStatement);
  if (node.children().size() < 2) return nullptr;
  const Symbol* else_ptr = node.children().back().get();
  if (else_ptr == nullptr) return nullptr;
  return &CheckNodeEnum(SymbolCastToNode(*else_ptr), NodeEnum::kElseClause);
}

const SyntaxTreeNode& GetLoopStatementBody(const Symbol& loop) {
  return GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(loop), NodeEnum::kForLoopStatement));
}

const SyntaxTreeNode& GetDoWhileStatementBody(const Symbol& do_while) {
  return GetSubtreeAsNode(SymbolCastToNode(do_while),
                          NodeEnum::kDoWhileLoopStatement, 1);
}

const SyntaxTreeNode& GetForeverStatementBody(const Symbol& forever) {
  return GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(forever), NodeEnum::kForeverLoopStatement));
}

const SyntaxTreeNode& GetForeachStatementBody(const Symbol& foreach) {
  return GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(foreach), NodeEnum::kForeachLoopStatement));
}

const SyntaxTreeNode& GetRepeatStatementBody(const Symbol& repeat) {
  return GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(repeat), NodeEnum::kRepeatLoopStatement));
}

const SyntaxTreeNode& GetWhileStatementBody(const Symbol& while_stmt) {
  return GetGenericStatementBody(CheckNodeEnum(SymbolCastToNode(while_stmt),
                                               NodeEnum::kWhileLoopStatement));
}

const SyntaxTreeNode& GetProceduralTimingControlStatementBody(
    const Symbol& proc_timing_control) {
  return GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(proc_timing_control),
                    NodeEnum::kProceduralTimingControlStatement));
}

const SyntaxTreeNode* GetAnyControlStatementBody(const Symbol& statement) {
  switch (NodeEnum(SymbolCastToNode(statement).Tag().tag)) {
    // generate
    case NodeEnum::kGenerateIfClause:
      return &GetIfClauseGenerateBody(statement);
    case NodeEnum::kGenerateElseClause:
      return &GetElseClauseGenerateBody(statement);
    case NodeEnum::kLoopGenerateConstruct:
      return &GetLoopGenerateBody(statement);
    // statements
    case NodeEnum::kIfClause:
      return &GetIfClauseStatementBody(statement);
    case NodeEnum::kElseClause:
      return &GetElseClauseStatementBody(statement);
    case NodeEnum::kForLoopStatement:
      return &GetLoopStatementBody(statement);
    case NodeEnum::kDoWhileLoopStatement:
      return &GetDoWhileStatementBody(statement);
    case NodeEnum::kForeverLoopStatement:
      return &GetForeverStatementBody(statement);
    case NodeEnum::kForeachLoopStatement:
      return &GetForeachStatementBody(statement);
    case NodeEnum::kRepeatLoopStatement:
      return &GetRepeatStatementBody(statement);
    case NodeEnum::kWhileLoopStatement:
      return &GetWhileStatementBody(statement);
    case NodeEnum::kProceduralTimingControlStatement:
      return &GetProceduralTimingControlStatementBody(statement);
    default:
      return nullptr;
  }
}

const SyntaxTreeNode* GetAnyConditionalIfClause(const Symbol& conditional) {
  switch (NodeEnum(SymbolCastToNode(conditional).Tag().tag)) {
    // generate
    case NodeEnum::kConditionalGenerateConstruct:
      return &GetConditionalGenerateIfClause(conditional);
    // statement
    case NodeEnum::kConditionalStatement:
      return &GetConditionalStatementIfClause(conditional);
    default:
      return nullptr;
  }
}

const SyntaxTreeNode* GetAnyConditionalElseClause(const Symbol& conditional) {
  switch (NodeEnum(SymbolCastToNode(conditional).Tag().tag)) {
    // generate
    case NodeEnum::kConditionalGenerateConstruct:
      return GetConditionalGenerateElseClause(conditional);
    // statement
    case NodeEnum::kConditionalStatement:
      return GetConditionalStatementElseClause(conditional);
    default:
      return nullptr;
  }
}

}  // namespace verilog
