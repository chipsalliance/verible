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

namespace verilog {

using verible::Symbol;
using verible::SyntaxTreeContext;
using verible::TokenInfo;

static const TokenInfo* GetSeqBlockLabelTokenInfo(const Symbol& symbol,
                                                  NodeEnum parent) {
  const auto* s = GetSubtreeAsSymbol(symbol, parent, 1);

  if (s == nullptr) {
    return nullptr;
  }

  const auto& node = SymbolCastToNode(*s);
  return &AutoUnwrapIdentifier(*node.children()[1].get())->get();
}

const TokenInfo* GetBeginLabelTokenInfo(const Symbol& symbol) {
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kBegin);
  return GetSeqBlockLabelTokenInfo(symbol, NodeEnum::kBegin);
}

const TokenInfo* GetEndLabelTokenInfo(const Symbol& symbol) {
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kEnd);
  return GetSeqBlockLabelTokenInfo(symbol, NodeEnum::kEnd);
}

const Symbol* GetMatchingEnd(const Symbol& symbol,
                             const SyntaxTreeContext& context) {
  CHECK_EQ(NodeEnum(symbol.Tag().tag), NodeEnum::kBegin);
  return context.top().children().back().get();
}

}  // namespace verilog
