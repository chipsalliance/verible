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

#ifndef VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_LINT_RULE_H_
#define VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_LINT_RULE_H_

#include "verible/common/analysis/lint-rule.h"
#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"

namespace verible {

// SyntaxTreeLintRule is a base class for analyzing syntax trees for lint
// violations.  Subclasses of this can be added to a SyntaxTreeLinter and can
// expect to have their HandleLeaf and HandleNode methods called on every
// leaf/node in the tree that the linter is run on.
//
// For usage, see linter.h
//
// See verible/doc/style_lint.md
// for a guide to implementing new syntax tree rules.
//
// Note that context is a stack nodes representing the ancestors of the
// Symbol currented being operated on. Most recent ancestors are at the
// top of the stack/back of vector.
class SyntaxTreeLintRule : public LintRule {
 public:
  ~SyntaxTreeLintRule() override = default;  // not yet final

  virtual void HandleLeaf(const SyntaxTreeLeaf &leaf,
                          const SyntaxTreeContext &context) {}
  virtual void HandleNode(const SyntaxTreeNode &node,
                          const SyntaxTreeContext &context) {}
  virtual void HandleSymbol(const Symbol &node,
                            const SyntaxTreeContext &context) {}
};

}  // namespace verible

#endif  // VERIBLE_COMMON_ANALYSIS_SYNTAX_TREE_LINT_RULE_H_
