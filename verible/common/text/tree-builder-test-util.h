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

// Suite of helper functions for concisely building trees inline

#ifndef VERIBLE_COMMON_TEXT_TREE_BUILDER_TEST_UTIL_H_
#define VERIBLE_COMMON_TEXT_TREE_BUILDER_TEST_UTIL_H_

#include <cstddef>
#include <initializer_list>
#include <utility>

#include "verible/common/text/concrete-syntax-leaf.h"
#include "verible/common/text/concrete-syntax-tree.h"

namespace verible {

template <typename... Args>
SymbolPtr Node(Args... args) {
  return MakeNode(args...);
}
template <typename... Args, typename Enum>
SymbolPtr TNode(Enum e, Args... args) {
  return MakeTaggedNode(e, args...);
}

template <typename... Args>
SymbolPtr Leaf(Args &&...args) {
  return SymbolPtr(new SyntaxTreeLeaf(std::forward<Args>(args)...));
}

// Use this for constructing leaves where you don't care about the token text.
SymbolPtr XLeaf(int token_enum);

// Descend through subtree, using path's indices, asserting that each subnode
// along the way is a node.
// If any intermediate element visited is not a SyntaxTreeNode, or the index
// into its children is out-of-bounds, then this will exit fatally.
// This assert-as-you-descend behavior is mostly useful for testing for direct
// contents of hand-crafted trees.
// This variant does not check node enums.
// TODO(fangism): implement one that verifies node enums.
const Symbol *DescendPath(const Symbol &symbol,
                          std::initializer_list<size_t> path);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_BUILDER_TEST_UTIL_H_
