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
#include "verilog/CST/declaration.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // IWYU pragma: keep

namespace verilog {

using verible::Symbol;
using verible::SymbolCastToNode;
using verible::SyntaxTreeNode;

std::vector<verible::TreeSearchMatch> FindAllForLoopsInitializations(
    const verible::Symbol& root) {
  return SearchSyntaxTree(root, NodekForInitialization());
}

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

const SyntaxTreeNode& GetAssertionStatementAssertClause(
    const Symbol& assertion_statement) {
  return GetSubtreeAsNode(assertion_statement, NodeEnum::kAssertionStatement, 0,
                          NodeEnum::kAssertionClause);
}

const SyntaxTreeNode* GetAssertionClauseStatementBody(
    const Symbol& assertion_clause) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(assertion_clause), NodeEnum::kAssertionClause));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kAssertionBody, 0));
}

const SyntaxTreeNode* GetAssertionStatementElseClause(
    const Symbol& assertion_statement) {
  const auto& node = CheckNodeEnum(SymbolCastToNode(assertion_statement),
                                   NodeEnum::kAssertionStatement);
  const Symbol* else_ptr = node.children().back().get();
  if (else_ptr == nullptr) return nullptr;
  return &CheckNodeEnum(SymbolCastToNode(*else_ptr), NodeEnum::kElseClause);
}

const SyntaxTreeNode& GetAssumeStatementAssumeClause(
    const Symbol& assume_statement) {
  return GetSubtreeAsNode(assume_statement, NodeEnum::kAssumeStatement, 0,
                          NodeEnum::kAssumeClause);
}

const SyntaxTreeNode* GetAssumeClauseStatementBody(
    const Symbol& assume_clause) {
  const auto& body_node = GetGenericStatementBody(
      CheckNodeEnum(SymbolCastToNode(assume_clause), NodeEnum::kAssumeClause));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kAssumeBody, 0));
}

const SyntaxTreeNode* GetAssumeStatementElseClause(
    const Symbol& assume_statement) {
  const auto& node = CheckNodeEnum(SymbolCastToNode(assume_statement),
                                   NodeEnum::kAssumeStatement);
  const Symbol* else_ptr = node.children().back().get();
  if (else_ptr == nullptr) return nullptr;
  return &CheckNodeEnum(SymbolCastToNode(*else_ptr), NodeEnum::kElseClause);
}

const SyntaxTreeNode* GetCoverStatementBody(const Symbol& cover_statement) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(cover_statement), NodeEnum::kCoverStatement));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kCoverBody, 0));
}

const SyntaxTreeNode* GetWaitStatementBody(const Symbol& wait_statement) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(wait_statement), NodeEnum::kWaitStatement));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kWaitBody, 0));
}

const SyntaxTreeNode& GetAssertPropertyStatementAssertClause(
    const Symbol& assert_property_statement) {
  return GetSubtreeAsNode(assert_property_statement,
                          NodeEnum::kAssertPropertyStatement, 0,
                          NodeEnum::kAssertPropertyClause);
}

const SyntaxTreeNode* GetAssertPropertyStatementBody(
    const Symbol& assert_clause) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(assert_clause), NodeEnum::kAssertPropertyClause));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kAssertPropertyBody, 0));
}

const SyntaxTreeNode* GetAssertPropertyStatementElseClause(
    const Symbol& assert_property_statement) {
  const auto& node = CheckNodeEnum(SymbolCastToNode(assert_property_statement),
                                   NodeEnum::kAssertPropertyStatement);
  const Symbol* else_ptr = node.children().back().get();
  if (else_ptr == nullptr) return nullptr;
  return &CheckNodeEnum(SymbolCastToNode(*else_ptr), NodeEnum::kElseClause);
}

const SyntaxTreeNode& GetAssumePropertyStatementAssumeClause(
    const Symbol& assume_property_statement) {
  return GetSubtreeAsNode(assume_property_statement,
                          NodeEnum::kAssumePropertyStatement, 0,
                          NodeEnum::kAssumePropertyClause);
}

const SyntaxTreeNode* GetAssumePropertyStatementBody(
    const Symbol& assume_clause) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(assume_clause), NodeEnum::kAssumePropertyClause));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kAssumePropertyBody, 0));
}

