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

#include "common/text/concrete_syntax_tree.h"

#include <memory>
#include <utility>
#include <vector>

#include "common/text/symbol.h"
#include "common/text/tree_compare.h"
#include "common/text/visitors.h"
#include "common/util/logging.h"

namespace verible {

// Checks if this is equal to SymbolPtr node under compare_token function
bool SyntaxTreeNode::equals(const Symbol* symbol,
                            const TokenComparator& compare_tokens) const {
  if (symbol->Kind() == SymbolKind::kNode) {
    const SyntaxTreeNode* node = down_cast<const SyntaxTreeNode*>(symbol);
    return equals(node, compare_tokens);
  } else {
    return false;
  }
}

// Checks if this is equal to a SyntaxTreeNode under compare_token function
// Returns true if both trees have same number of children and children are
// equal trees.
bool SyntaxTreeNode::equals(const SyntaxTreeNode* node,
                            const TokenComparator& compare_tokens) const {
  if (children_.size() != node->children().size()) {
    return false;
  } else {
    int size = children_.size();
    for (int i = 0; i < size; i++) {
      if (!EqualTrees(children_[i].get(), node->children()[i].get(),
                      compare_tokens))
        return false;
    }
  }
  return true;
}

SymbolPtr& SyntaxTreeNode::operator[](const size_t i) {
  CHECK_LT(i, children_.size());
  return children_[i];
}

const SymbolPtr& SyntaxTreeNode::operator[](const size_t i) const {
  CHECK_LT(i, children_.size());
  return children_[i];
}

// visits self, then forwards visitor to every child
void SyntaxTreeNode::Accept(TreeVisitorRecursive* visitor) const {
  visitor->Visit(*this);
  for (auto& child : children_) {
    if (child != nullptr) child->Accept(visitor);
  }
}

void SyntaxTreeNode::Accept(MutableTreeVisitorRecursive* visitor,
                            SymbolPtr* this_owned) {
  CHECK_EQ(ABSL_DIE_IF_NULL(this_owned)->get(), this);
  visitor->Visit(*this, this_owned);
  for (auto& child : children_) {
    if (child != nullptr) child->Accept(visitor, &child);
  }
}

void SyntaxTreeNode::Accept(SymbolVisitor* visitor) const {
  visitor->Visit(*this);
}

void SetChild_(const SymbolPtr& parent, int child_index, SymbolPtr new_child) {
  CHECK_EQ(ABSL_DIE_IF_NULL(parent)->Kind(), SymbolKind::kNode);

  auto* parent_node = down_cast<SyntaxTreeNode*>(parent.get());
  CHECK_LT(child_index, static_cast<int>(parent_node->children().size()));
  CHECK(parent_node->children()[child_index] == nullptr);

  parent_node->mutable_children()[child_index] = std::move(new_child);
}

}  // namespace verible
