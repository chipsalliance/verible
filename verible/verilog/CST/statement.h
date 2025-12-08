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

#ifndef VERIBLE_VERILOG_CST_STATEMENT_H_
#define VERIBLE_VERILOG_CST_STATEMENT_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/visitors.h"

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllConditionalStatements(
    const verible::Symbol &root);

std::vector<verible::TreeSearchMatch> FindAllForLoopsInitializations(
    const verible::Symbol &root);

std::vector<verible::TreeSearchMatch> FindAllGenerateBlocks(
    const verible::Symbol &root);

// Extract every nonblocking assignment starting from root
std::vector<verible::TreeSearchMatch> FindAllNonBlockingAssignments(
    const verible::Symbol &root);

// Generate flow control constructs
//
// TODO(fangism): consider moving the *GenerateBody functions to generate.{h,cc}

// Returns the generate-item body of a generate-if construct.
const verible::SyntaxTreeNode *GetIfClauseGenerateBody(
    const verible::Symbol &if_clause);

// Returns the generate-item body of a generate-else construct.
const verible::SyntaxTreeNode *GetElseClauseGenerateBody(
    const verible::Symbol &else_clause);

// Returns the generate-item body of a generate-for-loop construct.
const verible::SyntaxTreeNode *GetLoopGenerateBody(const verible::Symbol &loop);

// Returns the if-clause of a generate-if construct.
const verible::SyntaxTreeNode *GetConditionalGenerateIfClause(
    const verible::Symbol &conditional);

// Returns the else-clause of a generate-if construct, or nullptr.
const verible::SyntaxTreeNode *GetConditionalGenerateElseClause(
    const verible::Symbol &conditional);

// Statement flow control constructs

// For if-conditional statement blocks, return the construct's
// statement body (which should be some form of statement list).
const verible::SyntaxTreeNode *GetIfClauseStatementBody(
    const verible::Symbol &if_clause);

// For else-clause statement blocks, return the construct's
// statement body (which should be some form of statement list).
const verible::SyntaxTreeNode *GetElseClauseStatementBody(
    const verible::Symbol &else_clause);

// Returns the if-clause of a conditional statement construct.
const verible::SyntaxTreeNode *GetConditionalStatementIfClause(
    const verible::Symbol &conditional);

// Returns the else-clause of a conditional statement construct, or nullptr.
const verible::SyntaxTreeNode *GetConditionalStatementElseClause(
    const verible::Symbol &conditional);

// Immediate assertion statements

// Returns the assert-clause of an assertion statement, or nullptr.
const verible::SyntaxTreeNode *GetAssertionStatementAssertClause(
    const verible::Symbol &assertion_statement);

// Returns the else-clause of an assertion statement, or nullptr.
const verible::SyntaxTreeNode *GetAssertionStatementElseClause(
    const verible::Symbol &assertion_statement);

// Returns the assume-clause of an assume statement, or nullptr.
const verible::SyntaxTreeNode *GetAssumeStatementAssumeClause(
    const verible::Symbol &assume_statement);

// Returns the else-clause of an assume statement, or nullptr.
const verible::SyntaxTreeNode *GetAssumeStatementElseClause(
    const verible::Symbol &assume_statement);

// Returns the statement body of a cover statement, or nullptr.
const verible::SyntaxTreeNode *GetCoverStatementBody(
    const verible::Symbol &cover_statement);

// Returns the statement body of a wait statement, or nullptr.
const verible::SyntaxTreeNode *GetWaitStatementBody(
    const verible::Symbol &wait_statement);

// Concurrent assertion statements

// Returns the assert-clause of an assert property statement, or nullptr.
const verible::SyntaxTreeNode *GetAssertPropertyStatementAssertClause(
    const verible::Symbol &assert_property_statement);

// Returns the else-clause of an assert property statement, or nullptr.
const verible::SyntaxTreeNode *GetAssertPropertyStatementElseClause(
    const verible::Symbol &assert_property_statement);

// Returns the assume-clause of an assume property statement, or nullptr.
const verible::SyntaxTreeNode *GetAssumePropertyStatementAssumeClause(
    const verible::Symbol &assume_property_statement);

