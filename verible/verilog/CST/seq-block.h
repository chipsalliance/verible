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

// This unit provides helper functions that pertain to SystemVerilog
// net declaration nodes in the parser-generated concrete syntax tree.

#ifndef VERIBLE_VERILOG_CST_SEQ_BLOCK_H_
#define VERIBLE_VERILOG_CST_SEQ_BLOCK_H_

#include "verible/common/text/concrete-syntax-tree.h"
#include "verible/common/text/symbol.h"
#include "verible/common/text/syntax-tree-context.h"
#include "verible/common/text/token-info.h"

namespace verilog {

// Get TokenInfo of a label for a given kBegin symbol if exists, else nullptr
const verible::TokenInfo *GetBeginLabelTokenInfo(const verible::Symbol &symbol);

// Get TokenInfo of a label for a given kEnd symbol if exists, else nullptr
const verible::TokenInfo *GetEndLabelTokenInfo(const verible::Symbol &symbol);

// Find and return a pointer to a kEnd symbol corresponding to a given kBegin
const verible::Symbol *GetMatchingEnd(
    const verible::Symbol &symbol, const verible::SyntaxTreeContext &context);

}  // namespace verilog

#endif  // VERIBLE_VERILOG_CST_SEQ_BLOCK_H_
