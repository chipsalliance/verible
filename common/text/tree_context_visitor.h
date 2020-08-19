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

#ifndef VERIBLE_COMMON_TEXT_TREE_CONTEXT_VISITOR_H_
#define VERIBLE_COMMON_TEXT_TREE_CONTEXT_VISITOR_H_

#include "common/strings/display_utils.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/visitors.h"

namespace verible {

// This visitor traverses a tree and maintains a stack of context
// that points to all ancestors at any given node.
class TreeContextVisitor : public SymbolVisitor {
 public:
  TreeContextVisitor() = default;

 protected:
  void Visit(const SyntaxTreeLeaf& leaf) override {}
  void Visit(const SyntaxTreeNode& node) override;

  const SyntaxTreeContext& Context() const { return current_context_; }

  // Keeps track of ancestors as the visitor traverses tree.
  SyntaxTreeContext current_context_;
};

// Type that is used to keep track of positions descended from a root
// node to reach a particular node.
// Path types should be lexicographically comparable.
// This is very similar in spirit to VectorTree<>::Path(), but
// needs to be tracked in a stack-like manner during visitation
// because SyntaxTreeNode and Leaf do not maintain upward pointers
// to their parent nodes.
// e.g. use the LexicographicalLess comparator in common/util/algorithm.h.
// TODO(fangism): consider replacing with hybrid "small" vector to
// minimize heap allocations, because these are expected to be small.
using SyntaxTreePath = std::vector<size_t>;

// This visitor traverses a tree and maintains a stack of offsets
// that represents the positional path taken from the root to
// reach each node.
// This is useful for applications where the shape and positions of nodes
// within the tree are meaningful.
class TreeContextPathVisitor : public TreeContextVisitor {
 public:
  TreeContextPathVisitor() = default;

 protected:
  void Visit(const SyntaxTreeNode& node) override;

  const SyntaxTreePath& Path() const { return current_path_; }

  // Keeps track of path of descent from root node.
  SyntaxTreePath current_path_;
};

// Computes the path of the next sibling by incrementing the last element
// of the path.  Resulting path may not necessarily correspond to a valid
// element.  'path' must not be empty.
SyntaxTreePath NextSiblingPath(const SyntaxTreePath& path);

// Format SyntaxTreePaths using: stream << TreePathFormatter(path);
// It is necessary to define this way because SyntaxTreePath is a typedef to a
// generic container type.
// TODO(fangism): Use auto return type once C++17 become the minimum standard.
SequenceStreamFormatter<SyntaxTreePath> TreePathFormatter(
    const SyntaxTreePath& path);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_CONTEXT_VISITOR_H_
