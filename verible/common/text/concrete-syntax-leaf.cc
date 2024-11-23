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

#include "verible/common/text/concrete-syntax-leaf.h"

#include <iostream>
#include <memory>

#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/tree-compare.h"
#include "verible/common/text/visitors.h"
#include "verible/common/util/casts.h"
#include "verible/common/util/logging.h"

namespace verible {

// Tests if this is equal to SymbolPtr under compare_tokens function
bool SyntaxTreeLeaf::equals(const Symbol *symbol,
                            const TokenComparator &compare_tokens) const {
  if (symbol->Kind() == SymbolKind::kLeaf) {
    const auto *leaf = down_cast<const SyntaxTreeLeaf *>(symbol);
    return equals(leaf, compare_tokens);
  }
  return false;
}

// Tests if this is equal to SyntaxTreeLeaf under compare_tokens function
bool SyntaxTreeLeaf::equals(const SyntaxTreeLeaf *leaf,
                            const TokenComparator &compare_tokens) const {
  return compare_tokens(this->get(), leaf->get());
}

// Uses treevisitor to visit this
void SyntaxTreeLeaf::Accept(TreeVisitorRecursive *visitor) const {
  visitor->Visit(*this);
}

void SyntaxTreeLeaf::Accept(MutableTreeVisitorRecursive *visitor,
                            SymbolPtr *this_owned) {
  CHECK_EQ(ABSL_DIE_IF_NULL(this_owned)->get(), this);
  visitor->Visit(*this, this_owned);
}

void SyntaxTreeLeaf::Accept(SymbolVisitor *visitor) const {
  visitor->Visit(*this);
}

std::ostream &operator<<(std::ostream &os, const SyntaxTreeLeaf &l) {
  return os << l.get();
}

}  // namespace verible
