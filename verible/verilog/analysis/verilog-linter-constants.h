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

#ifndef VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_CONSTANTS_H_
#define VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_CONSTANTS_H_

#include <string_view>

namespace verilog {

// This is the leading string that makes a comment a lint waiver.
inline constexpr std::string_view kLinterTrigger = "verilog_lint:";

// This command says to waive one line (this or next applicable).
inline constexpr std::string_view kLinterWaiveLineCommand = "waive";

// This command says to start waiving a rule from this line...
inline constexpr std::string_view kLinterWaiveStartCommand = "waive-start";

// ... and stop waiving at this line.
inline constexpr std::string_view kLinterWaiveStopCommand = "waive-stop";

}  // namespace verilog

#endif  // VERIBLE_VERILOG_ANALYSIS_VERILOG_LINTER_CONSTANTS_H_
