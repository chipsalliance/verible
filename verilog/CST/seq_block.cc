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

#include "verilog/CST/seq_block.h"

#include "common/text/concrete_syntax_leaf.h"
#include "common/text/concrete_syntax_tree.h"
#include "common/text/symbol.h"
#include "common/text/syntax_tree_context.h"
#include "common/text/tree_utils.h"
#include "verilog/CST/identifier.h"
#include "verilog/CST/verilog_nonterminals.h"
#include "verilog/parser/verilog_token_enum.h"

namespace verilog {

using verible::Symbol;
using verible::SyntaxTreeContext;
using verible::SyntaxTreeNode;
using verible::TokenInfo;

// kLabel could be prefix "label :" or suffix ": label".  Handle both cases.
static const verible::SyntaxTreeLeaf& GetLabelLeafText(const Symbol& label) {
  const auto& node = CheckSymbolAsNode(label, NodeEnum::kLabel);
  CHECK_EQ(node.children().size(), 2);
  if (node.children().front()->Tag() ==
      verible::SymbolTag{verible::SymbolKind::kLeaf, ':'}) {
    return verible::SymbolCastToLeaf(*node.children().back());
  }
  CHECK((node.children().back()->Tag() ==
         verible::SymbolTag{verible::SymbolKind::kLeaf, ':'}));
  // in verilog.y, a prefix label could be an unqualified_id (to avoid grammar
  // conflicts), so descend to the leftmost leaf.
  return verible::SymbolCastToLeaf(
      *ABSL_DIE_IF_NULL(verible::GetLeftmostLeaf(*node.children().front())));
}

// Return tehe optional label node from a kBegin node.
// In verilog.y, kBegin is constructed one of two ways:
//   begin : label  (shaped as [begin [: label]])
//   label : begin  (shaped as [[label :] begin])
static const SyntaxTreeNode* GetBeginLabel(const Symbol& begin) {
  const auto& node = CheckSymbolAsNode(begin, NodeEnum::kBegin);
  CHECK_EQ(node.children().size(), 2);
  if (node.children().front()->Tag() ==
      verible::SymbolTag{verible::SymbolKind::kLeaf,
                         verilog_tokentype::TK_begin}) {
    return verible::CheckOptionalSymbolAsNode(node.children().back().get(),
                                              NodeEnum::kLabel);
  }
  CHECK((node.children().back()->Tag() ==
         verible::SymbolTag{verible::SymbolKind::kLeaf,
                            verilog_tokentype::TK_begin}));
  return verible::CheckOptionalSymbolAsNode(node.children().front().get(),
                                            NodeEnum::kLabel);
}

static const SyntaxTreeNode* GetEndLabel(const Symbol& end) {
  const auto* label = verible::GetSubtreeAsSymbol(end, NodeEnum::kEnd, 1);
  if (label == nullptr) return nullptr;
  return verible::CheckOptionalSymbolAsNode(label, NodeEnum::kLabel);
}

const TokenInfo* GetBeginLabelTokenInfo(const Symbol& symbol) {
  const SyntaxTreeNode* label = GetBeginLabel(symbol);
  if (label == nullptr) return nullptr;
  return &GetLabelLeafText(*label).get();
}

const TokenInfo* GetEndLabelTokenInfo(const Symbol& symbol) {
  const SyntaxTreeNode* label = GetEndLabel(symbol);
  if (label == nullptr) return nullptr;
  return &GetLabelLeafText(*label).get();
}

const Symbol* GetMatchingEnd(const Symbol& symbol,
                             const SyntaxTreeContext& context) {
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kBegin);
  return context.top().children().back().get();
}

}  // namespace verilog
