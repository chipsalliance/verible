// Copyright 2023 The Verible Authors.
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
//

#ifndef VERILOG_TOOLS_LS_AUTOEXPAND_H
#define VERILOG_TOOLS_LS_AUTOEXPAND_H

#include <vector>

#include "verible/common/lsp/lsp-protocol.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

// Functions for Emacs' Verilog-Mode-style AUTO expansion.

namespace verilog {
// Generate AUTO expansion code actions for the given code action params
std::vector<verible::lsp::CodeAction> GenerateAutoExpandCodeActions(
    SymbolTableHandler *symbol_table_handler, const BufferTracker *tracker,
    const verible::lsp::CodeActionParams &p);

}  // namespace verilog
#endif  // VERILOG_TOOLS_LS_AUTOEXPAND_H
