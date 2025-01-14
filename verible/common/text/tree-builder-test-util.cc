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

#include "verible/common/text/tree-builder-test-util.h"

#include <cstddef>
#include <initializer_list>
#include <string_view>

#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-utils.h"
#include "verible/common/util/logging.h"

namespace verible {

constexpr std::string_view kDontCareText;

SymbolPtr XLeaf(int token_enum) { return Leaf(token_enum, kDontCareText); }

const Symbol *DescendPath(const Symbol &symbol,
                          std::initializer_list<size_t> path) {
  const Symbol *node_symbol = &symbol;
  for (const auto &index : path) {
    const auto &node = SymbolCastToNode(*ABSL_DIE_IF_NULL(node_symbol));
    CHECK_LT(index, node.size());  // bounds check, like ::at()
    node_symbol = node[index].get();
  }
  return node_symbol;
}

}  // namespace verible
