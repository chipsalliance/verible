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

#ifndef VERIBLE_VERILOG_CST_TASKS_H_
#define VERIBLE_VERILOG_CST_TASKS_H_

#include <vector>

#include "common/analysis/syntax_tree_search.h"
#include "common/text/symbol.h"

namespace verilog {

// Find all task declarations, including class method declarations.
std::vector<verible::TreeSearchMatch> FindAllTaskDeclarations(
    const verible::Symbol&);

// Returns the task declaration header
const verible::Symbol* GetTaskHeader(const verible::Symbol&);

// Returns the task lifetime of the node.
const verible::Symbol* GetTaskLifetime(const verible::Symbol&);

// Returns the id of the task declaration.
const verible::Symbol* GetTaskId(const verible::Symbol&);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_TASKS_H_
