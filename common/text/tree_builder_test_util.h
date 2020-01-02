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

#include <utility>

#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"

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
SymbolPtr Leaf(Args&&... args) {
  return SymbolPtr(new SyntaxTreeLeaf(std::forward<Args>(args)...));
}

// Use this for constructing leaves where you don't care about the token text.
SymbolPtr XLeaf(int token_enum);

}  // namespace verible

#endif  // VERIBLE_COMMON_TEXT_TREE_BUILDER_TEST_UTIL_H_
