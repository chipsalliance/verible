// Copyright 2021 The Verible Authors.
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

#ifndef VERILOG_TOOLS_LS_VERIBLE_LSP_ADAPTER_H
#define VERILOG_TOOLS_LS_VERIBLE_LSP_ADAPTER_H

#include <vector>

#include "nlohmann/json.hpp"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

// Adapter functions converting verible state into lsp objects.

namespace verilog {

// Given the output of the parser and a lint status, create a diagnostic
// output to be sent in textDocument/publishDiagnostics notification.
std::vector<verible::lsp::Diagnostic> CreateDiagnostics(const BufferTracker &,
                                                        int message_limit);

// Generate code actions from autofixes provided by the linter.
std::vector<verible::lsp::CodeAction> GenerateLinterCodeActions(
    const BufferTracker *tracker, const verible::lsp::CodeActionParams &p);

// Generate all available code actions.
std::vector<verible::lsp::CodeAction> GenerateCodeActions(
    SymbolTableHandler *symbol_table_handler, const BufferTracker *tracker,
    const verible::lsp::CodeActionParams &p);

verible::lsp::FullDocumentDiagnosticReport GenerateDiagnosticReport(
    const BufferTracker *tracker,
    const verible::lsp::DocumentDiagnosticParams &p);

// Given a parse tree, generate a document symbol outline
// textDocument/documentSymbol request
// There is a workaround for the kate editor currently. Goal is to actually
// fix this upstream in the kate editor, but for now let's have an explicit
// boolean to make it visible what is needed.
nlohmann::json CreateDocumentSymbolOutline(
    const BufferTracker *tracker, const verible::lsp::DocumentSymbolParams &p,
    bool kate_compatible_tags = false, bool include_variables = true);

// Given a position in a document, return ranges in the buffer that should
// be highlighted.
// Current implementation: if cursor is over a symbol, highlight all symbols
// with the same name (NB: Does _not_ take scoping into account yet).
std::vector<verible::lsp::DocumentHighlight> CreateHighlightRanges(
    const BufferTracker *tracker,
    const verible::lsp::DocumentHighlightParams &p);

// Format given range (or whole document) and emit an edit.
std::vector<verible::lsp::TextEdit> FormatRange(
    const BufferTracker *tracker,
    const verible::lsp::DocumentFormattingParams &p);

}  // namespace verilog
#endif  // VERILOG_TOOLS_LS_VERIBLE_LSP_ADAPTER_H
