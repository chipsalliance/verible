// Copyright 2017-2019 The Verible Authors.
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

#include "verilog/CST/expression.h"

#include <memory>

#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/token_info.h"
#include "common/text/tree_utils.h"
#include "common/util/casts.h"
#include "verilog/CST/verilog_nonterminals.h"

namespace verilog {

using verible::down_cast;
using verible::Symbol;
using verible::SymbolKind;
using verible::SyntaxTreeLeaf;
using verible::SyntaxTreeNode;

bool IsExpression(const verible::SymbolPtr& symbol_ptr) {
  if (symbol_ptr == nullptr) return false;
  if (symbol_ptr->Kind() != SymbolKind::kNode) return false;
  const auto& node = down_cast<const SyntaxTreeNode&>(*symbol_ptr);
  return node.MatchesTag(NodeEnum::kExpression);
}

bool IsZero(const Symbol& expr) {
  const Symbol* child = verible::DescendThroughSingletons(expr);
  int value;
  if (ConstantIntegerValue(*child, &value)) {
    return value == 0;
  }
  if (child->Kind() != SymbolKind::kLeaf) return false;
  const auto& term = down_cast<const SyntaxTreeLeaf&>(*child);
  auto text = term.get().text;
  if (text == "\'0") return true;
  // TODO(fangism): Could do more sophisticated constant expression evaluation
  // but for now it is fine for this to conservatively return false.
  return false;
}

bool ConstantIntegerValue(const verible::Symbol& expr, int* value) {
  const Symbol* child = verible::DescendThroughSingletons(expr);
  if (child->Kind() != SymbolKind::kLeaf) return false;
  const auto& term = down_cast<const SyntaxTreeLeaf&>(*child);
  // Don't even need to check the leaf token's enumeration type.
  auto text = term.get().text;
  return absl::SimpleAtoi(text, value);
}

}  // namespace verilog
