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

#include "verible/verilog/CST/statement.h"

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"
#include "verible/verilog/CST/declaration.h"
#include "verible/verilog/CST/identifier.h"
#include "verible/verilog/CST/type.h"
#include "verible/verilog/CST/verilog-matchers.h"  // IWYU pragma: keep
#include "verible/verilog/CST/verilog-nonterminals.h"

namespace verilog {

using verible::Symbol;
using verible::SymbolCastToNode;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllForLoopsInitializations(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekForInitialization());
}

std::vector<verible::TreeSearchMatch> FindAllGenerateBlocks(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekGenerateBlock());
}

std::vector<verible::TreeSearchMatch> FindAllNonBlockingAssignments(
    const verible::Symbol &root) {
  return SearchSyntaxTree(root, NodekNonblockingAssignmentStatement());
}

static const SyntaxTreeNode *GetGenericStatementBody(
    const SyntaxTreeNode *node) {
  if (!node) return node;
  // In most controlled constructs, the controlled statement body is
  // in tail position.  Exceptions include: DoWhile.
  return &SymbolCastToNode(*node->back());
}

const SyntaxTreeNode *GetIfClauseGenerateBody(const Symbol &if_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(if_clause), NodeEnum::kGenerateIfClause));
  if (!body_node) return nullptr;
  return GetSubtreeAsNode(*body_node, NodeEnum::kGenerateIfBody, 0);
}

const SyntaxTreeNode *GetElseClauseGenerateBody(const Symbol &else_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(else_clause), NodeEnum::kGenerateElseClause));
  if (!body_node) return nullptr;
  return GetSubtreeAsNode(*body_node, NodeEnum::kGenerateElseBody, 0);
}

const SyntaxTreeNode *GetLoopGenerateBody(const Symbol &loop) {
  return GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(loop), NodeEnum::kLoopGenerateConstruct));
}

const SyntaxTreeNode *GetConditionalGenerateIfClause(
    const Symbol &conditional) {
  return GetSubtreeAsNode(conditional, NodeEnum::kConditionalGenerateConstruct,
                          0, NodeEnum::kGenerateIfClause);
}

const SyntaxTreeNode *GetConditionalGenerateElseClause(
    const Symbol &conditional) {
  const auto *node = MatchNodeEnumOrNull(
      SymbolCastToNode(conditional), NodeEnum::kConditionalGenerateConstruct);
  if (!node || node->size() < 2) return nullptr;
  const Symbol *else_ptr = node->back().get();
  if (else_ptr == nullptr) return nullptr;
  return MatchNodeEnumOrNull(SymbolCastToNode(*else_ptr),
                             NodeEnum::kGenerateElseClause);
}

const SyntaxTreeNode *GetIfClauseStatementBody(const Symbol &if_clause) {
  const auto *body_node = GetGenericStatementBody(
      MatchNodeEnumOrNull(SymbolCastToNode(if_clause), NodeEnum::kIfClause));
  if (!body_node) return nullptr;
  return GetSubtreeAsNode(*body_node, NodeEnum::kIfBody, 0);
}

const SyntaxTreeNode *GetElseClauseStatementBody(const Symbol &else_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(else_clause), NodeEnum::kElseClause));
  if (!body_node) return nullptr;
  return GetSubtreeAsNode(*body_node, NodeEnum::kElseBody, 0);
}

const SyntaxTreeNode *GetConditionalStatementIfClause(
    const Symbol &conditional) {
  return GetSubtreeAsNode(conditional, NodeEnum::kConditionalStatement, 0,
                          NodeEnum::kIfClause);
}

const SyntaxTreeNode *GetConditionalStatementElseClause(
    const Symbol &conditional) {
  const auto *node = MatchNodeEnumOrNull(SymbolCastToNode(conditional),
                                         NodeEnum::kConditionalStatement);
  if (!node || node->size() < 2) return nullptr;
  const Symbol *else_ptr = node->back().get();
  if (else_ptr == nullptr) return nullptr;
  return MatchNodeEnumOrNull(SymbolCastToNode(*else_ptr),
                             NodeEnum::kElseClause);
}

