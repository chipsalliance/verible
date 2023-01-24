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

#include "verilog/tools/ls/verilog-language-server.h"

#include <functional>

#include "absl/strings/string_view.h"
#include "common/lsp/lsp-protocol.h"
#include "common/util/init_command_line.h"
#include "verilog/tools/ls/verible-lsp-adapter.h"

namespace verilog {

VerilogLanguageServer::VerilogLanguageServer(const WriteFun &write_fun)
    : dispatcher_(write_fun), buffers_(&dispatcher_) {
  // All bodies the stream splitter extracts are pushed to the json dispatcher
  stream_splitter_.SetMessageProcessor(
      [this](absl::string_view header, absl::string_view body) {
        return dispatcher_.DispatchMessage(body);
      });

  // Whenever there is a new parse result ready, use that as an opportunity
  // to send diagnostics to the client.
  buffers_.SetChangeListener(parsed_buffers_.GetSubscriptionCallback());
  parsed_buffers_.SetChangeListener(
      [this](const std::string &uri,
             const verilog::BufferTracker &buffer_tracker) {
        SendDiagnostics(uri, buffer_tracker);
      });
  SetRequestHandlers();
}

void VerilogLanguageServer::SetRequestHandlers() {
  // Exchange of capabilities.
  dispatcher_.AddRequestHandler("initialize",
                                [this](const nlohmann::json &params) {
                                  return InitializeRequestHandler(params);
                                });

  dispatcher_.AddRequestHandler(  // Provide diagnostics on request
      "textDocument/diagnostic",
      [this](const verible::lsp::DocumentDiagnosticParams &p) {
        return verilog::GenerateDiagnosticReport(
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p);
      });

  dispatcher_.AddRequestHandler(  // Provide autofixes
      "textDocument/codeAction",
      [this](const verible::lsp::CodeActionParams &p) {
        return verilog::GenerateCodeActions(
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p);
      });

  dispatcher_.AddRequestHandler(  // Provide document outline/index
      "textDocument/documentSymbol",
      [this](const verible::lsp::DocumentSymbolParams &p) {
        return verilog::CreateDocumentSymbolOutline(
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p);
      });

  dispatcher_.AddRequestHandler(  // Highlight related symbols under cursor
      "textDocument/documentHighlight",
      [this](const verible::lsp::DocumentHighlightParams &p) {
        return verilog::CreateHighlightRanges(
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p);
      });

  dispatcher_.AddRequestHandler(  // format range of file
      "textDocument/rangeFormatting",
      [this](const verible::lsp::DocumentFormattingParams &p) {
        return verilog::FormatRange(
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p);
      });
  dispatcher_.AddRequestHandler(  // format entire file
      "textDocument/formatting",
      [this](const verible::lsp::DocumentFormattingParams &p) {
        return verilog::FormatRange(
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p);
      });
  // The client sends a request to shut down. Use that to exit our loop.
  dispatcher_.AddRequestHandler("shutdown", [this](const nlohmann::json &) {
    shutdown_requested_ = true;
    return nullptr;
  });
}

absl::Status VerilogLanguageServer::Step(const ReadFun &read_fun) {
  return stream_splitter_.PullFrom(read_fun);
}

absl::Status VerilogLanguageServer::Run(const ReadFun &read_fun) {
  shutdown_requested_ = false;
  absl::Status status = absl::OkStatus();
  while (status.ok() && !shutdown_requested_) {
    status = Step(read_fun);
  }
  return status;
}

void VerilogLanguageServer::PrintStatistics() const {
  if (shutdown_requested_) {
    std::cerr << "Shutting down due to shutdown request." << std::endl;
  }

  std::cerr << "Statistics" << std::endl;
  std::cerr << "Largest message seen: "
            << stream_splitter_.StatLargestBodySeen() / 1024 << " kiB "
            << std::endl;
  for (const auto &stats : dispatcher_.GetStatCounters()) {
    fprintf(stderr, "%30s %9d\n", stats.first.c_str(), stats.second);
  }
}

verible::lsp::InitializeResult VerilogLanguageServer::InitializeRequestHandler(
    const nlohmann::json &params) const {
  // Ignore passed client capabilities from params right now,
  // just announce what we do.
  verible::lsp::InitializeResult result;
  result.serverInfo = {
      .name = "Verible Verilog language server.",
      .version = verible::GetRepositoryVersion(),
  };
  result.capabilities = {
      {
          "textDocumentSync",
          {
              {"openClose", true},  // Want open/close events
              {"change", 2},        // Incremental updates
          },
      },
      {"codeActionProvider", true},               // Autofixes for lint errors
      {"documentSymbolProvider", true},           // Symbol-outline of file
      {"documentRangeFormattingProvider", true},  // Format selection
      {"documentFormattingProvider", true},       // Full file format
      {"documentHighlightProvider", true},        // Highlight same symbol
      {"diagnosticProvider",                      // Pull model of diagnostics.
       {
           {"interFileDependencies", false},
           {"workspaceDiagnostics", false},
       }},
  };

  return result;
}

void VerilogLanguageServer::SendDiagnostics(
    const std::string &uri, const verilog::BufferTracker &buffer_tracker) {
  // TODO(hzeller): Cache result and rate-limit.
  // This should not send anything if the diagnostics we're about to
  // send would be exactly the same as last time.
  verible::lsp::PublishDiagnosticsParams params;

  // For the diagnostic notification (that we send somewhat unsolicited), we
  // limit the number of diagnostic messages. In the
  // textDocument/diagnostic RPC request, we send all of them.
  // Arbitrary limit here. Maybe set with flag ?
  static constexpr int kDiagnosticLimit = 500;
  params.uri = uri;
  params.diagnostics =
      verilog::CreateDiagnostics(buffer_tracker, kDiagnosticLimit);
  dispatcher_.SendNotification("textDocument/publishDiagnostics", params);
}

};  // namespace verilog
