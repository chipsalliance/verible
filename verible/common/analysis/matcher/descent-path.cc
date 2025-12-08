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

#include "verible/common/analysis/matcher/descent-path.h"

#include <memory>
#include <vector>

#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/util/casts.h"

namespace verible {
namespace matcher {

// AggregateAllDescendantsFromPath is a local helper for
// GetAllDescendantsFromPath. It minimizes copying by using iterators
// and pushing all discovered symbols onto a single vector.
//
// Adds all descendants of symbol that are precisely along path to target
// children must have matching SymbolTag to the last element of path.
//
// Position should point to the start position of your path and
// end should point to the end position.
//
// Discovered children are pushed back onto target
static void AggregateAllDescendantsFromPath(
    const Symbol &symbol, const DescentPath::const_iterator &position,
    const DescentPath::const_iterator &end,
    std::vector<const Symbol *> *target);

std::vector<const Symbol *> GetAllDescendantsFromPath(const Symbol &symbol,
                                                      const DescentPath &path) {
  std::vector<const Symbol *> target;

  if (symbol.Kind() == SymbolKind::kNode) {
    const auto *node = down_cast<const SyntaxTreeNode *>(&symbol);
    for (const auto &child : node->children()) {
      if (child) {
        AggregateAllDescendantsFromPath(*child, path.begin(), path.end(),
                                        &target);
      }
    }
  }

  return target;
}

static void AggregateAllDescendantsFromPath(
    const Symbol &symbol, const DescentPath::const_iterator &position,
    const DescentPath::const_iterator &end,
    std::vector<const Symbol *> *target) {
  // If we are somehow operating on empty vector, stop recursion.
  if (position == end) {
    return;
  }

  // If we're at the last SymbolTag, stop recursion and check if we need to add
  // symbol to target.
  if (position + 1 == end) {
    if (symbol.Tag() == (*position)) {
      target->push_back(&symbol);
    }
    return;
  }

  // In order to recursively check descendants, symbol needs to be a node
  if (symbol.Kind() != SymbolKind::kNode) {
    return;
  }

  const auto *node = down_cast<const SyntaxTreeNode *>(&symbol);

  // If the cast fails or the node does not have the required tag or kind,
  // then stop recursion
  const auto tag_kind = node->Tag();
  if (tag_kind != *position) {
    return;
  }

  // Recurse on children
  for (const auto &child : node->children()) {
    if (child) {
      AggregateAllDescendantsFromPath(*child, position + 1, end, target);
    }
  }
}

}  // namespace matcher
}  // namespace verible