const SyntaxTreeNode *GetAssertionStatementAssertClause(
    const Symbol &assertion_statement) {
  return GetSubtreeAsNode(assertion_statement, NodeEnum::kAssertionStatement, 0,
                          NodeEnum::kAssertionClause);
}

const SyntaxTreeNode *GetAssertionClauseStatementBody(
    const Symbol &assertion_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(assertion_clause), NodeEnum::kAssertionClause));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kAssertionBody, 0));
}

const SyntaxTreeNode *GetAssertionStatementElseClause(
    const Symbol &assertion_statement) {
  const auto *node = MatchNodeEnumOrNull(SymbolCastToNode(assertion_statement),
                                         NodeEnum::kAssertionStatement);
  const Symbol *else_ptr = node->back().get();
  if (else_ptr == nullptr) return nullptr;
  return MatchNodeEnumOrNull(SymbolCastToNode(*else_ptr),
                             NodeEnum::kElseClause);
}

const SyntaxTreeNode *GetAssumeStatementAssumeClause(
    const Symbol &assume_statement) {
  return GetSubtreeAsNode(assume_statement, NodeEnum::kAssumeStatement, 0,
                          NodeEnum::kAssumeClause);
}

const SyntaxTreeNode *GetAssumeClauseStatementBody(
    const Symbol &assume_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(assume_clause), NodeEnum::kAssumeClause));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kAssumeBody, 0));
}

const SyntaxTreeNode *GetAssumeStatementElseClause(
    const Symbol &assume_statement) {
  const auto *node = MatchNodeEnumOrNull(SymbolCastToNode(assume_statement),
                                         NodeEnum::kAssumeStatement);
  if (!node) return nullptr;
  const Symbol *else_ptr = node->back().get();
  if (else_ptr == nullptr) return nullptr;
  return MatchNodeEnumOrNull(SymbolCastToNode(*else_ptr),
                             NodeEnum::kElseClause);
}

const SyntaxTreeNode *GetCoverStatementBody(const Symbol &cover_statement) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(cover_statement), NodeEnum::kCoverStatement));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kCoverBody, 0));
}

const SyntaxTreeNode *GetWaitStatementBody(const Symbol &wait_statement) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(wait_statement), NodeEnum::kWaitStatement));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kWaitBody, 0));
}

const SyntaxTreeNode *GetAssertPropertyStatementAssertClause(
    const Symbol &assert_property_statement) {
  return GetSubtreeAsNode(assert_property_statement,
                          NodeEnum::kAssertPropertyStatement, 0,
                          NodeEnum::kAssertPropertyClause);
}

const SyntaxTreeNode *GetAssertPropertyStatementBody(
    const Symbol &assert_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(assert_clause), NodeEnum::kAssertPropertyClause));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kAssertPropertyBody, 0));
}

const SyntaxTreeNode *GetAssertPropertyStatementElseClause(
    const Symbol &assert_property_statement) {
  const auto *node =
      MatchNodeEnumOrNull(SymbolCastToNode(assert_property_statement),
                          NodeEnum::kAssertPropertyStatement);
  if (!node) return nullptr;
  const Symbol *else_ptr = node->back().get();
  if (else_ptr == nullptr) return nullptr;
  return MatchNodeEnumOrNull(SymbolCastToNode(*else_ptr),
                             NodeEnum::kElseClause);
}

const SyntaxTreeNode *GetAssumePropertyStatementAssumeClause(
    const Symbol &assume_property_statement) {
  return GetSubtreeAsNode(assume_property_statement,
                          NodeEnum::kAssumePropertyStatement, 0,
                          NodeEnum::kAssumePropertyClause);
}

const SyntaxTreeNode *GetAssumePropertyStatementBody(
    const Symbol &assume_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(assume_clause), NodeEnum::kAssumePropertyClause));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kAssumePropertyBody, 0));
}

