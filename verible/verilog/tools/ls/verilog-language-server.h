// Copyright 2021-2022 The Verible Authors.
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

#ifndef VERILOG_TOOLS_LS_LS_WRAPPER_H
#define VERILOG_TOOLS_LS_LS_WRAPPER_H

#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/lsp/json-rpc-dispatcher.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/lsp/lsp-text-buffer.h"
#include "verible/common/lsp/message-stream-splitter.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"

namespace verilog {

// TODO add support for changing workspace
// TODO reset symbol table on workspace change?

// Class implementing the Language Server for Verilog
class VerilogLanguageServer {
 public:
  using ReadFun = verible::lsp::MessageStreamSplitter::ReadFun;
  using WriteFun = verible::lsp::JsonRpcDispatcher::WriteFun;

  // Constructor preparing the callbacks for Language Server requests
  explicit VerilogLanguageServer(const WriteFun &write_fun);

  // Reads single request and responds to it (public to mock in tests).
  absl::Status Step(const ReadFun &read_fun);

  // Runs the Language Server, calling "read_fun" until we receive shutdown.
  absl::Status Run(const ReadFun &read_fun);

  // Prints statistics of the current Language Server session.
  void PrintStatistics() const;

  // A flag that sets inclusion of variables into documentSymbol requests
  bool include_variables = true;

 private:
  // Creates callbacks for requests from Language Server Client
  void SetRequestHandlers();

  // The "initialize" method requests server capabilities.
  verible::lsp::InitializeResult InitializeRequestHandler(
      const verible::lsp::InitializeParams &params);

  // Returns language server capabilities in textDocument/initialize response
  // format
  verible::lsp::InitializeResult GetCapabilities();

  // sets up the VerilogProject structure for symbol table, sets listeners for
  // updating VerilogProject views for edited files
  // if "project_root" is an empty string, set to either current directory
  // or directory containing verible.filelist
  void ConfigureProject(std::string_view project_root);

  // Publish a diagnostic sent to the server.
  void SendDiagnostics(const std::string &uri,
                       const verilog::BufferTracker &buffer_tracker);

  // Stream splitter splits the input stream into messages (header/body).
  verible::lsp::MessageStreamSplitter stream_splitter_;

  // Parser for JSON messages from LS client
  verible::lsp::JsonRpcDispatcher dispatcher_;

  // Object for keeping track of updates in opened buffers on client's side
  verible::lsp::BufferCollection text_buffers_;

  // Tracks changes in buffers from BufferCollection and parses their contents
  verilog::BufferTrackerContainer parsed_buffers_;

  // Handles requests relying on the symbol table
  verilog::SymbolTableHandler symbol_table_handler_;

  // A flag for indicating "shutdown" request
  bool shutdown_requested_ = false;
};

}  // namespace verilog
#endif  // VERILOG_TOOLS_LS_LS_WRAPPER_H
