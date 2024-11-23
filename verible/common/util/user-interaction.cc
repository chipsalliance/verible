// Copyright 2017-2021 The Verible Authors.
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

#include "verible/common/util/user-interaction.h"

#include <cstdint>
#include <iostream>
#include <string>

#include "absl/strings/string_view.h"

#ifndef _WIN32
#include <unistd.h>  // for isatty
#else
#include <io.h>
// MSVC recommends to use _isatty...
#define isatty _isatty
#endif

namespace verible {

bool IsInteractiveTerminalSession(const std::ostream &s) {
  // Unix: STDIN_FILENO; windows: _fileno( stdin ). So just name the
  // file descriptors by number.
  static bool kStdinIsTerminal = isatty(0);
  static bool kStdoutIsTerminal = isatty(1);
  return kStdinIsTerminal &&
         ((s.rdbuf() == std::cout.rdbuf() || s.rdbuf() == std::cerr.rdbuf()) &&
          kStdoutIsTerminal);
}

char ReadCharFromUser(std::istream &input, std::ostream &output,
                      bool input_is_terminal, absl::string_view prompt) {
  if (input_is_terminal) {
    // Terminal input: print prompt, read whole line and return first character.
    term::bold(output, prompt) << std::flush;

    std::string line;
    std::getline(input, line);

    if (input.eof() || input.fail()) {
      return '\0';
    }
    return line.empty() ? '\n' : line.front();
  }
  // Input from a file or pipe: no prompt, read single character.
  char c;
  input.get(c);
  if (input.eof() || input.fail()) {
    return '\0';
  }
  return c;
}

namespace term {
// TODO(hzeller): assumption here that basic ANSI codes work on all
// platforms, but if not, change this with ifdef.
static constexpr absl::string_view kBoldEscape("\033[1m");
static constexpr absl::string_view kInverseEscape("\033[7m");
static constexpr absl::string_view kNormalEscape("\033[0m");

// clang-format off
static constexpr absl::string_view kColorsStart[static_cast<uint32_t>(Color::kNumColors)] = {
    "\033[1;32m", // GREEN
    "\033[1;36m", // CYAN
    "\033[1;31m", // RED
    "",           // NONE
};
// clang-format on

std::ostream &bold(std::ostream &out, absl::string_view s) {
  if (IsInteractiveTerminalSession(out)) {
    out << kBoldEscape << s << kNormalEscape;
  } else {
    out << s;
  }
  return out;
}
std::ostream &inverse(std::ostream &out, absl::string_view s) {
  if (IsInteractiveTerminalSession(out)) {
    out << kInverseEscape << s << kNormalEscape;
  } else {
    out << s;
  }
  return out;
}
std::ostream &Colored(std::ostream &out, absl::string_view s, Color c) {
  if (IsInteractiveTerminalSession(out) && c != Color::kNone) {
    out << kColorsStart[static_cast<uint32_t>(c)] << s << kNormalEscape;
  } else {
    out << s;
  }
  return out;
}

}  // namespace term
}  // namespace verible