const SyntaxTreeNode *GetAssumePropertyStatementElseClause(
    const Symbol &assume_property_statement) {
  const auto *node =
      MatchNodeEnumOrNull(SymbolCastToNode(assume_property_statement),
                          NodeEnum::kAssumePropertyStatement);
  if (!node) return nullptr;
  const Symbol *else_ptr = node->back().get();
  if (else_ptr == nullptr) return nullptr;
  return MatchNodeEnumOrNull(SymbolCastToNode(*else_ptr),
                             NodeEnum::kElseClause);
}

const SyntaxTreeNode *GetExpectPropertyStatementExpectClause(
    const Symbol &expect_property_statement) {
  return GetSubtreeAsNode(expect_property_statement,
                          NodeEnum::kExpectPropertyStatement, 0,
                          NodeEnum::kExpectPropertyClause);
}

const SyntaxTreeNode *GetExpectPropertyStatementBody(
    const Symbol &expect_clause) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(expect_clause), NodeEnum::kExpectPropertyClause));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kExpectPropertyBody, 0));
}

const SyntaxTreeNode *GetExpectPropertyStatementElseClause(
    const Symbol &expect_property_statement) {
  const auto *node =
      MatchNodeEnumOrNull(SymbolCastToNode(expect_property_statement),
                          NodeEnum::kExpectPropertyStatement);
  if (!node) return nullptr;
  const Symbol *else_ptr = node->back().get();
  if (else_ptr == nullptr) return nullptr;
  return MatchNodeEnumOrNull(SymbolCastToNode(*else_ptr),
                             NodeEnum::kElseClause);
}

const SyntaxTreeNode *GetCoverPropertyStatementBody(
    const Symbol &cover_property) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(cover_property), NodeEnum::kCoverPropertyStatement));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kCoverPropertyBody, 0));
}

const SyntaxTreeNode *GetCoverSequenceStatementBody(
    const Symbol &cover_sequence) {
  const auto *body_node = GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(cover_sequence), NodeEnum::kCoverSequenceStatement));
  if (!body_node) return nullptr;
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(*body_node, NodeEnum::kCoverSequenceBody, 0));
}

const SyntaxTreeNode *GetLoopStatementBody(const Symbol &loop) {
  return GetGenericStatementBody(
      MatchNodeEnumOrNull(SymbolCastToNode(loop), NodeEnum::kForLoopStatement));
}

const SyntaxTreeNode *GetDoWhileStatementBody(const Symbol &do_while) {
  return GetSubtreeAsNode(SymbolCastToNode(do_while),
                          NodeEnum::kDoWhileLoopStatement, 1);
}

const SyntaxTreeNode *GetForeverStatementBody(const Symbol &forever) {
  return GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(forever), NodeEnum::kForeverLoopStatement));
}

const SyntaxTreeNode *GetForeachStatementBody(const Symbol &foreach) {
  return GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(foreach), NodeEnum::kForeachLoopStatement));
}

const SyntaxTreeNode *GetRepeatStatementBody(const Symbol &repeat) {
  return GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(repeat), NodeEnum::kRepeatLoopStatement));
}

const SyntaxTreeNode *GetWhileStatementBody(const Symbol &while_stmt) {
  return GetGenericStatementBody(MatchNodeEnumOrNull(
      SymbolCastToNode(while_stmt), NodeEnum::kWhileLoopStatement));
}

const SyntaxTreeNode *GetProceduralTimingControlStatementBody(
    const Symbol &proc_timing_control) {
  return GetGenericStatementBody(
      MatchNodeEnumOrNull(SymbolCastToNode(proc_timing_control),
                          NodeEnum::kProceduralTimingControlStatement));
}

