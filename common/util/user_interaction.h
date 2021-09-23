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
// Convenience functions that wrap a string to output colored on screen, iff
// this is an interactive session.
std::string bold(absl::string_view s);
std::string inverse(absl::string_view s);
}  // namespace term

// Returns if this is likely a terminal session (tests if stdin filedescriptor
// is a terminal).
bool IsInteractiveTerminalSession();

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
//                                    IsInteractiveTerminalSession(),
//                                    "Type a letter and confirm with ENTER: ");
char ReadCharFromUser(std::istream& input, std::ostream& output,
                      bool input_is_terminal, absl::string_view prompt);

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_USER_INTERACTION_H_
