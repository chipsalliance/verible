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

#ifndef VERIBLE_COMMON_UTIL_INIT_COMMAND_LINE_H_
#define VERIBLE_COMMON_UTIL_INIT_COMMAND_LINE_H_

#include <vector>

#include "absl/strings/string_view.h"

namespace verible {

// Get a one-line build version string that based on the repository version.
std::string GetRepositoryVersion();

// Initializes command-line tool, including parsing flags.
// The recognized flags are initialized and their text removed from the
// input command line, returning the remaining positional parameters.
// Positional parameters after `--` are returned as-is and not interpreted
// as flags.
// Returns positional arguments, where element[0] is the program name.
std::vector<absl::string_view> InitCommandLine(absl::string_view usage,
                                               int* argc, char*** argv);

}  // namespace verible

#endif  // VERIBLE_COMMON_UTIL_INIT_COMMAND_LINE_H_
