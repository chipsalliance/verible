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

#ifndef VERIBLE_VERILOG_CST_STATEMENT_H_
#define VERIBLE_VERILOG_CST_STATEMENT_H_

#include <vector>

#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllConditionalStatements(
    const verible::Symbol& root);

// Generate flow control constructs
//
// TODO(fangism): consider moving the *GenerateBody functions to generate.{h,cc}

// Returns the generate-item body of a generate-if construct.
const verible::SyntaxTreeNode& GetIfClauseGenerateBody(
    const verible::Symbol& if_clause);

// Returns the generate-item body of a generate-else construct.
const verible::SyntaxTreeNode& GetElseClauseGenerateBody(
    const verible::Symbol& else_clause);

// Returns the generate-item body of a generate-for-loop construct.
const verible::SyntaxTreeNode& GetLoopGenerateBody(const verible::Symbol& loop);

// Returns the if-clause of a generate-if construct.
const verible::SyntaxTreeNode& GetConditionalGenerateIfClause(
    const verible::Symbol& conditional);

// Returns the else-clause of a generate-if construct, or nullptr.
const verible::SyntaxTreeNode* GetConditionalGenerateElseClause(
    const verible::Symbol& conditional);

// Statement flow control constructs

// For if-conditional statement blocks, return the construct's
// statement body (which should be some form of statement list).
const verible::SyntaxTreeNode& GetIfClauseStatementBody(
    const verible::Symbol& if_clause);

// For else-clause statement blocks, return the construct's
// statement body (which should be some form of statement list).
const verible::SyntaxTreeNode& GetElseClauseStatementBody(
    const verible::Symbol& else_clause);

// Returns the if-clause of a conditional statement construct.
const verible::SyntaxTreeNode& GetConditionalStatementIfClause(
    const verible::Symbol& conditional);

// Returns the else-clause of a conditional statement construct, or nullptr.
const verible::SyntaxTreeNode* GetConditionalStatementElseClause(
    const verible::Symbol& conditional);

// For loop statement blocks, return the looped statement body.
const verible::SyntaxTreeNode& GetLoopStatementBody(
    const verible::Symbol& loop);

// For do-while statement blocks, return the looped statement body.
const verible::SyntaxTreeNode& GetDoWhileStatementBody(
    const verible::Symbol& do_while);

// Return the statement body of forever blocks.
const verible::SyntaxTreeNode& GetForeverStatementBody(
    const verible::Symbol& forever);

// Return the statement body of foreach blocks.
const verible::SyntaxTreeNode& GetForeachStatementBody(
    const verible::Symbol& foreach);

// Return the statement body of repeat blocks.
const verible::SyntaxTreeNode& GetRepeatStatementBody(
    const verible::Symbol& repeat);

// Return the statement body of while blocks.
const verible::SyntaxTreeNode& GetWhileStatementBody(
    const verible::Symbol& while_stmt);

// TODO(fangism): case-items

// Return the statement body of procedural timing constructs.
const verible::SyntaxTreeNode& GetProceduralTimingControlStatementBody(
    const verible::Symbol& proc_timing_control);

// Combines all of the above Get*StatementBody.
// Also works for control flow generate constructs.
const verible::SyntaxTreeNode* GetAnyControlStatementBody(
    const verible::Symbol& statement);

// Returns the if-clause of a conditional generate/statement.
const verible::SyntaxTreeNode* GetAnyConditionalIfClause(
    const verible::Symbol& conditional);

// Returns the else-clause of a conditional generate/statement, or nullptr if it
// doesn't exist.
const verible::SyntaxTreeNode* GetAnyConditionalElseClause(
    const verible::Symbol& conditional);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_STATEMENT_H_
