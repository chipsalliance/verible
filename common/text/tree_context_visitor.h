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

#include <vector>

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
  void Visit(const SyntaxTreeLeaf &leaf) override {}  // not yet final
  void Visit(const SyntaxTreeNode &node) override;    // not yet final

  const SyntaxTreeContext &Context() const { return current_context_; }

  // Keeps track of ancestors as the visitor traverses tree.
  SyntaxTreeContext current_context_;
};

class SyntaxTreePath;

// Recursively compares two SyntaxTreePaths element by element. Out of bound
// elements are assumed to have values that are less than 0 but greater than any
// negative number. First non-matching elements pair determines the result:
// -1 if a < b, 1 if a > b. If all pairs are equal the result is 0.
int CompareSyntaxTreePath(const SyntaxTreePath &a, const SyntaxTreePath &b);

// Type that is used to keep track of positions descended from a root
// node to reach a particular node.
//
// This is very similar in spirit to VectorTree<>::Path(), but
// needs to be tracked in a stack-like manner during visitation
// because SyntaxTreeNode and Leaf do not maintain upward pointers
// to their parent nodes.
//
// Its comparison operators use slightly modified lexicographicall comparison.
// Negative values are always less than empty and non-negative values, e.g.
// [-1] < [] < [0].
//
// TODO(fangism): consider replacing with hybrid "small" vector to
// minimize heap allocations, because these are expected to be small.
class SyntaxTreePath : public std::vector<int> {
 public:
  using std::vector<int>::vector;  // Import base class constructors

  bool operator==(const SyntaxTreePath &rhs) const {
    return CompareSyntaxTreePath(*this, rhs) == 0;
  }
  bool operator<(const SyntaxTreePath &rhs) const {
    return CompareSyntaxTreePath(*this, rhs) < 0;
  }
  bool operator>(const SyntaxTreePath &rhs) const {
    return CompareSyntaxTreePath(*this, rhs) > 0;
  }
  bool operator!=(const SyntaxTreePath &rhs) const { return !(*this == rhs); }
  bool operator<=(const SyntaxTreePath &rhs) const { return !(*this > rhs); }
  bool operator>=(const SyntaxTreePath &rhs) const { return !(*this < rhs); }
};

// This visitor traverses a tree and maintains a stack of offsets
// that represents the positional path taken from the root to
// reach each node.
// This is useful for applications where the shape and positions of nodes
// within the tree are meaningful.
class TreeContextPathVisitor : public TreeContextVisitor {
 public:
  TreeContextPathVisitor() = default;

 protected:
  void Visit(const SyntaxTreeNode &node) override;

  const SyntaxTreePath &Path() const { return current_path_; }

  // Keeps track of path of descent from root node.
  SyntaxTreePath current_path_;
};

// Computes the path of the next sibling by incrementing the last element
// of the path.  Resulting path may not necessarily correspond to a valid
// element.  'path' must not be empty.
SyntaxTreePath NextSiblingPath(const SyntaxTreePath &path);

// Format SyntaxTreePaths using: stream << TreePathFormatter(path);
// It is necessary to define this way because SyntaxTreePath is a typedef to a
// generic container type.
// TODO(fangism): Use auto return type once C++17 become the minimum standard.
SequenceStreamFormatter<SyntaxTreePath> TreePathFormatter(
    const SyntaxTreePath &path);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_CONTEXT_VISITOR_H_
