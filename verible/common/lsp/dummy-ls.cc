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

// A super-simple dummy LSP without functionality except responding
// to initialize and shutdown as well as tracking file contents.
// This is merely to test that the json-rpc plumbing is working.

#include <cstdio>
#include <string_view>

#include "absl/status/status.h"
#include "nlohmann/json.hpp"
#include "verible/common/lsp/json-rpc-dispatcher.h"
#include "verible/common/lsp/lsp-protocol.h"
#include "verible/common/lsp/lsp-text-buffer.h"
#include "verible/common/lsp/message-stream-splitter.h"

#ifndef _WIN32
#include <unistd.h>
#else
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
// Windows doesn't have Posix read(), but something called _read
#define read(fd, buf, size) _read(fd, buf, size)
#endif

#include <iostream>

using verible::lsp::BufferCollection;
using verible::lsp::InitializeResult;
using verible::lsp::JsonRpcDispatcher;
using verible::lsp::MessageStreamSplitter;

// The "initialize" method requests server capabilities.
static InitializeResult InitializeServer(const nlohmann::json &params) {
  // Ignore passed client capabilities from params right now,
  // just announce what we do.
  InitializeResult result;
  result.serverInfo = {
      .name = "Verible testing language server.",
      .version = "0.1",
  };
  result.capabilities = {
      {
          "textDocumentSync",
          {
              {"openClose", true},  // Want open/close events
              {"change", 2},        // Incremental updates
          },
      },
      // This is mostly to test array support in json-rpc-expect
      {"ignored_property", nlohmann::json::array({1, 2, 3})},
  };
  return result;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
  // Windows messes with newlines by default. Fix this here.
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  std::cerr << "Note: this dummy-ls is for testing." << std::endl;

  // Input and output is stdin and stdout
  constexpr int kInputFD = 0;  // STDIN_FILENO, but Win does not have that macro
  JsonRpcDispatcher::WriteFun write_fun = [](std::string_view reply) {
    // Output formatting as header/body chunk as required by LSP spec.
    std::cout << "Content-Length: " << reply.size() << "\r\n\r\n";
    std::cout << reply << std::flush;
  };

  JsonRpcDispatcher dispatcher(write_fun);

  // All bodies the stream splitter extracts are pushed to the json dispatcher
  MessageStreamSplitter stream_splitter;
  stream_splitter.SetMessageProcessor(
      [&dispatcher](std::string_view /*header*/, std::string_view body) {
        dispatcher.DispatchMessage(body);
      });

  // The buffer collection keeps track of all the buffers opened in the editor.
  // It registers callbacks to receive the relevant events on the dispatcher.
  BufferCollection buffers(&dispatcher);

  // Exchange of capabilities.
  dispatcher.AddRequestHandler("initialize", InitializeServer);

  // The client sends a request to shut down. Use that to exit our loop.
  bool shutdown_requested = false;
  dispatcher.AddRequestHandler("shutdown",
                               [&shutdown_requested](const nlohmann::json &) {
                                 shutdown_requested = true;
                                 return nullptr;
                               });

  absl::Status status = absl::OkStatus();
  while (status.ok() && !shutdown_requested) {
    status = stream_splitter.PullFrom([](char *buf, int size) -> int {  //
      return read(kInputFD, buf, size);
    });
  }

  std::cerr << status.message() << std::endl;

  if (shutdown_requested) {
    std::cerr << "Shutting down due to shutdown request." << std::endl;
  }

  std::cerr << "Statistics" << std::endl;
  for (const auto &stats : dispatcher.GetStatCounters()) {
    fprintf(stderr, "%30s %9d\n", stats.first.c_str(), stats.second);
  }
}
