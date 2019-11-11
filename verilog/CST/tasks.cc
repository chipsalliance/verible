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

#include "verilog/CST/tasks.h"

#include <vector>

#include "common/analysis/matcher/matcher.h"
#include "common/analysis/matcher/matcher_builders.h"
#include "common/analysis/syntax_tree_search.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/util/casts.h"
#include "common/util/logging.h"
#include "verilog/CST/verilog_matchers.h"  // pragma IWYU: keep

namespace verilog {

using verible::down_cast;

std::vector<verible::TreeSearchMatch> FindAllTaskDeclarations(
    const verible::Symbol& root) {
  return verible::SearchSyntaxTree(root, NodekTaskDeclaration());
}

const verible::Symbol* GetTaskHeader(const verible::Symbol& symbol) {
  // Assert that symbol is a task declaration.
  auto t = symbol.Tag();
  CHECK_EQ(t.kind, verible::SymbolKind::kNode);
  CHECK_EQ(NodeEnum(t.tag), NodeEnum::kTaskDeclaration);

  const auto& node = down_cast<const verible::SyntaxTreeNode&>(symbol);
  // This is the sub-child of a TaskDeclaration node where header is
  // expected
  static const int kTaskHeaderIdx = 0;
  return node[kTaskHeaderIdx].get();
}

const verible::Symbol* GetTaskLifetime(const verible::Symbol& symbol) {
  const auto* header = GetTaskHeader(symbol);
  const auto& node = down_cast<const verible::SyntaxTreeNode&>(*header);
  // This is the sub-child of a TaskDeclaration node where lifetime is
  // expected
  static const int kTaskLifetimeIdx = 2;
  return node[kTaskLifetimeIdx].get();
}

const verible::Symbol* GetTaskId(const verible::Symbol& symbol) {
  const auto* header = GetTaskHeader(symbol);
  const auto& node = down_cast<const verible::SyntaxTreeNode&>(*header);
  // This is the sub-child of a TaskDeclaration node where task id is
  // expected.
  static const int kTaskIdIdx = 3;
  return node[kTaskIdIdx].get();
}

}  // namespace verilog