// Returns the else-clause of an assume property statement, or nullptr.
const verible::SyntaxTreeNode *GetAssumePropertyStatementElseClause(
    const verible::Symbol &assume_property_statement);

// Returns the expect-clause of an expect property statement, or nullptr.
const verible::SyntaxTreeNode *GetExpectPropertyStatementExpectClause(
    const verible::Symbol &expect_property_statement);

// Returns the else-clause of an expect property statement, or nullptr.
const verible::SyntaxTreeNode *GetExpectPropertyStatementElseClause(
    const verible::Symbol &expect_property_statement);

// Loop-like statements

// For loop statement blocks, return the looped statement body.
const verible::SyntaxTreeNode *GetLoopStatementBody(
    const verible::Symbol &loop);

// For do-while statement blocks, return the looped statement body.
const verible::SyntaxTreeNode *GetDoWhileStatementBody(
    const verible::Symbol &do_while);

// Return the statement body of forever blocks.
const verible::SyntaxTreeNode *GetForeverStatementBody(
    const verible::Symbol &forever);

// Return the statement body of foreach blocks.
const verible::SyntaxTreeNode *GetForeachStatementBody(
    const verible::Symbol &foreach);

// Return the statement body of repeat blocks.
const verible::SyntaxTreeNode *GetRepeatStatementBody(
    const verible::Symbol &repeat);

// Return the statement body of while blocks.
const verible::SyntaxTreeNode *GetWhileStatementBody(
    const verible::Symbol &while_stmt);

// TODO(fangism): case-items

// Return the statement body of procedural timing constructs.
const verible::SyntaxTreeNode *GetProceduralTimingControlStatementBody(
    const verible::Symbol &proc_timing_control);

// Combines all of the above Get*StatementBody.
// Also works for control flow generate constructs.
const verible::SyntaxTreeNode *GetAnyControlStatementBody(
    const verible::Symbol &statement);

// Returns the if-clause of a conditional generate/statement.
const verible::SyntaxTreeNode *GetAnyConditionalIfClause(
    const verible::Symbol &conditional);

// Returns the else-clause of a conditional generate/statement, or nullptr if it
// doesn't exist.
const verible::SyntaxTreeNode *GetAnyConditionalElseClause(
    const verible::Symbol &conditional);

// Returns the data type node from for loop initialization.
const verible::SyntaxTreeNode *GetDataTypeFromForInitialization(
    const verible::Symbol &);

// Returns the variable name leaf from for loop initialization.
const verible::SyntaxTreeLeaf *GetVariableNameFromForInitialization(
    const verible::Symbol &);

// Returns the rhs expression from for loop initialization.
const verible::SyntaxTreeNode *GetExpressionFromForInitialization(
    const verible::Symbol &);

// Returns the 'begin' node of a generate block.
const verible::SyntaxTreeNode *GetGenerateBlockBegin(
    const verible::Symbol &generate_block);

// Returns the 'end' node of a generate block.
const verible::SyntaxTreeNode *GetGenerateBlockEnd(
    const verible::Symbol &generate_block);

// Returns the procedural timing control statement of an always statement node
const verible::SyntaxTreeNode *GetProceduralTimingControlFromAlways(
    const verible::SyntaxTreeNode &always_statement);

// Returns the event control symbol of a procedural timing control statement
const verible::Symbol *GetEventControlFromProceduralTimingControl(
    const verible::SyntaxTreeNode &proc_timing_ctrl);

// Return the left hand side (lhs) from a nonblocking assignment
// Example: get 'x' from 'x <= y'
const verible::SyntaxTreeNode *GetNonBlockingAssignmentLhs(
    const verible::SyntaxTreeNode &non_blocking_assignment);

// Return the right hand side (rhs) from a nonblocking assignment
// Example: get 'y' from 'x <= y'
const verible::SyntaxTreeNode *GetNonBlockingAssignmentRhs(
    const verible::SyntaxTreeNode &non_blocking_assignment);

const verible::SyntaxTreeNode *GetIfClauseHeader(
    const verible::SyntaxTreeNode &if_clause);

const verible::SyntaxTreeNode *GetIfHeaderExpression(
    const verible::SyntaxTreeNode &if_header);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_STATEMENT_H_
