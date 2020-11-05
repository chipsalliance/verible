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

#include "verilog/CST/tasks.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

std::vector<verible::TreeSearchMatch> FindAllTaskDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekTaskDeclaration());
}

std::vector<verible::TreeSearchMatch> FindAllTaskPrototypes(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekTaskPrototype());
}

std::vector<verible::TreeSearchMatch> FindAllTaskHeaders(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekTaskHeader());
}

const verible::SyntaxTreeNode& GetTaskHeader(const verible::Symbol& task_decl) {
  return verible::GetSubtreeAsNode(task_decl, NodeEnum::kTaskDeclaration, 0,
                                   NodeEnum::kTaskHeader);
}

const verible::SyntaxTreeNode& GetTaskPrototypeHeader(
    const verible::Symbol& task_proto) {
  return verible::GetSubtreeAsNode(task_proto, NodeEnum::kTaskPrototype, 0,
                                   NodeEnum::kTaskHeader);
}

const verible::Symbol* GetTaskHeaderLifetime(
    const verible::Symbol& task_header) {
  return verible::GetSubtreeAsSymbol(task_header, NodeEnum::kTaskHeader, 2);
}

const verible::Symbol* GetTaskHeaderId(const verible::Symbol& task_header) {
  return verible::GetSubtreeAsSymbol(task_header, NodeEnum::kTaskHeader, 3);
}

const verible::Symbol* GetTaskLifetime(const verible::Symbol& task_decl) {
  const auto& header = GetTaskHeader(task_decl);
  return GetTaskHeaderLifetime(header);
}

const verible::Symbol* GetTaskId(const verible::Symbol& task_decl) {
  const auto& header = GetTaskHeader(task_decl);
  return GetTaskHeaderId(header);
}

const verible::SyntaxTreeLeaf* GetTaskName(const verible::Symbol& task_decl) {
  const auto* function_id = GetTaskId(task_decl);
  return GetIdentifier(*function_id);
}

const verible::SyntaxTreeNode& GetTaskStatementList(
    const verible::Symbol& task_decl) {
  return verible::GetSubtreeAsNode(task_decl, NodeEnum::kTaskDeclaration, 1,
                                   NodeEnum::kStatementList);
}

}  // namespace verilog
