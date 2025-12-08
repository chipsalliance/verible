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

#ifndef VERILOG_TOOLS_LS_HOVER_H_INCLUDED
#define VERILOG_TOOLS_LS_HOVER_H_INCLUDED

#include "verible/common/lsp/lsp-protocol.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

namespace verilog {
// Provides hover information for given location
verible::lsp::Hover CreateHoverInformation(
    SymbolTableHandler *symbol_table_handler,
    const BufferTrackerContainer &tracker, const verible::lsp::HoverParams &p);
}  // namespace verilog

#endif  // hover_h_INCLUDED
