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

#ifndef VERIBLE_VERILOG_CST_TASKS_H_
#define VERIBLE_VERILOG_CST_TASKS_H_

#include <vector>

#include "verible/common/analysis/syntax-tree-search.h"
#include "verible/common/text/concrete-syntax-leaf.h"  // IWYU pragma: export
#include "verible/common/text/concrete-syntax-tree.h"  // IWYU pragma: export
#include "verible/common/text/symbol.h"                // IWYU pragma: export

namespace verilog {

// Find all task declarations, including class method declarations.
std::vector<verible::TreeSearchMatch> FindAllTaskDeclarations(
    const verible::Symbol &);

// Find all task headers, which are common to declarations and prototypes.
std::vector<verible::TreeSearchMatch> FindAllTaskHeaders(
    const verible::Symbol &);

// Find all task prototypes, including class method prototypes.
std::vector<verible::TreeSearchMatch> FindAllTaskPrototypes(
    const verible::Symbol &);

// Returns the task declaration header
const verible::SyntaxTreeNode *GetTaskHeader(const verible::Symbol &task_decl);

// Returns the task prototype header
const verible::SyntaxTreeNode *GetTaskPrototypeHeader(
    const verible::Symbol &task_proto);

// task header accessors

// Returns the task header's lifetime.
const verible::Symbol *GetTaskHeaderLifetime(
    const verible::Symbol &task_header);

// Returns the id of the task declaration.
const verible::Symbol *GetTaskHeaderId(const verible::Symbol &task_header);

// task declaration accessors

// Returns the task lifetime of the node.
const verible::Symbol *GetTaskLifetime(const verible::Symbol &task_decl);

// Returns the id of the task declaration.
const verible::Symbol *GetTaskId(const verible::Symbol &task_decl);

// Returns leaf node for task name.
// e.g. task my_task(); return leaf node for "my_task".
const verible::SyntaxTreeLeaf *GetTaskName(const verible::Symbol &task_decl);

// Returns the task declaration body.
const verible::SyntaxTreeNode *GetTaskStatementList(
    const verible::Symbol &task_decl);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_TASKS_H_
