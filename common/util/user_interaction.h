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

#ifndef VERIBLE_COMMON_UTIL_USER_INTERACTION_H_
#define VERIBLE_COMMON_UTIL_USER_INTERACTION_H_

#include <iostream>

#include "absl/strings/string_view.h"

namespace verible {
namespace term {
// Convenience functions: Print bold or inverse if and only if this
// is an interactive session connected to a terminal. Otherwise just plain.
std::ostream& bold(std::ostream& out, absl::string_view s);
std::ostream& inverse(std::ostream& out, absl::string_view s);

enum Color {
  kGreen = 0,
  kCyan,
  kRed,
  kYellow,
  kNone,
  kNumColors,
};

// Start color string to be sent to an output stream. If we're not in an
// interactive setting, no changes are made.
absl::string_view StartColor(const std::ostream&, Color c);

// End color string to be sent to an output stream. If we're not in an
// interactive setting, no changes are made. If there is no color, an empty
// string is sent. We require a color as input parameter because we might want
// to color to Color::NONE (to simplify code, MarkedLine's operator <<), so we
// have to if this is the case to send an empty string.
//
// The default color makes it so we can call EndColor() if we know 100%
// we're coloring to SOME color.
absl::string_view EndColor(const std::ostream&, Color c = Color::kGreen);

}  // namespace term

// Returns if the stream is likely a terminal session: stream is connected to
// terminal and stdin is a terminal.
bool IsInteractiveTerminalSession(const std::ostream& s);

// Reads single character from user.
//
// When `input_is_terminal` is set to true, it is assumed the input is
// interactive. In this mode:
// * `prompt` is printed to `output` before reading anything.
// * The input must be confirmed with the Enter key. If a user typed more than
//   one character, first one is returned and the rest is dropped.
// In non-interactive mode (input_is_terminal=false):
// * `prompt` is not printed.
// * Exaclty one character is read from input and returned.
// * '\0' is returned on EOF.
//
// Typical use:
//
//   const char ch = ReadCharFromUser(std::cin, std::cout,
//                                    IsInteractiveTerminalSession(std::cout),
//                                    "Type a letter and confirm with ENTER: ");
char ReadCharFromUser(std::istream& input, std::ostream& output,
                      bool input_is_terminal, absl::string_view prompt);

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_USER_INTERACTION_H_
