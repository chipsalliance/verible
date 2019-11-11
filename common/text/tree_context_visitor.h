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

#ifndef VERIBLE_COMMON_TEXT_TREE_CONTEXT_VISITOR_H_
#define VERIBLE_COMMON_TEXT_TREE_CONTEXT_VISITOR_H_

#include "common/text/syntax_tree_context.h"
#include "common/text/visitors.h"

namespace verible {

// This visitor traverses a tree and maintains a stack of context
// that points to all ancestors at any given node.
class TreeContextVisitor : public SymbolVisitor {
 public:
  TreeContextVisitor() = default;

 protected:
  void Visit(const SyntaxTreeNode& node) override;

  const SyntaxTreeContext& Context() const { return current_context_; }

 private:
  // Keeps track of ancestors as the visitor traverses tree.
  SyntaxTreeContext current_context_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_CONTEXT_VISITOR_H_
