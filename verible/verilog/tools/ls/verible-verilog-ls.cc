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
//

#include <iostream>
#include <string_view>

#include "absl/status/status.h"
#include "verible/common/util/init-command-line.h"
#include "verible/verilog/tools/ls/verilog-language-server.h"

#ifndef _WIN32
#include <unistd.h>
#else
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
// Windows doesn't have Posix read(), but something called _read
#define read(fd, buf, size) _read(fd, buf, size)
#endif

// Since it is hard to see what exactly the editor passes to the language
// server, let's log it, so it can be inspected in the log.
static void LogCommandline(int argc, char **argv) {
  std::cerr << "commandline: ";
  for (int i = 0; i < argc; ++i) {
    std::cerr << argv[i] << " ";
  }
  std::cerr << "\n";
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
  // Windows messes with newlines by default. Fix this here.
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif

  std::cerr << "Verible Verilog Language Server built at "
            << verible::GetRepositoryVersion() << "\n";
  LogCommandline(argc, argv);

  // Initialize flags from the command line.
  verible::InitCommandLine(argv[0], &argc, &argv);

  // -- Input and output is stdin and stdout.

  // Output: provided write-function is called with entire response messages.
  verilog::VerilogLanguageServer server([](std::string_view reply) {
    // Output formatting as header/body chunk as required by LSP spec to stdout.
    std::cout << "Content-Length: " << reply.size() << "\r\n\r\n";
    std::cout << reply << std::flush;
  });

  // Input: Messages received from the read function are dispatched and
  // processed until shutdown message received.
  constexpr int kInputFD = 0;  // STDIN_FILENO, but Win does not have that macro
  absl::Status status = server.Run([](char *buf, int size) -> int {  //
    return read(kInputFD, buf, size);
  });

  std::cerr << status << '\n';

  server.PrintStatistics();
}