const SyntaxTreeNode *GetAnyControlStatementBody(const Symbol &statement) {
  switch (NodeEnum(SymbolCastToNode(statement).Tag().tag)) {
    // generate
    case NodeEnum::kGenerateIfClause:
      return GetIfClauseGenerateBody(statement);
    case NodeEnum::kGenerateElseClause:
      return GetElseClauseGenerateBody(statement);
    case NodeEnum::kLoopGenerateConstruct:
      return GetLoopGenerateBody(statement);

    // statements
    case NodeEnum::kIfClause:
      return GetIfClauseStatementBody(statement);
    case NodeEnum::kElseClause:
      return GetElseClauseStatementBody(statement);
    case NodeEnum::kForLoopStatement:
      return GetLoopStatementBody(statement);
    case NodeEnum::kDoWhileLoopStatement:
      return GetDoWhileStatementBody(statement);
    case NodeEnum::kForeverLoopStatement:
      return GetForeverStatementBody(statement);
    case NodeEnum::kForeachLoopStatement:
      return GetForeachStatementBody(statement);
    case NodeEnum::kRepeatLoopStatement:
      return GetRepeatStatementBody(statement);
    case NodeEnum::kWhileLoopStatement:
      return GetWhileStatementBody(statement);
    case NodeEnum::kProceduralTimingControlStatement:
      return GetProceduralTimingControlStatementBody(statement);

    // immediate assertions
    case NodeEnum::kAssertionClause:
      return GetAssertionClauseStatementBody(statement);
    case NodeEnum::kAssumeClause:
      return GetAssumeClauseStatementBody(statement);
    case NodeEnum::kCoverStatement:
      return GetCoverStatementBody(statement);

    case NodeEnum::kWaitStatement:
      return GetWaitStatementBody(statement);

    // concurrent assertions
    case NodeEnum::kAssertPropertyClause:
      return GetAssertPropertyStatementBody(statement);
    case NodeEnum::kAssumePropertyClause:
      return GetAssumePropertyStatementBody(statement);
    case NodeEnum::kExpectPropertyClause:
      return GetExpectPropertyStatementBody(statement);
    case NodeEnum::kCoverPropertyStatement:
      return GetCoverPropertyStatementBody(statement);
    case NodeEnum::kCoverSequenceStatement:
      return GetCoverSequenceStatementBody(statement);

    default:
      return nullptr;
  }
}

const SyntaxTreeNode *GetAnyConditionalIfClause(const Symbol &conditional) {
  // by IfClause, we main the first clause
  switch (NodeEnum(SymbolCastToNode(conditional).Tag().tag)) {
    // generate
    case NodeEnum::kConditionalGenerateConstruct:
      return GetConditionalGenerateIfClause(conditional);

    // statement
    case NodeEnum::kConditionalStatement:
      return GetConditionalStatementIfClause(conditional);

      // immediate assertions
    case NodeEnum::kAssertionStatement:
      return GetAssertionStatementAssertClause(conditional);
    case NodeEnum::kAssumeStatement:
      return GetAssumeStatementAssumeClause(conditional);

      // concurrent assertions
    case NodeEnum::kAssertPropertyStatement:
      return GetAssertPropertyStatementAssertClause(conditional);
    case NodeEnum::kAssumePropertyStatement:
      return GetAssumePropertyStatementAssumeClause(conditional);
    case NodeEnum::kExpectPropertyStatement:
      return GetExpectPropertyStatementExpectClause(conditional);

    default:
      return nullptr;
  }
}

const SyntaxTreeNode *GetAnyConditionalElseClause(const Symbol &conditional) {
  switch (NodeEnum(SymbolCastToNode(conditional).Tag().tag)) {
    // generate
    case NodeEnum::kConditionalGenerateConstruct:
      return GetConditionalGenerateElseClause(conditional);

    // statement
    case NodeEnum::kConditionalStatement:
      return GetConditionalStatementElseClause(conditional);

      // immediate assertions
    case NodeEnum::kAssertionStatement:
      return GetAssertionStatementElseClause(conditional);
    case NodeEnum::kAssumeStatement:
      return GetAssumeStatementElseClause(conditional);

      // concurrent assertions
    case NodeEnum::kAssertPropertyStatement:
      return GetAssertPropertyStatementElseClause(conditional);
    case NodeEnum::kAssumePropertyStatement:
      return GetAssumePropertyStatementElseClause(conditional);
    case NodeEnum::kExpectPropertyStatement:
      return GetExpectPropertyStatementElseClause(conditional);

    default:
      return nullptr;
  }
}

