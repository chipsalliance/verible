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

#include "common/lsp/lsp-protocol.h"
#include "nlohmann/json.hpp"
#include "verilog/tools/ls/lsp-parse-buffer.h"

// Adapter functions converting verible state into lsp objects.

namespace verilog {

// Given the output of the parser and a lint status, create a diagnostic
// output to be sent in textDocument/publishDiagnostics notification.
std::vector<verible::lsp::Diagnostic> CreateDiagnostics(const BufferTracker &);

}  // namespace verilog
#endif  // VERILOG_TOOLS_LS_VERIBLE_LSP_ADAPTER_H
