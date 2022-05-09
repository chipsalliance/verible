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

#include "common/analysis/syntax_tree_linter.h"

#include <memory>
#include <vector>

#include "common/analysis/lint_rule_status.h"
#include "common/analysis/syntax_tree_lint_rule.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/util/logging.h"

namespace verible {

void SyntaxTreeLinter::Lint(const Symbol& root) {
  VLOG(1) << "SyntaxTreeLinter analyzing syntax tree with " << rules_.size()
          << " rules.";
  root.Accept(this);
}

std::vector<LintRuleStatus> SyntaxTreeLinter::ReportStatus() const {
  std::vector<LintRuleStatus> status;
  status.reserve(rules_.size());
  for (const auto& rule : rules_) {
    status.push_back(ABSL_DIE_IF_NULL(rule)->Report());
  }
  return status;
}

// Visits a leaf. Every held rule handles that leaf.
void SyntaxTreeLinter::Visit(const SyntaxTreeLeaf& leaf) {
  for (const auto& rule : rules_) {
    // Have rule handle the leaf as both a leaf and a symbol.
    ABSL_DIE_IF_NULL(rule)->HandleLeaf(leaf, Context());
    rule->HandleSymbol(leaf, Context());
  }
}

// Visits a node. First, linter has every rule handle that node
// Second, linter recurses on every non-null child of that node in order
// to visit the entire tree
void SyntaxTreeLinter::Visit(const SyntaxTreeNode& node) {
  for (const auto& rule : rules_) {
    // Have rule handle the node as both a node and a symbol.
    ABSL_DIE_IF_NULL(rule)->HandleNode(node, Context());
    rule->HandleSymbol(node, Context());
  }

  // Visit subtree children.
  TreeContextVisitor::Visit(node);
}

}  // namespace verible