// Returns the data type node from for loop initialization.
const verible::SyntaxTreeNode *GetDataTypeFromForInitialization(
    const verible::Symbol &for_initialization) {
  const auto *data_type = verible::GetSubtreeAsSymbol(
      for_initialization, NodeEnum::kForInitialization, 1);
  if (data_type == nullptr) {
    return nullptr;
  }
  return &verible::SymbolCastToNode(*data_type);
}

// Returns the variable name leaf from for loop initialization.
const verible::SyntaxTreeLeaf *GetVariableNameFromForInitialization(
    const verible::Symbol &for_initialization) {
  const Symbol *child = verible::GetSubtreeAsSymbol(
      for_initialization, NodeEnum::kForInitialization, 2);
  if (child->Kind() == verible::SymbolKind::kLeaf) {
    return &SymbolCastToLeaf(*child);
  }
  const verible::SyntaxTreeNode *lpvalue =
      verible::GetSubtreeAsNode(*child, NodeEnum::kLPValue, 0);
  const verible::SyntaxTreeNode *local_root =
      GetLocalRootFromReference(*lpvalue);
  if (!local_root) return nullptr;
  const verible::Symbol *identifiers = GetIdentifiersFromLocalRoot(*local_root);
  return AutoUnwrapIdentifier(*identifiers);
}

// Returns the rhs expression from for loop initialization.
const verible::SyntaxTreeNode *GetExpressionFromForInitialization(
    const verible::Symbol &for_initialization) {
  return verible::GetSubtreeAsNode(for_initialization,
                                   NodeEnum::kForInitialization, 4,
                                   NodeEnum::kExpression);
}

const verible::SyntaxTreeNode *GetGenerateBlockBegin(
    const verible::Symbol &generate_block) {
  return verible::GetSubtreeAsNode(generate_block, NodeEnum::kGenerateBlock, 0,
                                   NodeEnum::kBegin);
}

const verible::SyntaxTreeNode *GetGenerateBlockEnd(
    const verible::Symbol &generate_block) {
  return verible::GetSubtreeAsNode(generate_block, NodeEnum::kGenerateBlock, 2,
                                   NodeEnum::kEnd);
}

const verible::SyntaxTreeNode *GetProceduralTimingControlFromAlways(
    const verible::SyntaxTreeNode &always_statement) {
  return verible::GetSubtreeAsNode(always_statement, NodeEnum::kAlwaysStatement,
                                   1,
                                   NodeEnum::kProceduralTimingControlStatement);
}

const verible::Symbol *GetEventControlFromProceduralTimingControl(
    const verible::SyntaxTreeNode &proc_timing_ctrl) {
  return verible::GetSubtreeAsNode(proc_timing_ctrl,
                                   NodeEnum::kProceduralTimingControlStatement,
                                   0, NodeEnum::kEventControl);
}

const verible::SyntaxTreeNode *GetNonBlockingAssignmentLhs(
    const verible::SyntaxTreeNode &non_blocking_assignment) {
  return verible::GetSubtreeAsNode(
      non_blocking_assignment, NodeEnum::kNonblockingAssignmentStatement, 0);
}

const verible::SyntaxTreeNode *GetNonBlockingAssignmentRhs(
    const verible::SyntaxTreeNode &non_blocking_assignment) {
  return verible::GetSubtreeAsNode(
      non_blocking_assignment, NodeEnum::kNonblockingAssignmentStatement, 3);
}

const verible::SyntaxTreeNode *GetIfClauseHeader(
    const verible::SyntaxTreeNode &if_clause) {
  return verible::GetSubtreeAsNode(if_clause, NodeEnum::kIfClause, 0);
}

const verible::SyntaxTreeNode *GetIfHeaderExpression(
    const verible::SyntaxTreeNode &if_header) {
  const verible::SyntaxTreeNode *paren_group =
      verible::GetSubtreeAsNode(if_header, NodeEnum::kIfHeader, 2);

  if (!paren_group) return nullptr;

  return verible::GetSubtreeAsNode(*paren_group, NodeEnum::kParenGroup, 1);
}

}  // namespace verilog
