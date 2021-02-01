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

#include "common/util/user_interaction.h"

#include <iostream>
#include <string>

#include "absl/strings/string_view.h"

namespace verible {

char ReadCharFromUser(std::istream& input, std::ostream& output,
                      bool input_is_terminal, absl::string_view prompt) {
  if (input_is_terminal) {
    // Terminal input: print prompt, read whole line and return first character.
    output << prompt << std::flush;

    std::string line;
    std::getline(input, line);

    if (input.eof() || input.fail()) {
      return '\0';
    }
    return line.empty() ? '\n' : line.front();
  } else {
    // Input from a file or pipe: no prompt, read single character.
    char c;
    input.get(c);
    if (input.eof() || input.fail()) {
      return '\0';
    }
    return c;
  }
}

}  // namespace verible
