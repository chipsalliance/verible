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

#include "verible/verilog/tools/ls/verilog-language-server.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "nlohmann/json.hpp"
#include "verible/common/lsp/lsp-file-utils.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/util/file-util.h"
#include "verible/common/util/init-command-line.h"
#include "verible/common/util/logging.h"
#include "verible/verilog/analysis/verilog-project.h"
#include "verible/verilog/tools/ls/hover.h"
#include "verible/verilog/tools/ls/lsp-parse-buffer.h"
#include "verible/verilog/tools/ls/symbol-table-handler.h"
#include "verible/verilog/tools/ls/verible-lsp-adapter.h"

ABSL_FLAG(bool, variables_in_outline, true,
          "Variables should be included into the symbol outline");

namespace verilog {

VerilogLanguageServer::VerilogLanguageServer(const WriteFun &write_fun)
    : dispatcher_(write_fun), text_buffers_(&dispatcher_) {
  // All bodies the stream splitter extracts are pushed to the json dispatcher
  stream_splitter_.SetMessageProcessor(
      [this](std::string_view header, std::string_view body) {
        dispatcher_.DispatchMessage(body);
      });

  // Whenever the text changes in the editor, reparse affected code.
  text_buffers_.SetChangeListener(parsed_buffers_.GetSubscriptionCallback());

  // Whenever there is a new parse result ready, use that as an opportunity
  // to send diagnostics to the client.
  parsed_buffers_.AddChangeListener(
      [this](const std::string &uri,
             const verilog::BufferTracker *buffer_tracker) {
        if (buffer_tracker) SendDiagnostics(uri, *buffer_tracker);
      });
  SetRequestHandlers();
}

verible::lsp::InitializeResult VerilogLanguageServer::GetCapabilities() {
  // send response with information what we do.
  verible::lsp::InitializeResult result;
  this->include_variables = absl::GetFlag(FLAGS_variables_in_outline);
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
      {"definitionProvider", true},               // Provide going to definition
      {"referencesProvider", true},               // Provide going to references
      // Hover enabled, but not yet offered to client until tested.
      {"hoverProvider", false},  // Hover info over cursor
      {"renameProvider", true},  // Provide symbol renaming
      {"diagnosticProvider",     // Pull model of diagnostics.
       {
           {"interFileDependencies", false},
           {"workspaceDiagnostics", false},
       }},
  };

  return result;
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
            &symbol_table_handler_,
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p);
      });

  dispatcher_.AddRequestHandler(  // Provide document outline/index
      "textDocument/documentSymbol",
      [this](const verible::lsp::DocumentSymbolParams &p) {
        // The `false` sets the kate workaround to the set default, as it was
        // not set here
        return verilog::CreateDocumentSymbolOutline(
            parsed_buffers_.FindBufferTrackerOrNull(p.textDocument.uri), p,
            false, this->include_variables);
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
  dispatcher_.AddRequestHandler(  // go-to definition
      "textDocument/definition",
      [this](const verible::lsp::DefinitionParams &p) {
        return symbol_table_handler_.FindDefinitionLocation(p, parsed_buffers_);
      });
  dispatcher_.AddRequestHandler(  // go-to references
      "textDocument/references",
      [this](const verible::lsp::ReferenceParams &p) {
        return symbol_table_handler_.FindReferencesLocations(p,
                                                             parsed_buffers_);
      });
  dispatcher_.AddRequestHandler(
      "textDocument/prepareRename",
      [this](const verible::lsp::PrepareRenameParams &p) -> nlohmann::json {
        auto range = symbol_table_handler_.FindRenameableRangeAtCursor(
            p, parsed_buffers_);
        if (range.has_value()) return range.value();
        return nullptr;
      });
  dispatcher_.AddRequestHandler(
      "textDocument/rename", [this](const verible::lsp::RenameParams &p) {
        return symbol_table_handler_.FindRenameLocationsAndCreateEdits(
            p, parsed_buffers_);
      });
  dispatcher_.AddRequestHandler(
      "textDocument/hover", [this](const verible::lsp::HoverParams &p) {
        return CreateHoverInformation(&symbol_table_handler_, parsed_buffers_,
                                      p);
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
    const verible::lsp::InitializeParams &p) {
  // set VerilogProject for the symbol table, if possible
  if (const char *override_path = getenv("VERIBLE_LS_PROJECTROOT_OVERRIDE")) {
    ConfigureProject(override_path);
  } else if (!p.rootUri.empty()) {
    std::string path = verible::lsp::LSPUriToPath(p.rootUri);
    if (path.empty()) {
      LOG(ERROR) << "Unsupported rootUri in initialize request:  " << p.rootUri
                 << std::endl;
      path = p.rootUri;
    }
    ConfigureProject(path);
  } else if (!p.rootPath.empty()) {
    ConfigureProject(p.rootPath);
  } else {
    LOG(INFO) << "No root URI provided in language server initialization "
              << "from IDE. Assuming root='.'";
    ConfigureProject("");
  }
  return GetCapabilities();
}

void VerilogLanguageServer::ConfigureProject(std::string_view project_root) {
  LOG(INFO) << "Initializing with project-root '" << project_root << "'";
  std::string proj_root = {project_root.begin(), project_root.end()};
  if (proj_root.empty()) {
    proj_root = std::string(verible::file::Dirname(FindFileList(".")));
  }
  if (proj_root.empty()) proj_root = ".";
  proj_root =
      std::filesystem::absolute({proj_root.begin(), proj_root.end()}).string();
  std::shared_ptr<VerilogProject> proj = std::make_shared<VerilogProject>(
      proj_root, std::vector<std::string>(), "");
  symbol_table_handler_.SetProject(proj);

  // Whenever an updated buffer in editor is available, update symbol table.
  parsed_buffers_.AddChangeListener(
      symbol_table_handler_.CreateBufferTrackerListener());
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
