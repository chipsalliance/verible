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

#ifndef VERIBLE_COMMON_TEXT_TREE_PATH_VISITOR_H_
#define VERIBLE_COMMON_TEXT_TREE_PATH_VISITOR_H_

#include <cstddef>
#include <vector>

#include "common/text/visitors.h"

namespace verible {

// Type that is used to keep track of positions descended from a root
// node to reach a particular node.
// Path types should be lexicographically comparable.
// This is very similar in spirit to VectorTree<>::Path(), but
// needs to be tracked in a stack-like manner during visitation
// because SyntaxTreeNode and Leaf do not maintain upward pointers
// to their parent nodes.
// TODO(fangism): consider replacing with hybrid "small" vector to
// minimize heap allocations, because these are expected to be small.
using SyntaxTreePath = std::vector<size_t>;

// This visitor traverses a tree and maintains a stack of offsets
// that represents the positional path taken from the root to
// reach each node.
// This is useful for applications where the shape and positions of nodes
// within the tree are more meaningful than the contents.
class TreePathVisitor : public SymbolVisitor {
 public:
  TreePathVisitor() = default;

 protected:
  void Visit(const SyntaxTreeNode& node) override;

  const SyntaxTreePath& Path() const { return current_path_; }

  // Keeps track of path of descent from root node.
  SyntaxTreePath current_path_;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_PATH_VISITOR_H_