const SyntaxTreeNode* GetAssumePropertyStatementElseClause(
    const Symbol& assume_property_statement) {
  const auto& node = CheckNodeEnum(SymbolCastToNode(assume_property_statement),
                                   NodeEnum::kAssumePropertyStatement);
  const Symbol* else_ptr = node.children().back().get();
  if (else_ptr == nullptr) return nullptr;
  return &CheckNodeEnum(SymbolCastToNode(*else_ptr), NodeEnum::kElseClause);
}

const SyntaxTreeNode& GetExpectPropertyStatementExpectClause(
    const Symbol& expect_property_statement) {
  return GetSubtreeAsNode(expect_property_statement,
                          NodeEnum::kExpectPropertyStatement, 0,
                          NodeEnum::kExpectPropertyClause);
}

const SyntaxTreeNode* GetExpectPropertyStatementBody(
    const Symbol& expect_clause) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(expect_clause), NodeEnum::kExpectPropertyClause));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kExpectPropertyBody, 0));
}

const SyntaxTreeNode* GetExpectPropertyStatementElseClause(
    const Symbol& expect_property_statement) {
  const auto& node = CheckNodeEnum(SymbolCastToNode(expect_property_statement),
                                   NodeEnum::kExpectPropertyStatement);
  const Symbol* else_ptr = node.children().back().get();
  if (else_ptr == nullptr) return nullptr;
  return &CheckNodeEnum(SymbolCastToNode(*else_ptr), NodeEnum::kElseClause);
}

const SyntaxTreeNode* GetCoverPropertyStatementBody(
    const Symbol& cover_property) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(cover_property), NodeEnum::kCoverPropertyStatement));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kCoverPropertyBody, 0));
}

const SyntaxTreeNode* GetCoverSequenceStatementBody(
    const Symbol& cover_sequence) {
  const auto& body_node = GetGenericStatementBody(CheckNodeEnum(
      SymbolCastToNode(cover_sequence), NodeEnum::kCoverSequenceStatement));
  return verible::CheckOptionalSymbolAsNode(
      GetSubtreeAsSymbol(body_node, NodeEnum::kCoverSequenceBody, 0));
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

const SyntaxTreeNode* GetAnyConditionalIfClause(const Symbol& conditional) {
  // by IfClause, we main the first clause
  switch (NodeEnum(SymbolCastToNode(conditional).Tag().tag)) {
    // generate
    case NodeEnum::kConditionalGenerateConstruct:
      return &GetConditionalGenerateIfClause(conditional);

    // statement
    case NodeEnum::kConditionalStatement:
      return &GetConditionalStatementIfClause(conditional);

      // immediate assertions
    case NodeEnum::kAssertionStatement:
      return &GetAssertionStatementAssertClause(conditional);
    case NodeEnum::kAssumeStatement:
      return &GetAssumeStatementAssumeClause(conditional);

      // concurrent assertions
    case NodeEnum::kAssertPropertyStatement:
      return &GetAssertPropertyStatementAssertClause(conditional);
    case NodeEnum::kAssumePropertyStatement:
      return &GetAssumePropertyStatementAssumeClause(conditional);
    case NodeEnum::kExpectPropertyStatement:
      return &GetExpectPropertyStatementExpectClause(conditional);

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
const verible::SyntaxTreeNode* GetDataTypeFromForInitialization(
    const verible::Symbol& for_initialization) {
  const auto* data_type = verible::GetSubtreeAsSymbol(
      for_initialization, NodeEnum::kForInitialization, 1);
  if (data_type == nullptr) {
    return nullptr;
  }
  return &verible::SymbolCastToNode(*data_type);
}

// Returns the variable name leaf from for loop initialization.
const verible::SyntaxTreeLeaf& GetVariableNameFromForInitialization(
    const verible::Symbol& for_initialization) {
  const Symbol* child = verible::GetSubtreeAsSymbol(
      for_initialization, NodeEnum::kForInitialization, 2);
  if (child->Kind() == verible::SymbolKind::kLeaf) {
    return SymbolCastToLeaf(*child);
  }
  return *AutoUnwrapIdentifier(GetUnqualifiedIdFromReferenceCallBase(
      verible::GetSubtreeAsNode(*child, NodeEnum::kLPValue, 0)));
}

// Returns the rhs expression from for loop initialization.
const verible::SyntaxTreeNode& GetExpressionFromForInitialization(
    const verible::Symbol& for_initialization) {
  return verible::GetSubtreeAsNode(for_initialization,
                                   NodeEnum::kForInitialization, 4,
                                   NodeEnum::kExpression);
}

}  // namespace verilog
