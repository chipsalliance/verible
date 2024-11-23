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

#ifndef VERIBLE_COMMON_TEXT_VISITORS_H_
#define VERIBLE_COMMON_TEXT_VISITORS_H_

#include "verible/common/text/symbol-ptr.h"

namespace verible {

// forward declaration to allow pointers in function prototypes
class SyntaxTreeLeaf;
class SyntaxTreeNode;

// TreeVisitorRecursive is an abstract tree visitor class from which
// visitors can be derived
// TreeVisitorRecursive and all subclasses will visit all leaves with
// a preorder traversal
//
// Usage:
//  SymbolPtr tree = ...
//  TreeVisitorRecursiveSubclass visitor;
//  tree->Accept(visitor);
//
class TreeVisitorRecursive {
 public:
  virtual ~TreeVisitorRecursive() = default;
  virtual void Visit(const SyntaxTreeLeaf &leaf) = 0;
  virtual void Visit(const SyntaxTreeNode &node) = 0;
};

// SymbolVisitor is an abstract visitor class from which visitors can be derived
// SymbolVisitor only visits a single Symbol, and does not recurse on nodes
//
// Usage:
//  SymbolPtr tree = ...
//  SymbolVisitorSubclass visitor;
//  tree->Accept(visitor);
//
class SymbolVisitor {
 public:
  virtual ~SymbolVisitor() = default;
  virtual void Visit(const SyntaxTreeLeaf &leaf) = 0;
  virtual void Visit(const SyntaxTreeNode &node) = 0;
};

// MutableTreeVisitorRecursive is the non-const version of TreeVisitorRecursive.
// Traversals that potentially want to modify the syntax tree should
// derive from this class.  The second argument is an owned pointer that
// corresponds to the leaf or node (*same* address as first argument).
// Passing the owned pointer makes it possible for subclass implementations to
// delete or mutate syntax tree nodes.
class MutableTreeVisitorRecursive {
 public:
  virtual ~MutableTreeVisitorRecursive() = default;
  virtual void Visit(const SyntaxTreeLeaf &leaf, SymbolPtr *) = 0;
  virtual void Visit(const SyntaxTreeNode &node, SymbolPtr *) = 0;
};

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_VISITORS_H_
