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

#ifndef VERIBLE_COMMON_UTIL_CMD_POSITIONAL_ARGUMENTS_H_
#define VERIBLE_COMMON_UTIL_CMD_POSITIONAL_ARGUMENTS_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace verible {

class CmdPositionalArguments {
 public:
  explicit CmdPositionalArguments(std::vector<char *> &args)
      : all_args_(std::move(args)){};

  // Main function that parses arguments.
  absl::Status ParseArgs();

  // Gets include directories arguments.
  std::vector<absl::string_view> GetIncludeDirs();

  // Gets macro defines arguments.
  std::vector<std::pair<absl::string_view, absl::string_view>> GetDefines();

  // Gets SV files arguments.
  std::vector<absl::string_view> GetFiles();

 private:
  // Sets a include directory argument.
  absl::Status SetIncludeDir(absl::string_view include_dir);

  // Sets a define arguments.
  absl::Status SetDefine(
      std::pair<absl::string_view, absl::string_view> define);

  // Sets a file argument.
  absl::Status SetFile(absl::string_view file);

  std::vector<char *>
      all_args_;  // contains all arguments (tool's name is included).
  std::vector<absl::string_view>
      include_dirs_;  // contains all arugments that follows +incdir+<dir>.
  std::vector<absl::string_view>
      files_;  // contains all SV files passed to the tool.
  std::vector<std::pair<absl::string_view, absl::string_view>>
      defines_;  // contains all arguments that follow +define+<name>[=<value>]
                 // as a pair<name,value>.
};               // class CmdPositionalArguments

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_CMD_POSITIONAL_ARGUMENTS_H_
