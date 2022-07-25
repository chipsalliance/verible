// Copyright 2017-2020 The Verible Authors.
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

#include "common/util/cmd_positional_arguments.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace verible {

// Sets a file argument.
absl::Status CmdPositionalArguments::SetFile(absl::string_view file) {
  files_.push_back(file);
  return absl::OkStatus();
}

// Sets a define arguments.
absl::Status CmdPositionalArguments::SetDefine(
    std::pair<absl::string_view, absl::string_view> define) {
  defines_.push_back(define);
  return absl::OkStatus();
}

// Sets a include directory argument.
absl::Status CmdPositionalArguments::SetIncludeDir(
    absl::string_view include_dir) {
  include_dirs_.push_back(include_dir);
  return absl::OkStatus();
}

// Gets include directories arguments.
std::vector<absl::string_view> CmdPositionalArguments::GetIncludeDirs() {
  return include_dirs_;
}

std::vector<std::pair<absl::string_view, absl::string_view>>

// Gets macro defines arguments.
CmdPositionalArguments::GetDefines() {
  return defines_;
}

// Gets SV files arguments.
std::vector<absl::string_view> CmdPositionalArguments::GetFiles() {
  return files_;
}

// Main function that parses arguments and add them to the correct data memeber.
absl::Status CmdPositionalArguments::ParseArgs() {
  // Positional arguments types:
  // 1) <file>
  // 2) +define+<foo>[=<value>]
  // 3) +incdir+<dir>

  for (int argument_index = 0; argument_index < all_args_.size();
       argument_index++) {
    if (!argument_index)
      continue;  // all_args_[0] is the tool's name, which can be skipped.
    absl::string_view argument = all_args_[argument_index];
    if (argument[0] != '+') {  // the argument is a SV file name.
      if (auto status = SetFile(argument); !status.ok())
        return status;  // add it to the file arguments.
    } else {            // it should be either a define or incdir.
      std::vector<absl::string_view> argument_plus_splitted =
          absl::StrSplit(argument, absl::ByChar('+'), absl::SkipEmpty());
      if (argument_plus_splitted.size() < 2) {
        // Unknown argument.
        return absl::InvalidArgumentError("Unkown argument.");
      }
      absl::string_view plus_argument_type = argument_plus_splitted[0];
      if (absl::StrContains(plus_argument_type, "define")) {
        // define argument.
        for (int define_argument_index = 1;
             define_argument_index < argument_plus_splitted.size();
             define_argument_index++) {
          absl::string_view define_argument =
              argument_plus_splitted[define_argument_index];
          // define_argument is something like <macro1>=<value>.
          std::pair<absl::string_view, absl::string_view> macro_pair =
              absl::StrSplit(
                  define_argument, absl::ByChar('='),
                  absl::SkipEmpty());  // parse the macro name and value.
          if (auto status = CmdPositionalArguments::SetDefine(macro_pair);
              !status.ok())
            return status;  // add the define argument.
        }
      } else if (absl::StrContains(plus_argument_type, "incdir")) {
        // incdir argument.
        for (int incdir_argument_index = 1;
             incdir_argument_index < argument_plus_splitted.size();
             incdir_argument_index++) {
          absl::string_view incdir_argument =
              argument_plus_splitted[incdir_argument_index];
          if (auto status =
                  CmdPositionalArguments::SetIncludeDir(incdir_argument);
              !status.ok())
            return status;  // add file argument.
        }
      } else {
        return absl::InvalidArgumentError("Unkown argument.");
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace verible
