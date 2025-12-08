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

// Implementation of SymbolVisitor that traverses a tree, keeps track of
// context (a list of ancestors), and apples each LintRule that it has to
// each Leaf/Node.

#ifndef VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_LINTER_H_
#define VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_LINTER_H_

#include <memory>
#include <utility>
#include <vector>

#include "verible/common/analysis/lint-rule-status.h"
#include "verible/common/analysis/syntax-tree-lint-rule.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-context-visitor.h"

namespace verible {

// SyntaxTreeLinter runs a set of analyses on a syntax tree.
// The collection of SyntaxTreeLintRule's form the set of analyses.
//
// Usage:
//  Linter linter;
//  linter.addRule(LintRulePtr(new DerivedClassOfLintRule);
//  linter.addRule(LintRulePtr(new AnotherDerivedClassOfLintRule);
//
//  SymbolPtr tree = ...
//  linter.Lint(tree)
//  std::vector<LintRuleStatus> status = linter.ReportStatus();
//
// Note that the tree is traversed in a preorder traversal.
//
class SyntaxTreeLinter : public TreeContextVisitor {
 public:
  SyntaxTreeLinter() = default;

  void Visit(const SyntaxTreeLeaf &leaf) final;
  void Visit(const SyntaxTreeNode &node) final;

  // Transfers ownership of rule into Linter
  void AddRule(std::unique_ptr<SyntaxTreeLintRule> rule) {
    rules_.emplace_back(std::move(rule));
  }

  // Aggregates results of each held LintRule
  std::vector<LintRuleStatus> ReportStatus() const;

  // Performs lint analysis on root
  void Lint(const Symbol &root);

 private:
  // List of rules that the linter is using. Rules are responsible for tracking
  // their own internal state.
  std::vector<std::unique_ptr<SyntaxTreeLintRule>> rules_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_LINTER_H_
